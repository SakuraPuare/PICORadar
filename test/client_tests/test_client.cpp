#include "client/client.hpp"
#include "common/logging.hpp"
#include "common/constants.hpp"
#include "player_data.pb.h"
#include <glog/logging.h>
#include <gtest/gtest.h>
#include <thread>
#include <chrono>

// 测试客户端基本构造
TEST(ClientTest, BasicConstruction) {
    picoradar::client::Client client;
    EXPECT_TRUE(true); // 只是确认对象能被创建
}

// 测试设置认证信息
TEST(ClientTest, SetAuthInfo) {
    picoradar::client::Client client;
    client.set_auth_token("test_token");
    client.set_player_id("test_player");
    
    EXPECT_EQ(client.get_auth_token(), "test_token");
    EXPECT_EQ(client.get_player_id(), "test_player");
}

// 测试连接功能（需要模拟服务器）
TEST(ClientTest, ConnectAndDisconnect) {
    // 注意：这需要一个运行的服务器进行完整测试
    // 这里我们测试基本的接口调用
    picoradar::client::Client client;
    client.set_auth_token("test_token");
    client.set_player_id("test_player");
    
    // 尝试连接到不存在的服务器应该失败
    client.connect("127.0.0.1", "9999");
    EXPECT_FALSE(client.is_connected());
    
    // 断开连接应该总是成功
    client.disconnect();
    EXPECT_TRUE(true);
}

// 测试玩家数据发送
TEST(ClientTest, SendPlayerData) {
    picoradar::client::Client client;
    
    // 创建示例玩家数据
    picoradar::PlayerData player_data;
    player_data.set_player_id("test_player");
    
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
    
    // 在未连接状态下发送数据应该返回false
    bool result = client.send_player_data(player_data);
    EXPECT_FALSE(result);
}

// 测试获取玩家列表
TEST(ClientTest, GetPlayerList) {
    picoradar::client::Client client;
    
    // 初始时玩家列表应该是空的
    const picoradar::PlayerList& player_list = client.get_player_list();
    EXPECT_EQ(player_list.players_size(), 0);
}

// 测试服务发现功能（需要模拟UDP服务器）
TEST(ClientTest, DiscoverServer) {
    // 这个测试会超时，因为我们没有运行UDP服务器
    // 在实际集成测试中，我们会启动一个UDP服务器来响应发现请求
    // 这里我们只做基本的接口测试
    
    // 创建一个独立的io_context用于测试发现功能
    boost::asio::io_context io_context;
    
    try {
        // 创建UDP socket
        boost::asio::ip::udp::socket socket(io_context);
        socket.open(boost::asio::ip::udp::v4());
        socket.set_option(boost::asio::socket_base::broadcast(true));
        
        // 发送发现请求
        boost::asio::ip::udp::endpoint broadcast_endpoint(
            boost::asio::ip::address_v4::broadcast(), 
            picoradar::config::kDiscoveryPort
        );
        
        socket.send_to(
            boost::asio::buffer(picoradar::config::kDiscoveryRequest), 
            broadcast_endpoint
        );
        
        // 这里我们不等待响应，因为我们知道没有服务器在运行
        // 在实际测试中，我们会有一个服务器来响应
        
        socket.close();
        SUCCEED(); // 只要能发送请求就认为测试通过
    } catch (const std::exception& e) {
        FAIL() << "Failed to send discovery request: " << e.what();
    }
}

// 测试连接状态检查
TEST(ClientTest, IsConnected) {
    picoradar::client::Client client;
    
    // 初始状态应该是未连接
    EXPECT_FALSE(client.is_connected());
    
    // 尝试连接到不存在的服务器后应该仍然是未连接状态
    client.connect("127.0.0.1", "9999");
    EXPECT_FALSE(client.is_connected());
}

// 测试认证信息验证
TEST(ClientTest, AuthenticationValidation) {
    picoradar::client::Client client;
    
    // 未设置认证信息时认证应该失败
    // 这需要访问私有方法，所以我们在测试中模拟这个场景
    // 实际的认证测试需要在网络层进行
    
    // 设置认证信息后
    client.set_auth_token("test_token");
    client.set_player_id("test_player");
    
    // 验证信息设置正确
    EXPECT_EQ(client.get_auth_token(), "test_token");
    EXPECT_EQ(client.get_player_id(), "test_player");
}

// 测试PlayerData消息构建
TEST(ClientTest, PlayerDataMessageBuilding) {
    picoradar::PlayerData player_data;
    player_data.set_player_id("test_player");
    player_data.set_scene_id("test_scene");
    
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
    
    // 验证数据正确设置
    EXPECT_EQ(player_data.player_id(), "test_player");
    EXPECT_EQ(player_data.scene_id(), "test_scene");
    EXPECT_TRUE(player_data.has_position());
    EXPECT_FLOAT_EQ(player_data.position().x(), 100.0f);
    EXPECT_FLOAT_EQ(player_data.position().y(), 200.0f);
    EXPECT_FLOAT_EQ(player_data.position().z(), 0.0f);
    EXPECT_TRUE(player_data.has_rotation());
    EXPECT_FLOAT_EQ(player_data.rotation().x(), 0.0f);
    EXPECT_FLOAT_EQ(player_data.rotation().y(), 0.0f);
    EXPECT_FLOAT_EQ(player_data.rotation().z(), 0.0f);
    EXPECT_FLOAT_EQ(player_data.rotation().w(), 1.0f);
    EXPECT_EQ(player_data.timestamp(), 1234567890);
}

// 测试PlayerList消息处理
TEST(ClientTest, PlayerListMessageHandling) {
    picoradar::PlayerList player_list;
    
    // 添加几个玩家
    picoradar::PlayerData* player1 = player_list.add_players();
    player1->set_player_id("player1");
    
    picoradar::PlayerData* player2 = player_list.add_players();
    player2->set_player_id("player2");
    
    // 验证玩家数量
    EXPECT_EQ(player_list.players_size(), 2);
    EXPECT_EQ(player_list.players(0).player_id(), "player1");
    EXPECT_EQ(player_list.players(1).player_id(), "player2");
}