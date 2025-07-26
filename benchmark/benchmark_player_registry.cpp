#include <benchmark/benchmark.h>
#include "player_registry.hpp"
#include "player.pb.h"
#include <random>
#include <vector>

using namespace picoradar;
using namespace picoradar::core;
using PlayerData = picoradar::PlayerData;

class PlayerRegistryBenchmark : public benchmark::Fixture {
public:
    void SetUp(const ::benchmark::State& state) override {
        registry_ = std::make_unique<PlayerRegistry>();
        
        // 预生成测试数据
        generateTestPlayers(1000);
    }
    
    void TearDown(const ::benchmark::State& state) override {
        registry_.reset();
    }

protected:
    std::unique_ptr<PlayerRegistry> registry_;
    std::vector<PlayerData> test_players_;
    std::vector<std::string> player_ids_;
    
    void generateTestPlayers(size_t count) {
        test_players_.reserve(count);
        player_ids_.reserve(count);
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> pos_dist(-100.0f, 100.0f);
        std::uniform_real_distribution<float> rot_dist(0.0f, 360.0f);
        
        for (size_t i = 0; i < count; ++i) {
            PlayerData player;
            std::string player_id = "player_" + std::to_string(i);
            
            player.set_player_id(player_id);
            player.set_timestamp(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::high_resolution_clock::now().time_since_epoch()).count());
            
            auto* position = player.mutable_position();
            position->set_x(pos_dist(gen));
            position->set_y(pos_dist(gen));
            position->set_z(pos_dist(gen));
            
            auto* rotation = player.mutable_rotation();
            rotation->set_x(rot_dist(gen));
            rotation->set_y(rot_dist(gen));
            rotation->set_z(rot_dist(gen));
            rotation->set_w(rot_dist(gen));
            
            test_players_.push_back(std::move(player));
            player_ids_.push_back(player_id);
        }
    }
    
    PlayerData createRandomPlayer(const std::string& id) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> pos_dist(-100.0f, 100.0f);
        std::uniform_real_distribution<float> rot_dist(0.0f, 360.0f);
        
        PlayerData player;
        player.set_player_id(id);
        player.set_timestamp(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count());
        
        auto* position = player.mutable_position();
        position->set_x(pos_dist(gen));
        position->set_y(pos_dist(gen));
        position->set_z(pos_dist(gen));
        
        auto* rotation = player.mutable_rotation();
        rotation->set_x(rot_dist(gen));
        rotation->set_y(rot_dist(gen));
        rotation->set_z(rot_dist(gen));
        rotation->set_w(rot_dist(gen));
        
        return player;
    }
};

// 测试添加玩家的性能
BENCHMARK_F(PlayerRegistryBenchmark, AddPlayer)(benchmark::State& state) {
    size_t player_index = 0;
    for (auto _ : state) {
        std::string player_id = "benchmark_player_" + std::to_string(player_index++);
        auto player = createRandomPlayer(player_id);
        registry_->updatePlayer(player_id, std::move(player));
    }
}

// 测试更新现有玩家的性能
BENCHMARK_F(PlayerRegistryBenchmark, UpdatePlayer)(benchmark::State& state) {
    // 预先添加一些玩家
    for (size_t i = 0; i < 20; ++i) {
        std::string player_id = "test_player_" + std::to_string(i);
        registry_->updatePlayer(player_id, PlayerData(test_players_[i]));
    }
    
    size_t player_index = 0;
    for (auto _ : state) {
        std::string player_id = player_ids_[player_index % 20];
        auto player = createRandomPlayer(player_id);
        registry_->updatePlayer(player_id, std::move(player));
        ++player_index;
    }
}

// 测试查询玩家的性能
BENCHMARK_F(PlayerRegistryBenchmark, GetPlayer)(benchmark::State& state) {
    // 预先添加玩家
    for (size_t i = 0; i < 100; ++i) {
        std::string player_id = "test_player_" + std::to_string(i);
        registry_->updatePlayer(player_id, PlayerData(test_players_[i]));
    }
    
    size_t player_index = 0;
    for (auto _ : state) {
        auto player = registry_->getPlayer(player_ids_[player_index % 100]);
        benchmark::DoNotOptimize(player);
        ++player_index;
    }
}

// 测试获取所有玩家的性能
BENCHMARK_F(PlayerRegistryBenchmark, GetAllPlayers)(benchmark::State& state) {
    // 预先添加不同数量的玩家
    size_t player_count = state.range(0);
    for (size_t i = 0; i < player_count; ++i) {
        std::string player_id = "test_player_" + std::to_string(i);
        registry_->updatePlayer(player_id, PlayerData(test_players_[i % test_players_.size()]));
    }
    
    for (auto _ : state) {
        auto players = registry_->getAllPlayers();
        benchmark::DoNotOptimize(players);
    }
}
BENCHMARK_REGISTER_F(PlayerRegistryBenchmark, GetAllPlayers)
    ->Arg(1)->Arg(5)->Arg(10)->Arg(20)->Arg(50)->Arg(100);

// 测试移除玩家的性能
BENCHMARK_F(PlayerRegistryBenchmark, RemovePlayer)(benchmark::State& state) {
    std::vector<std::string> temp_players;
    
    for (auto _ : state) {
        // 预先添加玩家用于移除
        std::string player_id = "temp_player_" + std::to_string(state.iterations());
        registry_->updatePlayer(player_id, createRandomPlayer(player_id));
        temp_players.push_back(player_id);
        
        registry_->removePlayer(player_id);
    }
}

// 测试并发操作的性能
BENCHMARK_F(PlayerRegistryBenchmark, ConcurrentOperations)(benchmark::State& state) {
    // 预先添加一些玩家
    if (state.thread_index() == 0) {
        for (size_t i = 0; i < 50; ++i) {
            std::string player_id = "test_player_" + std::to_string(i);
            registry_->updatePlayer(player_id, PlayerData(test_players_[i]));
        }
    }
    
    size_t operation_index = 0;
    for (auto _ : state) {
        size_t thread_id = state.thread_index();
        size_t op_type = (operation_index + thread_id) % 4;
        
        switch (op_type) {
            case 0: { // 更新玩家
                std::string player_id = "thread_" + std::to_string(thread_id) + "_player";
                auto player = createRandomPlayer(player_id);
                registry_->updatePlayer(player_id, std::move(player));
                break;
            }
            case 1: { // 查询玩家
                std::string player_id = player_ids_[operation_index % player_ids_.size()];
                auto player = registry_->getPlayer(player_id);
                benchmark::DoNotOptimize(player);
                break;
            }
            case 2: { // 获取所有玩家
                auto players = registry_->getAllPlayers();
                benchmark::DoNotOptimize(players);
                break;
            }
            case 3: { // 检查玩家存在
                std::string player_id = player_ids_[operation_index % player_ids_.size()];
                auto player = registry_->getPlayer(player_id);
                bool exists = (player != nullptr);
                benchmark::DoNotOptimize(exists);
                break;
            }
        }
        ++operation_index;
    }
}
BENCHMARK_REGISTER_F(PlayerRegistryBenchmark, ConcurrentOperations)
    ->Threads(1)->Threads(2)->Threads(4)->Threads(8);

// 测试大规模玩家数据的性能
BENCHMARK_F(PlayerRegistryBenchmark, LargeScaleOperations)(benchmark::State& state) {
    size_t player_count = state.range(0);
    
    for (auto _ : state) {
        // 创建新的注册表以确保干净的开始
        registry_ = std::make_unique<PlayerRegistry>();
        
        // 批量添加玩家
        for (size_t i = 0; i < player_count; ++i) {
            std::string player_id = "test_player_" + std::to_string(i);
            registry_->updatePlayer(player_id, PlayerData(test_players_[i % test_players_.size()]));
        }
        
        // 执行一些查询操作
        auto all_players = registry_->getAllPlayers();
        benchmark::DoNotOptimize(all_players);
    }
}
BENCHMARK_REGISTER_F(PlayerRegistryBenchmark, LargeScaleOperations)
    ->Arg(10)->Arg(20)->Arg(50)->Arg(100)->Arg(200);

BENCHMARK_MAIN();
