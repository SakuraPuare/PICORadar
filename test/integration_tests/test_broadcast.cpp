#include <gtest/gtest.h>

#include <future>
#include <memory>
#include <thread>

#include "client/client.hpp"
#include "common/constants.hpp"
#include "core/player_registry.hpp"
#include "network/websocket_server.hpp"
#include "utils/network_utils.hpp"

using namespace picoradar;

class BroadcastIntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    player_registry_ = std::make_shared<core::PlayerRegistry>();
    server_ =
        std::make_shared<network::WebsocketServer>(ioc_, *player_registry_);

    uint16_t port = test::get_available_port();
    ASSERT_NE(port, 0);

    server_thread_ = std::thread([this, port]() {
      server_->start("127.0.0.1", port, 2);
      ioc_.run();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    server_port_ = port;
  }

  void TearDown() override {
    server_->stop();
    ioc_.stop();
    if (server_thread_.joinable()) {
      server_thread_.join();
    }
  }

  net::io_context ioc_;
  std::shared_ptr<core::PlayerRegistry> player_registry_;
  std::shared_ptr<network::WebsocketServer> server_;
  std::thread server_thread_;
  uint16_t server_port_;
};

TEST_F(BroadcastIntegrationTest, BroadcastOnNewData) {
  // Arrange:
  picoradar::client::Client seeder_client;
  seeder_client.set_player_id("seeder");
  seeder_client.set_auth_token(picoradar::config::kDefaultAuthToken);

  picoradar::client::Client listener_client;
  listener_client.set_player_id("listener");
  listener_client.set_auth_token(picoradar::config::kDefaultAuthToken);

  seeder_client.connect("127.0.0.1", std::to_string(server_port_));
  listener_client.connect("127.0.0.1", std::to_string(server_port_));

  // 等待客户端连接和认证
  for (int i = 0; i < 10 && (!seeder_client.is_authenticated() ||
                             !listener_client.is_authenticated());
       ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  ASSERT_TRUE(seeder_client.is_authenticated());
  ASSERT_TRUE(listener_client.is_authenticated());

  // Act:
  picoradar::PlayerData data;
  data.set_player_id("seeder");
  data.mutable_position()->set_x(1.0);
  seeder_client.send_player_data(data);

  // Assert:
  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  auto player_list = listener_client.get_player_list();
  bool seeder_found = false;
  for (const auto& player : player_list.players()) {
    if (player.player_id() == "seeder") {
      seeder_found = true;
      EXPECT_DOUBLE_EQ(player.position().x(), 1.0);
    }
  }
  ASSERT_TRUE(seeder_found) << "Listener did not receive data from seeder.";

  // Cleanup
  seeder_client.disconnect();
  listener_client.disconnect();
}