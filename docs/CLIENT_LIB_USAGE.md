---
title: "PICO Radar客户端库使用示例"
date: 2025-07-23
---

# PICO Radar客户端库使用示例

本文档展示如何在您的项目中使用PICO Radar客户端库。

## 基本用法

```cpp
#include "client/client.hpp"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    // 创建客户端实例
    picoradar::client::Client client;
    
    // 设置认证信息
    client.set_auth_token("YOUR_SECRET_TOKEN");
    client.set_player_id("player_123");
    
    // 自动发现服务器
    std::string server_address = client.discover_server();
    
    if (!server_address.empty()) {
        // 分离主机和端口
        size_t pos = server_address.find(':');
        if (pos != std::string::npos) {
            std::string host = server_address.substr(0, pos);
            std::string port = server_address.substr(pos + 1);
            
            // 连接到服务器
            if (client.connect(host, port)) {
                std::cout << "成功连接到PICO Radar服务器" << std::endl;
                
                // 主循环 - 发送玩家位置数据
                while (client.is_connected()) {
                    // 创建玩家数据
                    picoradar::PlayerData player_data;
                    player_data.set_player_id(client.get_player_id());
                    player_data.set_scene_id("main_scene");
                    
                    // 设置位置（这里应该是从VR设备获取的实际位置）
                    auto* position = player_data.mutable_position();
                    position->set_x(1.0F);
                    position->set_y(0.0F);
                    position->set_z(0.0F);
                    
                    // 设置旋转（这里应该是从VR设备获取的实际旋转）
                    auto* rotation = player_data.mutable_rotation();
                    rotation->set_x(0.0F);
                    rotation->set_y(0.0F);
                    rotation->set_z(0.0F);
                    rotation->set_w(1.0F);
                    
                    // 设置时间戳
                    player_data.set_timestamp(std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count());
                    
                    // 发送数据到服务器
                    if (!client.send_player_data(player_data)) {
                        std::cerr << "发送玩家数据失败" << std::endl;
                        break;
                    }
                    
                    // 获取其他玩家数据
                    const auto& player_list = client.get_player_list();
                    std::cout << "当前在线玩家数: " << player_list.players_size() << std::endl;
                    
                    // 等待下一帧
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }
                
                // 断开连接
                client.disconnect();
            } else {
                std::cerr << "连接到服务器失败" << std::endl;
            }
        }
    } else {
        std::cerr << "未发现PICO Radar服务器" << std::endl;
    }
    
    return 0;
}
```

## 集成到游戏引擎

要在游戏引擎中集成PICO Radar客户端库，请按照以下步骤操作：

1. 将`client_lib`添加到您的项目依赖中
2. 在游戏初始化时创建`Client`实例
3. 设置认证令牌和玩家ID
4. 在游戏主循环中定期发送玩家位置数据
5. 使用`get_player_list()`获取其他玩家的位置信息来渲染虚拟形象

## API参考

### Client类

#### 构造函数
```cpp
Client();
```

#### 连接管理
```cpp
bool connect(const std::string& host, const std::string& port);
std::string discover_server(uint16_t discovery_port = 9001);
void disconnect();
bool is_connected() const;
```

#### 认证
```cpp
void set_auth_token(const std::string& token);
const std::string& get_auth_token() const;
void set_player_id(const std::string& player_id);
const std::string& get_player_id() const;
```

#### 数据传输
```cpp
bool send_player_data(const picoradar::PlayerData& player_data);
const picoradar::PlayerList& get_player_list() const;
```

## 注意事项

1. 确保在使用客户端库之前正确设置了认证令牌
2. 客户端库使用异步网络操作，请确保在主线程中适当处理
3. 在多线程环境中使用客户端库时，请注意线程安全