#include <benchmark/benchmark.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <string>
#include <memory>
#include <chrono>
#include <shared_mutex>
#include <mutex>

// 简化的配置管理器用于基准测试
class SimpleConfigManager {
private:
    nlohmann::json config_;
    mutable std::shared_mutex mutex_;
    
public:
    bool loadFromFile(const std::string& path) {
        try {
            std::ifstream file(path);
            if (!file.is_open()) return false;
            file >> config_;
            return true;
        } catch (...) {
            return false;
        }
    }
    
    public:
    bool loadFromFile(const std::string& path) {
        try {
            std::unique_lock lock(mutex_);
            std::ifstream file(path);
            if (!file.is_open()) {
                return false;
            }
            file >> config_;
            return true;
        } catch (...) {
            return false;
        }
    }
    
    template<typename T>
    T getValue(const std::string& key, const T& defaultValue = T{}) const {
    
    bool setValue(const std::string& key, const nlohmann::json& value) {
        std::unique_lock lock(mutex_);
        try {
            config_[key] = value;
            return true;
        } catch (...) {
            return false;
        }
    }
    
    size_t getCacheSize() const {
        std::shared_lock lock(mutex_);
        return config_.size();
    }
};

class ConfigManagerBenchmark : public benchmark::Fixture {
public:
    void SetUp(const ::benchmark::State& state) override {
        // 创建测试配置文件
        createTestConfigFile();
        
        // 初始化ConfigManager
        config_manager_ = std::make_unique<SimpleConfigManager>();
        if (!config_manager_->loadFromFile(test_config_path_)) {
            throw std::runtime_error("Failed to load test config");
        }
    }
    
    void TearDown(const ::benchmark::State& state) override {
        // 清理测试文件
        std::filesystem::remove(test_config_path_);
    }

protected:
    std::unique_ptr<SimpleConfigManager> config_manager_;
    std::string test_config_path_ = "benchmark_config.json";
    
    void createTestConfigFile() {
        nlohmann::json config;
        
        // 创建多层嵌套的配置结构
        config["server"]["host"] = "localhost";
        config["server"]["port"] = 9002;
        config["server"]["auth"]["token"] = "test_token_12345";
        config["server"]["auth"]["timeout"] = 5000;
        
        config["client"]["reconnect"]["enabled"] = true;
        config["client"]["reconnect"]["max_attempts"] = 5;
        config["client"]["reconnect"]["interval"] = 1000;
        
        config["network"]["websocket"]["timeout"] = 3000;
        config["network"]["udp"]["discovery_port"] = 9003;
        config["network"]["udp"]["broadcast_interval"] = 1000;
        
        config["logging"]["level"] = "INFO";
        config["logging"]["file_path"] = "/var/log/picoradar.log";
        config["logging"]["max_file_size"] = "10MB";
        
        // 添加大量的测试键用于性能测试
        for (int i = 0; i < 1000; ++i) {
            config["test"]["key_" + std::to_string(i)] = "value_" + std::to_string(i);
        }
        
        std::ofstream file(test_config_path_);
        file << config.dump(2);
    }
};

// 测试基本配置读取性能
BENCHMARK_F(ConfigManagerBenchmark, BasicConfigRead)(benchmark::State& state) {
    for (auto _ : state) {
        auto result = config_manager_->getValue<std::string>("server.host", "default");
        benchmark::DoNotOptimize(result);
    }
}

// 测试深层嵌套配置读取
BENCHMARK_F(ConfigManagerBenchmark, DeepNestedConfigRead)(benchmark::State& state) {
    for (auto _ : state) {
        auto result = config_manager_->getValue<std::string>("server.auth.token", "default");
        benchmark::DoNotOptimize(result);
    }
}

// 测试数值配置读取
BENCHMARK_F(ConfigManagerBenchmark, IntegerConfigRead)(benchmark::State& state) {
    for (auto _ : state) {
        auto result = config_manager_->getValue<int>("server.port", 8080);
        benchmark::DoNotOptimize(result);
    }
}

// 测试布尔配置读取
BENCHMARK_F(ConfigManagerBenchmark, BooleanConfigRead)(benchmark::State& state) {
    for (auto _ : state) {
        auto result = config_manager_->getValue<bool>("client.reconnect.enabled", false);
        benchmark::DoNotOptimize(result);
    }
}

// 测试大量随机键的读取性能（模拟缓存命中/未命中）
BENCHMARK_F(ConfigManagerBenchmark, RandomKeyAccess)(benchmark::State& state) {
    std::vector<std::string> keys;
    for (int i = 0; i < 100; ++i) {
        keys.push_back("test.key_" + std::to_string(i * 10));
    }
    
    size_t key_index = 0;
    for (auto _ : state) {
        auto result = config_manager_->getValue<std::string>(keys[key_index % keys.size()], "default");
        benchmark::DoNotOptimize(result);
        ++key_index;
    }
}

// 测试并发配置读取
BENCHMARK_F(ConfigManagerBenchmark, ConcurrentConfigRead)(benchmark::State& state) {
    for (auto _ : state) {
        auto result = config_manager_->getValue<std::string>("server.host", "default");
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK_REGISTER_F(ConfigManagerBenchmark, ConcurrentConfigRead)->Threads(1)->Threads(2)->Threads(4)->Threads(8);

// 测试配置重载性能
BENCHMARK_F(ConfigManagerBenchmark, ConfigReload)(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        createTestConfigFile(); // 重新创建配置文件
        state.ResumeTiming();
        
        auto result = config_manager_->loadFromFile(test_config_path_);
        benchmark::DoNotOptimize(result);
    }
}

// 测试缓存大小操作的性能
BENCHMARK_F(ConfigManagerBenchmark, CacheSizeOperation)(benchmark::State& state) {
    for (auto _ : state) {
        size_t cache_size = config_manager_->getCacheSize();
        benchmark::DoNotOptimize(cache_size);
    }
}

// 测试不存在键的查询性能
BENCHMARK_F(ConfigManagerBenchmark, NonExistentKeyLookup)(benchmark::State& state) {
    for (auto _ : state) {
        auto result = config_manager_->getValue<std::string>("non.existent.key", "default");
        benchmark::DoNotOptimize(result);
    }
}

// 测试配置写入性能
BENCHMARK_F(ConfigManagerBenchmark, ConfigWrite)(benchmark::State& state) {
    for (auto _ : state) {
        bool result = config_manager_->setValue("benchmark.test", "test_value");
        benchmark::DoNotOptimize(result);
    }
}

// 测试JSON序列化性能
BENCHMARK_F(ConfigManagerBenchmark, JSONParsing)(benchmark::State& state) {
    const std::string json_str = R"({
        "server": {
            "host": "localhost",
            "port": 9002,
            "auth": {
                "token": "test_token",
                "timeout": 5000
            }
        },
        "client": {
            "reconnect": {
                "enabled": true,
                "max_attempts": 5
            }
        }
    })";
    
    for (auto _ : state) {
        nlohmann::json parsed = nlohmann::json::parse(json_str);
        benchmark::DoNotOptimize(parsed);
    }
}

// 添加benchmark的main函数
BENCHMARK_MAIN();
