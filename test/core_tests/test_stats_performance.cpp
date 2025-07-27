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

class StatsPerformanceTest : public ::testing::Test {
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

TEST_F(StatsPerformanceTest, MessageIncrementPerformance) {
  const size_t num_increments = 1000000;  // 1 million increments

  auto start_time = std::chrono::high_resolution_clock::now();

  for (size_t i = 0; i < num_increments; ++i) {
    server_->incrementMessagesSent();
  }

  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
      end_time - start_time);

  EXPECT_EQ(server_->getMessagesSent(), num_increments);

  // Should complete in reasonable time (less than 1 second for 1M increments)
  EXPECT_LT(duration.count(), 1000000);  // Less than 1 second in microseconds

  // Log performance for reference
  double increments_per_second =
      static_cast<double>(num_increments) / (duration.count() / 1000000.0);
  std::cout << "Performance: " << increments_per_second << " increments/second"
            << std::endl;
}

TEST_F(StatsPerformanceTest, ConcurrentStatsReadPerformance) {
  const int num_threads = 10;
  const int reads_per_thread = 100000;
  std::atomic<int> total_reads{0};

  auto start_time = std::chrono::high_resolution_clock::now();

  std::vector<std::thread> threads;
  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([this, reads_per_thread, &total_reads] {
      for (int j = 0; j < reads_per_thread; ++j) {
        volatile size_t connections = server_->getConnectionCount();
        volatile size_t received = server_->getMessagesReceived();
        volatile size_t sent = server_->getMessagesSent();
        volatile size_t players = registry_->getPlayerCount();
        (void)connections;
        (void)received;
        (void)sent;
        (void)players;
        total_reads++;
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
      end_time - start_time);

  EXPECT_EQ(total_reads.load(), num_threads * reads_per_thread);

  // Should complete in reasonable time
  EXPECT_LT(duration.count(), 5000000);  // Less than 5 seconds

  double reads_per_second =
      static_cast<double>(total_reads.load()) / (duration.count() / 1000000.0);
  std::cout << "Performance: " << reads_per_second << " concurrent reads/second"
            << std::endl;
}

TEST_F(StatsPerformanceTest, PlayerRegistryPerformance) {
  const size_t num_players = 10000;

  auto start_time = std::chrono::high_resolution_clock::now();

  // Add players
  for (size_t i = 0; i < num_players; ++i) {
    std::string player_id = "player_" + std::to_string(i);
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

  auto add_end_time = std::chrono::high_resolution_clock::now();

  // Test player count performance
  auto count_start_time = std::chrono::high_resolution_clock::now();
  size_t player_count = registry_->getPlayerCount();
  auto count_end_time = std::chrono::high_resolution_clock::now();

  // Test getAllPlayers performance
  auto get_all_start_time = std::chrono::high_resolution_clock::now();
  auto all_players = registry_->getAllPlayers();
  auto get_all_end_time = std::chrono::high_resolution_clock::now();

  EXPECT_EQ(player_count, num_players);
  EXPECT_EQ(all_players.size(), num_players);

  auto add_duration = std::chrono::duration_cast<std::chrono::microseconds>(
      add_end_time - start_time);
  auto count_duration = std::chrono::duration_cast<std::chrono::microseconds>(
      count_end_time - count_start_time);
  auto get_all_duration = std::chrono::duration_cast<std::chrono::microseconds>(
      get_all_end_time - get_all_start_time);

  // Performance expectations
  EXPECT_LT(add_duration.count(), 1000000);  // Adding 10k players in < 1 second
  EXPECT_LT(count_duration.count(), 1000);   // Getting count in < 1ms
  EXPECT_LT(get_all_duration.count(),
            100000);  // Getting all players in < 100ms

  std::cout << "Add " << num_players << " players: " << add_duration.count()
            << " microseconds" << std::endl;
  std::cout << "Get player count: " << count_duration.count() << " microseconds"
            << std::endl;
  std::cout << "Get all players: " << get_all_duration.count()
            << " microseconds" << std::endl;
}

TEST_F(StatsPerformanceTest, MixedOperationsPerformance) {
  const int num_iterations = 10000;

  auto start_time = std::chrono::high_resolution_clock::now();

  for (int i = 0; i < num_iterations; ++i) {
    // Mix of operations
    server_->incrementMessagesSent();

    if (i % 10 == 0) {
      std::string player_id = "player_" + std::to_string(i);
      picoradar::PlayerData player_data;
      player_data.set_player_id(player_id);
      player_data.mutable_position()->set_x(static_cast<float>(i));

      registry_->updatePlayer(player_id, std::move(player_data));
    }

    if (i % 100 == 0) {
      volatile size_t count = registry_->getPlayerCount();
      volatile size_t sent = server_->getMessagesSent();
      (void)count;
      (void)sent;
    }
  }

  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
      end_time - start_time);

  EXPECT_EQ(server_->getMessagesSent(), num_iterations);
  EXPECT_EQ(registry_->getPlayerCount(), num_iterations / 10);

  // Should complete mixed operations in reasonable time
  EXPECT_LT(duration.count(), 1000000);  // Less than 1 second

  double ops_per_second =
      static_cast<double>(num_iterations) / (duration.count() / 1000000.0);
  std::cout << "Mixed operations: " << ops_per_second << " operations/second"
            << std::endl;
}
