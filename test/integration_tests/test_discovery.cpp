#include <gtest/gtest.h>
#include "core/player_registry.hpp"
#include "network/websocket_server.hpp"
#include "mock_client/sync_client.hpp"
#include <thread>

using namespace picoradar;

// 定义测试常量
const std::string HOST_D = "127.0.0.1";
const uint16_t PORT_D = 9006; // 为发现测试使用一个新端口

class DiscoveryIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        registry_ = std::make_shared<core::PlayerRegistry>();
        server_ = std::make_shared<network::WebsocketServer>(ioc_, *registry_);
    }

    void TearDown() override {
        server_->stop();
    }
    
    net::io_context ioc_;
    std::shared_ptr<core::PlayerRegistry> registry_;
    std::shared_ptr<network::WebsocketServer> server_;
};

TEST_F(DiscoveryIntegrationTest, DiscoveryShouldSucceed) {
    // Arrange: 启动服务器，它将自动开始UDP广播
    server_->start(HOST_D, PORT_D, 1);

    // Act: 创建一个客户端并以发现模式运行
    mock_client::SyncClient client;
    int result = client.discover_and_run("discovery_tester");

    // Assert
    EXPECT_EQ(result, 0) << "客户端在发现模式下未能成功连接。";
    // Since the client connects and immediately disconnects in discover_and_run,
    // we expect the player count to be 0 after giving the server a moment to process.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(registry_->getPlayerCount(), 0) << "发现并连接后，客户端应立即断开，玩家数量应为0。";
} 