#include "gtest/gtest.h"
#include "core/player_registry.hpp"

using namespace picoradar::core;

// 创建一个辅助函数来生成测试用的玩家数据
picoradar::PlayerData createTestPlayer(const std::string& id, float x) {
    picoradar::PlayerData player;
    player.set_player_id(id);
    player.mutable_position()->set_x(x);
    return player;
}

// 测试套件(Test Suite) for PlayerRegistry
class PlayerRegistryTest : public ::testing::Test {
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
    auto p1 = createTestPlayer("player1", 1.0f);
    registry.updatePlayer(p1);

    EXPECT_EQ(registry.getPlayerCount(), 1);
    auto retrieved = registry.getPlayer("player1");
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->player_id(), "player1");
    EXPECT_FLOAT_EQ(retrieved->position().x(), 1.0f);
}

// 测试用例: 添加多个玩家
TEST_F(PlayerRegistryTest, AddMultiplePlayers) {
    registry.updatePlayer(createTestPlayer("player1", 1.0f));
    registry.updatePlayer(createTestPlayer("player2", 2.0f));
    
    EXPECT_EQ(registry.getPlayerCount(), 2);
    
    auto players = registry.getAllPlayers();
    EXPECT_EQ(players.size(), 2);

    // 检查是否能找到两个玩家 (unordered_map不保证顺序)
    bool p1_found = false;
    bool p2_found = false;
    for(const auto& p : players) {
        if (p.player_id() == "player1") p1_found = true;
        if (p.player_id() == "player2") p2_found = true;
    }
    EXPECT_TRUE(p1_found);
    EXPECT_TRUE(p2_found);
}

// 测试用例: 更新现有玩家
TEST_F(PlayerRegistryTest, UpdateExistingPlayer) {
    registry.updatePlayer(createTestPlayer("player1", 1.0f));
    
    // 确认初始状态
    auto old_player = registry.getPlayer("player1");
    ASSERT_NE(old_player, nullptr);
    EXPECT_FLOAT_EQ(old_player->position().x(), 1.0f);

    // 更新玩家
    registry.updatePlayer(createTestPlayer("player1", 99.0f));

    EXPECT_EQ(registry.getPlayerCount(), 1); // 数量不应改变
    auto updated_player = registry.getPlayer("player1");
    ASSERT_NE(updated_player, nullptr);
    EXPECT_FLOAT_EQ(updated_player->position().x(), 99.0f); // 检查值是否已更新
}

// 测试用例: 移除玩家
TEST_F(PlayerRegistryTest, RemovePlayer) {
    registry.updatePlayer(createTestPlayer("player1", 1.0f));
    registry.updatePlayer(createTestPlayer("player2", 2.0f));
    
    EXPECT_EQ(registry.getPlayerCount(), 2);

    registry.removePlayer("player1");
    
    EXPECT_EQ(registry.getPlayerCount(), 1);
    EXPECT_EQ(registry.getPlayer("player1"), nullptr); // 确认player1已被移除
    EXPECT_NE(registry.getPlayer("player2"), nullptr); // 确认player2还在
}

// 测试用例: 移除不存在的玩家
TEST_F(PlayerRegistryTest, RemoveNonExistentPlayer) {
    registry.updatePlayer(createTestPlayer("player1", 1.0f));
    
    // 移除一个不存在的玩家，不应该发生任何事或崩溃
    registry.removePlayer("player_ghost");
    
    EXPECT_EQ(registry.getPlayerCount(), 1);
}
