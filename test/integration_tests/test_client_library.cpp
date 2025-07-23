#include <gtest/gtest.h>
#include "core/player_registry.hpp"
#include "network/websocket_server.hpp"
#include "network/udp_discovery_server.hpp"
#include "client/client.hpp"
#include <glog/logging.h>
#include <chrono>
#include <future>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

using namespace picoradar;

// 定义测试常量
const std::string HOST = "127.0.0.1";
const uint16_t PORT = 9007; // 为客户端库测试使用一个新端口
const uint16_t DISCOVERY_PORT = 9008; // 发现端口

class ClientLibraryIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        registry_ = std::make_shared<core::PlayerRegistry>();
        websocket_server_ = std::make_shared<network::WebsocketServer>(ioc_, *registry_);
        discovery_server_ = std::make_shared<network::UdpDiscoveryServer>(
            ioc_, DISCOVERY_PORT, PORT, HOST);
    }

    void TearDown() override {
        websocket_server_->stop();
        discovery_server_->stop();
    }

    net::io_context ioc_;
    std::shared_ptr<core::PlayerRegistry> registry_;
    std::shared_ptr<network::WebsocketServer> websocket_server_;
    std::shared_ptr<network::UdpDiscoveryServer> discovery_server_;
};

TEST_F(ClientLibraryIntegrationTest, ClientLibraryDiscoveryAndConnect) {
    // Arrange: 启动服务器
    websocket_server_->start(HOST, PORT, 1);
    discovery_server_->start();
    
    // 确保服务器有时间启动
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Act: 使用新客户端库发现并连接到服务器
    client::Client client;
    client.set_auth_token("test_token");
    client.set_player_id("test_player");
    
    // 发现服务器
    std::string server_address = client.discover_server(DISCOVERY_PORT);
    
    // 验证发现结果
    EXPECT_FALSE(server_address.empty()) << "客户端应该能够发现服务器";
    
    // 解析服务器地址
    size_t pos = server_address.find(':');
    ASSERT_NE(pos, std::string::npos) << "服务器地址格式应该为 host:port";
    
    std::string host = server_address.substr(0, pos);
    std::string port = server_address.substr(pos + 1);
    
    // 连接到服务器
    client.connect(host, port);
    std::this_thread::sleep_for(std::chrono::milliseconds(200)); // 等待连接和认证
    
    // Assert: 验证连接结果
    EXPECT_TRUE(client.is_connected()) << "客户端应该能够成功连接到服务器";
    EXPECT_TRUE(client.is_connected()) << "客户端应该处于连接状态";
    
    // 等待服务器处理连接
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_EQ(registry_->getPlayerCount(), 1) << "应该有一个玩家连接到服务器";
    
    // 断开连接
    client.disconnect();
    EXPECT_FALSE(client.is_connected()) << "客户端断开后应该不再处于连接状态";
    
    // 等待服务器处理断开连接
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_EQ(registry_->getPlayerCount(), 0) << "客户端断开后，玩家数量应为0";
}

TEST_F(ClientLibraryIntegrationTest, ClientLibrarySendAndReceiveData) {
    // Arrange: 启动服务器
    websocket_server_->start(HOST, PORT, 1);
    discovery_server_->start();
    
    // 确保服务器有时间启动
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 创建两个客户端：一个发送数据，一个接收数据
    client::Client sender_client;
    sender_client.set_auth_token("sender_token");
    sender_client.set_player_id("sender_player");
    
    client::Client receiver_client;
    receiver_client.set_auth_token("receiver_token");
    receiver_client.set_player_id("receiver_player");
    
    // 发现并连接发送方客户端
    std::string server_address = sender_client.discover_server(DISCOVERY_PORT);
    LOG(INFO) << "Sender discovered server at: " << server_address;
    size_t pos = server_address.find(':');
    std::string host = server_address.substr(0, pos);
    std::string port = server_address.substr(pos + 1);
    sender_client.connect(host, port);
    std::this_thread::sleep_for(std::chrono::milliseconds(200)); // 等待连接和认证
    LOG(INFO) << "Sender connected: " << sender_client.is_connected();
    ASSERT_TRUE(sender_client.is_connected()) << "发送方客户端应该能够连接到服务器";
    
    // 发现并连接接收方客户端
    server_address = receiver_client.discover_server(DISCOVERY_PORT);
    LOG(INFO) << "Receiver discovered server at: " << server_address;
    pos = server_address.find(':');
    host = server_address.substr(0, pos);
    port = server_address.substr(pos + 1);
    receiver_client.connect(host, port);
    std::this_thread::sleep_for(std::chrono::milliseconds(200)); // 等待连接和认证
    LOG(INFO) << "Receiver connected: " << receiver_client.is_connected();
    ASSERT_TRUE(receiver_client.is_connected()) << "接收方客户端应该能够连接到服务器";
    
    // 等待服务器处理连接
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    LOG(INFO) << "Player count after connections: " << registry_->getPlayerCount();
    EXPECT_EQ(registry_->getPlayerCount(), 2) << "应该有两个玩家连接到服务器";
    
    // Act: 发送玩家数据
    picoradar::PlayerData player_data;
    player_data.set_player_id("sender_player");
    
    // 设置位置
    picoradar::Vector3* position = player_data.mutable_position();
    position->set_x(100.0f);
    position->set_y(200.0f);
    position->set_z(0.0f);
    
    // 设置旋转
    picoradar::Quaternion* rotation = player_data.mutable_rotation();
    rotation->set_x(0.0f);
    rotation->set_y(0.0f);
    rotation->set_z(0.0f);
    rotation->set_w(1.0f);
    
    player_data.set_timestamp(1234567890);
    
    bool sent = sender_client.send_player_data(player_data);
    LOG(INFO) << "Player data sent: " << sent;
    EXPECT_TRUE(sent) << "发送方客户端应该能够成功发送玩家数据";
    
    // 等待数据传播
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    
    // Assert: 验证接收方是否收到了数据
    const picoradar::PlayerList& player_list = receiver_client.get_player_list();
    LOG(INFO) << "Player list size: " << player_list.players_size();
    EXPECT_GE(player_list.players_size(), 1) << "接收方客户端应该收到至少一个玩家的数据";
    
    // 检查是否收到了发送方的数据
    bool found_sender = false;
    for (int i = 0; i < player_list.players_size(); ++i) {
        const auto& player = player_list.players(i);
        LOG(INFO) << "Player in list: " << player.player_id();
        if (player.player_id() == "sender_player") {
            found_sender = true;
            EXPECT_FLOAT_EQ(player.position().x(), 100.0f);
            EXPECT_FLOAT_EQ(player.position().y(), 200.0f);
            EXPECT_FLOAT_EQ(player.position().z(), 0.0f);
            break;
        }
    }
    EXPECT_TRUE(found_sender) << "接收方客户端应该收到发送方玩家的数据";
    
    // 清理
    sender_client.disconnect();
    receiver_client.disconnect();
    
    // 等待服务器处理断开连接
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_EQ(registry_->getPlayerCount(), 0) << "客户端断开后，玩家数量应为0";
}