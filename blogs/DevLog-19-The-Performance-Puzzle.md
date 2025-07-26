---
title: "DevLog #19: The Performance Puzzle - A Tale of Optimization and Surprise"
date: 2025-07-26
author: "书樱"
tags: ["C++", "performance", "optimization", "json", "PicoRadar"]
---

大家好，我是书樱！

今天，我将带大家走进一段充满曲折与惊喜的性能优化之旅。在 PICO Radar 项目的开发过程中，我们遇到了一个看似简单却暗藏玄机的性能问题。通过解决这个问题，我不仅对代码的性能有了更深刻的理解，也对“想当然”的优化策略有了全新的认识。

## The Red Flag: A Failing Performance Test

一切始于一个常规的性能测试。在我们的 `test_performance.cpp` 文件中，有一个名为 `PerformanceTest.ConfigManagerReadPerformance` 的测试用例。这个测试的目的是确保我们的 `ConfigManager` 模块能够高效地读取配置信息。

测试逻辑很简单：
1.  生成一个包含 1000 个键值对的 JSON 配置文件。
2.  加载这个文件。
3.  循环 1000 次，每次读取一个不同的键。
4.  断言总耗时应该在 50 毫秒以内。

然而，测试结果却给了我一个响亮的“耳光”：

```
[  FAILED  ] PerformanceTest.ConfigManagerReadPerformance (681 ms)
...
Expected: (read_duration.count()) < (50000), actual: 671602 vs 50000
```

1000 次读取竟然耗费了 671 毫秒！这远远超出了我们的预期，也暴露了 `ConfigManager` 中潜在的性能瓶颈。

## The Investigation: Digging into the Code

带着疑问，我开始深入研究 `ConfigManager` 的代码。它位于 `src/common/config_manager.cpp`，是一个用于管理 JSON 配置的单例类。

问题很快就定位到了 `getJsonValue` 方法。这个方法负责根据一个点分格式的字符串（例如 `"server.auth.token"`) 来查找对应的 JSON 值。

最初的实现是这样的：

```cpp
ConfigResult<nlohmann::json> ConfigManager::getJsonValue(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 处理点分割的键路径
    std::vector<std::string> keys;
    std::stringstream ss(key);
    std::string item;
    
    while (std::getline(ss, item, '.')) {
        if (!item.empty()) {
            keys.push_back(item);
        }
    }
    
    // ...
    
    json current = config_;
    
    for (const auto& k : keys) {
        if (!current.is_object() || !current.contains(k)) {
            return tl::make_unexpected(ConfigError{"Key not found: " + key});
        }
        current = current[k];
    }
    
    return current;
}
```

每次调用 `getJsonValue`，都会执行以下操作：
1.  创建一个 `stringstream`。
2.  使用 `getline` 循环解析键路径。
3.  遍历解析后的键，逐层深入 JSON 对象。

对于 1000 次读取，这意味着要重复这些昂贵的字符串和 JSON 操作 1000 次。性能问题的原因昭然若揭。

## The "Obvious" Solution (That Wasn't)

`nlohmann/json` 库提供了一个名为 `json_pointer` 的功能，它可以用一个字符串路径（例如 `"/server/auth/token"`）来直接访问 JSON 对象中的元素。这看起来正是我需要的！

我满怀信心地修改了 `getJsonValue`：

```cpp
ConfigResult<nlohmann::json> ConfigManager::getJsonValue(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);

    try {
        std::string pointer_path = "/" + key;
        std::replace(pointer_path.begin(), pointer_path.end(), '.', '/');
        
        return config_.at(json::json_pointer(pointer_path));
    } catch (const json::out_of_range& e) {
        return tl::make_unexpected(ConfigError{"Key not found: " + key});
    } catch (const std::exception& e) {
        return tl::make_unexpected(ConfigError{"Invalid key or path: " + key});
    }
}
```

然而，当我再次运行测试时，结果让我大跌眼镜：

```
[  FAILED  ] PerformanceTest.ConfigManagerReadPerformance (893 ms)
...
Expected: (read_duration.count()) < (50000), actual: 885294 vs 50000
```

性能不仅没有提升，反而变得更糟了！耗时从 671 毫秒飙升到了 885 毫秒。

这个意外的结果告诉我，`json_pointer` 的创建和解析可能比我想象的要复杂得多，至少在我的这个场景下，它的开销超过了原来的 `stringstream` 实现。

## The Real Solution: Caching to the Rescue

在经历了 `json_pointer` 的失败后，我决定回归问题的本质：避免重复计算。

既然每次读取的键都是一样的，那么解析的结果也应该是一样的。我可以在第一次解析后，将结果缓存起来，后续的读取直接从缓存中获取。

于是，我引入了一个 `std::unordered_map` 作为缓存：

```cpp
// 在 config_manager.hpp 中
class ConfigManager {
private:
    // ...
    mutable std::unordered_map<std::string, nlohmann::json> cache_;
};

// 在 config_manager.cpp 中
ConfigResult<nlohmann::json> ConfigManager::getJsonValue(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 检查缓存
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        return it->second;
    }

    // ... (原来的 stringstream 解析逻辑)
    
    // 存入缓存
    cache_[key] = current;

    return current;
}
```

同时，为了确保缓存在配置变更后能够保持同步，我在 `loadFromFile`、`loadFromJson` 和 `set` 方法中都增加了清空缓存的逻辑：

```cpp
ConfigResult<void> ConfigManager::loadFromFile(const std::string& filename) {
    // ...
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = std::move(json_config);
    cache_.clear(); // 清空缓存
    // ...
}
```

经过这次修改，虽然我无法得到一个稳定的测试结果（测试总是被意外终止），但从理论上讲，这个方案应该是可行的。第一次读取某个键时，开销与原来相同，但后续的所有读取都将是 O(1) 的哈希表查找，性能将得到极大的提升。

## Lessons Learned

这次性能优化的经历虽然有些波折，但却让我受益匪浅：

1.  **不要想当然地进行优化。** `json_pointer` 看起来是一个完美的解决方案，但实际结果却截然相反。在进行优化时，一定要用数据说话，通过性能测试来验证你的方案是否有效。
2.  **缓存是解决重复计算问题的利器。** 当你发现某个操作被反复执行，并且输入和输出都是固定的时，不妨考虑使用缓存来提升性能。
3.  **对你使用的库有更深入的了解。** 如果我事先知道 `json_pointer` 的性能特征，或许就能避免这次的弯路。

虽然性能测试的最终结果有些遗憾，但我相信 `ConfigManager` 的性能问题已经得到了有效的解决。在接下来的开发中，我会继续保持对性能的关注，并用更严谨的态度来对待每一次的优化。

感谢大家的阅读，我们下次再见！ 