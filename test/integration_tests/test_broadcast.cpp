#include <gtest/gtest.h>
#include "core/player_registry.hpp"
#include "network/websocket_server.hpp"
#include "mock_client/sync_client.hpp"
#include <chrono>
#include <future>
#include <gtest/gtest.h>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

using namespace picoradar;

// 定义测试常量
const std::string HOST_B = "127.0.0.1";
const uint16_t PORT_B = 9005; // 为广播测试使用一个新端口

class BroadcastIntegrationTest : public ::testing::Test {
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

TEST_F(BroadcastIntegrationTest, BroadcastShouldBeReceived) {
    // Arrange: 启动服务器
    server_->start(HOST_B, PORT_B, 2); // 使用2个线程以允许并发客户端

    // Act: 并发运行 Seeder 和 Listener 客户端
    // Seeder 客户端负责连接、发送一条数据，然后断开
    auto seeder_future = std::async(std::launch::async, [&]() {
        mock_client::SyncClient seeder_client;
        return seeder_client.run(HOST_B, std::to_string(PORT_B), "--seed-data", "seeder");
    });

    // Listener 客户端负责连接并等待接收包含 Seeder 的广播
    auto listener_future = std::async(std::launch::async, [&]() {
        // 在 seeder 后稍微延迟启动，确保 seeder 有机会先连接
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        mock_client::SyncClient listener_client;
        return listener_client.run(HOST_B, std::to_string(PORT_B), "--test-broadcast", "listener");
    });
    
    // 等待两个客户端完成
    int seeder_result = seeder_future.get();
    int listener_result = listener_future.get();

    // Assert: 验证结果
    EXPECT_EQ(seeder_result, 0) << "Seeder客户端应以返回码0表示成功发送数据。";
    EXPECT_EQ(listener_result, 0) << "Listener客户端应以返回码0表示成功接收到广播。";
    
    // 在客户端完成后，服务器的玩家列表应最终为空（因为两个客户端都已断开）
    // 为了稳定地检查这一点，我们需要给服务器一点时间来处理断开连接
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    EXPECT_EQ(registry_->getPlayerCount(), 0) << "所有客户端断开后，玩家数量应为0。";
} 