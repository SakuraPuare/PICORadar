#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <boost/asio.hpp>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

#include "core/player_registry.hpp"
#include "network/websocket_server.hpp"

class StatsThreadSafetyTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ioc_ = std::make_unique<boost::asio::io_context>();
    registry_ = std::make_shared<picoradar::core::PlayerRegistry>();
    server_ = std::make_unique<picoradar::network::WebsocketServer>(*ioc_,
                                                                    *registry_);
  }

  void TearDown() override {
    if (server_) {
      server_->stop();
    }
  }

  std::unique_ptr<boost::asio::io_context> ioc_;
  std::shared_ptr<picoradar::core::PlayerRegistry> registry_;
  std::unique_ptr<picoradar::network::WebsocketServer> server_;
};

TEST_F(StatsThreadSafetyTest, ConcurrentMessagesSentIncrement) {
  const int num_threads = 10;
  const int increments_per_thread = 1000;
  std::vector<std::thread> threads;

  // Start multiple threads incrementing messages sent
  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([this, increments_per_thread] {
      for (int j = 0; j < increments_per_thread; ++j) {
        server_->incrementMessagesSent();
      }
    });
  }

  // Wait for all threads to complete
  for (auto& t : threads) {
    t.join();
  }

  // Verify final count
  EXPECT_EQ(server_->getMessagesSent(), num_threads * increments_per_thread);
}

TEST_F(StatsThreadSafetyTest, ConcurrentStatsAccess) {
  std::atomic<bool> stop_flag{false};
  std::vector<std::thread> threads;
  std::atomic<int> read_errors{0};

  // Thread that increments counters
  threads.emplace_back([this, &stop_flag] {
    while (!stop_flag) {
      server_->incrementMessagesSent();
      std::this_thread::sleep_for(std::chrono::microseconds(1));
    }
  });

  // Multiple threads reading stats
  for (int i = 0; i < 5; ++i) {
    threads.emplace_back([this, &stop_flag, &read_errors] {
      while (!stop_flag) {
        try {
          volatile size_t connections = server_->getConnectionCount();
          volatile size_t received = server_->getMessagesReceived();
          volatile size_t sent = server_->getMessagesSent();
          (void)connections;
          (void)received;
          (void)sent;
        } catch (...) {
          read_errors++;
        }
        std::this_thread::sleep_for(std::chrono::microseconds(1));
      }
    });
  }

  // Let threads run for a short time
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  stop_flag = true;

  // Wait for all threads to complete
  for (auto& t : threads) {
    t.join();
  }

  // No errors should occur during concurrent access
  EXPECT_EQ(read_errors.load(), 0);

  // Messages sent should be greater than 0 (the increment thread was running)
  EXPECT_GT(server_->getMessagesSent(), 0);
}

TEST_F(StatsThreadSafetyTest, PlayerRegistryThreadSafety) {
  const int num_threads = 5;
  const int operations_per_thread = 100;
  std::vector<std::thread> threads;
  std::atomic<int> exceptions{0};

  // Multiple threads adding/removing players
  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([this, i, operations_per_thread, &exceptions] {
      try {
        for (int j = 0; j < operations_per_thread; ++j) {
          std::string player_id =
              "player_" + std::to_string(i) + "_" + std::to_string(j);

          picoradar::PlayerData player_data;
          player_data.set_player_id(player_id);
          player_data.mutable_position()->set_x(static_cast<float>(i));
          player_data.mutable_position()->set_y(static_cast<float>(j));
          player_data.mutable_position()->set_z(0.0f);
          player_data.set_timestamp(
              std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::system_clock::now().time_since_epoch())
                  .count());

          registry_->updatePlayer(player_id, std::move(player_data));

          // Sometimes remove the player
          if (j % 3 == 0) {
            registry_->removePlayer(player_id);
          }
        }
      } catch (...) {
        exceptions++;
      }
    });
  }

  // Thread continuously reading player count
  std::atomic<bool> stop_reading{false};
  std::thread reader([this, &stop_reading, &exceptions] {
    try {
      while (!stop_reading) {
        volatile size_t count = registry_->getPlayerCount();
        (void)count;
        auto players = registry_->getAllPlayers();
        std::this_thread::sleep_for(std::chrono::microseconds(10));
      }
    } catch (...) {
      exceptions++;
    }
  });

  // Wait for all worker threads to complete
  for (auto& t : threads) {
    t.join();
  }

  stop_reading = true;
  reader.join();

  // No exceptions should occur
  EXPECT_EQ(exceptions.load(), 0);

  // Final player count should be reasonable
  EXPECT_GE(registry_->getPlayerCount(), 0);
  EXPECT_LE(registry_->getPlayerCount(), num_threads * operations_per_thread);
}

// Test statistics reset behavior
class StatsResetTest : public ::testing::Test {
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

TEST_F(StatsResetTest, StatsResetOnServerRestart) {
  // Increment some counters
  server_->incrementMessagesSent();
  server_->incrementMessagesSent();
  EXPECT_EQ(server_->getMessagesSent(), 2);

  // Create new server instance (simulating restart)
  server_.reset();
  server_ =
      std::make_unique<picoradar::network::WebsocketServer>(*ioc_, *registry_);

  // Stats should reset to zero
  EXPECT_EQ(server_->getMessagesSent(), 0);
  EXPECT_EQ(server_->getMessagesReceived(), 0);
  EXPECT_EQ(server_->getConnectionCount(), 0);
}
