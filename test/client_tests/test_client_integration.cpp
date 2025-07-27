#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <atomic>
#include <future>

#include "client.hpp"
#include "server/include/server.hpp"
#include "common/logging.hpp"
#include "common/config_manager.hpp"
#include <nlohmann/json.hpp>

using namespace picoradar::client;
using namespace picoradar;
using namespace picoradar::server;

class ClientIntegrationTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        // 初始化日志系统
        logger::Logger::Init("client_integration_test", "./logs", logger::LogLevel::INFO, 10, false);
        
        // 直接使用JSON配置测试服务器
        auto& config = picoradar::common::ConfigManager::getInstance();
        nlohmann::json test_config = {
            {"server", {
                {"port", 11452},
                {"host", "0.0.0.0"},
                {"websocket_port", 11452}
            }},
            {"auth", {
                {"token", "pico_radar_secret_token"}
            }},
            {"discovery", {
                {"udp_port", 11452},
                {"broadcast_interval_ms", 5000},
                {"request_message", "PICO_RADAR_DISCOVERY_REQUEST"},
                {"response_prefix", "PICORADAR_SERVER_AT_"}
            }},
            {"logging", {
                {"level", "INFO"},
                {"file_enabled", true},
                {"console_enabled", true}
            }},
            {"network", {
                {"max_connections", 20},
                {"broadcast_interval_ms", 50},
                {"heartbeat_interval_ms", 30000}
            }}
        };
        
        auto result = config.loadFromJson(test_config);
        if (!result) {
            throw std::runtime_error("Failed to load test config: " + result.error().message);
        }
    }
    
    static void TearDownTestSuite() {
        // glog 会自动清理
    }

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
    auto status = future.wait_for(std::chrono::seconds(5));
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
    auto status = future.wait_for(std::chrono::seconds(5));
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
    pos->set_x(1.0F);
    pos->set_y(2.0F);
    pos->set_z(3.0F);
    
    auto* rot = data.mutable_rotation();
    rot->set_x(0.0F);
    rot->set_y(0.0F);
    rot->set_z(0.0F);
    rot->set_w(1.0F);
    
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

    // 为每个客户端存储接收到的玩家数据
    std::vector<std::map<std::string, PlayerData>> received_data_maps(num_clients);
    std::vector<std::mutex> map_mutexes(num_clients);
    
    // 创建多个客户端
    for (int i = 0; i < num_clients; ++i) {
        auto client = std::make_unique<Client>();
        
        // 为每个客户端设置回调以存储数据
        client->setOnPlayerListUpdate([&, i](const std::vector<PlayerData>& players) {
            std::lock_guard<std::mutex> lock(map_mutexes[i]);
            LOG_DEBUG << "Client " << i << " received player list with " 
                      << players.size() << " players";
            for (const auto& player : players) {
                received_data_maps[i][player.player_id()] = player;
            }
        });
        
        // 开始连接
        auto future = client->connect(
            "127.0.0.1:" + std::to_string(test_port_),
            "test_player_" + std::to_string(i),
            "pico_radar_secret_token"
        );
        
        futures.push_back(std::move(future));
        clients.push_back(std::move(client));
        
        // 在连接之间添加小延迟以避免竞争条件
        if (i < num_clients - 1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    
    // 等待所有连接完成（带超时）
    for (int i = 0; i < futures.size(); ++i) {
        auto& future = futures[i];
        auto status = future.wait_for(std::chrono::seconds(2));
        if (status == std::future_status::timeout) {
            LOG_ERROR << "Client " << i << " connection timeout";
            FAIL() << "Client " << i << " connection timeout";
        }
        EXPECT_NO_THROW(future.get());
    }
    
    // 验证所有客户端都已连接
    for (const auto& client : clients) {
        EXPECT_TRUE(client->isConnected());
    }
    
    // 等待服务器处理所有连接和认证
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
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
    
    // 等待数据传播和服务器处理
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    
    // 验证服务器上的玩家数量
    EXPECT_EQ(server_->getPlayerCount(), num_clients);

    // 验证每个客户端收到的数据是否正确
    for (int i = 0; i < num_clients; ++i) { // 遍历每个客户端
        std::lock_guard<std::mutex> lock(map_mutexes[i]);
        const auto& received_map = received_data_maps[i];

        // 客户端应该收到所有玩家（包括自己）的数据
        ASSERT_EQ(received_map.size(), num_clients)
            << "Client " << i << " expected to receive data for " << num_clients
            << " players, but got " << received_map.size();

        for (int j = 0; j < num_clients; ++j) { // 验证来自每个发送者的数据
            std::string expected_player_id = "test_player_" + std::to_string(j);
            auto it = received_map.find(expected_player_id);

            ASSERT_NE(it, received_map.end())
                << "Client " << i << " did not receive data for player " << expected_player_id;

            const auto& data = it->second;
            EXPECT_EQ(data.scene_id(), "test_scene");
            EXPECT_FLOAT_EQ(data.position().x(), static_cast<float>(j));
            EXPECT_FLOAT_EQ(data.position().y(), static_cast<float>(j * 2));
            EXPECT_FLOAT_EQ(data.position().z(), static_cast<float>(j * 3));
        }
    }
    
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
        
        // 对于快速连接/断开，可能成功也可能因为取消而失败
        // 两种情况都是正常的
        try {
            future.get();
            // 如果连接成功，客户端应该是已连接状态
            EXPECT_TRUE(client.isConnected());
        } catch (const std::exception& e) {
            // 连接可能被取消或失败，这在快速连接/断开中是正常的
            EXPECT_FALSE(client.isConnected());
        }
        
        // 立即断开 - 无论连接状态如何都应该安全
        EXPECT_NO_THROW(client.disconnect());
        EXPECT_FALSE(client.isConnected());
        
        // 短暂等待以避免资源竞争
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}
