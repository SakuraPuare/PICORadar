#include <gtest/gtest.h>
#include <chrono>
#include <thread>

#include "client.hpp"
#include "common/logging.hpp"

using namespace picoradar::client;
using namespace picoradar;

class ClientBasicTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 每个测试前的设置
    }

    void TearDown() override {
        // 每个测试后的清理
    }
};

/**
 * @brief 测试客户端的基本构造和析构
 */
TEST_F(ClientBasicTest, ConstructionAndDestruction) {
    // 测试客户端可以正常创建和销毁
    EXPECT_NO_THROW({
        Client client;
        EXPECT_FALSE(client.isConnected());
    });
}

/**
 * @brief 测试设置回调函数
 */
TEST_F(ClientBasicTest, SetPlayerListCallback) {
    Client client;
    
    bool callback_called = false;
    client.setOnPlayerListUpdate([&callback_called](const std::vector<PlayerData>& players) {
        callback_called = true;
    });
    
    // 设置回调不应该抛异常
    EXPECT_FALSE(callback_called);
}

/**
 * @brief 测试无效地址格式
 */
TEST_F(ClientBasicTest, InvalidAddressFormat) {
    Client client;
    
    // 测试无效的地址格式
    auto future1 = client.connect("invalid_address", "player1", "token");
    EXPECT_THROW(future1.get(), std::invalid_argument);
    
    auto future2 = client.connect("", "player1", "token");
    EXPECT_THROW(future2.get(), std::invalid_argument);
    
    auto future3 = client.connect("host:", "player1", "token");
    EXPECT_THROW(future3.get(), std::invalid_argument);
    
    auto future4 = client.connect(":port", "player1", "token");
    EXPECT_THROW(future4.get(), std::invalid_argument);
}

/**
 * @brief 测试重复连接
 */
TEST_F(ClientBasicTest, DuplicateConnect) {
    Client client;
    
    // 第一次连接（会失败，因为没有服务器，但不应该立即抛异常）
    auto future1 = client.connect("127.0.0.1:99999", "player1", "token");
    
    // 立即尝试第二次连接应该失败
    auto future2 = client.connect("127.0.0.1:99998", "player1", "token");
    EXPECT_THROW(future2.get(), std::runtime_error);
    
    // 清理第一个连接
    try {
        future1.get();
    } catch (...) {
        // 预期会失败
    }
}

/**
 * @brief 测试在未连接状态下发送数据
 */
TEST_F(ClientBasicTest, SendDataWhenDisconnected) {
    Client client;
    
    PlayerData data;
    data.set_player_id("test_player");
    
    // 在未连接状态下发送数据应该被静默忽略
    EXPECT_NO_THROW(client.sendPlayerData(data));
}

/**
 * @brief 测试客户端状态
 */
TEST_F(ClientBasicTest, ClientState) {
    Client client;
    
    // 初始状态应该是未连接
    EXPECT_FALSE(client.isConnected());
    
    // 尝试连接到不存在的服务器
    auto future = client.connect("127.0.0.1:99999", "player1", "token");
    
    // 连接过程中状态应该仍然是未连接（因为还没有认证成功）
    EXPECT_FALSE(client.isConnected());
    
    // 等待连接失败
    EXPECT_THROW(future.get(), std::exception);
    
    // 连接失败后应该仍然是未连接状态
    EXPECT_FALSE(client.isConnected());
}

/**
 * @brief 测试断开连接
 */
TEST_F(ClientBasicTest, DisconnectWhenNotConnected) {
    Client client;
    
    // 在未连接状态下断开连接应该是安全的
    EXPECT_NO_THROW(client.disconnect());
    EXPECT_FALSE(client.isConnected());
}

/**
 * @brief 测试析构函数自动断开连接
 */
TEST_F(ClientBasicTest, DestructorDisconnects) {
    // 创建一个作用域来测试析构函数
    {
        Client client;
        // 尝试连接（会失败，但这不影响测试析构函数）
        auto future = client.connect("127.0.0.1:99999", "player1", "token");
        
        // 当 client 离开作用域时，析构函数应该自动调用 disconnect()
    }
    
    // 如果析构函数正常工作，这里不应该有任何问题
    SUCCEED();
}
