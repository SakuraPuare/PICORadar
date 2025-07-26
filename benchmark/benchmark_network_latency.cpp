#include <benchmark/benchmark.h>
#include "player.pb.h"
#include "client.pb.h"
#include "server.pb.h"
#include <chrono>
#include <random>

using PlayerData = picoradar::PlayerData;

// 测试Protocol Buffers序列化/反序列化性能
class ProtobufBenchmark : public benchmark::Fixture {
public:
    void SetUp(const ::benchmark::State& state) override {
        generateTestData();
    }

protected:
    picoradar::PlayerData test_player_data_;
    picoradar::ClientToServer test_client_message_;
    picoradar::ServerToClient test_server_message_;
    std::string serialized_player_data_;
    std::string serialized_client_message_;
    std::string serialized_server_message_;
    
    void generateTestData() {
        // 生成PlayerData
        test_player_data_.set_player_id("test_player_12345");
        test_player_data_.set_timestamp(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::high_resolution_clock::now().time_since_epoch()
            ).count()
        );
        
        auto* position = test_player_data_.mutable_position();
        position->set_x(123.456f);
        position->set_y(78.901f);
        position->set_z(234.567f);
        
        auto* rotation = test_player_data_.mutable_rotation();
        rotation->set_x(0.707f);
        rotation->set_y(0.0f);
        rotation->set_z(0.0f);
        rotation->set_w(0.707f);
        
        // 生成ClientToServer
        test_client_message_.set_message_type(picoradar::ClientToServer::PLAYER_UPDATE);
        test_client_message_.set_timestamp(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::high_resolution_clock::now().time_since_epoch()
            ).count()
        );
        *test_client_message_.mutable_player_data() = test_player_data_;
        
        // 生成ServerToClient（包含多个玩家数据）
        test_server_message_.set_message_type(picoradar::ServerToClient::PLAYER_LIST_UPDATE);
        test_server_message_.set_timestamp(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::high_resolution_clock::now().time_since_epoch()
            ).count()
        );
        
        // 添加多个玩家数据到服务器消息
        for (int i = 0; i < 20; ++i) {
            auto* player = test_server_message_.add_player_list();
            *player = test_player_data_;
            player->set_player_id("player_" + std::to_string(i));
        }
        
        // 预序列化数据
        test_player_data_.SerializeToString(&serialized_player_data_);
        test_client_message_.SerializeToString(&serialized_client_message_);
        test_server_message_.SerializeToString(&serialized_server_message_);
    }
};

// 测试PlayerData序列化性能
BENCHMARK_F(ProtobufBenchmark, PlayerDataSerialization)(benchmark::State& state) {
    for (auto _ : state) {
        std::string output;
        bool success = test_player_data_.SerializeToString(&output);
        benchmark::DoNotOptimize(success);
        benchmark::DoNotOptimize(output);
    }
}

// 测试PlayerData反序列化性能
BENCHMARK_F(ProtobufBenchmark, PlayerDataDeserialization)(benchmark::State& state) {
    for (auto _ : state) {
        picoradar::PlayerData player;
        bool success = player.ParseFromString(serialized_player_data_);
        benchmark::DoNotOptimize(success);
        benchmark::DoNotOptimize(player);
    }
}

// 测试ClientToServer序列化性能
BENCHMARK_F(ProtobufBenchmark, ClientToServerSerialization)(benchmark::State& state) {
    for (auto _ : state) {
        std::string output;
        bool success = test_client_message_.SerializeToString(&output);
        benchmark::DoNotOptimize(success);
        benchmark::DoNotOptimize(output);
    }
}

// 测试ClientToServer反序列化性能
BENCHMARK_F(ProtobufBenchmark, ClientToServerDeserialization)(benchmark::State& state) {
    for (auto _ : state) {
        picoradar::ClientToServer message;
        bool success = message.ParseFromString(serialized_client_message_);
        benchmark::DoNotOptimize(success);
        benchmark::DoNotOptimize(message);
    }
}

// 测试ServerToClient序列化性能（多玩家数据）
BENCHMARK_F(ProtobufBenchmark, ServerToClientSerialization)(benchmark::State& state) {
    for (auto _ : state) {
        std::string output;
        bool success = test_server_message_.SerializeToString(&output);
        benchmark::DoNotOptimize(success);
        benchmark::DoNotOptimize(output);
    }
}

// 测试ServerToClient反序列化性能（多玩家数据）
BENCHMARK_F(ProtobufBenchmark, ServerToClientDeserialization)(benchmark::State& state) {
    for (auto _ : state) {
        picoradar::ServerToClient message;
        bool success = message.ParseFromString(serialized_server_message_);
        benchmark::DoNotOptimize(success);
        benchmark::DoNotOptimize(message);
    }
}

// 测试不同玩家数量下的序列化性能
static void BM_MultiPlayerSerialization(benchmark::State& state) {
    size_t player_count = state.range(0);
    
    picoradar::ServerToClient server_message;
    server_message.set_message_type(picoradar::ServerToClient::PLAYER_LIST_UPDATE);
    server_message.set_timestamp(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()
        ).count()
    );
    
    // 添加指定数量的玩家
    for (size_t i = 0; i < player_count; ++i) {
        auto* player = server_message.add_player_list();
        player->set_player_id("player_" + std::to_string(i));
        player->set_timestamp(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::high_resolution_clock::now().time_since_epoch()
            ).count()
        );
        
        auto* position = player->mutable_position();
        position->set_x(static_cast<float>(i));
        position->set_y(static_cast<float>(i * 2));
        position->set_z(static_cast<float>(i * 3));
        
        auto* rotation = player->mutable_rotation();
        rotation->set_x(0.707f);
        rotation->set_y(0.0f);
        rotation->set_z(0.0f);
        rotation->set_w(0.707f);
    }
    
    for (auto _ : state) {
        std::string output;
        bool success = server_message.SerializeToString(&output);
        benchmark::DoNotOptimize(success);
        benchmark::DoNotOptimize(output);
        
        // 报告数据大小
        state.counters["MessageSize"] = benchmark::Counter(
            output.size(), benchmark::Counter::kAvgThreads
        );
        state.counters["PlayersPerSecond"] = benchmark::Counter(
            player_count, benchmark::Counter::kIsRate
        );
    }
}
BENCHMARK(BM_MultiPlayerSerialization)->Arg(1)->Arg(5)->Arg(10)->Arg(20)->Arg(50);

// 测试序列化数据大小的增长
static void BM_SerializationSize(benchmark::State& state) {
    size_t player_count = state.range(0);
    
    picoradar::ServerToClient server_message;
    server_message.set_message_type(picoradar::ServerToClient::PLAYER_LIST_UPDATE);
    server_message.set_timestamp(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()
        ).count()
    );
    
    for (size_t i = 0; i < player_count; ++i) {
        auto* player = server_message.add_player_list();
        player->set_player_id("player_" + std::to_string(i));
        
        auto* position = player->mutable_position();
        position->set_x(123.456f);
        position->set_y(789.012f);
        position->set_z(345.678f);
        
        auto* rotation = player->mutable_rotation();
        rotation->set_x(0.1f);
        rotation->set_y(0.2f);
        rotation->set_z(0.3f);
        rotation->set_w(0.9f);
    }
    
    std::string serialized;
    server_message.SerializeToString(&serialized);
    
    for (auto _ : state) {
        benchmark::DoNotOptimize(serialized.size());
    }
    
    // 报告指标
    state.counters["Players"] = player_count;
    state.counters["TotalSize"] = serialized.size();
    state.counters["BytesPerPlayer"] = benchmark::Counter(
        serialized.size() / static_cast<double>(player_count)
    );
}
BENCHMARK(BM_SerializationSize)->DenseRange(1, 50, 5);

// 模拟网络延迟的基准测试
static void BM_NetworkLatencySimulation(benchmark::State& state) {
    // 模拟不同的网络延迟 (微秒)
    auto latency_us = state.range(0);
    
    picoradar::PlayerData player_data;
    player_data.set_player_id("test_player");
    player_data.set_timestamp(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()
        ).count()
    );
    
    for (auto _ : state) {
        auto start = std::chrono::high_resolution_clock::now();
        
        // 序列化
        std::string serialized;
        player_data.SerializeToString(&serialized);
        
        // 模拟网络延迟
        std::this_thread::sleep_for(std::chrono::microseconds(latency_us));
        
        // 反序列化
        picoradar::PlayerData received_player;
        received_player.ParseFromString(serialized);
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        state.counters["TotalLatency"] = benchmark::Counter(
            duration.count(), benchmark::Counter::kAvgThreads
        );
        state.counters["NetworkLatency"] = latency_us;
        state.counters["ProcessingLatency"] = benchmark::Counter(
            duration.count() - latency_us, benchmark::Counter::kAvgThreads
        );
        
        benchmark::DoNotOptimize(received_player);
    }
}
BENCHMARK(BM_NetworkLatencySimulation)->Arg(0)->Arg(1000)->Arg(5000)->Arg(10000)->Arg(50000);
