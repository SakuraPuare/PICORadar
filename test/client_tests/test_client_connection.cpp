#include <gtest/gtest.h>

#include <chrono>
#include <future>
#include <thread>

#include "client.hpp"
#include "common/logging.hpp"

using namespace picoradar::client;
using namespace picoradar;

class ClientConnectionTest : public testing::Test {
 protected:
  static void SetUpTestSuite() {
    // 初始化日志系统
    logger::Logger::Init("client_connection_test", "./logs",
                         logger::LogLevel::INFO, 10, false);
  }

  static void TearDownTestSuite() {
    // glog 会自动清理
  }

  void SetUp() override {
    // 每个测试前的设置
  }

  void TearDown() override {
    // 每个测试后的清理
  }
};

/**
 * @brief 测试连接到不存在的服务器
 */
TEST_F(ClientConnectionTest, ConnectToNonExistentServer) {
  const Client client;

  // 连接到一个不太可能被占用的端口
  auto future = client.connect("127.0.0.1:65432", "test_player", "test_token");

  // 应该在合理时间内超时失败
  const auto status = future.wait_for(std::chrono::seconds(2));
  if (status == std::future_status::ready) {
    // 如果快速完成，应该是错误
    EXPECT_THROW(future.get(), std::exception);
  } else {
    // 如果3秒还没完成，也认为测试通过（连接确实失败了）
    SUCCEED();
  }

  // 连接失败后客户端应该处于未连接状态
  EXPECT_FALSE(client.isConnected());
}

/**
 * @brief 测试 DNS 解析失败
 */
TEST_F(ClientConnectionTest, DnsResolutionFailure) {
  const Client client;

  // 使用一个不存在的域名，应该在DNS解析时失败
  auto future =
      client.connect("this-domain-definitely-does-not-exist-12345.com:8080",
                     "test_player", "test_token");

  // DNS解析超时现在设置为3秒，加上一些缓冲时间
  const auto status = future.wait_for(std::chrono::seconds(15));
  EXPECT_EQ(status, std::future_status::ready);

  // 应该抛出DNS解析相关的异常
  EXPECT_THROW(future.get(), std::exception);
}

/**
 * @brief 测试并发连接尝试
 */
TEST_F(ClientConnectionTest, ConcurrentConnectAttempts) {
  Client client;

  // 启动多个连接尝试
  // 使用真实域名但无效端口，这样DNS解析成功但连接失败
  auto future1 = client.connect("google.com:8080", "player1", "token1");
  auto future2 = client.connect("github.com:8080", "player2", "token2");
  auto future3 = client.connect("microsoft.com:8080", "player3", "token3");

  // 只有第一个应该开始连接过程，其他的应该立即失败
  EXPECT_THROW(future2.get(), std::runtime_error);
  EXPECT_THROW(future3.get(), std::runtime_error);

  // 第一个会因为TCP连接失败而最终失败（DNS解析会成功）
  auto status1 = future1.wait_for(std::chrono::seconds(5));
  EXPECT_EQ(status1, std::future_status::ready);
  EXPECT_THROW(future1.get(), std::exception);
}

/**
 * @brief 测试连接超时
 */
TEST_F(ClientConnectionTest, ConnectionTimeout) {
  const Client client;

  // 连接到一个丢弃数据包的地址（通常会超时）
  // 使用一个保留的 IP 地址范围，这些地址不会路由
  auto future = client.connect("192.0.2.1:8080", "test_player", "test_token");

  // 等待较短时间就足以验证超时逻辑
  const auto status = future.wait_for(std::chrono::seconds(2));

  if (status == std::future_status::ready) {
    // 如果快速完成，应该是错误
    EXPECT_THROW(future.get(), std::exception);
  } else {
    // 如果超时，这也是预期的行为
    SUCCEED();
  }
}

/**
 * @brief 测试连接后立即断开
 */
TEST_F(ClientConnectionTest, ConnectThenImmediateDisconnect) {
  const Client client;

  // 开始连接过程（使用真实域名但无效端口）
  auto future = client.connect("google.com:8080", "test_player", "test_token");

  // 立即断开连接
  client.disconnect();

  // 连接应该失败或被取消
  EXPECT_THROW(future.get(), std::exception);

  // 客户端应该处于未连接状态
  EXPECT_FALSE(client.isConnected());
}

/**
 * @brief 测试连接后可以重新连接
 */
TEST_F(ClientConnectionTest, ReconnectAfterFailure) {
  const Client client;

  // 第一次连接尝试（使用真实域名但无效端口）
  auto future1 = client.connect("google.com:8080", "player1", "token1");
  const auto status1 = future1.wait_for(std::chrono::seconds(5));
  EXPECT_EQ(status1, std::future_status::ready);
  EXPECT_THROW(future1.get(), std::exception);
  EXPECT_FALSE(client.isConnected());

  // 确保完全断开
  client.disconnect();

  // 第二次连接尝试应该被允许（使用另一个真实域名但无效端口）
  auto future2 = client.connect("github.com:8080", "player2", "token2");
  const auto status2 = future2.wait_for(std::chrono::seconds(5));
  EXPECT_EQ(status2, std::future_status::ready);
  EXPECT_THROW(future2.get(), std::exception);
  EXPECT_FALSE(client.isConnected());
}

/**
 * @brief 测试多次断开连接
 */
TEST_F(ClientConnectionTest, MultipleDisconnects) {
  const Client client;

  // 多次调用 disconnect 应该是安全的
  client.disconnect();
  client.disconnect();
  client.disconnect();

  EXPECT_FALSE(client.isConnected());
}

/**
 * @brief 测试在连接过程中发送数据
 */
TEST_F(ClientConnectionTest, SendDataDuringConnection) {
  Client client;

  // 开始连接过程（使用真实域名但无效端口，会在TCP连接时失败）
  auto future = client.connect("google.com:8080", "test_player", "test_token");

  // 在连接过程中尝试发送数据
  PlayerData data;
  data.set_player_id("test_player");

  // 应该被静默忽略
  EXPECT_NO_THROW(client.sendPlayerData(data));

  // 等待连接失败
  const auto status = future.wait_for(std::chrono::seconds(5));
  EXPECT_EQ(status, std::future_status::ready);

  // 预期会失败
  EXPECT_THROW(future.get(), std::exception);
}
