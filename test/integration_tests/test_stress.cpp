#include <glog/logging.h>
#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "common/constants.hpp"
#include "common/logging.hpp"
#include "core/player_registry.hpp"
#include "mock_client/sync_client.hpp"
#include "network/websocket_server.hpp"
#include "test/utils/network_utils.hpp"

namespace {
const std::string HOST = "127.0.0.1";
const int NUM_CLIENTS = 20;
const std::chrono::seconds TEST_DURATION(10);  // 缩短测试时间以加速CI
}  // namespace

class StressIntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    LOG(INFO) << "Setting up stress test...";
    port_ = test::get_available_port();
    ASSERT_NE(port_, 0) << true;
    registry_ = std::make_shared<picoradar::core::PlayerRegistry>();
    server_ =
        std::make_shared<picoradar::network::WebsocketServer>(ioc_, *registry_);
    server_->start(HOST, port_, 4);  // 使用4个IO线程
  }

  void TearDown() override {
    LOG(INFO) << "Tearing down stress test...";
    server_->stop();
    // 确保IO上下文停止
    if (!ioc_.stopped()) {
      ioc_.stop();
    }
  }

  net::io_context ioc_;
  std::shared_ptr<picoradar::core::PlayerRegistry> registry_;
  std::shared_ptr<picoradar::network::WebsocketServer> server_;
  uint16_t port_;
};

void run_client_task(const std::string& player_id, uint16_t port) {
  LOG(INFO) << "Client thread started for player: " << player_id;
  picoradar::mock_client::SyncClient client;
  // 使用 "--test-stress" 模式, 它会在连接后持续发送数据
  client.run(HOST, std::to_string(port), "--test-stress", player_id);
  LOG(INFO) << "Client thread finished for player: " << player_id;
}

TEST_F(StressIntegrationTest, Handle20ConcurrentClients) {
  std::vector<std::thread> client_threads;
  client_threads.reserve(NUM_CLIENTS);

  LOG(INFO) << "Starting " << NUM_CLIENTS << " client threads...";

  for (int i = 0; i < NUM_CLIENTS; ++i) {
    std::string player_id = "stress_player_" + std::to_string(i);
    client_threads.emplace_back(run_client_task, player_id, port_);
    // 短暂间隔以错开连接高峰
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  LOG(INFO) << "All " << NUM_CLIENTS
            << " clients are connecting. Running test for "
            << TEST_DURATION.count() << " seconds...";

  // 检查初始连接数
  // 由于连接是异步的，我们需要稍微等待
  std::this_thread::sleep_for(std::chrono::seconds(5));
  EXPECT_EQ(registry_->getPlayerCount(), NUM_CLIENTS);

  std::this_thread::sleep_for(TEST_DURATION);

  LOG(INFO) << "Test duration elapsed. Stopping clients and server...";

  // 此时客户端仍在运行，我们需要停止它们
  // SyncClient 的 run 函数是阻塞的，我们需要一种方式来中断它
  // 目前的SyncClient没有优雅的外部停止机制，它只在内部循环或遇到错误时退出
  // 对于压力测试，我们暂时依赖服务器关闭连接来触发客户端退出。
  server_->stop();

  LOG(INFO) << "Waiting for all client threads to join...";
  // 等待所有线程完成
  for (auto& t : client_threads) {
    if (t.joinable()) {
      t.join();
    }
  }

  LOG(INFO) << "All client threads finished.";

  // 在服务器停止后，所有客户端都应该断开连接，玩家数量应为0
  ASSERT_EQ(registry_->getPlayerCount(), 0);
}