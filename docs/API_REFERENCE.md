# PICORadar API 参考文档

本文档详细介绍 PICORadar 系统的核心 API，包括服务端接口、客户端库接口和网络协议。

## 目录

- [客户端库 API](#客户端库-api)
- [服务端配置 API](#服务端配置-api)
- [网络协议规范](#网络协议规范)
- [错误代码参考](#错误代码参考)

## 客户端库 API

### Client 类

主要的客户端接口类，提供与 PICORadar 服务器的连接和通信功能。

#### 构造函数

```cpp
namespace picoradar::client {

class Client {
public:
    // 默认构造函数
    Client();
    
    // 指定IO上下文的构造函数
    explicit Client(boost::asio::io_context& io_context);
    
    // 移动构造函数
    Client(Client&& other) noexcept;
    
    // 析构函数
    ~Client();
};

}
```

#### 核心方法

##### 连接管理

```cpp
// 异步连接到服务器
std::future<void> connect_async(
    const std::string& host,
    const std::string& port,
    const std::string& token
);

// 同步连接到服务器
void connect(
    const std::string& host, 
    const std::string& port,
    const std::string& token
);

// 断开连接
void disconnect();

// 检查连接状态
bool is_connected() const;
```

##### 数据传输

```cpp
// 发送玩家位置数据
std::future<void> send_player_data_async(const PlayerData& data);

// 同步发送玩家位置数据
void send_player_data(const PlayerData& data);

// 设置数据接收回调
void set_data_received_callback(
    std::function<void(const std::vector<PlayerData>&)> callback
);

// 设置错误处理回调
void set_error_callback(
    std::function<void(const std::string&)> callback
);
```

##### 服务发现

```cpp
// 自动发现服务器
std::future<ServerInfo> discover_server_async(
    const std::string& token,
    std::chrono::milliseconds timeout = std::chrono::milliseconds(5000)
);

// 同步服务器发现
ServerInfo discover_server(
    const std::string& token,
    std::chrono::milliseconds timeout = std::chrono::milliseconds(5000)
);
```

#### 数据结构

##### PlayerData

```cpp
struct PlayerData {
    std::string player_id;           // 玩家唯一标识
    
    // 位置信息
    float position_x;
    float position_y;
    float position_z;
    
    // 旋转信息（四元数）
    float rotation_x;
    float rotation_y;
    float rotation_z;
    float rotation_w;
    
    // 时间戳
    uint64_t timestamp;
    
    // 可选的扩展数据
    std::map<std::string, std::string> metadata;
};
```

##### ServerInfo

```cpp
struct ServerInfo {
    std::string host;                // 服务器主机地址
    std::string port;                // 服务器端口
    std::string server_name;         // 服务器名称
    uint32_t max_players;            // 最大玩家数
    uint32_t current_players;        // 当前玩家数
    uint64_t discovery_timestamp;    // 发现时间戳
};
```

#### 使用示例

```cpp
#include "client.hpp"
using namespace picoradar::client;

int main() {
    try {
        Client client;
        
        // 自动发现服务器
        auto server_info = client.discover_server("your-secret-token");
        std::cout << "发现服务器: " << server_info.host 
                  << ":" << server_info.port << std::endl;
        
        // 连接到服务器
        client.connect(server_info.host, server_info.port, "your-secret-token");
        
        // 设置数据接收回调
        client.set_data_received_callback([](const auto& players) {
            for (const auto& player : players) {
                std::cout << "玩家 " << player.player_id 
                          << " 位置: (" << player.position_x 
                          << ", " << player.position_y 
                          << ", " << player.position_z << ")" << std::endl;
            }
        });
        
        // 发送玩家数据
        PlayerData my_data;
        my_data.player_id = "player_001";
        my_data.position_x = 1.0f;
        my_data.position_y = 0.0f;
        my_data.position_z = 2.5f;
        my_data.rotation_x = 0.0f;
        my_data.rotation_y = 0.0f;
        my_data.rotation_z = 0.0f;
        my_data.rotation_w = 1.0f;
        my_data.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        
        // 异步发送数据
        auto send_future = client.send_player_data_async(my_data);
        send_future.wait();
        
        // 保持连接
        std::this_thread::sleep_for(std::chrono::seconds(10));
        
        client.disconnect();
        
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
```

## 服务端配置 API

### JSON 配置文件格式

服务端使用 JSON 格式的配置文件 (`config/server.json`)：

```json
{
  "server": {
    "port": 9002,
    "host": "0.0.0.0",
    "max_connections": 20,
    "heartbeat_interval_ms": 30000,
    "connection_timeout_ms": 60000
  },
  "discovery": {
    "enabled": true,
    "port": 9001,
    "broadcast_interval_ms": 2000,
    "server_name": "PICORadar-Server"
  },
  "authentication": {
    "enabled": true,
    "token": "your-secret-token-here",
    "token_validation_timeout_ms": 5000
  },
  "logging": {
    "level": "INFO",
    "file_logging": true,
    "console_logging": true,
    "max_log_file_size_mb": 100,
    "max_log_files": 5
  },
  "performance": {
    "thread_pool_size": 4,
    "message_queue_size": 1000,
    "compression_enabled": false
  }
}
```

### 配置项说明

#### Server 配置

- `port`: WebSocket 服务端口 (默认: 9002)
- `host`: 绑定的主机地址 (默认: "0.0.0.0")
- `max_connections`: 最大同时连接数 (默认: 20)
- `heartbeat_interval_ms`: 心跳间隔毫秒数 (默认: 30000)
- `connection_timeout_ms`: 连接超时毫秒数 (默认: 60000)

#### Discovery 配置

- `enabled`: 是否启用服务发现 (默认: true)
- `port`: UDP 广播端口 (默认: 9001)
- `broadcast_interval_ms`: 广播间隔毫秒数 (默认: 2000)
- `server_name`: 服务器名称 (默认: "PICORadar-Server")

#### Authentication 配置

- `enabled`: 是否启用身份验证 (默认: true)
- `token`: 预共享令牌
- `token_validation_timeout_ms`: 令牌验证超时毫秒数 (默认: 5000)

### 环境变量覆盖

支持通过环境变量覆盖配置文件设置：

```bash
export PICORADAR_SERVER_PORT=9003
export PICORADAR_SERVER_HOST=127.0.0.1
export PICORADAR_AUTH_TOKEN=my-custom-token
export PICORADAR_LOG_LEVEL=DEBUG
```

## 网络协议规范

### Protocol Buffers 消息定义

#### 客户端消息 (client.proto)

```protobuf
syntax = "proto3";

package picoradar;

// 客户端发送的玩家数据
message ClientPlayerData {
  string player_id = 1;
  
  // 位置信息
  float position_x = 2;
  float position_y = 3;
  float position_z = 4;
  
  // 旋转信息（四元数）
  float rotation_x = 5;
  float rotation_y = 6;
  float rotation_z = 7;
  float rotation_w = 8;
  
  // 时间戳
  uint64 timestamp = 9;
  
  // 扩展数据
  map<string, string> metadata = 10;
}

// 客户端请求消息
message ClientRequest {
  enum Type {
    AUTHENTICATION = 0;
    PLAYER_DATA = 1;
    HEARTBEAT = 2;
    DISCONNECT = 3;
  }
  
  Type type = 1;
  string token = 2;               // 认证令牌
  ClientPlayerData player_data = 3;
}
```

#### 服务端消息 (server.proto)

```protobuf
syntax = "proto3";

package picoradar;

// 服务端发送的玩家数据
message ServerPlayerData {
  string player_id = 1;
  
  float position_x = 2;
  float position_y = 3;
  float position_z = 4;
  
  float rotation_x = 5;
  float rotation_y = 6;
  float rotation_z = 7;
  float rotation_w = 8;
  
  uint64 timestamp = 9;
  map<string, string> metadata = 10;
}

// 服务端响应消息
message ServerResponse {
  enum Status {
    SUCCESS = 0;
    ERROR = 1;
    AUTHENTICATION_REQUIRED = 2;
    AUTHENTICATION_FAILED = 3;
    PLAYER_LIMIT_REACHED = 4;
  }
  
  Status status = 1;
  string message = 2;
  repeated ServerPlayerData players = 3;  // 所有玩家数据
  uint32 player_count = 4;                // 当前玩家数
  uint64 server_timestamp = 5;            // 服务器时间戳
}
```

### WebSocket 通信流程

#### 1. 连接建立

```
Client -> Server: WebSocket 握手
Server -> Client: 握手确认
```

#### 2. 身份验证

```
Client -> Server: ClientRequest {
  type: AUTHENTICATION,
  token: "secret-token"
}

Server -> Client: ServerResponse {
  status: SUCCESS,
  message: "认证成功"
}
```

#### 3. 数据交换

```
Client -> Server: ClientRequest {
  type: PLAYER_DATA,
  player_data: {...}
}

Server -> All Clients: ServerResponse {
  status: SUCCESS,
  players: [...]  // 所有玩家数据
}
```

#### 4. 心跳机制

```
Client -> Server: ClientRequest {
  type: HEARTBEAT
}

Server -> Client: ServerResponse {
  status: SUCCESS,
  server_timestamp: 1234567890
}
```

### UDP 服务发现协议

#### 发现请求

```
Client -> Broadcast: {
  "type": "discovery_request",
  "token": "secret-token",
  "client_id": "unique-client-id"
}
```

#### 发现响应

```
Server -> Client: {
  "type": "discovery_response",
  "server_name": "PICORadar-Server",
  "host": "192.168.1.100",
  "port": 9002,
  "max_players": 20,
  "current_players": 5,
  "timestamp": 1234567890
}
```

## 错误代码参考

### 客户端错误代码

| 错误代码 | 说明 | 解决方案 |
|---------|------|---------|
| `CONNECTION_FAILED` | 无法连接到服务器 | 检查网络连接和服务器地址 |
| `AUTHENTICATION_FAILED` | 身份验证失败 | 验证令牌是否正确 |
| `TIMEOUT` | 操作超时 | 检查网络延迟或增加超时时间 |
| `INVALID_DATA` | 数据格式无效 | 检查发送的数据格式 |
| `NETWORK_ERROR` | 网络通信错误 | 检查网络连接稳定性 |

### 服务端错误代码

| 错误代码 | 说明 | 处理方式 |
|---------|------|---------|
| `INVALID_TOKEN` | 令牌无效 | 拒绝连接请求 |
| `PLAYER_LIMIT_REACHED` | 玩家数量达到上限 | 拒绝新连接 |
| `MALFORMED_MESSAGE` | 消息格式错误 | 忽略消息并记录日志 |
| `CONNECTION_LOST` | 连接丢失 | 清理玩家数据 |
| `INTERNAL_ERROR` | 内部服务器错误 | 记录错误日志 |

### 调试技巧

#### 启用详细日志

```bash
# 设置日志级别为 DEBUG
export PICORADAR_LOG_LEVEL=DEBUG

# 运行服务器
./build/src/server/server
```

#### 网络抓包分析

```bash
# 使用 tcpdump 监控网络流量
sudo tcpdump -i any -w picoradar.pcap port 9002

# 使用 Wireshark 分析抓包文件
wireshark picoradar.pcap
```

#### 性能监控

```bash
# 使用内置的性能基准测试
./build/test/benchmark_server

# 监控系统资源使用
top -p $(pgrep server)
```

## 最佳实践

### 客户端最佳实践

1. **连接管理**: 总是在适当的时候调用 `disconnect()`
2. **错误处理**: 设置错误回调并适当处理异常
3. **资源管理**: 避免频繁创建和销毁 Client 对象
4. **数据频率**: 根据实际需求调整数据发送频率（推荐 30-60 FPS）

### 服务端最佳实践

1. **配置管理**: 使用环境变量来管理敏感配置
2. **日志监控**: 定期检查日志文件，监控系统健康状态
3. **性能调优**: 根据实际负载调整线程池大小和缓冲区配置
4. **安全考虑**: 定期更换认证令牌，监控异常连接

---

更多信息请参考：
- [客户端库使用指南](CLIENT_LIB_USAGE.md)
- [CLI 界面指南](CLI_INTERFACE_GUIDE.md)
- [技术设计文档](../TECHNICAL_DESIGN.md)
