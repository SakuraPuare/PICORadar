#include <benchmark/benchmark.h>
#include "player_registry.hpp"
#include "player.pb.h"
#include <chrono>
#include <random>
#include <vector>
#include <thread>
#include <atomic>
#include <memory>
#include <future>

using namespace picoradar;
using namespace picoradar::core;

// 高级基准测试类
class AdvancedBenchmark : public benchmark::Fixture {
public:
    void SetUp(const ::benchmark::State& state) override {
        registry_ = std::make_unique<PlayerRegistry>();
        generateTestData();
    }
    
    void TearDown(const ::benchmark::State& state) override {
        registry_.reset();
    }

protected:
    std::unique_ptr<PlayerRegistry> registry_;
    std::vector<PlayerData> test_players_;
    std::vector<std::string> test_player_ids_;
    
    void generateTestData() {
        test_players_.reserve(1000);
        test_player_ids_.reserve(1000);
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> pos_dist(-1000.0f, 1000.0f);
        std::uniform_real_distribution<float> rot_dist(0.0f, 1.0f);
        
        for (size_t i = 0; i < 1000; ++i) {
            PlayerData player;
            std::string player_id = "advanced_player_" + std::to_string(i);
            
            player.set_player_id(player_id);
            player.set_scene_id("advanced_scene_" + std::to_string(i % 10));
            player.set_timestamp(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::high_resolution_clock::now().time_since_epoch()
                ).count()
            );
            
            auto* position = player.mutable_position();
            position->set_x(pos_dist(gen));
            position->set_y(pos_dist(gen));
            position->set_z(pos_dist(gen));
            
            auto* rotation = player.mutable_rotation();
            rotation->set_x(rot_dist(gen));
            rotation->set_y(rot_dist(gen));
            rotation->set_z(rot_dist(gen));
            rotation->set_w(rot_dist(gen));
            
            test_players_.push_back(player);
            test_player_ids_.push_back(player_id);
        }
    }
};

// 测试批量玩家更新性能
BENCHMARK_F(AdvancedBenchmark, BatchPlayerUpdates)(benchmark::State& state) {
    const size_t batch_size = state.range(0);
    
    for (auto _ : state) {
        for (size_t i = 0; i < batch_size && i < test_players_.size(); ++i) {
            PlayerData player_copy = test_players_[i];
            
            // 修改位置模拟真实更新
            auto* pos = player_copy.mutable_position();
            pos->set_x(pos->x() + 1.0f);
            pos->set_y(pos->y() + 1.0f);
            pos->set_z(pos->z() + 1.0f);
            
            registry_->updatePlayer(player_copy.player_id(), std::move(player_copy));
        }
    }
    
    state.counters["BatchSize"] = batch_size;
    state.counters["UpdatesPerSecond"] = benchmark::Counter(
        batch_size * state.iterations(), benchmark::Counter::kIsRate
    );
}
BENCHMARK_REGISTER_F(AdvancedBenchmark, BatchPlayerUpdates)
    ->Arg(1)->Arg(5)->Arg(10)->Arg(25)->Arg(50)->Arg(100);

// 测试高频率更新性能
BENCHMARK_F(AdvancedBenchmark, HighFrequencyUpdates)(benchmark::State& state) {
    const size_t updates_per_iteration = state.range(0);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> player_dist(0, test_players_.size() - 1);
    
    for (auto _ : state) {
        for (size_t i = 0; i < updates_per_iteration; ++i) {
            size_t player_idx = player_dist(gen);
            PlayerData player_copy = test_players_[player_idx];
            
            // 模拟高频率的小幅度位置变化
            auto* pos = player_copy.mutable_position();
            pos->set_x(pos->x() + 0.1f);
            pos->set_y(pos->y() + 0.1f);
            pos->set_z(pos->z() + 0.1f);
            
            player_copy.set_timestamp(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::high_resolution_clock::now().time_since_epoch()
                ).count()
            );
            
            registry_->updatePlayer(player_copy.player_id(), std::move(player_copy));
        }
    }
    
    state.counters["UpdateFrequency"] = updates_per_iteration;
    state.counters["UpdatesPerSecond"] = benchmark::Counter(
        updates_per_iteration * state.iterations(), benchmark::Counter::kIsRate
    );
}
BENCHMARK_REGISTER_F(AdvancedBenchmark, HighFrequencyUpdates)
    ->Arg(10)->Arg(30)->Arg(60)->Arg(120)->Arg(240);

// 测试混合操作性能（读写混合）
BENCHMARK_F(AdvancedBenchmark, MixedOperations)(benchmark::State& state) {
    // 预先填充一些玩家
    for (size_t i = 0; i < 100; ++i) {
        PlayerData player_copy = test_players_[i];
        registry_->updatePlayer(player_copy.player_id(), std::move(player_copy));
    }
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> player_dist(0, 99);
    std::uniform_int_distribution<> op_dist(0, 99);
    
    size_t operations = 0;
    for (auto _ : state) {
        for (int i = 0; i < 20; ++i) {  // 每次迭代执行20个操作
            int op_type = op_dist(gen);
            size_t player_idx = player_dist(gen);
            
            if (op_type < 60) {  // 60% 读操作
                auto player = registry_->getPlayer(test_player_ids_[player_idx]);
                benchmark::DoNotOptimize(player);
            } else if (op_type < 90) {  // 30% 更新操作
                PlayerData player_copy = test_players_[player_idx];
                auto* pos = player_copy.mutable_position();
                pos->set_x(pos->x() + 0.5f);
                registry_->updatePlayer(player_copy.player_id(), std::move(player_copy));
            } else {  // 10% 获取所有玩家
                auto all_players = registry_->getAllPlayers();
                benchmark::DoNotOptimize(all_players);
            }
            operations++;
        }
    }
    
    state.counters["OperationsPerSecond"] = benchmark::Counter(
        operations, benchmark::Counter::kIsRate
    );
}
BENCHMARK_REGISTER_F(AdvancedBenchmark, MixedOperations);

// 测试大规模并发性能
BENCHMARK_F(AdvancedBenchmark, LargeConcurrentLoad)(benchmark::State& state) {
    // 预先填充玩家
    for (size_t i = 0; i < 500; ++i) {
        PlayerData player_copy = test_players_[i];
        registry_->updatePlayer(player_copy.player_id(), std::move(player_copy));
    }
    
    const size_t thread_count = state.threads();
    
    for (auto _ : state) {
        std::vector<std::future<void>> futures;
        futures.reserve(thread_count);
        
        // 每个线程执行不同的操作
        for (size_t t = 0; t < thread_count; ++t) {
            futures.push_back(std::async(std::launch::async, [this, t]() {
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<> player_dist(0, 499);
                
                for (int i = 0; i < 10; ++i) {
                    size_t player_idx = player_dist(gen);
                    
                    if (t % 3 == 0) {  // 更新操作
                        PlayerData player_copy = test_players_[player_idx];
                        auto* pos = player_copy.mutable_position();
                        pos->set_x(pos->x() + static_cast<float>(t));
                        registry_->updatePlayer(player_copy.player_id(), std::move(player_copy));
                    } else if (t % 3 == 1) {  // 查询操作
                        auto player = registry_->getPlayer(test_player_ids_[player_idx]);
                        (void)player;  // 避免编译器优化
                    } else {  // 统计操作
                        auto all_players = registry_->getAllPlayers();
                        (void)all_players.size();  // 避免编译器优化
                    }
                }
            }));
        }
        
        // 等待所有线程完成
        for (auto& future : futures) {
            future.wait();
        }
    }
    
    state.counters["ThreadCount"] = thread_count;
    state.counters["OperationsPerSecond"] = benchmark::Counter(
        thread_count * 10 * state.iterations(), benchmark::Counter::kIsRate
    );
}
BENCHMARK_REGISTER_F(AdvancedBenchmark, LargeConcurrentLoad)
    ->Threads(1)->Threads(2)->Threads(4)->Threads(8)->Threads(16);

// 测试内存使用效率
BENCHMARK_F(AdvancedBenchmark, MemoryEfficiency)(benchmark::State& state) {
    const size_t player_count = state.range(0);
    
    for (auto _ : state) {
        state.PauseTiming();
        // 清空注册表
        registry_ = std::make_unique<PlayerRegistry>();
        state.ResumeTiming();
        
        // 添加指定数量的玩家
        for (size_t i = 0; i < player_count && i < test_players_.size(); ++i) {
            PlayerData player_copy = test_players_[i];
            registry_->updatePlayer(player_copy.player_id(), std::move(player_copy));
        }
        
        // 执行一些查询来确保数据被访问
        auto all_players = registry_->getAllPlayers();
        benchmark::DoNotOptimize(all_players);
    }
    
    state.counters["PlayerCount"] = player_count;
    state.SetComplexityN(player_count);
}
BENCHMARK_REGISTER_F(AdvancedBenchmark, MemoryEfficiency)
    ->RangeMultiplier(2)->Range(10, 1000)->Complexity();

// 测试序列化吞吐量
static void BM_SerializationThroughput(benchmark::State& state) {
    const size_t message_count = state.range(0);
    std::vector<PlayerData> players;
    std::vector<std::string> serialized_data;
    
    players.reserve(message_count);
    serialized_data.reserve(message_count);
    
    // 生成测试数据
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> pos_dist(-100.0f, 100.0f);
    
    for (size_t i = 0; i < message_count; ++i) {
        PlayerData player;
        player.set_player_id("throughput_player_" + std::to_string(i));
        player.set_scene_id("scene_" + std::to_string(i % 5));
        
        auto* position = player.mutable_position();
        position->set_x(pos_dist(gen));
        position->set_y(pos_dist(gen));
        position->set_z(pos_dist(gen));
        
        players.push_back(player);
    }
    
    for (auto _ : state) {
        serialized_data.clear();
        
        for (const auto& player : players) {
            std::string data;
            player.SerializeToString(&data);
            serialized_data.push_back(std::move(data));
        }
        
        benchmark::DoNotOptimize(serialized_data);
    }
    
    size_t total_bytes = 0;
    for (const auto& data : serialized_data) {
        total_bytes += data.size();
    }
    
    state.counters["MessageCount"] = message_count;
    state.counters["TotalBytes"] = total_bytes;
    state.counters["BytesPerSecond"] = benchmark::Counter(
        total_bytes * state.iterations(), benchmark::Counter::kIsRate
    );
    state.SetComplexityN(message_count);
}
BENCHMARK(BM_SerializationThroughput)->RangeMultiplier(2)->Range(10, 500)->Complexity();

BENCHMARK_MAIN();
