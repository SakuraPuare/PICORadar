---
title: "开发日志 #11：驯龙记——重构CMake与核心功能的新生"
date: 2024-05-22
author: "书樱"
tags: ["C++", "CMake", "WebSocket", "Protobuf", "PICO", "VR", "DevLog", "Refactoring"]
---

## 前言：风暴前的宁静

大家晚上好，我是书樱！

今天的开发经历，用“跌宕起伏”来形容真是再贴切不过了。我们最初的目标只是修复一个看似微不足道的 CMake 链接错误，没想到却像捅了马蜂窝，引发了一连串的连锁问题。我清晰地记得那个下午，阳光正好，代码库也看似一片祥和。然而，终端回应我的，却不是熟悉的配置完成信息，而是一片刺眼的红色错误。

我当时天真地以为这只是一个小问题，但未曾预料到，这只是一条线头。当我轻轻一拉，整个毛衣——我们项目整个错综复杂的依赖关系网——开始在我面前分崩离析。

这，就是我们与那条名为“技术债”的 CMake 巨龙搏斗的开始。最终，我们不仅驯服了这条巨龙，更是在一片清爽的地基上，建起了系统最核心的功能高塔。

## 第一章：与 CMake 巨龙的四回合搏斗

一切始于一个经典的链接错误：`undefined reference to 'google::LogMessage::stream()'`。这明显是 `glog` 库没有被正确链接。然而，这只是冰山一角。

### 第一战：初探龙穴，依赖之链的连锁反应

我们很快发现，问题远非表面那么简单。修复了一个链接，又冒出无数个“头文件找不到”的错误。项目的模块化本是我们的骄傲，但现在，模块间的通信线路（`#include` 路径和库链接）却已然锈迹斑斑。我们意识到，不能再打补丁了，必须进行一次彻底的、自上而下的重构。

**核心诊断**：
*   **链接目标名称不一致**：`picoradar_common` vs `common_lib`。
*   **重复链接**：底层库已 `PUBLIC` 链接的依赖，上层还在重复链接。
*   **路径管理混乱**：缺乏统一的“src 根目录”概念。

### 第二战：军阀割据的终结 - 统一依赖管理

我们采用现代 CMake 的最佳实践，将每个模块都改造成了职责清晰、接口干净的“独立王国”。

例如，我们的 `common` 库被改造成这样，清晰地声明了它的源文件、公开头文件以及它对外暴露的依赖 `glog::glog`：

```cmake
# src/common/CMakeLists.txt
add_library(common_lib STATIC)

# ... target_sources and other configurations ...

# 使用导入目标 glog::glog，并将其 PUBLIC 传播给依赖 common_lib 的目标
target_link_libraries(common_lib PUBLIC glog::glog)
```

`core_logic` 库则清晰地声明了它对 `common_lib` 和 `proto_gen` 的依赖。依赖关系像一条清晰的河流，从下游顺畅地流向上游。

### 第三战：釜底抽薪 - `project_includes` 的诞生

理论是丰满的，但“找不到头文件”的报错依然顽固。我们犯了一个典型的“只见树木，不见森林”的错误：代码里写的是 `#include "common/constants.hpp"`，而编译器只被告知在 `src/common` 目录里查找，它当然找不到 `common/` 这个子目录。

必须釜底抽薪！我们决定在根 `CMakeLists.txt` 中建立一个“中央集权”的头文件管理机构——一个名为 `project_includes` 的**接口库 (INTERFACE library)**。

```cmake
# 根 CMakeLists.txt
add_library(project_includes INTERFACE)
target_include_directories(project_includes INTERFACE
    "${CMAKE_SOURCE_DIR}/src"  # 项目代码的根目录
    "${CMAKE_SOURCE_DIR}/test" # 测试代码的根目录
)
```
这个库像一个“圣旨”，所有链接到它的目标，都会自动被告知：“你们要找的头文件，都从 `/src` 和 `/test` 这两个地方开始找！”

### 第四战：测试系统的最终统一

在我们解决了所有依赖和头文件问题后，GTest 又开始“全面罢工”，大量的 `undefined reference to 'testing::...'` 涌现。

我们再次意识到，测试部分的 CMake 配置存在很多重复和疏漏。为了从根本上解决问题，我们创建了一个共享的、包含我们自定义 `main` 函数的 **`OBJECT` 库**，让所有测试目标都依赖它。

```cmake
# test/CMakeLists.txt
# 将我们的自定义 main.cpp 编译成一个“对象库”
add_library(gtest_main_obj OBJECT main.cpp)

# 这个对象库本身需要链接 common_lib（为了日志）和 GTest
target_link_libraries(gtest_main_obj PRIVATE common_lib GTest::gtest GTest::gmock)
```

从此，每个测试的 `CMakeLists.txt` 都变得极其简洁，只需引用这个 `gtest_main_obj` 对象库，就自动拥有了正确的入口点和所有必要的链接。

## 第二章：雨过天晴，核心功能的新生

在驯服了 CMake 这条巨龙，为项目打下坚实的地基后，我们终于可以将精力集中在核心功能的开发上。之前被阻塞的思路豁然开朗，功能实现一蹴而就。

### 1. 增强的日志系统

我们首先创建了一个专用的日志模块 (`src/common/logging.hpp|cpp`)，提供了一个简单的 `setup_logging` 函数，告别了繁琐的glog flags手动设置。

```cpp
// src/common/logging.hpp
namespace picoradar::common {
    void setup_logging(std::string_view app_name, bool log_to_file = true,
                       const std::string& log_dir = "./logs");
}
```

### 2. WebSocket 服务器的“完全体”

今天最大的突破，无疑是 `WebsocketServer` 的全面升级。它现在具备了处理真实业务逻辑的能力。当看到服务器日志里打印出成功向所有客户端广播玩家列表时，那种“苦尽甘甘甘甘甘来”的成就感，足以冲淡之前所有的沮丧。

**核心流程如下**：

1.  **接收数据**：客户端通过 WebSocket 发送一个序列化后的 `ClientToServer` Protobuf 消息。
2.  **解析消息**：服务器的 `processMessage` 函数接收到二进制数据，并将其反序列化为 `ClientToServer` 对象。
3.  **更新状态**：服务器从消息中提取出 `PlayerData`，并调用 `PlayerRegistry::updatePlayer` 来更新该玩家的最新位置和状态。如果这是玩家第一次发消息，我们还会将其 ID 与当前的 `Session` 关联起来，完成“注册”。
4.  **广播状态**：更新完玩家状态后，服务器会立即调用 `broadcastPlayerList`。

`broadcastPlayerList` 是我们系统的“心脏”，它的逻辑很简单，但至关重要：

```cpp
// src/network/websocket_server.cpp (简化后)
void WebsocketServer::broadcastPlayerList() {
    // 1. 创建一个 ServerToClient 消息
    ServerToClient response;
    auto* player_list = response.mutable_player_list();
    
    // 2. 从 PlayerRegistry 获取所有玩家的数据，并填充到消息中
    for(const auto& player_pair : registry_.getAllPlayers()) {
        auto* player_data = player_list->add_players();
        player_data->CopyFrom(player_pair.second);
    }
    
    // 3. 将消息序列化成字符串
    std::string serialized_response;
    response.SerializeToString(&serialized_response);

    // 4. 遍历所有已连接的客户端 Session，将完整的玩家列表发送给他们
    for (const auto& session : sessions_) {
        session->send(serialized_response);
    }
}
```

此外，当一个客户端断开连接时 (`onSessionClosed`)，我们会从 `PlayerRegistry` 中移除该玩家，并再次广播最新的玩家列表，确保所有客户端都能实时同步到“某人已下线”的状态。

## 总结与展望

今天，我们从构建系统的泥潭中挣脱，不仅加固了项目的工程化基础，更一鼓作气地实现了PICO Radar最核心的数据同步功能。服务器不再是一个空架子，它已经变成了一个能与客户端进行有意义数据交换的、真正意义上的“雷达站”。

这次经历也再次证明了，坚实的工程基础（如清晰的构建系统和规范的代码结构）是快速推进功能开发的关键。

接下来，我们的重心将毫无疑问地转移到 `client_lib` 的实现上。前方的道路越来越清晰，让我们继续前进吧！

感谢大家的阅读，我们下期再见！
--- 