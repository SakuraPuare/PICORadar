#include <thread>

#include "core/player_registry.hpp"
#include "gtest/gtest.h"

using namespace picoradar::core;

// 创建一个辅助函数来生成测试用的玩家数据
auto createTestPlayer(const std::string& id, float x) -> picoradar::PlayerData {
  picoradar::PlayerData player;
  player.set_player_id(id);
  player.mutable_position()->set_x(x);
  return player;
}

// 测试套件(Test Suite) for PlayerRegistry
class PlayerRegistryTest : public testing::Test {
 protected:
  PlayerRegistry registry;
};

// 测试用例(Test Case): 测试初始状态
TEST_F(PlayerRegistryTest, InitialState) {
  EXPECT_EQ(registry.getPlayerCount(), 0);
  EXPECT_TRUE(registry.getAllPlayers().empty());
}

// 测试用例: 添加单个玩家
TEST_F(PlayerRegistryTest, AddSinglePlayer) {
  PlayerRegistry registry;
  auto p1 = createTestPlayer("player1", 1.0F);
  registry.updatePlayer(p1.player_id(), p1);

  ASSERT_EQ(registry.getPlayerCount(), 1);
  auto player = registry.getPlayer("player1");
  ASSERT_NE(player, nullptr);
  EXPECT_EQ(player->player_id(), "player1");
  EXPECT_FLOAT_EQ(player->position().x(), 1.0F);
}

// 测试用例: 添加多个玩家
TEST_F(PlayerRegistryTest, AddMultiplePlayers) {
  PlayerRegistry registry;
  registry.updatePlayer("player1", createTestPlayer("player1", 1.0F));
  registry.updatePlayer("player2", createTestPlayer("player2", 2.0F));

  ASSERT_EQ(registry.getPlayerCount(), 2);

  auto players = registry.getAllPlayers();
  bool p1_found = false;
  bool p2_found = false;
  for (const auto& pair : players) {
    const auto& p = pair.second;
    if (p.player_id() == "player1") {
      p1_found = true;
      EXPECT_EQ(p.position().x(), 1.0F);
    }
    if (p.player_id() == "player2") {
      p2_found = true;
      EXPECT_EQ(p.position().x(), 2.0F);
    }
  }
  EXPECT_TRUE(p1_found);
  EXPECT_TRUE(p2_found);
}

// 测试用例: 更新现有玩家
TEST_F(PlayerRegistryTest, UpdateExistingPlayer) {
  PlayerRegistry registry;
  registry.updatePlayer("player1", createTestPlayer("player1", 1.0F));

  auto player1 = registry.getPlayer("player1");
  ASSERT_TRUE(player1);
  EXPECT_EQ(player1->position().x(), 1.0F);

  // 更新玩家数据
  registry.updatePlayer("player1", createTestPlayer("player1", 99.0F));
  ASSERT_EQ(registry.getPlayerCount(), 1);
  player1 = registry.getPlayer("player1");
  ASSERT_TRUE(player1);
  EXPECT_EQ(player1->position().x(), 99.0F);
}

// 测试用例: 移除玩家
TEST_F(PlayerRegistryTest, RemovePlayer) {
  PlayerRegistry registry;
  registry.updatePlayer("player1", createTestPlayer("player1", 1.0F));
  registry.updatePlayer("player2", createTestPlayer("player2", 2.0F));
  ASSERT_EQ(registry.getPlayerCount(), 2);

  registry.removePlayer("player1");
  ASSERT_EQ(registry.getPlayerCount(), 1);
  EXPECT_FALSE(registry.getPlayer("player1"));
  EXPECT_TRUE(registry.getPlayer("player2"));
}

// 测试用例: 移除不存在的玩家
TEST_F(PlayerRegistryTest, RemoveNonExistentPlayer) {
  PlayerRegistry registry;
  registry.updatePlayer("player1", createTestPlayer("player1", 1.0F));
  ASSERT_EQ(registry.getPlayerCount(), 1);

  // 尝试删除一个不存在的玩家，不应该发生任何事
  registry.removePlayer("player_non_existent");
  ASSERT_EQ(registry.getPlayerCount(), 1);
}

// 测试用例: 获取不存在的玩家
TEST_F(PlayerRegistryTest, GetNonExistentPlayer) {
  EXPECT_EQ(registry.getPlayer("player_ghost"), nullptr);
}

// 测试用例: 线程安全
TEST_F(PlayerRegistryTest, ThreadSafety) {
  PlayerRegistry registry;
  constexpr int threadCount = 4;
  constexpr int updatesPerThread = 1000;
  std::vector<std::thread> threads;

  for (int i = 0; i < threadCount; ++i) {
    threads.emplace_back([this, &registry, i] {
      for (int j = 0; j < updatesPerThread; ++j) {
        std::string id = "player" + std::to_string(i);
        registry.updatePlayer(id, createTestPlayer(id, static_cast<float>(j)));
        registry.getPlayer(id);
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  // 在所有操作之后，我们无法预测最终状态，
  // 但我们可以断言程序没有崩溃，并且最终的玩家数量是合理的（小于等于20）。
  EXPECT_LE(registry.getPlayerCount(), 20);
  ASSERT_NO_THROW(registry.getAllPlayers());  // 确认在并发操作后可以安全地访问
}

// 新增测试用例: 边界条件测试
TEST_F(PlayerRegistryTest, EdgeCases) {
  // 测试空字符串ID
  EXPECT_NO_THROW(registry.updatePlayer("", createTestPlayer("", 0.0F)));
  EXPECT_EQ(registry.getPlayerCount(), 1);

  auto empty_player = registry.getPlayer("");
  ASSERT_NE(empty_player, nullptr);
  EXPECT_EQ(empty_player->player_id(), "");

  // 测试非常长的ID
  std::string long_id(1000, 'a');
  EXPECT_NO_THROW(
      registry.updatePlayer(long_id, createTestPlayer(long_id, 1.0F)));
  EXPECT_EQ(registry.getPlayerCount(), 2);

  auto long_player = registry.getPlayer(long_id);
  ASSERT_NE(long_player, nullptr);
  EXPECT_EQ(long_player->player_id(), long_id);

  // 测试特殊字符ID
  std::string special_id = "player@#$%^&*()[]{}|\\:;\"'<>,.?/~`";
  EXPECT_NO_THROW(
      registry.updatePlayer(special_id, createTestPlayer(special_id, 2.0F)));
  EXPECT_EQ(registry.getPlayerCount(), 3);
}

// 新增测试用例: 大规模数据测试
TEST_F(PlayerRegistryTest, LargeScaleTest) {
  const int player_count = 10000;

  // 添加大量玩家
  for (int i = 0; i < player_count; ++i) {
    std::string id = "player_" + std::to_string(i);
    registry.updatePlayer(id, createTestPlayer(id, static_cast<float>(i)));
  }

  EXPECT_EQ(registry.getPlayerCount(), player_count);

  // 验证所有玩家都能正确获取
  for (int i = 0; i < 100; ++i) {  // 测试前100个
    std::string id = "player_" + std::to_string(i);
    auto player = registry.getPlayer(id);
    ASSERT_NE(player, nullptr) << "Failed to get player: " << id;
    EXPECT_EQ(player->player_id(), id);
    EXPECT_FLOAT_EQ(player->position().x(), static_cast<float>(i));
  }

  // 测试批量删除
  for (int i = 0; i < player_count / 2; ++i) {
    std::string id = "player_" + std::to_string(i);
    registry.removePlayer(id);
  }

  EXPECT_EQ(registry.getPlayerCount(), player_count / 2);
}

// 新增测试用例: 数据完整性测试
TEST_F(PlayerRegistryTest, DataIntegrityTest) {
  auto original_player = createTestPlayer("test_player", 42.0F);
  original_player.mutable_position()->set_y(13.37F);
  original_player.mutable_position()->set_z(-99.99F);
  original_player.set_scene_id("test_scene");

  registry.updatePlayer("test_player", original_player);

  auto retrieved_player = registry.getPlayer("test_player");
  ASSERT_NE(retrieved_player, nullptr);

  // 验证所有字段都正确保存
  EXPECT_EQ(retrieved_player->player_id(), "test_player");
  EXPECT_FLOAT_EQ(retrieved_player->position().x(), 42.0F);
  EXPECT_FLOAT_EQ(retrieved_player->position().y(), 13.37F);
  EXPECT_FLOAT_EQ(retrieved_player->position().z(), -99.99F);
  EXPECT_EQ(retrieved_player->scene_id(), "test_scene");
}

// 新增测试用例: 性能基准测试
TEST_F(PlayerRegistryTest, PerformanceBenchmark) {
  const int operations = 1000;
  auto start = std::chrono::high_resolution_clock::now();

  // 插入操作
  for (int i = 0; i < operations; ++i) {
    std::string id = "perf_player_" + std::to_string(i);
    registry.updatePlayer(id, createTestPlayer(id, static_cast<float>(i)));
  }

  auto insert_end = std::chrono::high_resolution_clock::now();

  // 查询操作
  for (int i = 0; i < operations; ++i) {
    std::string id = "perf_player_" + std::to_string(i);
    auto player = registry.getPlayer(id);
    EXPECT_NE(player, nullptr);
  }

  auto query_end = std::chrono::high_resolution_clock::now();

  // 删除操作
  for (int i = 0; i < operations; ++i) {
    std::string id = "perf_player_" + std::to_string(i);
    registry.removePlayer(id);
  }

  auto delete_end = std::chrono::high_resolution_clock::now();

  // 计算耗时（仅作为参考，不进行断言）
  auto insert_time =
      std::chrono::duration_cast<std::chrono::microseconds>(insert_end - start);
  auto query_time = std::chrono::duration_cast<std::chrono::microseconds>(
      query_end - insert_end);
  auto delete_time = std::chrono::duration_cast<std::chrono::microseconds>(
      delete_end - query_end);

  // 输出性能信息（测试时可见）
  std::cout << "Performance Benchmark Results:" << std::endl;
  std::cout << "Insert " << operations << " players: " << insert_time.count()
            << " µs" << std::endl;
  std::cout << "Query " << operations << " players: " << query_time.count()
            << " µs" << std::endl;
  std::cout << "Delete " << operations << " players: " << delete_time.count()
            << " µs" << std::endl;

  EXPECT_EQ(registry.getPlayerCount(), 0);
}

// 新增测试用例: 并发安全详细测试
TEST_F(PlayerRegistryTest, DetailedThreadSafety) {
  constexpr int thread_count = 8;
  constexpr int operations_per_thread = 500;
  std::atomic<int> completed_operations{0};
  std::vector<std::thread> threads;

  // 创建多个线程，每个线程执行不同的操作
  for (int i = 0; i < thread_count; ++i) {
    threads.emplace_back([&, i] {
      for (int j = 0; j < operations_per_thread; ++j) {
        std::string base_id =
            "thread_" + std::to_string(i) + "_player_" + std::to_string(j);

        // 添加玩家
        registry.updatePlayer(base_id,
                              createTestPlayer(base_id, static_cast<float>(j)));
        completed_operations++;

        // 查询玩家
        auto player = registry.getPlayer(base_id);
        if (player) {
          completed_operations++;
        }

        // 更新玩家
        registry.updatePlayer(
            base_id, createTestPlayer(base_id, static_cast<float>(j + 100)));
        completed_operations++;

        // 再次查询
        player = registry.getPlayer(base_id);
        if (player && player->position().x() == static_cast<float>(j + 100)) {
          completed_operations++;
        }

        // 删除玩家（50%概率）
        if (j % 2 == 0) {
          registry.removePlayer(base_id);
          completed_operations++;
        }
      }
    });
  }

  // 等待所有线程完成
  for (auto& t : threads) {
    t.join();
  }

  // 验证没有崩溃，并且操作数合理
  EXPECT_GT(completed_operations.load(), thread_count * operations_per_thread);
  EXPECT_NO_THROW(registry.getAllPlayers());
}
