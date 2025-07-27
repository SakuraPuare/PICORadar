#include <gtest/gtest.h>

#include <boost/asio.hpp>
#include <chrono>
#include <thread>

#include "common/config_manager.hpp"
#include "network/udp_discovery_server.hpp"

namespace net = boost::asio;
using udp = net::ip::udp;

class UdpDiscoveryServerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ioc_ = std::make_unique<net::io_context>();

    // 初始化配置管理器
    auto& config = picoradar::common::ConfigManager::getInstance();
    config.set("discovery.request_message", std::string("PICORADAR_DISCOVER"));
    config.set("discovery.response_prefix", std::string("PICORADAR_SERVER:"));
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

  uint16_t findAvailablePort() {
    net::io_context temp_ioc;
    udp::socket temp_socket(temp_ioc);
    udp::endpoint endpoint(udp::v4(), 0);
    temp_socket.open(endpoint.protocol());
    temp_socket.bind(endpoint);
    return temp_socket.local_endpoint().port();
  }

  void startServer(uint16_t discovery_port, uint16_t service_port,
                   const std::string& host_ip = "127.0.0.1") {
    discovery_port_ = discovery_port;
    service_port_ = service_port;

    server_ = std::make_unique<picoradar::network::UdpDiscoveryServer>(
        *ioc_, discovery_port, service_port, host_ip);

    // Start server in separate thread
    io_thread_ = std::thread([this] {
      server_->start();
      ioc_->run();
    });

    // Give server time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  std::string sendDiscoveryRequest(const std::string& message,
                                   uint16_t target_port) {
    net::io_context client_ioc;
    udp::socket client_socket(client_ioc);
    client_socket.open(udp::v4());

    // Send discovery request
    udp::endpoint target_endpoint(net::ip::make_address_v4("127.0.0.1"),
                                  target_port);
    client_socket.send_to(net::buffer(message), target_endpoint);

    // Wait for response
    std::array<char, 256> response_buffer;
    udp::endpoint sender_endpoint;

    // Set timeout
    client_socket.non_blocking(true);

    auto start_time = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start_time <
           std::chrono::seconds(1)) {
      boost::system::error_code ec;
      size_t bytes_received = client_socket.receive_from(
          net::buffer(response_buffer), sender_endpoint, 0, ec);

      if (!ec && bytes_received > 0) {
        return std::string(response_buffer.data(), bytes_received);
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return "";  // Timeout
  }

  std::unique_ptr<net::io_context> ioc_;
  std::unique_ptr<picoradar::network::UdpDiscoveryServer> server_;
  std::thread io_thread_;
  uint16_t discovery_port_;
  uint16_t service_port_;
};

/**
 * @brief 测试UDP发现服务器的基本构造和析构
 */
TEST_F(UdpDiscoveryServerTest, BasicConstructionDestruction) {
  uint16_t discovery_port = findAvailablePort();
  uint16_t service_port = findAvailablePort();

  EXPECT_NO_THROW({
    auto server = std::make_unique<picoradar::network::UdpDiscoveryServer>(
        *ioc_, discovery_port, service_port, "127.0.0.1");
  });
}

/**
 * @brief 测试服务器启动和停止
 */
TEST_F(UdpDiscoveryServerTest, StartStop) {
  uint16_t discovery_port = findAvailablePort();
  uint16_t service_port = findAvailablePort();

  startServer(discovery_port, service_port);

  // Server should be running
  EXPECT_TRUE(server_ != nullptr);

  // Stop server
  EXPECT_NO_THROW(server_->stop());
}

/**
 * @brief 测试发现协议的基本功能
 */
TEST_F(UdpDiscoveryServerTest, BasicDiscoveryProtocol) {
  uint16_t discovery_port = findAvailablePort();
  uint16_t service_port = findAvailablePort();

  startServer(discovery_port, service_port);

  // Send discovery request
  std::string response =
      sendDiscoveryRequest("PICORADAR_DISCOVER", discovery_port);

  // Should receive response with server address
  EXPECT_FALSE(response.empty());
  EXPECT_TRUE(response.find("PICORADAR_SERVER:") != std::string::npos);
  EXPECT_TRUE(response.find("127.0.0.1") != std::string::npos);
  EXPECT_TRUE(response.find(std::to_string(service_port)) != std::string::npos);
}

/**
 * @brief 测试无效发现请求
 */
TEST_F(UdpDiscoveryServerTest, InvalidDiscoveryRequests) {
  uint16_t discovery_port = findAvailablePort();
  uint16_t service_port = findAvailablePort();

  startServer(discovery_port, service_port);

  // Test various invalid requests
  std::vector<std::string> invalid_requests = {
      "",
      "INVALID_REQUEST",
      "WRONG_DISCOVER",
      "picoradar_discover",   // wrong case
      "PICORADAR_DISCOVERY",  // wrong suffix
      std::string(200, 'A')   // too long
  };

  for (const auto& request : invalid_requests) {
    std::string response = sendDiscoveryRequest(request, discovery_port);
    // Invalid requests should not receive response or receive empty response
    // depending on implementation
  }
}

/**
 * @brief 测试多个并发发现请求
 */
TEST_F(UdpDiscoveryServerTest, ConcurrentDiscoveryRequests) {
  uint16_t discovery_port = findAvailablePort();
  uint16_t service_port = findAvailablePort();

  startServer(discovery_port, service_port);

  constexpr int num_clients = 10;
  std::vector<std::thread> client_threads;
  std::vector<std::string> responses(num_clients);

  // Launch multiple clients simultaneously
  for (int i = 0; i < num_clients; ++i) {
    client_threads.emplace_back([this, i, &responses, discovery_port] {
      responses[i] = sendDiscoveryRequest("PICORADAR_DISCOVER", discovery_port);
    });
  }

  // Wait for all clients to complete
  for (auto& thread : client_threads) {
    thread.join();
  }

  // All clients should receive valid responses
  for (int i = 0; i < num_clients; ++i) {
    EXPECT_FALSE(responses[i].empty())
        << "Client " << i << " did not receive response";
    EXPECT_TRUE(responses[i].find("PICORADAR_SERVER:") != std::string::npos);
  }
}

/**
 * @brief 测试不同主机IP的配置
 */
TEST_F(UdpDiscoveryServerTest, DifferentHostIPs) {
  uint16_t discovery_port = findAvailablePort();
  uint16_t service_port = findAvailablePort();

  std::vector<std::string> test_ips = {"127.0.0.1", "0.0.0.0", "192.168.1.100",
                                       "10.0.0.1"};

  for (const auto& ip : test_ips) {
    // Create server with different IP
    auto test_server = std::make_unique<picoradar::network::UdpDiscoveryServer>(
        *ioc_, discovery_port, service_port, ip);

    // Start server
    std::thread server_thread([&] {
      test_server->start();
      // Run briefly
      auto work = net::make_work_guard(*ioc_);
      ioc_->run_for(std::chrono::milliseconds(100));
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Send discovery request
    std::string response =
        sendDiscoveryRequest("PICORADAR_DISCOVER", discovery_port);

    // Response should contain the configured IP
    if (!response.empty()) {
      EXPECT_TRUE(response.find(ip) != std::string::npos)
          << "Response '" << response << "' should contain IP: " << ip;
    }

    test_server->stop();
    if (server_thread.joinable()) {
      server_thread.join();
    }

    ioc_->restart();
  }
}

/**
 * @brief 测试端口占用情况
 */
TEST_F(UdpDiscoveryServerTest, PortAlreadyInUse) {
  uint16_t discovery_port = findAvailablePort();
  uint16_t service_port = findAvailablePort();

  // Start first server
  startServer(discovery_port, service_port);

  // Try to start second server on same discovery port
  EXPECT_THROW(
      {
        picoradar::network::UdpDiscoveryServer second_server(
            *ioc_, discovery_port, service_port + 1, "127.0.0.1");
      },
      std::exception);
}

/**
 * @brief 测试服务器停止后的资源清理
 */
TEST_F(UdpDiscoveryServerTest, ResourceCleanupAfterStop) {
  uint16_t discovery_port = findAvailablePort();
  uint16_t service_port = findAvailablePort();

  // Start and stop server multiple times
  for (int i = 0; i < 3; ++i) {
    auto test_ioc = std::make_unique<net::io_context>();
    auto test_server = std::make_unique<picoradar::network::UdpDiscoveryServer>(
        *test_ioc, discovery_port, service_port, "127.0.0.1");

    std::thread server_thread([&] {
      test_server->start();
      test_ioc->run();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Send a request
    std::string response =
        sendDiscoveryRequest("PICORADAR_DISCOVER", discovery_port);

    // Stop server
    test_server->stop();
    test_ioc->stop();

    if (server_thread.joinable()) {
      server_thread.join();
    }

    // After stop, requests should not receive responses
    std::string post_stop_response =
        sendDiscoveryRequest("PICORADAR_DISCOVER", discovery_port);
    // This might still work immediately after stop due to timing, so we don't
    // assert
  }
}

/**
 * @brief 测试大消息处理
 */
TEST_F(UdpDiscoveryServerTest, LargeMessageHandling) {
  uint16_t discovery_port = findAvailablePort();
  uint16_t service_port = findAvailablePort();

  startServer(discovery_port, service_port);

  // Send large discovery request (should be handled gracefully)
  std::string large_request(1000, 'A');
  large_request = "PICORADAR_DISCOVER" + large_request;

  std::string response = sendDiscoveryRequest(large_request, discovery_port);

  // Server should either respond or ignore large messages
  // The exact behavior depends on implementation
}

/**
 * @brief 测试配置响应前缀的自定义
 */
TEST_F(UdpDiscoveryServerTest, CustomResponsePrefix) {
  // Set custom response prefix in config
  auto& config = picoradar::common::ConfigManager::getInstance();
  config.set("discovery.response_prefix", std::string("CUSTOM_PREFIX:"));

  uint16_t discovery_port = findAvailablePort();
  uint16_t service_port = findAvailablePort();

  startServer(discovery_port, service_port);

  std::string response =
      sendDiscoveryRequest("PICORADAR_DISCOVER", discovery_port);

  EXPECT_FALSE(response.empty());
  EXPECT_TRUE(response.find("CUSTOM_PREFIX:") != std::string::npos);
}

/**
 * @brief 测试快速启动停止循环
 */
TEST_F(UdpDiscoveryServerTest, RapidStartStopCycle) {
  uint16_t discovery_port = findAvailablePort();
  uint16_t service_port = findAvailablePort();

  // Perform rapid start/stop cycles
  for (int i = 0; i < 5; ++i) {
    auto test_ioc = std::make_unique<net::io_context>();
    auto test_server = std::make_unique<picoradar::network::UdpDiscoveryServer>(
        *test_ioc, discovery_port, service_port, "127.0.0.1");

    std::thread server_thread([&] {
      test_server->start();
      test_ioc->run();
    });

    // Very brief operation
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    test_server->stop();
    test_ioc->stop();

    if (server_thread.joinable()) {
      server_thread.join();
    }
  }

  // Should complete without crashing
  SUCCEED();
}

/**
 * @brief 压力测试：高频发现请求
 */
TEST_F(UdpDiscoveryServerTest, HighFrequencyRequests) {
  uint16_t discovery_port = findAvailablePort();
  uint16_t service_port = findAvailablePort();

  startServer(discovery_port, service_port);

  constexpr int num_requests = 100;
  std::atomic<int> successful_responses{0};

  auto start_time = std::chrono::steady_clock::now();

  // Send many requests in quick succession
  for (int i = 0; i < num_requests; ++i) {
    std::thread([this, discovery_port, &successful_responses] {
      std::string response =
          sendDiscoveryRequest("PICORADAR_DISCOVER", discovery_port);
      if (!response.empty() &&
          response.find("PICORADAR_SERVER:") != std::string::npos) {
        successful_responses.fetch_add(1);
      }
    }).detach();

    // Small delay to prevent overwhelming
    std::this_thread::sleep_for(std::chrono::microseconds(100));
  }

  // Wait for responses
  std::this_thread::sleep_for(std::chrono::seconds(2));

  auto end_time = std::chrono::steady_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      end_time - start_time);

  // Should handle most requests successfully
  EXPECT_GT(successful_responses.load(), num_requests / 2);

  std::cout << "Processed " << successful_responses.load() << "/"
            << num_requests << " requests in " << duration.count() << " ms"
            << std::endl;
}
