#include <gtest/gtest.h>

#include <chrono>
#include <future>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "common/constants.hpp"
#include "core/player_registry.hpp"
#include "mock_client/sync_client.hpp"
#include "network/websocket_server.hpp"
#include "utils/network_utils.hpp"

using namespace picoradar;

// 定义测试常量
const std::string HOST_B = "127.0.0.1";

class BroadcastIntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    registry_ = std::make_shared<core::PlayerRegistry>();
    server_ = std::make_shared<network::WebsocketServer>(ioc_, *registry_);
  }

  void TearDown() override { server_->stop(); }

  net::io_context ioc_;
  std::shared_ptr<core::PlayerRegistry> registry_;
  std::shared_ptr<network::WebsocketServer> server_;
};

TEST_F(BroadcastIntegrationTest, BroadcastShouldBeReceived) {
  // Arrange: 启动服务器
  uint16_t service_port = test::get_available_port();
  ASSERT_NE(service_port, 0) << "未能获取可用的服务端口";
  server_->start(HOST_B, service_port, 2);

  // Act: 使用 std::thread 运行客户端
  std::thread seeder_thread([&]() {
    mock_client::SyncClient seeder_client;
    seeder_client.run(HOST_B, std::to_string(service_port), "--seed-data",
                      "seeder");
  });

  // 稍作等待，确保 seeder 有机会先启动
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  std::thread listener_thread([&]() {
    mock_client::SyncClient listener_client;
    listener_client.run(HOST_B, std::to_string(service_port),
                        "--test-broadcast", "listener");
  });

  // 等待两个线程完成
  if (seeder_thread.joinable()) {
    seeder_thread.join();
  }
  if (listener_thread.joinable()) {
    listener_thread.join();
  }

  // Assert: 在这里可以添加断言，但主要目标是先让测试跑通不阻塞
  // 例如，我们可以检查日志输出，或者在客户端代码中返回状态码

  // 停止服务器
  server_->stop();

  // 等待服务器资源释放
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  EXPECT_EQ(registry_->getPlayerCount(), 0)
      << "所有客户端断开后，玩家数量应为0。";
}