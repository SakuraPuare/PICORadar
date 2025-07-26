#include <benchmark/benchmark.h>
#include <thread>
#include <shared_mutex>
#include <chrono>
#include <random>
#include <string>
#include <memory>
#include <nlohmann/json.hpp>
#include <fstream>

#include "player_registry.hpp"
#include "player.pb.h"

using namespace picoradar;
using namespace picoradar::core;

// 生成测试数据的工具函数
static PlayerData createTestPlayer(const std::string& id) {
    PlayerData player;
    player.set_player_id(id);
    player.set_scene_id("test_scene");
    
    // 设置位置
    auto* position = player.mutable_position();
    position->set_x(100.0f);
    position->set_y(200.0f);
    position->set_z(300.0f);
    
    // 设置旋转
    auto* rotation = player.mutable_rotation();
    rotation->set_x(0.0f);
    rotation->set_y(0.0f);
    rotation->set_z(0.0f);
    rotation->set_w(1.0f);
    
    player.set_timestamp(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    return player;
}

// PlayerRegistry 基准测试
static void BM_PlayerRegistry_UpdatePlayer(benchmark::State& state) {
    PlayerRegistry registry;
    
    for (auto _ : state) {
        std::string player_id = "player_" + std::to_string(state.iterations());
        PlayerData player_data = createTestPlayer(player_id);
        
        registry.updatePlayer(player_id, std::move(player_data));
        benchmark::DoNotOptimize(player_id);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_PlayerRegistry_UpdatePlayer);

// 获取玩家数据性能测试
static void BM_PlayerRegistry_GetPlayer(benchmark::State& state) {
    PlayerRegistry registry;
    const int num_players = state.range(0);
    
    // 预填充数据
    for (int i = 0; i < num_players; ++i) {
        std::string player_id = "player_" + std::to_string(i);
        PlayerData player_data = createTestPlayer(player_id);
        registry.updatePlayer(player_id, std::move(player_data));
    }
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, num_players - 1);
    
    for (auto _ : state) {
        std::string player_id = "player_" + std::to_string(dis(gen));
        auto player = registry.getPlayer(player_id);
        benchmark::DoNotOptimize(player);
    }
    
    state.SetItemsProcessed(state.iterations());
    state.SetComplexityN(num_players);
}
BENCHMARK(BM_PlayerRegistry_GetPlayer)->RangeMultiplier(2)->Range(8, 8192)->Complexity();

// 获取所有玩家性能测试
static void BM_PlayerRegistry_GetAllPlayers(benchmark::State& state) {
    PlayerRegistry registry;
    const int num_players = state.range(0);
    
    // 预填充数据
    for (int i = 0; i < num_players; ++i) {
        std::string player_id = "player_" + std::to_string(i);
        PlayerData player_data = createTestPlayer(player_id);
        registry.updatePlayer(player_id, std::move(player_data));
    }
    
    for (auto _ : state) {
        auto all_players = registry.getAllPlayers();
        benchmark::DoNotOptimize(all_players);
    }
    
    state.SetItemsProcessed(state.iterations());
    state.SetComplexityN(num_players);
}
BENCHMARK(BM_PlayerRegistry_GetAllPlayers)->RangeMultiplier(2)->Range(8, 1024)->Complexity();

// Protobuf 序列化性能测试
static void BM_Protobuf_Serialization(benchmark::State& state) {
    PlayerData player = createTestPlayer("test_player");
    
    for (auto _ : state) {
        std::string serialized_data;
        player.SerializeToString(&serialized_data);
        benchmark::DoNotOptimize(serialized_data);
    }
    
    state.SetBytesProcessed(state.iterations() * player.ByteSizeLong());
}
BENCHMARK(BM_Protobuf_Serialization);

// Protobuf 反序列化性能测试
static void BM_Protobuf_Deserialization(benchmark::State& state) {
    PlayerData original_player = createTestPlayer("test_player");
    std::string serialized_data;
    original_player.SerializeToString(&serialized_data);
    
    for (auto _ : state) {
        PlayerData deserialized_player;
        deserialized_player.ParseFromString(serialized_data);
        benchmark::DoNotOptimize(deserialized_player);
    }
    
    state.SetBytesProcessed(state.iterations() * serialized_data.size());
}
BENCHMARK(BM_Protobuf_Deserialization);

// 并发访问性能测试
static void BM_PlayerRegistry_ConcurrentAccess(benchmark::State& state) {
    static PlayerRegistry registry;
    const int num_threads = state.range(0);
    
    // 预填充一些数据
    static std::once_flag init_flag;
    std::call_once(init_flag, []() {
        for (int i = 0; i < 100; ++i) {
            std::string player_id = "player_" + std::to_string(i);
            PlayerData player_data = createTestPlayer(player_id);
            registry.updatePlayer(player_id, std::move(player_data));
        }
    });
    
    for (auto _ : state) {
        // 模拟多线程操作
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 99);
        
        for (int i = 0; i < 10; ++i) {
            if (i % 3 == 0) {
                // 更新操作
                std::string player_id = "player_" + std::to_string(dis(gen));
                PlayerData player_data = createTestPlayer(player_id);
                registry.updatePlayer(player_id, std::move(player_data));
            } else if (i % 3 == 1) {
                // 查询操作
                std::string player_id = "player_" + std::to_string(dis(gen));
                auto player = registry.getPlayer(player_id);
                benchmark::DoNotOptimize(player);
            } else {
                // 获取所有玩家
                auto all_players = registry.getAllPlayers();
                benchmark::DoNotOptimize(all_players);
            }
        }
    }
    
}
BENCHMARK(BM_PlayerRegistry_ConcurrentAccess)->RangeMultiplier(2)->Range(1, 16);

// 内存使用模式测试
static void BM_Memory_PlayerDataSize(benchmark::State& state) {
    const int num_players = state.range(0);
    std::vector<PlayerData> players;
    players.reserve(num_players);
    
    for (auto _ : state) {
        players.clear();
        
        for (int i = 0; i < num_players; ++i) {
            players.emplace_back(createTestPlayer("player_" + std::to_string(i)));
        }
        
        benchmark::DoNotOptimize(players);
    }
    
    size_t total_memory = num_players * sizeof(PlayerData);
    state.SetBytesProcessed(state.iterations() * total_memory);
    state.SetComplexityN(num_players);
}
BENCHMARK(BM_Memory_PlayerDataSize)->RangeMultiplier(2)->Range(8, 1024)->Complexity();

// ============================================================================
// 配置管理基准测试
// ============================================================================

// 简化的配置管理器用于基准测试
class SimpleConfigManager {
private:
    nlohmann::json config_;
    mutable std::shared_mutex mutex_;
    
public:
    bool loadFromString(const std::string& json_str) {
        try {
            std::unique_lock lock(mutex_);
            config_ = nlohmann::json::parse(json_str);
            return true;
        } catch (...) {
            return false;
        }
    }
    
    template<typename T>
    T getValue(const std::string& key, const T& defaultValue = T{}) const {
        std::shared_lock lock(mutex_);
        try {
            return config_.value(key, defaultValue);
        } catch (...) {
            return defaultValue;
        }
    }
    
    size_t getCacheSize() const {
        std::shared_lock lock(mutex_);
        return config_.size();
    }
};

// JSON解析性能测试
static void BM_Config_JSONParsing(benchmark::State& state) {
    const std::string json_str = R"({
        "server": {
            "host": "localhost",
            "port": 9002,
            "auth": {
                "token": "test_token_12345",
                "timeout": 5000
            }
        },
        "client": {
            "reconnect": {
                "enabled": true,
                "max_attempts": 5,
                "interval": 1000
            }
        },
        "network": {
            "websocket": {
                "timeout": 3000
            },
            "udp": {
                "discovery_port": 9003,
                "broadcast_interval": 1000
            }
        }
    })";
    
    for (auto _ : state) {
        nlohmann::json parsed = nlohmann::json::parse(json_str);
        benchmark::DoNotOptimize(parsed);
    }
    
    state.SetBytesProcessed(state.iterations() * json_str.size());
}
BENCHMARK(BM_Config_JSONParsing);

// 配置读取性能测试
static void BM_Config_ValueAccess(benchmark::State& state) {
    SimpleConfigManager config;
    const std::string json_str = R"({
        "server": {"host": "localhost", "port": 9002},
        "network": {"timeout": 3000}
    })";
    
    config.loadFromString(json_str);
    
    for (auto _ : state) {
        auto host = config.getValue<std::string>("server.host", "default");
        auto port = config.getValue<int>("server.port", 8080);
        benchmark::DoNotOptimize(host);
        benchmark::DoNotOptimize(port);
    }
}
BENCHMARK(BM_Config_ValueAccess);

// ============================================================================
// 网络序列化基准测试
// ============================================================================

// 复杂消息序列化测试
static void BM_Network_ComplexSerialization(benchmark::State& state) {
    PlayerData player = createTestPlayer("complex_player_123");
    
    // 添加更多复杂数据
    player.set_scene_id("complex_scene_with_long_name_for_testing");
    
    for (auto _ : state) {
        std::string serialized;
        bool success = player.SerializeToString(&serialized);
        benchmark::DoNotOptimize(success);
        benchmark::DoNotOptimize(serialized);
    }
    
    state.SetBytesProcessed(state.iterations() * player.ByteSizeLong());
}
BENCHMARK(BM_Network_ComplexSerialization);

// 复杂消息反序列化测试
static void BM_Network_ComplexDeserialization(benchmark::State& state) {
    PlayerData original = createTestPlayer("complex_player_123");
    original.set_scene_id("complex_scene_with_long_name_for_testing");
    
    std::string serialized;
    original.SerializeToString(&serialized);
    
    for (auto _ : state) {
        PlayerData deserialized;
        bool success = deserialized.ParseFromString(serialized);
        benchmark::DoNotOptimize(success);
        benchmark::DoNotOptimize(deserialized);
    }
    
    state.SetBytesProcessed(state.iterations() * serialized.size());
}
BENCHMARK(BM_Network_ComplexDeserialization);

// ============================================================================
// 综合性能测试
// ============================================================================

// 模拟实际应用场景：多玩家更新
static void BM_Realistic_MultiPlayerUpdate(benchmark::State& state) {
    PlayerRegistry registry;
    const int num_players = state.range(0);
    
    // 预先注册玩家
    for (int i = 0; i < num_players; ++i) {
        auto player = createTestPlayer("player_" + std::to_string(i));
        registry.updatePlayer(player.player_id(), std::move(player));
    }
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, num_players - 1);
    
    for (auto _ : state) {
        // 模拟随机玩家更新
        int player_idx = dis(gen);
        auto player = createTestPlayer("player_" + std::to_string(player_idx));
        
        // 修改位置以模拟真实更新
        auto* pos = player.mutable_position();
        pos->set_x(pos->x() + 1.0f);
        pos->set_y(pos->y() + 1.0f);
        pos->set_z(pos->z() + 1.0f);
        
        registry.updatePlayer(player.player_id(), std::move(player));
        
        // 偶尔查询其他玩家
        if (player_idx % 10 == 0) {
            auto other_player = registry.getPlayer("player_" + std::to_string((player_idx + 1) % num_players));
            benchmark::DoNotOptimize(other_player);
        }
    }
    
    state.SetComplexityN(num_players);
}
BENCHMARK(BM_Realistic_MultiPlayerUpdate)->RangeMultiplier(2)->Range(8, 256)->Complexity();

// 模拟广播场景：获取所有玩家
static void BM_Realistic_BroadcastScenario(benchmark::State& state) {
    PlayerRegistry registry;
    const int num_players = state.range(0);
    
    // 预先注册玩家
    for (int i = 0; i < num_players; ++i) {
        auto player = createTestPlayer("player_" + std::to_string(i));
        registry.updatePlayer(player.player_id(), std::move(player));
    }
    
    for (auto _ : state) {
        auto all_players = registry.getAllPlayers();
        
        // 模拟序列化所有玩家数据
        size_t total_size = 0;
        for (const auto& [player_id, player_data] : all_players) {
            total_size += player_data.ByteSizeLong();
        }
        
        benchmark::DoNotOptimize(all_players);
        benchmark::DoNotOptimize(total_size);
    }
    
    state.SetComplexityN(num_players);
}
BENCHMARK(BM_Realistic_BroadcastScenario)->RangeMultiplier(2)->Range(8, 256)->Complexity();

BENCHMARK_MAIN();
