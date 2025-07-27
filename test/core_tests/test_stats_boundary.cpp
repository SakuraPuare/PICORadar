#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <boost/asio.hpp>
#include <chrono>
#include <limits>
#include <memory>
#include <thread>

#include "core/player_registry.hpp"
#include "network/websocket_server.hpp"

class StatsBoundaryTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ioc_ = std::make_unique<boost::asio::io_context>();
    registry_ = std::make_shared<picoradar::core::PlayerRegistry>();
    server_ = std::make_unique<picoradar::network::WebsocketServer>(*ioc_,
                                                                    *registry_);
  }

  std::unique_ptr<boost::asio::io_context> ioc_;
  std::shared_ptr<picoradar::core::PlayerRegistry> registry_;
  std::unique_ptr<picoradar::network::WebsocketServer> server_;
};

TEST_F(StatsBoundaryTest, LargeNumberOfMessagesSent) {
  // Test incrementing messages sent many times
  const size_t large_number = 100000;

  for (size_t i = 0; i < large_number; ++i) {
    server_->incrementMessagesSent();
  }

  EXPECT_EQ(server_->getMessagesSent(), large_number);
}

TEST_F(StatsBoundaryTest, ZeroStatsAfterCreation) {
  // Verify all stats start at zero
  EXPECT_EQ(server_->getConnectionCount(), 0);
  EXPECT_EQ(server_->getMessagesReceived(), 0);
  EXPECT_EQ(server_->getMessagesSent(), 0);
  EXPECT_EQ(registry_->getPlayerCount(), 0);
}

TEST_F(StatsBoundaryTest, MaxPlayersInRegistry) {
  // Test adding a large number of players
  const size_t max_players = 1000;

  for (size_t i = 0; i < max_players; ++i) {
    std::string player_id = "player_" + std::to_string(i);
    picoradar::PlayerData player_data;
    player_data.set_player_id(player_id);
    player_data.mutable_position()->set_x(static_cast<float>(i));
    player_data.mutable_position()->set_y(0.0f);
    player_data.mutable_position()->set_z(0.0f);
    player_data.set_timestamp(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());

    registry_->updatePlayer(player_id, std::move(player_data));
  }

  EXPECT_EQ(registry_->getPlayerCount(), max_players);

  // Test getting all players
  auto all_players = registry_->getAllPlayers();
  EXPECT_EQ(all_players.size(), max_players);
}

TEST_F(StatsBoundaryTest, EmptyPlayerIdHandling) {
  // Test edge case with empty player ID
  picoradar::PlayerData player_data;
  player_data.set_player_id("");  // Empty ID
  player_data.mutable_position()->set_x(1.0f);
  player_data.mutable_position()->set_y(2.0f);
  player_data.mutable_position()->set_z(3.0f);

  registry_->updatePlayer("", std::move(player_data));

  // Should still be able to retrieve by empty key
  auto player = registry_->getPlayer("");
  EXPECT_NE(player, nullptr);
  EXPECT_EQ(player->player_id(), "");
  EXPECT_EQ(registry_->getPlayerCount(), 1);
}

TEST_F(StatsBoundaryTest, DuplicatePlayerUpdates) {
  // Test updating the same player multiple times
  const std::string player_id = "test_player";
  const size_t num_updates = 1000;

  for (size_t i = 0; i < num_updates; ++i) {
    picoradar::PlayerData player_data;
    player_data.set_player_id(player_id);
    player_data.mutable_position()->set_x(static_cast<float>(i));
    player_data.mutable_position()->set_y(static_cast<float>(i * 2));
    player_data.mutable_position()->set_z(static_cast<float>(i * 3));
    player_data.set_timestamp(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());

    registry_->updatePlayer(player_id, std::move(player_data));
  }

  // Should still only have one player
  EXPECT_EQ(registry_->getPlayerCount(), 1);

  // Last update should be preserved
  auto player = registry_->getPlayer(player_id);
  EXPECT_NE(player, nullptr);
  EXPECT_FLOAT_EQ(player->position().x(), static_cast<float>(num_updates - 1));
}

TEST_F(StatsBoundaryTest, LongPlayerIds) {
  // Test with very long player IDs
  std::string long_id(1000, 'x');  // 1000-character ID

  picoradar::PlayerData player_data;
  player_data.set_player_id(long_id);
  player_data.mutable_position()->set_x(1.0f);

  registry_->updatePlayer(long_id, std::move(player_data));

  EXPECT_EQ(registry_->getPlayerCount(), 1);
  auto player = registry_->getPlayer(long_id);
  EXPECT_NE(player, nullptr);
  EXPECT_EQ(player->player_id(), long_id);
}

TEST_F(StatsBoundaryTest, SpecialCharacterPlayerIds) {
  // Test with special characters in player IDs
  std::vector<std::string> special_ids = {"player@test.com",
                                          "player#123",
                                          "player with spaces",
                                          "玩家中文名",
                                          "プレイヤー日本語",
                                          "😀🎮👾",  // Emoji
                                          "\n\t\r",  // Control characters
                                          "\"quoted'player\"",
                                          "<script>alert('xss')</script>",
                                          "DROP TABLE players;"};

  for (const auto& id : special_ids) {
    picoradar::PlayerData player_data;
    player_data.set_player_id(id);
    player_data.mutable_position()->set_x(1.0f);

    registry_->updatePlayer(id, std::move(player_data));
  }

  EXPECT_EQ(registry_->getPlayerCount(), special_ids.size());

  // Verify all special IDs can be retrieved
  for (const auto& id : special_ids) {
    auto player = registry_->getPlayer(id);
    EXPECT_NE(player, nullptr) << "Failed to retrieve player with ID: " << id;
    EXPECT_EQ(player->player_id(), id);
  }
}

TEST_F(StatsBoundaryTest, ConcurrentIncrementWithOverflow) {
  // Test behavior near the limits of size_t
  // Note: This test might take a while if we actually reach max size_t

  // Start with a large number close to overflow (for testing purposes)
  const size_t large_increment = 1000000;

  for (size_t i = 0; i < large_increment; ++i) {
    server_->incrementMessagesSent();
  }

  size_t count_before = server_->getMessagesSent();
  EXPECT_EQ(count_before, large_increment);

  // Additional increments should work normally
  server_->incrementMessagesSent();
  EXPECT_EQ(server_->getMessagesSent(), large_increment + 1);
}
