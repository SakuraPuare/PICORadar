#include <gtest/gtest.h>

#include <thread>

#include "common/constants.hpp"
#include "core/player_registry.hpp"
#include "mock_client/sync_client.hpp"
#include "network/websocket_server.hpp"
#include "utils/network_utils.hpp"

using namespace picoradar;

// 定义测试常量
const std::string HOST_D = "127.0.0.1";

class DiscoveryIntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    registry_ = std::make_shared<core::PlayerRegistry>();
    server_ = std::make_shared<network::WebsocketServer>(ioc_, *registry_);
    discovery_port_ = test::get_available_port();
    ASSERT_NE(discovery_port_, 0) << "未能获取可用的发现端口";
  }

  void TearDown() override { server_->stop(); }

  net::io_context ioc_;
  std::shared_ptr<core::PlayerRegistry> registry_;
  std::shared_ptr<network::WebsocketServer> server_;
  uint16_t discovery_port_;
};

TEST_F(DiscoveryIntegrationTest, DiscoveryShouldSucceed) {
  // Arrange: 启动服务器，WebSocket端口用随机，UDP发现端口用固定
  uint16_t ws_port = test::get_available_port();
  server_->start(HOST_D, ws_port, 1);

  // Act: 创建一个客户端并以发现模式运行（UDP发现端口用固定值）
  mock_client::SyncClient client;
  int result = client.discover_and_run(
      "discovery_tester", picoradar::config::kDefaultDiscoveryPort);

  // Assert
  EXPECT_EQ(result, 0) << "客户端在发现模式下未能成功连接。";
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  EXPECT_EQ(registry_->getPlayerCount(), 0)
      << "发现并连接后，客户端应立即断开，玩家数量应为0。";
}