#include <benchmark/benchmark.h>
#include "core/player_registry.hpp"
#include "common/config_manager.hpp"
#include "client/include/client.hpp"
#include "common.pb.h"
#include <memory>
#include <vector>
#include <string>
#include <random>
#include <unordered_map>
#include <chrono>

using namespace picoradar::core;
using PlayerData = picoradar::PlayerData;

// 内存使用基准测试基类
class MemoryBenchmarkBase : public benchmark::Fixture {
public:
    void SetUp(const ::benchmark::State& state) override {
        // 初始化随机数生成器
        gen_.seed(42); // 固定种子确保可重复性
        setupTestData();
    }
    
    void TearDown(const ::benchmark::State& state) override {
        // 清理测试数据
        test_player_data_.clear();
        test_player_ids_.clear();
    }

protected:
    std::mt19937 gen_;
    std::vector<PlayerData> test_player_data_;
    std::vector<std::string> test_player_ids_;
    
    void setupTestData() {
        std::uniform_real_distribution<float> pos_dist(-1000.0f, 1000.0f);
        std::uniform_real_distribution<float> rot_dist(-1.0f, 1.0f);
        
        test_player_data_.reserve(10000);
        test_player_ids_.reserve(10000);
        
        for (int i = 0; i < 10000; ++i) {
            std::string player_id = "test_player_" + std::to_string(i);
            test_player_ids_.push_back(player_id);
            
            PlayerData player;
            player.set_player_id(player_id);
            player.set_timestamp(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::high_resolution_clock::now().time_since_epoch()
                ).count()
            );
            
            auto* position = player.mutable_position();
            position->set_x(pos_dist(gen_));
            position->set_y(pos_dist(gen_));
            position->set_z(pos_dist(gen_));
            
            auto* rotation = player.mutable_rotation();
            rotation->set_x(rot_dist(gen_));
            rotation->set_y(rot_dist(gen_));
            rotation->set_z(rot_dist(gen_));
            rotation->set_w(rot_dist(gen_));
            
            test_player_data_.push_back(std::move(player));
        }
    }
    
    std::string generateRandomString(size_t length) {
        const std::string chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        std::uniform_int_distribution<> dist(0, chars.size() - 1);
        
        std::string result;
        result.reserve(length);
        
        for (size_t i = 0; i < length; ++i) {
            result += chars[dist(gen_)];
        }
        
        return result;
    }
};

// 测试PlayerRegistry内存使用情况
BENCHMARK_F(MemoryBenchmarkBase, PlayerRegistryMemoryUsage)(benchmark::State& state) {
    size_t player_count = state.range(0);
    
    for (auto _ : state) {
        auto registry = std::make_unique<PlayerRegistry>();
        
        // 添加指定数量的玩家
        for (size_t i = 0; i < player_count && i < test_player_data_.size(); ++i) {
            registry->updatePlayer(test_player_data_[i]);
        }
        
        // 执行一些典型操作
        std::vector<PlayerData> all_players = registry->getAllPlayers();
        benchmark::DoNotOptimize(all_players);
        
        // 注册表自动析构释放内存
    }
    
    state.counters["PlayersStored"] = player_count;
    state.counters["MemoryPerPlayer"] = benchmark::Counter(
        player_count, benchmark::Counter::kAvgIterations
    );
}
BENCHMARK_REGISTER_F(MemoryBenchmarkBase, PlayerRegistryMemoryUsage)
    ->Arg(10)->Arg(50)->Arg(100)->Arg(500)->Arg(1000)->Arg(5000);

// 测试大量PlayerData对象的内存分配
BENCHMARK_F(MemoryBenchmarkBase, PlayerDataMassAllocation)(benchmark::State& state) {
    size_t allocation_count = state.range(0);
    
    for (auto _ : state) {
        std::vector<PlayerData> players;
        players.reserve(allocation_count);
        
        // 批量创建PlayerData对象
        for (size_t i = 0; i < allocation_count; ++i) {
            PlayerData player;
            player.set_player_id("mass_player_" + std::to_string(i));
            player.set_timestamp(i);
            
            auto* position = player.mutable_position();
            position->set_x(static_cast<float>(i));
            position->set_y(static_cast<float>(i + 1));
            position->set_z(static_cast<float>(i + 2));
            
            players.push_back(std::move(player));
        }
        
        // 访问所有数据确保分配完成
        size_t total_size = 0;
        for (const auto& player : players) {
            total_size += player.ByteSizeLong();
        }
        benchmark::DoNotOptimize(total_size);
    }
    
    state.counters["ObjectCount"] = allocation_count;
    state.counters["AllocationsPerSecond"] = benchmark::Counter(
        allocation_count * state.iterations(), benchmark::Counter::kIsRate
    );
}
BENCHMARK_REGISTER_F(MemoryBenchmarkBase, PlayerDataMassAllocation)
    ->Arg(100)->Arg(500)->Arg(1000)->Arg(5000)->Arg(10000);

// 测试字符串内存使用（模拟大量ID和消息）
BENCHMARK_F(MemoryBenchmarkBase, StringMemoryUsage)(benchmark::State& state) {
    size_t string_count = state.range(0);
    size_t string_length = 50; // 模拟典型ID长度
    
    for (auto _ : state) {
        std::vector<std::string> strings;
        strings.reserve(string_count);
        
        // 生成大量字符串
        for (size_t i = 0; i < string_count; ++i) {
            strings.push_back(generateRandomString(string_length));
        }
        
        // 模拟字符串操作
        std::unordered_map<std::string, size_t> string_map;
        for (size_t i = 0; i < strings.size(); ++i) {
            string_map[strings[i]] = i;
        }
        
        benchmark::DoNotOptimize(string_map);
    }
    
    state.counters["StringCount"] = string_count;
    state.counters["TotalStringMemory"] = string_count * string_length;
}
BENCHMARK_REGISTER_F(MemoryBenchmarkBase, StringMemoryUsage)
    ->Arg(1000)->Arg(5000)->Arg(10000)->Arg(50000);

// 测试序列化数据的内存使用
BENCHMARK_F(MemoryBenchmarkBase, SerializationMemoryUsage)(benchmark::State& state) {
    size_t data_count = state.range(0);
    
    for (auto _ : state) {
        std::vector<std::string> serialized_data;
        serialized_data.reserve(data_count);
        
        // 序列化指定数量的PlayerData
        for (size_t i = 0; i < data_count && i < test_player_data_.size(); ++i) {
            std::string serialized;
            test_player_data_[i].SerializeToString(&serialized);
            serialized_data.push_back(std::move(serialized));
        }
        
        // 计算总序列化大小
        size_t total_size = 0;
        for (const auto& data : serialized_data) {
            total_size += data.size();
        }
        
        benchmark::DoNotOptimize(total_size);
    }
    
    state.counters["SerializedObjects"] = data_count;
    state.counters["SerializationsPerSecond"] = benchmark::Counter(
        data_count * state.iterations(), benchmark::Counter::kIsRate
    );
}
BENCHMARK_REGISTER_F(MemoryBenchmarkBase, SerializationMemoryUsage)
    ->Arg(100)->Arg(500)->Arg(1000)->Arg(5000);

// 测试内存池模式 vs 动态分配
BENCHMARK_F(MemoryBenchmarkBase, MemoryPoolVsDynamicAllocation)(benchmark::State& state) {
    bool use_pool = state.range(0) == 1;
    size_t allocation_count = 1000;
    
    if (use_pool) {
        // 模拟内存池：预分配大块内存
        std::vector<PlayerData> memory_pool;
        memory_pool.resize(allocation_count);
        
        for (auto _ : state) {
            // 使用预分配的内存
            for (size_t i = 0; i < allocation_count; ++i) {
                PlayerData& player = memory_pool[i];
                player.set_player_id("pool_player_" + std::to_string(i));
                player.set_timestamp(i);
                
                auto* position = player.mutable_position();
                position->set_x(static_cast<float>(i));
                position->set_y(static_cast<float>(i + 1));
                position->set_z(static_cast<float>(i + 2));
            }
            
            // 访问数据
            for (const auto& player : memory_pool) {
                benchmark::DoNotOptimize(player.player_id());
            }
        }
    } else {
        // 动态分配
        for (auto _ : state) {
            std::vector<PlayerData> players;
            players.reserve(allocation_count);
            
            for (size_t i = 0; i < allocation_count; ++i) {
                PlayerData player;
                player.set_player_id("dynamic_player_" + std::to_string(i));
                player.set_timestamp(i);
                
                auto* position = player.mutable_position();
                position->set_x(static_cast<float>(i));
                position->set_y(static_cast<float>(i + 1));
                position->set_z(static_cast<float>(i + 2));
                
                players.push_back(std::move(player));
            }
            
            // 访问数据
            for (const auto& player : players) {
                benchmark::DoNotOptimize(player.player_id());
            }
        }
    }
    
    state.counters["AllocationMode"] = use_pool ? 1 : 0; // 1=Pool, 0=Dynamic
    state.counters["AllocationsPerSecond"] = benchmark::Counter(
        allocation_count * state.iterations(), benchmark::Counter::kIsRate
    );
}
BENCHMARK_REGISTER_F(MemoryBenchmarkBase, MemoryPoolVsDynamicAllocation)
    ->Arg(0)->Arg(1); // 0=Dynamic, 1=Pool

// 测试内存碎片化影响
static void BM_MemoryFragmentation(benchmark::State& state) {
    size_t cycle_count = state.range(0);
    
    for (auto _ : state) {
        std::vector<std::unique_ptr<PlayerData>> allocated_objects;
        
        // 分配和释放循环，模拟内存碎片化
        for (size_t cycle = 0; cycle < cycle_count; ++cycle) {
            // 分配阶段
            for (int i = 0; i < 10; ++i) {
                auto player = std::make_unique<PlayerData>();
                player->set_player_id("frag_player_" + std::to_string(cycle * 10 + i));
                allocated_objects.push_back(std::move(player));
            }
            
            // 部分释放阶段（释放一半，模拟碎片化）
            size_t to_remove = allocated_objects.size() / 2;
            for (size_t i = 0; i < to_remove; ++i) {
                allocated_objects.pop_back();
            }
        }
        
        // 最终访问剩余对象
        for (const auto& player : allocated_objects) {
            benchmark::DoNotOptimize(player->player_id());
        }
    }
    
    state.counters["FragmentationCycles"] = cycle_count;
    state.counters["CyclesPerSecond"] = benchmark::Counter(
        cycle_count * state.iterations(), benchmark::Counter::kIsRate
    );
}
BENCHMARK(BM_MemoryFragmentation)->Arg(10)->Arg(50)->Arg(100)->Arg(500);

// 测试大对象 vs 小对象的内存性能
static void BM_ObjectSizeImpact(benchmark::State& state) {
    bool large_objects = state.range(0) == 1;
    size_t object_count = 1000;
    
    for (auto _ : state) {
        if (large_objects) {
            // 创建大对象（包含更多数据）
            std::vector<PlayerData> players;
            players.reserve(object_count);
            
            for (size_t i = 0; i < object_count; ++i) {
                PlayerData player;
                player.set_player_id("large_player_with_very_long_id_" + std::to_string(i) + "_extra_data");
                player.set_timestamp(i);
                
                // 添加多个位置数据
                for (int j = 0; j < 5; ++j) {
                    auto* position = player.mutable_position();
                    position->set_x(static_cast<float>(i + j));
                    position->set_y(static_cast<float>(i + j + 1));
                    position->set_z(static_cast<float>(i + j + 2));
                }
                
                players.push_back(std::move(player));
            }
            
            benchmark::DoNotOptimize(players);
        } else {
            // 创建小对象（最少必要数据）
            std::vector<PlayerData> players;
            players.reserve(object_count);
            
            for (size_t i = 0; i < object_count; ++i) {
                PlayerData player;
                player.set_player_id(std::to_string(i));
                player.set_timestamp(i);
                
                players.push_back(std::move(player));
            }
            
            benchmark::DoNotOptimize(players);
        }
    }
    
    state.counters["ObjectType"] = large_objects ? 1 : 0; // 1=Large, 0=Small
    state.counters["ObjectsPerSecond"] = benchmark::Counter(
        object_count * state.iterations(), benchmark::Counter::kIsRate
    );
}
BENCHMARK(BM_ObjectSizeImpact)->Arg(0)->Arg(1); // 0=Small, 1=Large
