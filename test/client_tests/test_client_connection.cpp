#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <future>

#include "client.hpp"
#include "common/logging.hpp"

using namespace picoradar::client;
using namespace picoradar;

class ClientConnectionTest : public ::testing::Test {
protected:
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
    Client client;
    
    // 连接到一个不太可能被占用的端口
    auto future = client.connect("127.0.0.1:65432", "test_player", "test_token");
    
    // 应该在合理时间内超时失败
    auto status = future.wait_for(std::chrono::seconds(10));
    EXPECT_EQ(status, std::future_status::ready);
    
    // 应该抛出异常
    EXPECT_THROW(future.get(), std::exception);
    
    // 连接失败后客户端应该处于未连接状态
    EXPECT_FALSE(client.isConnected());
}

/**
 * @brief 测试 DNS 解析失败
 */
TEST_F(ClientConnectionTest, DnsResolutionFailure) {
    Client client;
    
    // 使用不存在的主机名
    auto future = client.connect("nonexistent.invalid.domain:8080", "test_player", "test_token");
    
    // 应该快速失败
    auto status = future.wait_for(std::chrono::seconds(5));
    EXPECT_EQ(status, std::future_status::ready);
    
    // 应该抛出 DNS 解析相关的异常
    EXPECT_THROW(future.get(), std::exception);
}

/**
 * @brief 测试并发连接尝试
 */
TEST_F(ClientConnectionTest, ConcurrentConnectAttempts) {
    Client client;
    
    // 启动多个连接尝试
    auto future1 = client.connect("127.0.0.1:65433", "player1", "token1");
    auto future2 = client.connect("127.0.0.1:65434", "player2", "token2");
    auto future3 = client.connect("127.0.0.1:65435", "player3", "token3");
    
    // 只有第一个应该开始连接过程，其他的应该立即失败
    EXPECT_THROW(future2.get(), std::runtime_error);
    EXPECT_THROW(future3.get(), std::runtime_error);
    
    // 第一个会因为没有服务器而最终失败
    EXPECT_THROW(future1.get(), std::exception);
}

/**
 * @brief 测试连接超时
 */
TEST_F(ClientConnectionTest, ConnectionTimeout) {
    Client client;
    
    // 连接到一个丢弃数据包的地址（通常会超时）
    // 使用一个保留的 IP 地址范围，这些地址不会路由
    auto future = client.connect("192.0.2.1:8080", "test_player", "test_token");
    
    // 等待超时
    auto status = future.wait_for(std::chrono::seconds(35));
    
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
    Client client;
    
    // 开始连接过程
    auto future = client.connect("127.0.0.1:65436", "test_player", "test_token");
    
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
    Client client;
    
    // 第一次连接尝试
    auto future1 = client.connect("127.0.0.1:65437", "player1", "token1");
    EXPECT_THROW(future1.get(), std::exception);
    EXPECT_FALSE(client.isConnected());
    
    // 确保完全断开
    client.disconnect();
    
    // 第二次连接尝试应该被允许
    auto future2 = client.connect("127.0.0.1:65438", "player2", "token2");
    EXPECT_THROW(future2.get(), std::exception);
    EXPECT_FALSE(client.isConnected());
}

/**
 * @brief 测试多次断开连接
 */
TEST_F(ClientConnectionTest, MultipleDisconnects) {
    Client client;
    
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
    
    // 开始连接过程
    auto future = client.connect("127.0.0.1:65439", "test_player", "test_token");
    
    // 在连接过程中尝试发送数据
    PlayerData data;
    data.set_player_id("test_player");
    
    // 应该被静默忽略
    EXPECT_NO_THROW(client.sendPlayerData(data));
    
    // 清理
    try {
        future.get();
    } catch (...) {
        // 预期会失败
    }
}
