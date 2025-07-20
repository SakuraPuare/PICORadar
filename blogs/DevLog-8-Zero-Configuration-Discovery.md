# 开发日志 #8：服务发现协议——实现“零配置”的UDP魔法

**作者：书樱**
**日期：2025年7月21日**

> **摘要**: 本文将深入探讨PICO Radar项目“零配置”服务发现功能的协议设计与技术实现。我们将对比不同的网络通信模式（广播、多播、单播），阐述为何选择“客户端请求/服务器响应”模型。同时，我们将剖析基于Boost.Asio的异步UDP服务器和同步UDP客户端的关键网络编程技术，并强调端到端集成测试对于验证一个自定义网络协议正确性的至关重要性。

---

大家好，我是书樱！

在构建任何网络服务的过程中，一个核心的用户体验问题是：客户端如何找到服务器？在PICO Radar的VR场景中，要求用户手动配置IP地址是完全不可接受的。我们追求的，是一种“即插即用”的魔法体验。今天，我将分享我们如何通过设计和实现一个迷你的**服务发现协议 (Service Discovery Protocol)**，来施展这个魔法。

### 协议设计的权衡：从“呐喊”到“问答”

服务发现的本质，是在一个动态的网络环境中定位一个或多个服务实例。我们探讨了两种主要的设计模式：

1.  **服务器持续广播 (Server-Side Broadcast)**: 服务器像一座灯塔，定期向整个局域网广播（Broadcast）自己的存在。这个方案简单，但其缺点是会产生持续的网络“噪音”，即使在没有客户端需要连接时也是如此。
2.  **客户端请求/服务器响应 (Client-Request/Server-Response)**: 客户端在需要时，向局域网广播一个“发现请求”。所有监听到此请求的服务器，会向该客户端进行**单播 (Unicast)** 回复。

我们最终选择了第二种模型，其灵感源于DHCP等成熟的网络协议。它的优势是显而易见的：
-   **网络效率**: 只在需要时才产生网络流量，避免了不必要的广播风暴。
-   **即时响应**: 客户端可以主动发起请求并立即等待响应，而非被动地等待下一次广播。
-   **架构优雅**: 这是一种在分布式系统中被广泛采用的、成熟的请求/响应模式。

我们的协议被设计得极为简洁：
-   **客户端请求**: 向UDP端口`9001`的广播地址发送一个UTF-8字符串：`PICO_RADAR_DISCOVERY_REQUEST`。
-   **服务器响应**: 向请求的源IP和端口，单播回复一个字符串：`PICO_RADAR_SERVER:IP:PORT`，其中`IP:PORT`是WebSocket服务的地址。

### 技术实现剖析：UDP网络编程

我们将这个协议转化为了代码，分别在服务器和客户端实现了UDP通信逻辑。

#### 服务端：异步UDP监听

我们创建了`UdpDiscoveryServer`类，它在一个独立的后台线程中运行，并采用**异步**模式来处理请求，以避免阻塞主服务器的I/O线程。

```cpp
// src/network/udp_discovery_server.cpp (核心逻辑)
void UdpDiscoveryServer::do_receive() {
    // 发起一个异步接收操作，提供一个缓冲区和用于存储源地址的endpoint
    socket_.async_receive_from(
        net::buffer(recv_buffer_), remote_endpoint_,
        [this](beast::error_code ec, std::size_t bytes) {
            if (!ec && bytes > 0) {
                // 检查收到的“暗号”是否正确
                if (is_valid_request(recv_buffer_, bytes)) {
                    // 构造响应消息，并向请求的源地址单播回复
                    socket_.async_send_to(net::buffer(response_message_), remote_endpoint_, ...);
                }
            }
            // 无论成功与否，都再次调用do_receive()，形成一个无限的监听循环
            do_receive();
        });
}
```

#### 客户端：同步UDP广播与接收

与服务器不同，`mock_client`的服务发现逻辑是**同步**的，因为在成功发现服务器之前，它无法进行任何后续操作。使用同步（阻塞）IO可以极大地简化其线性逻辑。

```cpp
// test/mock_client/main.cpp (核心逻辑)
int SyncClient::discover_and_run(...) {
    // 1. 创建一个IPv4 UDP socket
    udp::socket socket(ioc_);
    socket.open(udp::v4());

    // 2. 向操作系统申请发送广播包的权限
    socket.set_option(net::socket_base::broadcast(true));

    // 3. 定义广播目标：特殊广播地址255.255.255.255和约定好的端口9001
    udp::endpoint broadcast_endpoint(net::ip::address_v4::broadcast(), 9001);
    socket.send_to(net::buffer(DISCOVERY_REQUEST), broadcast_endpoint);

    // 4. 阻塞式地等待服务器的单播响应
    udp::endpoint server_endpoint;
    size_t len = socket.receive_from(net::buffer(recv_buf), server_endpoint);
    
    // 5. 解析响应，并从响应的源endpoint中提取服务器IP
    std::string response(recv_buf.data(), len);
    host_ = server_endpoint.address().to_string();
    // ...

    // 6. 使用获取到的地址进行后续的WebSocket连接
    return run_internal(...);
}
```

### 用集成测试验证协议的端到端正确性

与所有其他功能一样，我们为服务发现编写了一个专属的、自动化的集成测试`DiscoveryTest`。这个测试的重要性在于，它验证的不是服务器或客户端的单独功能，而是**整个服务发现协议的端到端（End-to-End）正确性**。

当`ctest`中代表`DiscoveryTest`的绿灯亮起时，我们知道，PICO Radar的“零配置”魔法已经从一个设计理念，变成了一个经过严格验证、坚如磐石的现实。

### 结语

通过这次迭代，我们不仅为项目增添了一个提升用户体验的关键功能，更完成了一次迷你的、完整的网络协议设计与实现之旅。它再次印证了我们的核心开发理念：通过深入的技术思辨来驱动设计，并通过严谨的自动化测试来保证质量。

现在，我们的服务器不仅功能强大、代码健壮，而且已经变得非常“聪明”和易于使用了。

感谢您的陪伴，我们下次见！

---
书樱
2025年7月21日
