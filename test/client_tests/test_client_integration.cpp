#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <atomic>
#include <future>

#include "client.hpp"
#include "server/include/server.hpp"
#include "common/logging.hpp"

using namespace picoradar::client;
using namespace picoradar;
using namespace picoradar::server;

class ClientIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 启动测试服务器
        server_ = std::make_unique<Server>();
        server_->start(test_port_, 1);
        
        // 给服务器一些时间启动
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    void TearDown() override {
        // 停止测试服务器
        if (server_) {
            server_->stop();
            server_.reset();
        }
    }

    static constexpr uint16_t test_port_ = 11452;  // 使用不同的端口避免冲突
    std::unique_ptr<Server> server_;
};

/**
 * @brief 测试成功连接到真实服务器
 */
TEST_F(ClientIntegrationTest, SuccessfulConnection) {
    Client client;
    
    // 连接到测试服务器
    auto future = client.connect(
        "127.0.0.1:" + std::to_string(test_port_),
        "test_player_integration",
        "pico_radar_secret_token"  // 使用正确的令牌
    );
    
    // 等待连接完成
    auto status = future.wait_for(std::chrono::seconds(10));
    ASSERT_EQ(status, std::future_status::ready);
    
    // 连接应该成功
    EXPECT_NO_THROW(future.get());
    
    // 客户端应该处于已连接状态
    EXPECT_TRUE(client.isConnected());
    
    // 正常断开连接
    client.disconnect();
    EXPECT_FALSE(client.isConnected());
}

/**
 * @brief 测试认证失败
 */
TEST_F(ClientIntegrationTest, AuthenticationFailure) {
    Client client;
    
    // 使用错误的令牌连接
    auto future = client.connect(
        "127.0.0.1:" + std::to_string(test_port_),
        "test_player",
        "wrong_token"
    );
    
    // 等待连接完成
    auto status = future.wait_for(std::chrono::seconds(10));
    ASSERT_EQ(status, std::future_status::ready);
    
    // 认证应该失败
    EXPECT_THROW(future.get(), std::exception);
    
    // 客户端应该处于未连接状态
    EXPECT_FALSE(client.isConnected());
}

/**
 * @brief 测试发送和接收数据
 */
TEST_F(ClientIntegrationTest, SendAndReceiveData) {
    Client client;
    
    // 设置玩家列表回调
    std::atomic<bool> callback_called{false};
    std::atomic<size_t> player_count{0};
    
    client.setOnPlayerListUpdate([&](const std::vector<PlayerData>& players) {
        callback_called = true;
        player_count = players.size();
        LOG_DEBUG << "Received player list with " << players.size() << " players";
    });
    
    // 连接到服务器
    auto future = client.connect(
        "127.0.0.1:" + std::to_string(test_port_),
        "integration_test_player",
        "pico_radar_secret_token"
    );
    
    ASSERT_NO_THROW(future.get());
    ASSERT_TRUE(client.isConnected());
    
    // 发送玩家数据
    PlayerData data;
    data.set_player_id("integration_test_player");
    data.set_scene_id("test_scene");
    
    auto* pos = data.mutable_position();
    pos->set_x(1.0f);
    pos->set_y(2.0f);
    pos->set_z(3.0f);
    
    auto* rot = data.mutable_rotation();
    rot->set_x(0.0f);
    rot->set_y(0.0f);
    rot->set_z(0.0f);
    rot->set_w(1.0f);
    
    data.set_timestamp(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    
    // 发送数据
    EXPECT_NO_THROW(client.sendPlayerData(data));
    
    // 等待一段时间让服务器处理数据并发送玩家列表
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // 验证是否收到了玩家列表回调
    EXPECT_TRUE(callback_called.load());
    EXPECT_GE(player_count.load(), 1);  // 至少应该有我们自己的玩家
    
    // 断开连接
    client.disconnect();
    EXPECT_FALSE(client.isConnected());
}

/**
 * @brief 测试多个客户端同时连接
 */
TEST_F(ClientIntegrationTest, MultipleClients) {
    const int num_clients = 3;
    std::vector<std::unique_ptr<Client>> clients;
    std::vector<std::future<void>> futures;
    
    // 创建多个客户端
    for (int i = 0; i < num_clients; ++i) {
        auto client = std::make_unique<Client>();
        
        // 为每个客户端设置回调
        client->setOnPlayerListUpdate([i](const std::vector<PlayerData>& players) {
            LOG_DEBUG << "Client " << i << " received player list with " 
                      << players.size() << " players";
        });
        
        // 开始连接
        auto future = client->connect(
            "127.0.0.1:" + std::to_string(test_port_),
            "test_player_" + std::to_string(i),
            "pico_radar_secret_token"
        );
        
        futures.push_back(std::move(future));
        clients.push_back(std::move(client));
    }
    
    // 等待所有连接完成
    for (auto& future : futures) {
        EXPECT_NO_THROW(future.get());
    }
    
    // 验证所有客户端都已连接
    for (const auto& client : clients) {
        EXPECT_TRUE(client->isConnected());
    }
    
    // 每个客户端发送数据
    for (int i = 0; i < num_clients; ++i) {
        PlayerData data;
        data.set_player_id("test_player_" + std::to_string(i));
        data.set_scene_id("test_scene");
        
        auto* pos = data.mutable_position();
        pos->set_x(static_cast<float>(i));
        pos->set_y(static_cast<float>(i * 2));
        pos->set_z(static_cast<float>(i * 3));
        
        clients[i]->sendPlayerData(data);
    }
    
    // 等待数据传播
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // 验证服务器上的玩家数量
    EXPECT_EQ(server_->getPlayerCount(), num_clients);
    
    // 断开所有连接
    for (auto& client : clients) {
        client->disconnect();
        EXPECT_FALSE(client->isConnected());
    }
}

/**
 * @brief 测试客户端在服务器关闭时的行为
 */
TEST_F(ClientIntegrationTest, ServerShutdownDuringConnection) {
    Client client;
    
    // 连接到服务器
    auto future = client.connect(
        "127.0.0.1:" + std::to_string(test_port_),
        "test_player",
        "pico_radar_secret_token"
    );
    
    ASSERT_NO_THROW(future.get());
    ASSERT_TRUE(client.isConnected());
    
    // 关闭服务器
    server_->stop();
    
    // 等待一段时间让客户端检测到连接断开
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // 尝试发送数据（应该失败或被忽略）
    PlayerData data;
    data.set_player_id("test_player");
    
    // 这不应该崩溃，即使连接已断开
    EXPECT_NO_THROW(client.sendPlayerData(data));
    
    // 客户端断开连接应该是安全的
    EXPECT_NO_THROW(client.disconnect());
}

/**
 * @brief 测试快速连接和断开
 */
TEST_F(ClientIntegrationTest, RapidConnectDisconnect) {
    Client client;
    
    for (int i = 0; i < 5; ++i) {
        // 连接
        auto future = client.connect(
            "127.0.0.1:" + std::to_string(test_port_),
            "rapid_test_player_" + std::to_string(i),
            "pico_radar_secret_token"
        );
        
        EXPECT_NO_THROW(future.get());
        EXPECT_TRUE(client.isConnected());
        
        // 立即断开
        client.disconnect();
        EXPECT_FALSE(client.isConnected());
        
        // 短暂等待
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}
