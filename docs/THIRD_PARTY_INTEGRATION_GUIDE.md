# PICORadar 第三方语言接入指南

本文档为希望使用其他编程语言（如 Python、JavaScript、Go、Rust 等）或不使用官方 C++ 客户端库的开发者提供详细的服务器接入说明。

## 概述

PICORadar 使用标准的网络协议，因此任何支持 WebSocket 和 UDP 的编程语言都可以接入。系统主要包含两个通信部分：

1. **UDP 服务发现**：用于自动发现服务器位置
2. **WebSocket 数据通信**：用于实时位置数据传输

## 网络架构

### 默认端口配置

- **WebSocket 服务端口**：11451
- **UDP 服务发现端口**：11452
- **默认服务器地址**：0.0.0.0（监听所有接口）

### 数据格式

所有消息都使用 **Protocol Buffers** 进行序列化，定义文件位于 `proto/` 目录。

## 1. 服务发现（可选）

如果您已知服务器地址和端口，可以跳过此步骤。否则，可以通过 UDP 广播自动发现服务器。

### 发现协议

#### 发现请求

向端口 **11452** 发送 UDP 广播：

```text
消息内容: "PICO_RADAR_DISCOVERY_REQUEST"
目标地址: 255.255.255.255:11452（或局域网广播地址）
```

#### 发现响应

服务器会回复格式为：

```text
消息内容: "PICORADAR_SERVER_AT_<host>:<port>"
示例: "PICORADAR_SERVER_AT_192.168.1.100:11451"
```

### 实现示例

#### Python 实现

```python
import socket
import time

def discover_server(timeout=5):
    # 创建UDP套接字
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    sock.settimeout(timeout)
    
    try:
        # 发送发现请求
        message = "PICO_RADAR_DISCOVERY_REQUEST"
        sock.sendto(message.encode(), ('<broadcast>', 11452))
        
        # 等待响应
        data, addr = sock.recvfrom(1024)
        response = data.decode()
        
        if response.startswith("PICORADAR_SERVER_AT_"):
            server_info = response[len("PICORADAR_SERVER_AT_"):]
            host, port = server_info.split(':')
            return host, int(port)
            
    except socket.timeout:
        print("服务器发现超时")
        return None, None
    finally:
        sock.close()

# 使用示例
host, port = discover_server()
if host:
    print(f"发现服务器: {host}:{port}")
```

#### JavaScript (Node.js) 实现
```javascript
const dgram = require('dgram');

function discoverServer(timeout = 5000) {
    return new Promise((resolve, reject) => {
        const client = dgram.createSocket('udp4');
        
        client.on('message', (msg, info) => {
            const response = msg.toString();
            if (response.startsWith('PICORADAR_SERVER_AT_')) {
                const serverInfo = response.substring('PICORADAR_SERVER_AT_'.length);
                const [host, port] = serverInfo.split(':');
                client.close();
                resolve({ host, port: parseInt(port) });
            }
        });
        
        client.bind(() => {
            client.setBroadcast(true);
            const message = Buffer.from('PICO_RADAR_DISCOVERY_REQUEST');
            client.send(message, 11452, '255.255.255.255');
        });
        
        setTimeout(() => {
            client.close();
            reject(new Error('服务器发现超时'));
        }, timeout);
    });
}

// 使用示例
discoverServer()
    .then(({host, port}) => console.log(`发现服务器: ${host}:${port}`))
    .catch(err => console.error(err.message));
```

## 2. Protocol Buffers 消息定义

### 安装 Protocol Buffers

需要为您的语言安装 protobuf 支持：

- **Python**: `pip install protobuf`
- **JavaScript**: `npm install protobufjs`
- **Go**: `go get google.golang.org/protobuf`
- **Rust**: 在 `Cargo.toml` 中添加 `prost` 和 `prost-types`

### 消息结构

#### common.proto
```protobuf
syntax = "proto3";
package picoradar;

message Vector3 {
  float x = 1;
  float y = 2;
  float z = 3;
}

message Quaternion {
  float x = 1;
  float y = 2;
  float z = 3;
  float w = 4;
}
```

#### player.proto
```protobuf
syntax = "proto3";
package picoradar;

import "common.proto";

message PlayerData {
  string player_id = 1;
  string scene_id = 2;
  Vector3 position = 3;
  Quaternion rotation = 4;
  int64 timestamp = 5;
}
```

#### client.proto
```protobuf
syntax = "proto3";
package picoradar;

import "player.proto";

message AuthRequest {
  string token = 1;
  string player_id = 2;
}

message ClientToServer {
  oneof message_type {
    AuthRequest auth_request = 1;
    PlayerData player_data = 2;
  }
}
```

#### server.proto
```protobuf
syntax = "proto3";
package picoradar;

import "player.proto";

message AuthResponse {
  bool success = 1;
  string message = 2;
}

message PlayerList {
  repeated PlayerData players = 1;
}

message ServerToClient {
  oneof message_type {
    AuthResponse auth_response = 1;
    PlayerList player_list = 2;
  }
}
```

### 生成代码

#### Python
```bash
# 安装 protoc
sudo apt-get install protobuf-compiler  # Ubuntu/Debian
# 或
brew install protobuf  # macOS

# 生成 Python 代码
protoc --python_out=. *.proto
```

#### JavaScript
```bash
# 使用 protobufjs
npx pbjs -t static-module -w commonjs -o bundle.js *.proto
npx pbts -o bundle.d.ts bundle.js
```

#### Go
```bash
# 安装 protoc-gen-go
go install google.golang.org/protobuf/cmd/protoc-gen-go@latest

# 生成 Go 代码
protoc --go_out=. *.proto
```

## 3. WebSocket 连接和通信

### 连接流程

1. **建立 WebSocket 连接**：连接到 `ws://host:port`
2. **身份验证**：发送 `AuthRequest` 消息
3. **数据交换**：发送 `PlayerData`，接收 `PlayerList`
4. **断开连接**：正常关闭 WebSocket

### 实现示例

#### Python 实现
```python
import asyncio
import websockets
import time
from generated import client_pb2, server_pb2, player_pb2, common_pb2

class PICORadarClient:
    def __init__(self):
        self.websocket = None
        self.player_id = None
        
    async def connect(self, host, port, token, player_id):
        self.player_id = player_id
        uri = f"ws://{host}:{port}"
        
        try:
            self.websocket = await websockets.connect(uri)
            print(f"已连接到 {uri}")
            
            # 发送身份验证
            await self._authenticate(token, player_id)
            
            # 启动消息接收循环
            asyncio.create_task(self._message_loop())
            
        except Exception as e:
            print(f"连接失败: {e}")
            raise
    
    async def _authenticate(self, token, player_id):
        # 创建认证请求
        auth_request = client_pb2.AuthRequest()
        auth_request.token = token
        auth_request.player_id = player_id
        
        # 包装在 ClientToServer 消息中
        message = client_pb2.ClientToServer()
        message.auth_request.CopyFrom(auth_request)
        
        # 序列化并发送
        data = message.SerializeToString()
        await self.websocket.send(data)
        print("已发送认证请求")
    
    async def send_player_data(self, position, rotation, scene_id="default"):
        if not self.websocket:
            raise Exception("未连接到服务器")
        
        # 创建玩家数据
        player_data = player_pb2.PlayerData()
        player_data.player_id = self.player_id
        player_data.scene_id = scene_id
        
        # 设置位置
        player_data.position.x = position[0]
        player_data.position.y = position[1]
        player_data.position.z = position[2]
        
        # 设置旋转
        player_data.rotation.x = rotation[0]
        player_data.rotation.y = rotation[1]
        player_data.rotation.z = rotation[2]
        player_data.rotation.w = rotation[3]
        
        # 设置时间戳
        player_data.timestamp = int(time.time() * 1000)
        
        # 包装消息
        message = client_pb2.ClientToServer()
        message.player_data.CopyFrom(player_data)
        
        # 发送
        data = message.SerializeToString()
        await self.websocket.send(data)
    
    async def _message_loop(self):
        try:
            async for message in self.websocket:
                await self._handle_message(message)
        except websockets.exceptions.ConnectionClosed:
            print("服务器连接已断开")
        except Exception as e:
            print(f"消息循环错误: {e}")
    
    async def _handle_message(self, raw_data):
        try:
            # 解析服务器消息
            message = server_pb2.ServerToClient()
            message.ParseFromString(raw_data)
            
            if message.HasField('auth_response'):
                auth_resp = message.auth_response
                if auth_resp.success:
                    print("认证成功!")
                else:
                    print(f"认证失败: {auth_resp.message}")
                    
            elif message.HasField('player_list'):
                player_list = message.player_list
                print(f"收到玩家列表，共 {len(player_list.players)} 个玩家:")
                
                for player in player_list.players:
                    print(f"  玩家 {player.player_id}: "
                          f"({player.position.x:.2f}, "
                          f"{player.position.y:.2f}, "
                          f"{player.position.z:.2f})")
                          
        except Exception as e:
            print(f"消息处理错误: {e}")
    
    async def disconnect(self):
        if self.websocket:
            await self.websocket.close()
            print("已断开连接")

# 使用示例
async def main():
    client = PICORadarClient()
    
    try:
        # 连接到服务器
        await client.connect("127.0.0.1", 11451, "secure_production_token_change_me_2025", "python_player")
        
        # 发送位置数据
        for i in range(5):
            position = (float(i), 0.0, 0.0)
            rotation = (0.0, 0.0, 0.0, 1.0)
            
            await client.send_player_data(position, rotation)
            print(f"发送位置: {position}")
            
            await asyncio.sleep(1)
            
    except Exception as e:
        print(f"错误: {e}")
    finally:
        await client.disconnect()

if __name__ == "__main__":
    asyncio.run(main())
```

#### JavaScript 实现
```javascript
const WebSocket = require('ws');
const protobuf = require('protobufjs');

class PICORadarClient {
    constructor() {
        this.ws = null;
        this.player_id = null;
        this.root = null;
    }
    
    async loadProtoDefinitions() {
        // 加载 protobuf 定义
        this.root = await protobuf.load([
            'common.proto', 'player.proto', 
            'client.proto', 'server.proto'
        ]);
        
        this.AuthRequest = this.root.lookupType('picoradar.AuthRequest');
        this.ClientToServer = this.root.lookupType('picoradar.ClientToServer');
        this.ServerToClient = this.root.lookupType('picoradar.ServerToClient');
        this.PlayerData = this.root.lookupType('picoradar.PlayerData');
    }
    
    connect(host, port, token, playerId) {
        return new Promise((resolve, reject) => {
            this.player_id = playerId;
            const url = `ws://${host}:${port}`;
            
            this.ws = new WebSocket(url);
            
            this.ws.on('open', async () => {
                console.log(`已连接到 ${url}`);
                await this.authenticate(token, playerId);
                resolve();
            });
            
            this.ws.on('message', (data) => {
                this.handleMessage(data);
            });
            
            this.ws.on('error', (error) => {
                console.error('WebSocket 错误:', error);
                reject(error);
            });
            
            this.ws.on('close', () => {
                console.log('连接已关闭');
            });
        });
    }
    
    async authenticate(token, playerId) {
        const authRequest = this.AuthRequest.create({
            token: token,
            player_id: playerId
        });
        
        const message = this.ClientToServer.create({
            auth_request: authRequest
        });
        
        const buffer = this.ClientToServer.encode(message).finish();
        this.ws.send(buffer);
        console.log('已发送认证请求');
    }
    
    async sendPlayerData(position, rotation, sceneId = 'default') {
        if (!this.ws || this.ws.readyState !== WebSocket.OPEN) {
            throw new Error('未连接到服务器');
        }
        
        const playerData = this.PlayerData.create({
            player_id: this.player_id,
            scene_id: sceneId,
            position: {
                x: position[0],
                y: position[1],
                z: position[2]
            },
            rotation: {
                x: rotation[0],
                y: rotation[1],
                z: rotation[2],
                w: rotation[3]
            },
            timestamp: Date.now()
        });
        
        const message = this.ClientToServer.create({
            player_data: playerData
        });
        
        const buffer = this.ClientToServer.encode(message).finish();
        this.ws.send(buffer);
    }
    
    handleMessage(data) {
        try {
            const message = this.ServerToClient.decode(data);
            
            if (message.auth_response) {
                const authResp = message.auth_response;
                if (authResp.success) {
                    console.log('认证成功!');
                } else {
                    console.log(`认证失败: ${authResp.message}`);
                }
            } else if (message.player_list) {
                const playerList = message.player_list;
                console.log(`收到玩家列表，共 ${playerList.players.length} 个玩家:`);
                
                playerList.players.forEach(player => {
                    console.log(`  玩家 ${player.player_id}: ` +
                              `(${player.position.x.toFixed(2)}, ` +
                              `${player.position.y.toFixed(2)}, ` +
                              `${player.position.z.toFixed(2)})`);
                });
            }
        } catch (error) {
            console.error('消息处理错误:', error);
        }
    }
    
    disconnect() {
        if (this.ws) {
            this.ws.close();
            console.log('已断开连接');
        }
    }
}

// 使用示例
async function main() {
    const client = new PICORadarClient();
    
    try {
        await client.loadProtoDefinitions();
        await client.connect('127.0.0.1', 11451, 'secure_production_token_change_me_2025', 'js_player');
        
        // 发送位置数据
        for (let i = 0; i < 5; i++) {
            const position = [i, 0.0, 0.0];
            const rotation = [0.0, 0.0, 0.0, 1.0];
            
            await client.sendPlayerData(position, rotation);
            console.log(`发送位置: (${position.join(', ')})`);
            
            await new Promise(resolve => setTimeout(resolve, 1000));
        }
        
    } catch (error) {
        console.error('错误:', error);
    } finally {
        client.disconnect();
    }
}

main();
```

## 4. 配置说明

### 认证令牌
默认的认证令牌在服务器配置文件 `config/server.json` 中设置：

```json
{
    "auth": {
        "token": "secure_production_token_change_me_2025"
    }
}
```

**注意**：生产环境中请务必更换为安全的令牌。

### 其他配置项
您可以通过修改 `config/server.json` 来调整：

- 服务端口
- 发现端口
- 连接超时时间
- 日志级别等

## 5. 错误处理

### 常见错误

1. **连接被拒绝**：检查服务器是否运行，端口是否正确
2. **认证失败**：检查令牌是否正确
3. **消息格式错误**：确保 protobuf 消息结构正确
4. **网络超时**：检查网络连接，增加超时时间

### 调试技巧

1. **启用服务器详细日志**：
   ```bash
   export PICORADAR_LOG_LEVEL=DEBUG
   ./picoradar-server
   ```

2. **网络抓包分析**：
   ```bash
   sudo tcpdump -i any -w capture.pcap port 11451
   ```

3. **WebSocket 连接测试**：
   ```bash
   # 使用 wscat 测试 WebSocket 连接
   npm install -g wscat
   wscat -c ws://127.0.0.1:11451
   ```

## 6. 性能优化建议

1. **消息频率**：建议位置数据更新频率为 30-60 FPS
2. **连接复用**：重用 WebSocket 连接，避免频繁重连
3. **批量处理**：如果可能，批量处理位置更新
4. **压缩**：WebSocket 支持压缩，可以减少带宽使用

## 7. 示例项目

完整的示例代码可以在以下目录找到：

- **C++ 客户端示例**：`examples/client_example.cpp`
- **官方客户端库**：`src/client/`
- **Unreal Engine 插件**：`unreal/`

## 8. 参考文档

- [API 参考文档](docs/API_REFERENCE.md)
- [技术设计文档](TECHNICAL_DESIGN.md)
- [Protocol Buffers 官方文档](https://protobuf.dev/)
- [WebSocket 协议规范](https://tools.ietf.org/html/rfc6455)

---

如果您在实现过程中遇到问题，欢迎提交 Issue 或查看现有的文档和示例代码。我们致力于让 PICORadar 支持更多的编程语言和平台。
