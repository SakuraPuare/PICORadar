#include <benchmark/benchmark.h>
#include "client/include/client.hpp"
#include "common.pb.h"
#include <thread>
#include <atomic>
#include <vector>
#include <memory>
#include <chrono>
#include <random>

using namespace picoradar::client;
using PlayerData = picoradar::PlayerData;

// 模拟客户端吞吐量测试（不需要真实服务器）
class ClientThroughputBenchmark : public benchmark::Fixture {
public:
    void SetUp(const ::benchmark::State& state) override {
        // 生成测试用的PlayerData
        generateTestPlayerData();
    }

protected:
    std::vector<PlayerData> test_player_data_;
    
    void generateTestPlayerData() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> pos_dist(-100.0f, 100.0f);
        std::uniform_real_distribution<float> rot_dist(-1.0f, 1.0f);
        
        test_player_data_.reserve(1000);
        
        for (int i = 0; i < 1000; ++i) {
            PlayerData player;
            player.set_player_id("player_" + std::to_string(i));
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
            
            test_player_data_.push_back(std::move(player));
        }
    }
    
    PlayerData createPlayerUpdate() {
        static size_t counter = 0;
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_real_distribution<float> pos_dist(-10.0f, 10.0f);
        
        PlayerData player = test_player_data_[counter % test_player_data_.size()];
        
        // 更新位置模拟移动
        auto* position = player.mutable_position();
        position->set_x(position->x() + pos_dist(gen));
        position->set_y(position->y() + pos_dist(gen));
        position->set_z(position->z() + pos_dist(gen));
        
        // 更新时间戳
        player.set_timestamp(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::high_resolution_clock::now().time_since_epoch()
            ).count()
        );
        
        ++counter;
        return player;
    }
};

// 测试PlayerData创建的性能
BENCHMARK_F(ClientThroughputBenchmark, PlayerDataCreation)(benchmark::State& state) {
    for (auto _ : state) {
        auto player = createPlayerUpdate();
        benchmark::DoNotOptimize(player);
    }
    
    state.counters["UpdatesPerSecond"] = benchmark::Counter(
        state.iterations(), benchmark::Counter::kIsRate
    );
}

// 测试批量PlayerData处理性能
BENCHMARK_F(ClientThroughputBenchmark, BatchPlayerDataProcessing)(benchmark::State& state) {
    size_t batch_size = state.range(0);
    
    for (auto _ : state) {
        std::vector<PlayerData> batch;
        batch.reserve(batch_size);
        
        for (size_t i = 0; i < batch_size; ++i) {
            batch.push_back(createPlayerUpdate());
        }
        
        // 模拟处理批量数据
        for (const auto& player : batch) {
            std::string serialized;
            player.SerializeToString(&serialized);
            benchmark::DoNotOptimize(serialized);
        }
    }
    
    state.counters["BatchSize"] = batch_size;
    state.counters["PlayersPerSecond"] = benchmark::Counter(
        batch_size * state.iterations(), benchmark::Counter::kIsRate
    );
}
BENCHMARK_REGISTER_F(ClientThroughputBenchmark, BatchPlayerDataProcessing)
    ->Arg(1)->Arg(5)->Arg(10)->Arg(20)->Arg(50);

// 模拟高频率更新的性能测试
BENCHMARK_F(ClientThroughputBenchmark, HighFrequencyUpdates)(benchmark::State& state) {
    auto update_frequency = state.range(0); // updates per second
    auto update_interval = std::chrono::microseconds(1000000 / update_frequency);
    
    auto last_update = std::chrono::high_resolution_clock::now();
    size_t updates_sent = 0;
    
    for (auto _ : state) {
        auto now = std::chrono::high_resolution_clock::now();
        
        if (now - last_update >= update_interval) {
            auto player = createPlayerUpdate();
            
            // 模拟发送更新
            std::string serialized;
            player.SerializeToString(&serialized);
            benchmark::DoNotOptimize(serialized);
            
            last_update = now;
            ++updates_sent;
        }
        
        // 模拟一些处理时间
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    
    state.counters["TargetFrequency"] = update_frequency;
    state.counters["ActualUpdatesPerSecond"] = benchmark::Counter(
        updates_sent, benchmark::Counter::kIsRate
    );
}
BENCHMARK_REGISTER_F(ClientThroughputBenchmark, HighFrequencyUpdates)
    ->Arg(10)->Arg(30)->Arg(60)->Arg(120);

// 测试多线程客户端数据处理性能
BENCHMARK_F(ClientThroughputBenchmark, MultiThreadedProcessing)(benchmark::State& state) {
    if (state.thread_index() == 0) {
        // 主线程初始化
    }
    
    for (auto _ : state) {
        auto player = createPlayerUpdate();
        
        // 每个线程处理不同的操作
        switch (state.thread_index() % 3) {
            case 0: { // 序列化
                std::string serialized;
                player.SerializeToString(&serialized);
                benchmark::DoNotOptimize(serialized);
                break;
            }
            case 1: { // 位置计算
                const auto& pos = player.position();
                float distance = std::sqrt(pos.x() * pos.x() + pos.y() * pos.y() + pos.z() * pos.z());
                benchmark::DoNotOptimize(distance);
                break;
            }
            case 2: { // 数据复制
                PlayerData copy = player;
                benchmark::DoNotOptimize(copy);
                break;
            }
        }
    }
    
    state.counters["ThreadOperationsPerSecond"] = benchmark::Counter(
        state.iterations(), benchmark::Counter::kIsRate
    );
}
BENCHMARK_REGISTER_F(ClientThroughputBenchmark, MultiThreadedProcessing)
    ->Threads(1)->Threads(2)->Threads(4)->Threads(8);

// 模拟网络消息队列处理性能
static void BM_MessageQueueProcessing(benchmark::State& state) {
    size_t queue_size = state.range(0);
    std::vector<std::string> message_queue;
    message_queue.reserve(queue_size);
    
    // 预填充消息队列
    for (size_t i = 0; i < queue_size; ++i) {
        PlayerData player;
        player.set_player_id("player_" + std::to_string(i));
        player.set_timestamp(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::high_resolution_clock::now().time_since_epoch()
            ).count()
        );
        
        std::string serialized;
        player.SerializeToString(&serialized);
        message_queue.push_back(std::move(serialized));
    }
    
    for (auto _ : state) {
        // 处理整个消息队列
        size_t processed = 0;
        for (const auto& message : message_queue) {
            PlayerData player;
            bool success = player.ParseFromString(message);
            if (success) {
                ++processed;
            }
            benchmark::DoNotOptimize(player);
        }
        
        benchmark::DoNotOptimize(processed);
    }
    
    state.counters["QueueSize"] = queue_size;
    state.counters["MessagesPerSecond"] = benchmark::Counter(
        queue_size * state.iterations(), benchmark::Counter::kIsRate
    );
    state.counters["TotalBytesProcessed"] = benchmark::Counter(
        std::accumulate(message_queue.begin(), message_queue.end(), 0ULL,
                       [](size_t sum, const std::string& msg) { return sum + msg.size(); }) * state.iterations(),
        benchmark::Counter::kIsRate
    );
}
BENCHMARK(BM_MessageQueueProcessing)->Arg(10)->Arg(50)->Arg(100)->Arg(500)->Arg(1000);

// 测试内存分配和释放的性能影响
static void BM_MemoryAllocationPerformance(benchmark::State& state) {
    size_t allocation_count = state.range(0);
    
    for (auto _ : state) {
        std::vector<std::unique_ptr<PlayerData>> players;
        players.reserve(allocation_count);
        
        // 分配内存
        for (size_t i = 0; i < allocation_count; ++i) {
            auto player = std::make_unique<PlayerData>();
            player->set_player_id("player_" + std::to_string(i));
            players.push_back(std::move(player));
        }
        
        // 使用数据
        for (const auto& player : players) {
            std::string serialized;
            player->SerializeToString(&serialized);
            benchmark::DoNotOptimize(serialized);
        }
        
        // 自动释放内存（vector析构）
    }
    
    state.counters["AllocationsPerSecond"] = benchmark::Counter(
        allocation_count * state.iterations(), benchmark::Counter::kIsRate
    );
}
BENCHMARK(BM_MemoryAllocationPerformance)->Arg(1)->Arg(10)->Arg(100)->Arg(1000);
