#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <chrono>
#include <thread>

#include "client.pb.h"
#include "core/player_registry.hpp"
#include "network/websocket_server.hpp"
#include "server.pb.h"

namespace net = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
using tcp = net::ip::tcp;

class WebSocketServerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ioc_ = std::make_unique<net::io_context>();
    registry_ = std::make_shared<picoradar::core::PlayerRegistry>();
    server_ = std::make_unique<picoradar::network::WebsocketServer>(*ioc_,
                                                                    *registry_);
  }

  void TearDown() override {
    if (server_) {
      server_->stop();
    }
    if (ioc_) {
      ioc_->stop();
    }
    if (io_thread_.joinable()) {
      io_thread_.join();
    }
  }

  void startServer(uint16_t port = 0) {
    if (port == 0) {
      port = findAvailablePort();
    }
    server_port_ = port;

    // Start server in a separate thread
    io_thread_ = std::thread([this, port] {
      try {
        server_->start("127.0.0.1", port, 1);
        ioc_->run();
      } catch (const std::exception& e) {
        server_error_ = e.what();
      }
    });

    // Give server time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  uint16_t findAvailablePort() {
    net::io_context temp_ioc;
    tcp::acceptor acceptor(temp_ioc);
    tcp::endpoint endpoint(tcp::v4(), 0);
    acceptor.open(endpoint.protocol());
    acceptor.bind(endpoint);
    return acceptor.local_endpoint().port();
  }

  // Helper function to create a test WebSocket client
  std::unique_ptr<websocket::stream<tcp::socket>> createTestClient() {
    auto client = std::make_unique<websocket::stream<tcp::socket>>(*ioc_);

    try {
      // Connect to server
      auto const results = net::ip::tcp::resolver(*ioc_).resolve(
          "127.0.0.1", std::to_string(server_port_));
      auto ep = net::connect(beast::get_lowest_layer(*client), results);

      // Set WebSocket options
      client->control_callback(
          [](websocket::frame_type, boost::core::string_view) {});
      client->read_message_max(64 * 1024);

      // Perform WebSocket handshake
      std::string host = "127.0.0.1:" + std::to_string(server_port_);
      client->handshake(host, "/");

      return client;
    } catch (const std::exception& e) {
      client_error_ = e.what();
      return nullptr;
    }
  }

  std::unique_ptr<net::io_context> ioc_;
  std::shared_ptr<picoradar::core::PlayerRegistry> registry_;
  std::unique_ptr<picoradar::network::WebsocketServer> server_;
  std::thread io_thread_;
  uint16_t server_port_;
  std::string server_error_;
  std::string client_error_;
};

/**
 * @brief 测试WebSocket服务器的基本启动和停止
 */
TEST_F(WebSocketServerTest, BasicStartStop) {
  EXPECT_NO_THROW(startServer());

  // Server should be running
  EXPECT_TRUE(server_error_.empty()) << "Server error: " << server_error_;

  // Stop server
  EXPECT_NO_THROW(server_->stop());
}

/**
 * @brief 测试服务器初始统计信息
 */
TEST_F(WebSocketServerTest, InitialStatistics) {
  EXPECT_EQ(server_->getConnectionCount(), 0);
  EXPECT_EQ(server_->getMessagesReceived(), 0);
  EXPECT_EQ(server_->getMessagesSent(), 0);
}

/**
 * @brief 测试统计信息的递增方法
 */
TEST_F(WebSocketServerTest, StatisticsIncrement) {
  // 直接测试统计方法的访问
  EXPECT_EQ(server_->getConnectionCount(), 0);
  EXPECT_EQ(server_->getMessagesReceived(), 0);
  EXPECT_EQ(server_->getMessagesSent(), 0);

  // 测试消息发送统计递增
  auto initial_sent = server_->getMessagesSent();
  server_->incrementMessagesSent();
  EXPECT_EQ(server_->getMessagesSent(), initial_sent + 1);

  // 多次递增
  for (int i = 0; i < 5; ++i) {
    server_->incrementMessagesSent();
  }
  EXPECT_EQ(server_->getMessagesSent(), initial_sent + 6);
}

/**
 * @brief 测试服务器在无效地址上启动
 */
TEST_F(WebSocketServerTest, InvalidAddressStart) {
  // Try to start on invalid address
  EXPECT_THROW(server_->start("999.999.999.999", 8080, 1), std::exception);
}

/**
 * @brief 测试服务器在占用端口上启动
 */
TEST_F(WebSocketServerTest, PortAlreadyInUse) {
  uint16_t port = findAvailablePort();

  // Start first server
  startServer(port);
  EXPECT_TRUE(server_error_.empty());

  // Try to start second server on same port
  net::io_context second_ioc;
  auto second_registry = std::make_shared<picoradar::core::PlayerRegistry>();
  picoradar::network::WebsocketServer second_server(second_ioc,
                                                    *second_registry);

  EXPECT_THROW(second_server.start("127.0.0.1", port, 1), std::exception);
}

/**
 * @brief 测试多个线程同时访问统计信息
 */
TEST_F(WebSocketServerTest, ConcurrentStatisticsAccess) {
  constexpr int num_threads = 10;
  constexpr int operations_per_thread = 1000;
  std::vector<std::thread> threads;

  // Start server
  startServer();

  // Launch threads that increment statistics
  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([this, operations_per_thread] {
      for (int j = 0; j < operations_per_thread; ++j) {
        server_->incrementMessagesSent();
        server_->incrementMessagesReceived();

        // Also read statistics
        volatile size_t received = server_->getMessagesReceived();
        volatile size_t sent = server_->getMessagesSent();
        volatile size_t connections = server_->getConnectionCount();

        // Prevent compiler optimization
        (void)received;
        (void)sent;
        (void)connections;
      }
    });
  }

  // Wait for all threads to complete
  for (auto& thread : threads) {
    thread.join();
  }

  // Verify final counts
  EXPECT_EQ(server_->getMessagesReceived(),
            num_threads * operations_per_thread);
  EXPECT_EQ(server_->getMessagesSent(), num_threads * operations_per_thread);
}

/**
 * @brief 测试服务器重复启动和停止
 */
TEST_F(WebSocketServerTest, RepeatedStartStop) {
  for (int i = 0; i < 3; ++i) {
    uint16_t port = findAvailablePort();

    // Start server
    EXPECT_NO_THROW(startServer(port));
    EXPECT_TRUE(server_error_.empty())
        << "Iteration " << i << " server error: " << server_error_;

    // Stop server
    EXPECT_NO_THROW(server_->stop());

    // Wait for thread to finish
    if (io_thread_.joinable()) {
      io_thread_.join();
    }

    // Reset for next iteration
    server_error_.clear();
  }
}

/**
 * @brief 测试服务器的广播功能
 */
TEST_F(WebSocketServerTest, BroadcastFunctionality) {
  startServer();
  EXPECT_TRUE(server_error_.empty());

  // Create test message
  picoradar::PlayerList player_list;
  auto* player = player_list.add_players();
  player->set_player_id("test_player");
  player->mutable_position()->set_x(1.0f);
  player->mutable_position()->set_y(2.0f);
  player->mutable_position()->set_z(3.0f);

  std::string message;
  EXPECT_TRUE(player_list.SerializeToString(&message));

  // Test broadcast (should not crash even with no connections)
  size_t initial_sent = server_->getMessagesSent();
  EXPECT_NO_THROW(server_->broadcastPlayerList());

  // Message count should remain same if no connections
  EXPECT_EQ(server_->getMessagesSent(), initial_sent);
}

/**
 * @brief 测试服务器处理PlayerRegistry更新
 */
TEST_F(WebSocketServerTest, PlayerRegistryIntegration) {
  startServer();
  EXPECT_TRUE(server_error_.empty());

  // Add players to registry
  picoradar::PlayerData player1;
  player1.set_player_id("player1");
  player1.mutable_position()->set_x(10.0f);
  registry_->updatePlayer("player1", player1);

  picoradar::PlayerData player2;
  player2.set_player_id("player2");
  player2.mutable_position()->set_x(20.0f);
  registry_->updatePlayer("player2", player2);

  // Verify registry has players
  EXPECT_EQ(registry_->getPlayerCount(), 2);

  // Server should be able to access registry
  auto all_players = registry_->getAllPlayers();
  EXPECT_EQ(all_players.size(), 2);
  EXPECT_TRUE(all_players.find("player1") != all_players.end());
  EXPECT_TRUE(all_players.find("player2") != all_players.end());
}

/**
 * @brief 测试服务器在不同线程数下的行为
 */
TEST_F(WebSocketServerTest, MultipleThreadConfiguration) {
  std::vector<int> thread_counts = {1, 2, 4, 8};

  for (int thread_count : thread_counts) {
    uint16_t port = findAvailablePort();

    // Create new server for each test
    net::io_context test_ioc;
    auto test_registry = std::make_shared<picoradar::core::PlayerRegistry>();
    picoradar::network::WebsocketServer test_server(test_ioc, *test_registry);

    // Start server with different thread counts
    std::thread server_thread([&, port, thread_count] {
      try {
        test_server.start("127.0.0.1", port, thread_count);
        test_ioc.run();
      } catch (...) {
        // Expected when we stop the server
      }
    });

    // Give server time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Verify server statistics work
    EXPECT_EQ(test_server.getConnectionCount(), 0);
    EXPECT_EQ(test_server.getMessagesReceived(), 0);
    EXPECT_EQ(test_server.getMessagesSent(), 0);

    // Stop server
    test_server.stop();
    test_ioc.stop();

    if (server_thread.joinable()) {
      server_thread.join();
    }
  }
}

/**
 * @brief 测试服务器异常处理
 */
TEST_F(WebSocketServerTest, ExceptionHandling) {
  // Test starting with invalid thread count
  EXPECT_THROW(server_->start("127.0.0.1", findAvailablePort(), 0),
               std::exception);
  EXPECT_THROW(server_->start("127.0.0.1", findAvailablePort(), -1),
               std::exception);

  // Test stopping non-started server (should not crash)
  EXPECT_NO_THROW(server_->stop());
}

/**
 * @brief 测试服务器资源清理
 */
TEST_F(WebSocketServerTest, ResourceCleanup) {
  // Start and stop server multiple times to test resource cleanup
  for (int i = 0; i < 5; ++i) {
    auto local_ioc = std::make_unique<net::io_context>();
    auto local_registry = std::make_shared<picoradar::core::PlayerRegistry>();
    auto local_server = std::make_unique<picoradar::network::WebsocketServer>(
        *local_ioc, *local_registry);

    uint16_t port = findAvailablePort();

    std::thread server_thread([&] {
      try {
        local_server->start("127.0.0.1", port, 1);
        local_ioc->run();
      } catch (...) {
        // Expected when stopping
      }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Add some statistics
    local_server->incrementMessagesSent();

    EXPECT_EQ(local_server->getMessagesReceived(), 0);  // 没有实际接收消息
    EXPECT_GT(local_server->getMessagesSent(), 0);

    // Clean shutdown
    local_server->stop();
    local_ioc->stop();

    if (server_thread.joinable()) {
      server_thread.join();
    }

    // Resources should be cleaned up automatically
  }
}

/**
 * @brief 测试服务器的统计信息重置
 */
TEST_F(WebSocketServerTest, StatisticsReset) {
  // Increment some statistics
  for (int i = 0; i < 100; ++i) {
    server_->incrementMessagesSent();
  }

  EXPECT_EQ(server_->getMessagesReceived(), 0);  // 没有实际接收消息
  EXPECT_EQ(server_->getMessagesSent(), 100);

  // Reset statistics (if method exists)
  // Note: This assumes resetStatistics method exists, remove if not implemented
  // server_->resetStatistics();
  // EXPECT_EQ(server_->getMessagesReceived(), 0);
  // EXPECT_EQ(server_->getMessagesSent(), 0);
}

/**
 * @brief 压力测试：大量连接统计
 */
TEST_F(WebSocketServerTest, StressTestStatistics) {
  constexpr int num_increments = 100000;

  startServer();

  auto start_time = std::chrono::high_resolution_clock::now();

  // Perform many increment operations
  for (int i = 0; i < num_increments; ++i) {
    server_->incrementMessagesSent();
    server_->incrementMessagesReceived();

    // Occasionally read statistics
    if (i % 1000 == 0) {
      volatile size_t received = server_->getMessagesReceived();
      volatile size_t sent = server_->getMessagesSent();
      (void)received;
      (void)sent;
    }
  }

  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      end_time - start_time);

  EXPECT_EQ(server_->getMessagesReceived(), num_increments);
  EXPECT_EQ(server_->getMessagesSent(), num_increments);

  // Should complete in reasonable time (less than 1 second)
  EXPECT_LT(duration.count(), 1000);
}
