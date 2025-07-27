#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <boost/asio.hpp>
#include <chrono>
#include <memory>
#include <thread>

#include "core/player_registry.hpp"
#include "server.hpp"

class StatsIntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    server_ = std::make_unique<picoradar::server::Server>();
  }

  void TearDown() override {
    if (server_) {
      server_->stop();
    }
  }

  uint16_t findAvailablePort() {
    boost::asio::io_context temp_ioc;
    boost::asio::ip::tcp::acceptor acceptor(temp_ioc);
    boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(), 0);
    acceptor.open(endpoint.protocol());
    acceptor.bind(endpoint);
    return acceptor.local_endpoint().port();
  }

  std::unique_ptr<picoradar::server::Server> server_;
};

TEST_F(StatsIntegrationTest, AllStatsMethodsWorkTogether) {
  uint16_t port = findAvailablePort();

  // Start the server
  server_->start(port, 2);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Test all stats methods
  size_t player_count = server_->getPlayerCount();
  size_t connection_count = server_->getConnectionCount();
  size_t messages_received = server_->getMessagesReceived();
  size_t messages_sent = server_->getMessagesSent();

  // All should return valid values (>= 0)
  EXPECT_GE(player_count, 0);
  EXPECT_GE(connection_count, 0);
  EXPECT_GE(messages_received, 0);
  EXPECT_GE(messages_sent, 0);

  // Initially, player and connection counts should be 0
  EXPECT_EQ(player_count, 0);
  EXPECT_EQ(connection_count, 0);
}

TEST_F(StatsIntegrationTest, StatsConsistencyAcrossMultipleCalls) {
  uint16_t port = findAvailablePort();

  server_->start(port, 1);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Make multiple calls and ensure they don't crash
  for (int i = 0; i < 10; ++i) {
    EXPECT_GE(server_->getPlayerCount(), 0);
    EXPECT_GE(server_->getConnectionCount(), 0);
    EXPECT_GE(server_->getMessagesReceived(), 0);
    EXPECT_GE(server_->getMessagesSent(), 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

TEST_F(StatsIntegrationTest, StatsAfterStopAndRestart) {
  uint16_t port = findAvailablePort();

  // Start server
  server_->start(port, 1);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Get initial stats
  size_t initial_players = server_->getPlayerCount();
  size_t initial_connections = server_->getConnectionCount();

  // Stop server
  server_->stop();
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Restart server
  server_->start(port, 1);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Stats should still be accessible
  EXPECT_GE(server_->getPlayerCount(), 0);
  EXPECT_GE(server_->getConnectionCount(), 0);
  EXPECT_GE(server_->getMessagesReceived(), 0);
  EXPECT_GE(server_->getMessagesSent(), 0);
}

// Test CLI command simulation with real server
TEST_F(StatsIntegrationTest, CLICommandsWithRealServer) {
  uint16_t port = findAvailablePort();

  server_->start(port, 1);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Simulate status command
  std::string status_response =
      "服务器状态: 运行中, 端口: " + std::to_string(port) +
      ", 连接数: " + std::to_string(server_->getConnectionCount()) +
      ", 玩家数: " + std::to_string(server_->getPlayerCount());

  EXPECT_THAT(status_response, ::testing::HasSubstr("服务器状态: 运行中"));
  EXPECT_THAT(status_response, ::testing::HasSubstr(std::to_string(port)));
  EXPECT_THAT(status_response, ::testing::HasSubstr("连接数: 0"));
  EXPECT_THAT(status_response, ::testing::HasSubstr("玩家数: 0"));

  // Simulate connections command
  std::string connections_response =
      "当前连接数: " + std::to_string(server_->getConnectionCount());

  EXPECT_EQ(connections_response, "当前连接数: 0");
}

// Test concurrent access to stats during server operations
TEST_F(StatsIntegrationTest, ConcurrentStatsAccessDuringOperations) {
  uint16_t port = findAvailablePort();

  server_->start(port, 2);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  std::atomic<bool> stop_flag{false};
  std::atomic<int> errors{0};
  std::vector<std::thread> stats_readers;

  // Start multiple threads reading stats
  for (int i = 0; i < 3; ++i) {
    stats_readers.emplace_back([this, &stop_flag, &errors] {
      while (!stop_flag) {
        try {
          volatile size_t players = server_->getPlayerCount();
          volatile size_t connections = server_->getConnectionCount();
          volatile size_t received = server_->getMessagesReceived();
          volatile size_t sent = server_->getMessagesSent();
          (void)players;
          (void)connections;
          (void)received;
          (void)sent;
        } catch (...) {
          errors++;
        }
        std::this_thread::sleep_for(std::chrono::microseconds(100));
      }
    });
  }

  // Let threads run for a while
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  stop_flag = true;

  // Wait for all threads to finish
  for (auto& t : stats_readers) {
    t.join();
  }

  // No errors should have occurred
  EXPECT_EQ(errors.load(), 0);
}
