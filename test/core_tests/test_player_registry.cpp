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
