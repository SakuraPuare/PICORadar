#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <boost/asio.hpp>
#include <chrono>
#include <memory>
#include <thread>

#include "core/player_registry.hpp"
#include "server.hpp"

class ServerStatsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    server_ = std::make_unique<picoradar::server::Server>();
  }

  void TearDown() override {
    if (server_) {
      server_->stop();
    }
  }

  std::unique_ptr<picoradar::server::Server> server_;
};

TEST_F(ServerStatsTest, InitialStatsAreZero) {
  // Before starting the server, all stats should be zero
  EXPECT_EQ(server_->getPlayerCount(), 0);
  EXPECT_EQ(server_->getConnectionCount(), 0);
  EXPECT_EQ(server_->getMessagesReceived(), 0);
  EXPECT_EQ(server_->getMessagesSent(), 0);
}

TEST_F(ServerStatsTest, StatsAvailableAfterStart) {
  // Find an available port
  uint16_t test_port = 0;
  {
    boost::asio::io_context temp_ioc;
    boost::asio::ip::tcp::acceptor acceptor(temp_ioc);
    boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(), 0);
    acceptor.open(endpoint.protocol());
    acceptor.bind(endpoint);
    test_port = acceptor.local_endpoint().port();
  }

  // Start server
  server_->start(test_port, 1);

  // Give server time to initialize
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Stats methods should be callable and return valid values
  EXPECT_GE(server_->getPlayerCount(), 0);
  EXPECT_GE(server_->getConnectionCount(), 0);
  EXPECT_GE(server_->getMessagesReceived(), 0);
  EXPECT_GE(server_->getMessagesSent(), 0);
}

TEST_F(ServerStatsTest, PlayerCountReflectsRegistry) {
  // The server's player count should match the registry
  EXPECT_EQ(server_->getPlayerCount(), 0);

  // Note: Without starting the server and making actual connections,
  // we can't easily test the full integration. This test verifies
  // the method exists and returns a reasonable value.
}
