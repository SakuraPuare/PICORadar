#include <gtest/gtest.h>

#include <string>
#include <thread>
#include <vector>

#include "client/client.hpp"
#include "common/constants.hpp"
#include "network/udp_discovery_server.hpp"
#include "network/websocket_server.hpp"
#include "utils/network_utils.hpp"

using namespace picoradar;

namespace {
// Helper function to split string by delimiter
void split(const std::string& s, char delimiter,
           std::vector<std::string>& tokens) {
  std::string token;
  std::istringstream tokenStream(s);
  while (std::getline(tokenStream, token, delimiter)) {
    tokens.push_back(token);
  }
}
}  // namespace

class UdpDiscoveryTest : public ::testing::Test {
 protected:
  void SetUp() override {
    service_port_ = test::get_available_port();
    discovery_server_ = std::make_unique<network::UdpDiscoveryServer>(
        ioc_, config::kDefaultDiscoveryPort, service_port_, "127.0.0.1");
    server_thread_ = std::thread([this] {
      discovery_server_->start();
      ioc_.run();
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  void TearDown() override {
    discovery_server_->stop();
    ioc_.stop();
    if (server_thread_.joinable()) {
      server_thread_.join();
    }
  }

  net::io_context ioc_;
  std::unique_ptr<network::UdpDiscoveryServer> discovery_server_;
  std::thread server_thread_;
  uint16_t service_port_{};
};

TEST_F(UdpDiscoveryTest, ServerDiscovery) {
  const auto discovered_address = client::Client::discover_server();
  ASSERT_FALSE(discovered_address.empty());

  const auto expected_port = std::to_string(service_port_);
  ASSERT_NE(discovered_address.find(expected_port), std::string::npos);
}

class DiscoveryToConnectTest : public ::testing::Test {
 protected:
  void SetUp() override {
    registry_ = std::make_shared<core::PlayerRegistry>();

    uint16_t ws_port = test::get_available_port();
    ASSERT_NE(ws_port, 0) << "未能获取可用的WebSocket端口";
    ws_server_ = std::make_shared<network::WebsocketServer>(ioc_, *registry_);
    ws_server_->start(HOST, ws_port, 1);

    discovery_server_ = std::make_unique<network::UdpDiscoveryServer>(
        ioc_, config::kDefaultDiscoveryPort, ws_port, HOST);
    discovery_thread_ = std::thread([this] {
      discovery_server_->start();
      if (!ioc_.stopped()) {
        ioc_.run();
      }
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  void TearDown() override {
    ws_server_->stop();
    discovery_server_->stop();
    ioc_.stop();
    if (discovery_thread_.joinable()) {
      discovery_thread_.join();
    }
  }

  const std::string HOST = "127.0.0.1";
  net::io_context ioc_;
  std::shared_ptr<core::PlayerRegistry> registry_;
  std::shared_ptr<network::WebsocketServer> ws_server_;
  std::unique_ptr<network::UdpDiscoveryServer> discovery_server_;
  std::thread discovery_thread_;
};
TEST_F(DiscoveryToConnectTest, DiscoveryShouldSucceed) {
  // The server is already started in SetUp.
  // We can just proceed with the client logic.

  // Create a client
  auto client = std::make_unique<picoradar::client::Client>();
  client->set_player_id("test_player_discovery");
  client->set_auth_token(picoradar::config::kDefaultAuthToken);

  // Discover the server
  // The discovery server is running on kDefaultDiscoveryPort
  std::string server_address =
      client->discover_server(picoradar::config::kDefaultDiscoveryPort);
  ASSERT_FALSE(server_address.empty());

  // Parse the address and port
  std::vector<std::string> parts;
  split(server_address, ':', parts);
  ASSERT_EQ(parts.size(), 2);
  std::string host = parts[0];
  std::string port = parts[1];

  // Connect to the server
  client->connect(host, port);

  // Check if connected and authenticated
  ASSERT_TRUE(client->is_connected());
  std::this_thread::sleep_for(
      std::chrono::milliseconds(200));  // Allow time for auth
  ASSERT_TRUE(client->is_authenticated());

  // Check if player is registered on the server
  // Wait a bit for the player to be registered
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  EXPECT_EQ(registry_->getPlayerCount(), 1);

  // Disconnect the client
  client->disconnect();

  // Allow time for disconnection to be processed by server
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  EXPECT_EQ(registry_->getPlayerCount(), 0);
}