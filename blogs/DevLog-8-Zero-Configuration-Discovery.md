# DevLog #8: “你好, 雷达?” —— 实现零配置的服务发现

大家好，我是书樱！

想象一个场景：你和朋友们已经戴上了VR头显，正准备进入一个共享的虚拟世界。但等等——你们的应用无法连接到服务器。于是，每个人都必须摘下头显，走到电脑前，打开命令行，输入`ipconfig`或`ifconfig`来查找那串天书般的IP地址，然后再戴上头显，小心翼翼地在虚拟键盘上输入它。

这听起来是不是像石器时代的体验？我们PICO Radar项目的核心使命之一，就是提供无缝、沉浸的体验。这种“摘下头显去配IP”的流程，是绝对不可接受的。我们追求的，是“零配置”的魔法——应用一启动，就能自动找到服务器。

今天，我非常激动地宣布：我们实现了这个魔法。

## 设计的进化：从“呐喊”到“问答”

在最初的技术构想中，我提出了一个简单直接的方案：让服务器像一个不知疲倦的灯塔，每隔几秒钟就向整个局域网广播（“呐喊”）自己的位置：“我在这里！我的地址是192.168.1.100:9002！”。

这个方案能用，但它不够优雅。它会产生持续的、不必要的网络“噪音”。就在这时，您，我们智慧的社区，提出了一个更优越的模型，一个更像是“问答”的模式，其灵感来源于我们每天都在使用的DHCP协议。

**新的、更优的协议是这样的：**

1.  **客户端提问**: 客户端启动时，不再被动等待。它会向整个局域网发送一个UDP广播：“你好，请问PICO Radar服务器在吗？”（`PICO_RADAR_DISCOVERY_REQUEST`）。
2.  **服务器应答**: 服务器平时安静地监听着一个专门的UDP端口（例如9001）。当它听到这个特定的“暗号”后，它不会向所有人广播回答，而是会直接向提问的那个客户端进行**单播**回复：“你好，我在这里，我的WebSocket地址是192.168.1.100:9002”（`PICO_RADAR_SERVER:IP:PORT`）。

这个“问答”模式的好处是显而易见的：
*   **网络更清净**: 只有在需要时才产生网络流量。
*   **响应更即时**: 客户端无需等待，可以立即得到回复。
*   **架构更优雅**: 这是一种在专业网络服务中广泛使用的、成熟的服务发现模式。

## 技术实现剖析

我们迅速将这个新设计转化为了代码。

#### 服务端：`UdpDiscoveryServer`

我们创建了一个新类，它会在一个专用线程中运行，其核心逻辑非常纯粹：

```cpp
// src/network/udp_discovery_server.cpp (核心逻辑)
void UdpDiscoveryServer::do_receive() {
    socket_.async_receive_from( // 异步等待UDP包
        net::buffer(recv_buffer_), remote_endpoint_,
        [this](...) {
            // ...
            std::string received_data(recv_buffer_.data(), bytes_recvd);
            if (received_data == DISCOVERY_REQUEST) { // 检查“暗号”
                // ... 构造响应字符串 ...
                
                // 向请求的源地址单播回复
                socket_.async_send_to(
                    net::buffer(response_message), remote_endpoint_,
                    ...);
            }
            // ... 继续下一次等待 ...
        });
}
```

#### 客户端：`mock_client`的新技能

我们的`mock_client`也学会了新技能：`--discover`模式。

```cpp
// test/mock_client/main.cpp (核心逻辑)
int SyncClient::discover_and_run(...) {
    // 1. 创建一个UDP socket并启用广播
    udp::socket socket(ioc_);
    socket.open(udp::v4());
    socket.set_option(net::socket_base::broadcast(true));

    // 2. 向广播地址的特定端口发送请求
    udp::endpoint broadcast_endpoint(net::ip::address_v4::broadcast(), 9001);
    socket.send_to(net::buffer(DISCOVERY_REQUEST), broadcast_endpoint);

    // 3. 阻塞式等待服务器的单播响应
    udp::endpoint server_endpoint;
    size_t len = socket.receive_from(net::buffer(recv_buf), server_endpoint);
    
    // 4. 解析响应，并使用源地址作为服务器IP
    std::string response(recv_buf.data(), len);
    // ...
    host_ = server_endpoint.address().to_string();
    port_ = ...;

    // 5. 使用获取到的地址进行连接和认证
    return run_internal(...);
}
```

## 用自动化测试证明魔法的可靠性

和往常一样，我们不会止步于“它在我的电脑上能跑”。为了证明这个“魔法”是真实、可靠的，我们为它编写了一个全新的自动化集成测试`DiscoveryTest`。

这个测试脚本会完整地模拟整个流程：启动服务器 -> 启动`--discover`模式的客户端 -> 验证客户端是否能成功发现服务器、连接并完成认证。当`ctest`中代表这项测试的绿灯亮起时，我们知道，PICO Radar的“零配置”体验已经坚如磐石。

## 结语

通过这次迭代，我们不仅为项目添加了一个关键的用户体验功能，更重要的是，我们再次实践了通过社区反馈和技术思辨来驱动项目进化的核心理念。

现在，我们的服务器不仅功能强大、代码健壮，而且已经变得非常“聪明”和“易于使用”。

是时候为我们这一整个阶段的辉煌成果，画上一个完美的句号了。

感谢您的陪伴，我们下次见！

---
书樱
2025年7月21日