#include <benchmark/benchmark.h>
#include "player.pb.h"
#include <chrono>
#include <random>
#include <vector>
#include <thread>

using PlayerData = picoradar::PlayerData;

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
    
    player.set_timestamp(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()
        ).count()
    );
    
    return player;
}

// 测试Protocol Buffers序列化性能
static void BM_Protobuf_Serialization(benchmark::State& state) {
    PlayerData test_player = createTestPlayer("test_player_12345");
    
    for (auto _ : state) {
        std::string serialized;
        bool success = test_player.SerializeToString(&serialized);
        benchmark::DoNotOptimize(success);
        benchmark::DoNotOptimize(serialized);
    }
    
    state.SetBytesProcessed(state.iterations() * test_player.ByteSizeLong());
}
BENCHMARK(BM_Protobuf_Serialization);

// 测试Protocol Buffers反序列化性能
static void BM_Protobuf_Deserialization(benchmark::State& state) {
    PlayerData original = createTestPlayer("test_player_12345");
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
BENCHMARK(BM_Protobuf_Deserialization);

// 测试多玩家数据序列化
static void BM_MultiPlayer_Serialization(benchmark::State& state) {
    const int num_players = state.range(0);
    std::vector<PlayerData> players;
    
    // 生成测试玩家
    for (int i = 0; i < num_players; ++i) {
        players.emplace_back(createTestPlayer("player_" + std::to_string(i)));
    }
    
    for (auto _ : state) {
        std::vector<std::string> serialized_players;
        serialized_players.reserve(num_players);
        
        for (const auto& player : players) {
            std::string serialized;
            player.SerializeToString(&serialized);
            serialized_players.push_back(std::move(serialized));
        }
        
        benchmark::DoNotOptimize(serialized_players);
    }
    
    size_t total_bytes = 0;
    for (const auto& player : players) {
        total_bytes += player.ByteSizeLong();
    }
    state.SetBytesProcessed(state.iterations() * total_bytes);
    state.SetComplexityN(num_players);
}
BENCHMARK(BM_MultiPlayer_Serialization)->RangeMultiplier(2)->Range(8, 256)->Complexity();

// 测试序列化数据大小随玩家数量的变化
static void BM_Serialization_Size(benchmark::State& state) {
    const int num_players = state.range(0);
    std::vector<PlayerData> players;
    
    // 生成测试玩家
    for (int i = 0; i < num_players; ++i) {
        players.emplace_back(createTestPlayer("player_" + std::to_string(i)));
    }
    
    for (auto _ : state) {
        size_t total_size = 0;
        
        for (const auto& player : players) {
            total_size += player.ByteSizeLong();
        }
        
        benchmark::DoNotOptimize(total_size);
    }
    
    state.SetComplexityN(num_players);
}
BENCHMARK(BM_Serialization_Size)->RangeMultiplier(2)->Range(8, 512)->Complexity();

// 模拟网络延迟的序列化测试
static void BM_Network_Latency_Simulation(benchmark::State& state) {
    const int latency_us = state.range(0);  // 微秒级延迟
    PlayerData test_player = createTestPlayer("latency_test_player");
    
    for (auto _ : state) {
        // 序列化
        std::string serialized;
        test_player.SerializeToString(&serialized);
        
        // 模拟网络延迟
        std::this_thread::sleep_for(std::chrono::microseconds(latency_us));
        
        // 反序列化
        PlayerData deserialized;
        deserialized.ParseFromString(serialized);
        
        benchmark::DoNotOptimize(serialized);
        benchmark::DoNotOptimize(deserialized);
    }
}
BENCHMARK(BM_Network_Latency_Simulation)->Arg(100)->Arg(500)->Arg(1000)->Arg(5000);

// 批量数据序列化性能
static void BM_Batch_Serialization(benchmark::State& state) {
    const int batch_size = state.range(0);
    std::vector<PlayerData> batch;
    
    // 创建一批玩家数据
    for (int i = 0; i < batch_size; ++i) {
        batch.emplace_back(createTestPlayer("batch_player_" + std::to_string(i)));
    }
    
    for (auto _ : state) {
        // 将所有玩家数据序列化到单个字符串
        std::string combined_data;
        for (const auto& player : batch) {
            std::string serialized;
            player.SerializeToString(&serialized);
            
            // 添加长度前缀（简单的分隔符）
            combined_data += std::to_string(serialized.size()) + ":";
            combined_data += serialized;
        }
        
        benchmark::DoNotOptimize(combined_data);
    }
    
    state.SetComplexityN(batch_size);
}
BENCHMARK(BM_Batch_Serialization)->RangeMultiplier(2)->Range(8, 128)->Complexity();

// 测试玩家数据压缩后的大小
static void BM_PlayerData_Compression(benchmark::State& state) {
    PlayerData player = createTestPlayer("compression_test_player");
    
    // 添加更多数据以便压缩
    player.set_scene_id("very_long_scene_name_for_compression_testing_purposes");
    
    std::string original_data;
    player.SerializeToString(&original_data);
    
    for (auto _ : state) {
        // 简单的重复数据检测（模拟压缩）
        std::string processed_data = original_data;
        
        // 模拟某种压缩处理
        size_t compression_ratio = processed_data.size() * 0.7;  // 假设70%压缩比
        
        benchmark::DoNotOptimize(processed_data);
        benchmark::DoNotOptimize(compression_ratio);
    }
    
    state.SetBytesProcessed(state.iterations() * original_data.size());
}
BENCHMARK(BM_PlayerData_Compression);

// 添加benchmark的main函数
BENCHMARK_MAIN();
