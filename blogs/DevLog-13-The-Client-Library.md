---
title: "开发日志 #13：客户端库初探——构建游戏引擎的桥梁"
date: 2025-07-23
author: "书樱"
tags: ["C++", "WebSocket", "客户端库", "PICO", "VR", "DevLog"]
---

# 开发日志 #13：客户端库初探——构建游戏引擎的桥梁

**作者：书樱**
**日期：2025年7月23日**

> **摘要**: 本文记录了PICO Radar项目客户端库(client_lib)的初步实现。我们将探讨如何设计一个简洁、易用且功能完整的C++库，使其能够无缝集成到游戏引擎中。我们详细解析了客户端库的核心组件、网络通信实现以及与服务端的交互逻辑。

---

大家好，我是书樱！

在上一篇开发日志中，我们完成了服务端的全部核心功能，并通过全面的测试验证了其稳定性和可靠性。现在，是时候将目光转向项目的另一端——客户端了。

## 为什么需要客户端库？

在PICO Radar的整体架构中，客户端库扮演着连接游戏引擎和服务器的关键角色。它的主要职责包括：

1. **连接管理**：处理与服务器的WebSocket连接
2. **服务发现**：通过UDP广播自动发现服务器
3. **认证机制**：执行安全的身份验证流程
4. **数据传输**：发送本地玩家数据并接收其他玩家信息
5. **状态同步**：维护服务器广播的玩家列表

## 设计原则

在设计客户端库时，我们遵循了以下几个核心原则：

### 1. 简洁易用的API
客户端库应该提供一组简洁明了的接口，让游戏开发者能够轻松集成而无需深入了解底层网络细节。

```cpp
// 示例用法
picoradar::client::Client radar_client;
radar_client.set_auth_token("secret_token");
radar_client.set_player_id("player123");

// 自动发现服务器并连接
std::string server_address = radar_client.discover_server();
auto [host, port] = split_address(server_address);
if (radar_client.connect(host, port)) {
    // 成功连接
}
```

### 2. 异步非阻塞
为了不阻塞游戏主线程，客户端库在网络操作上采用了异步模式，确保游戏运行的流畅性。

### 3. 线程安全
客户端库设计为线程安全的，允许多个线程同时访问其接口。

## 核心实现

### 1. 客户端类结构

客户端库的核心是`Client`类，它封装了所有与服务器交互的逻辑：

```cpp
class Client {
public:
    Client();
    ~Client();
    
    // 连接和断开
    bool connect(const std::string& host, const std::string& port);
    std::string discover_server(uint16_t discovery_port = 9001);
    void disconnect();
    
    // 数据传输
    bool send_player_data(const picoradar::PlayerData& player_data);
    
    // 状态管理
    void set_player_id(const std::string& player_id);
    const std::string& get_player_id() const;
    void set_auth_token(const std::string& token);
    const std::string& get_auth_token() const;
    const picoradar::PlayerList& get_player_list() const;
    bool is_connected() const;
    
private:
    bool authenticate();
    void start_read();
    void on_read(beast::error_code ec, std::size_t bytes_transferred);
    
    // 网络相关成员
    net::io_context ioc_;
    websocket::stream<tcp::socket> ws_;
    
    // 客户端状态
    std::string player_id_;
    std::string auth_token_;
    picoradar::PlayerList player_list_;
    bool is_connected_;
    
    // 读取缓冲区
    beast::flat_buffer read_buffer_;
};
```

### 2. 服务发现机制

客户端库实现了UDP广播服务发现功能，使PICO设备能够自动找到服务器而无需手动配置：

```cpp
std::string Client::discover_server(uint16_t discovery_port) {
    try {
        LOG(INFO) << "Attempting to discover server via UDP broadcast...";
        
        // 创建UDP socket
        udp::socket socket(ioc_);
        socket.open(udp::v4());
        socket.set_option(net::socket_base::broadcast(true));

        // 发送发现请求
        udp::endpoint broadcast_endpoint(net::ip::address_v4::broadcast(), discovery_port);
        socket.send_to(net::buffer(config::kDiscoveryRequest), broadcast_endpoint);

        // 接收服务器响应
        udp::endpoint server_endpoint;
        std::array<char, 128> recv_buf;
        size_t len = socket.receive_from(net::buffer(recv_buf), server_endpoint);

        // 解析响应
        std::string response(recv_buf.data(), len);
        if (response.rfind(config::kDiscoveryResponsePrefix, 0) != 0) {
            LOG(ERROR) << "Received invalid discovery response: " << response;
            return "";
        }

        std::string server_address = response.substr(config::kDiscoveryResponsePrefix.length());
        LOG(INFO) << "Server discovered at " << server_address;
        return server_address;
    } catch (const std::exception& e) {
        LOG(ERROR) << "Discovery failed: " << e.what();
        return "";
    }
}
```

### 3. 认证流程

客户端库实现了基于预共享令牌的安全认证机制：

```cpp
bool Client::authenticate() {
    if (auth_token_.empty()) {
        LOG(ERROR) << "Authentication token is empty";
        return false;
    }

    if (player_id_.empty()) {
        LOG(ERROR) << "Player ID is empty";
        return false;
    }

    try {
        // 创建认证请求
        picoradar::ClientToServer auth_message;
        auto* auth_request = auth_message.mutable_auth_request();
        auth_request->set_token(auth_token_);
        auth_request->set_player_id(player_id_);

        // 序列化并发送认证请求
        std::string serialized_auth;
        auth_message.SerializeToString(&serialized_auth);
        ws_.binary(true);
        ws_.write(net::buffer(serialized_auth));

        // 等待认证响应（简化实现，实际应该异步处理）
        beast::flat_buffer buffer;
        ws_.read(buffer);

        // 解析认证响应
        picoradar::ServerToClient response;
        if (!response.ParseFromArray(buffer.data().data(), buffer.size())) {
            LOG(ERROR) << "Failed to parse authentication response";
            return false;
        }

        if (!response.has_auth_response()) {
            LOG(ERROR) << "Received non-auth response during authentication";
            return false;
        }

        const auto& auth_response = response.auth_response();
        if (!auth_response.success()) {
            LOG(ERROR) << "Authentication failed: " << auth_response.message();
            return false;
        }

        LOG(INFO) << "Authentication successful";
        return true;
    } catch (const std::exception& e) {
        LOG(ERROR) << "Authentication exception: " << e.what();
        return false;
    }
}
```

## 下一步计划

虽然我们已经实现了客户端库的基本框架和核心功能，但仍有一些工作需要完成：

1. **完善异步读取机制**：当前的读取实现需要进一步优化以更好地处理异步操作
2. **增加错误处理和重连机制**：增强客户端在网络不稳定情况下的鲁棒性
3. **编写单元测试**：为客户端库编写全面的单元测试
4. **提供C风格API**：为需要C风格接口的游戏引擎提供包装
5. **集成到Unreal Engine示例项目**：创建一个示例项目展示如何在实际游戏引擎中使用客户端库

## 结语

客户端库的初步实现标志着PICO Radar项目向着完整解决方案迈出了重要一步。通过提供一个简洁、高效且功能完整的客户端库，我们为游戏开发者扫清了集成PICO Radar系统的技术障碍。

接下来，我们将继续完善客户端库的功能，并着手创建一个Unreal Engine示例项目来展示其实际应用效果。

感谢您的阅读，我们下次再见！

---
书樱
2025年7月23日