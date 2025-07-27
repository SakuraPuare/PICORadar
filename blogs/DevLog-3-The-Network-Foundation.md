# 开发日志 #3：网络基石——异步I/O与服务器架构设计

**作者：书樱**
**日期：2025年7月21日**

> **摘要**: 本文详细记录了PICO Radar项目从纯粹的本地核心逻辑迈向功能性网络服务器的关键一步。我们将深入探讨从`websocketpp`迁移至`Boost.Beast`的技术决策过程，重点剖析Boost.Asio库的异步I/O模型（Proactor模式）及其在高并发场景下的优势。同时，我们将解构完整的服务器架构实现，阐述依赖注入、I/O线程池、会话管理、以及优雅停机等现代服务器设计的核心原则。最终展示如何将纯粹的业务逻辑与网络通信层完美解耦，构建出一个可扩展、可维护的服务器架构。

---

大家好，我是书樱。

在上一篇日志中，我们通过TDD为`PlayerRegistry`核心模块建立了坚实的、可验证的基础。它就像一台经过精密调校的引擎，现在是时候为这台引擎构建传动系统和底盘，让它真正连接到外部世界。今天，我们将深入探讨如何构建一个功能完备的WebSocket服务器，将核心逻辑暴露于网络之中。

## 第一阶段：技术栈演进与架构升级

### 从websocketpp到Boost.Beast的迁移

我们最初的技术选型包含了`websocketpp`，但在深入评估后，我们决定进行一次重要的技术升级，全面转向**Boost.Beast**。这一决策的驱动力来自多个技术层面的考量：

#### 技术债务的现实

首先是`websocketpp`的技术债务问题：

```cmake
# websocketpp的问题
cmake_minimum_required(VERSION 2.8.8)  # 过于陈旧
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")  # 不支持现代C++

# 与我们的现代CMake 3.20+产生不可调和的冲突
# 尝试的兼容性方案都无法完美解决
```

这些技术债务迫使我们需要维护fork版本，增加了维护复杂度。

#### Boost.Beast的技术优势

**1. 现代C++设计哲学**

```cpp
// Beast完全拥抱C++14/17的特性
template<class AsyncWriteStream, class Handler>
auto async_write_some(AsyncWriteStream& stream, 
                     ConstBufferSequence const& buffers, 
                     Handler&& handler)
    -> std::enable_if_t<is_async_write_stream<AsyncWriteStream>::value>;
```

**2. Header-Only设计的便利性**

```cmake
# 简洁的集成方式
find_package(Boost REQUIRED COMPONENTS system)
target_link_libraries(network_lib PRIVATE Boost::system)
# 无需复杂的构建配置，编译时优化机会更多
```

**3. 与Asio的深度集成**

```cpp
// 无缝的事件循环集成
net::io_context ioc;
tcp::socket socket{ioc};
beast::websocket::stream<tcp::socket> ws{std::move(socket)};
```

### Boost.Asio异步I/O模型深度解析

Boost.Asio是整个网络层的基石，它实现了**Proactor设计模式**，这与传统的同步I/O或Reactor模式有本质区别：

#### Proactor vs Reactor模式对比

**传统Reactor模式（如epoll）**：

```cpp
// Reactor模式：通知你"可以读/写了"
while (true) {
    events = epoll_wait(epfd, events, MAX_EVENTS, -1);
    for (each event) {
        if (event.events & EPOLLIN) {
            // 你需要自己调用read()
            ssize_t bytes = read(event.fd, buffer, sizeof(buffer));
            // 处理数据...
        }
    }
}
```

**Asio的Proactor模式**：

```cpp
// Proactor模式：通知你"操作已完成"
socket.async_read_some(
    net::buffer(data_, max_length),
    [this](std::error_code ec, std::size_t length) {
        if (!ec) {
            // 数据已经在缓冲区中，直接处理
            process_data(data_, length);
            do_read();  // 继续下一次异步读取
        }
    }
);
```

## 第二阶段：WebSocket服务器架构实现

### 核心WebsocketServer类的设计

我们的WebSocket服务器实现体现了现代C++的设计原则：

```cpp
// src/network/websocket_server.hpp
#pragma once
#include <memory>
#include <vector>
#include <mutex>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/ip/tcp.hpp>
#include "player_registry.hpp"

namespace picoradar {
namespace network {

namespace beast = boost::beast;
namespace net = boost::asio;
using tcp = net::ip::tcp;

class WebsocketServer {
private:
    std::shared_ptr<net::io_context> ioc_;
    tcp::acceptor acceptor_;
    std::shared_ptr<core::PlayerRegistry> registry_;
    
public:
    WebsocketServer(std::shared_ptr<net::io_context> ioc,
                    std::shared_ptr<core::PlayerRegistry> registry);
    
    void run(const std::string& address, unsigned short port, int threads);
    void stop();
    
private:
    void do_accept();
};

} // namespace network
} // namespace picoradar
```

### 完整的服务器实现

```cpp
// src/network/websocket_server.cpp
#include "websocket_server.hpp"
#include "session.hpp"
#include <glog/logging.h>
#include <thread>

namespace picoradar {
namespace network {

WebsocketServer::WebsocketServer(std::shared_ptr<net::io_context> ioc,
                                 std::shared_ptr<core::PlayerRegistry> registry)
    : ioc_(ioc)
    , acceptor_(*ioc)
    , registry_(registry) {}

void WebsocketServer::run(const std::string& address, unsigned short port, int threads) {
    beast::error_code ec;
    
    // 解析地址
    auto const endpoint = tcp::endpoint{net::ip::make_address(address), port};
    
    // 配置acceptor
    acceptor_.open(endpoint.protocol(), ec);
    if (ec) {
        LOG(FATAL) << "Failed to open acceptor: " << ec.message();
        return;
    }
    
    acceptor_.set_option(net::socket_base::reuse_address(true), ec);
    if (ec) {
        LOG(ERROR) << "Failed to set reuse_address: " << ec.message();
    }
    
    acceptor_.bind(endpoint, ec);
    if (ec) {
        LOG(FATAL) << "Failed to bind to " << endpoint << ": " << ec.message();
        return;
    }
    
    acceptor_.listen(net::socket_base::max_listen_connections, ec);
    if (ec) {
        LOG(FATAL) << "Failed to listen: " << ec.message();
        return;
    }
    
    LOG(INFO) << "WebSocket server listening on " << endpoint;
    
    // 开始接受连接
    do_accept();
    
    // 启动工作线程池
    std::vector<std::thread> workers;
    workers.reserve(threads);
    
    for (int i = 0; i < threads; ++i) {
        workers.emplace_back([this] {
            try {
                ioc_->run();
            } catch (const std::exception& e) {
                LOG(ERROR) << "Worker thread exception: " << e.what();
            }
        });
    }
    
    LOG(INFO) << "Started " << threads << " worker threads";
    
    // 等待所有工作线程完成
    for (auto& worker : workers) {
        worker.join();
    }
    
    LOG(INFO) << "WebSocket server stopped";
}

void WebsocketServer::do_accept() {
    acceptor_.async_accept(
        net::make_strand(*ioc_),
        [this](beast::error_code ec, tcp::socket socket) {
            if (!ec) {
                LOG(INFO) << "New connection from " << socket.remote_endpoint();
                
                // 创建新的会话来处理这个连接
                std::make_shared<Session>(std::move(socket), registry_)->start();
            } else {
                LOG(ERROR) << "Accept error: " << ec.message();
            }
            
            // 继续接受下一个连接
            do_accept();
        }
    );
}

void WebsocketServer::stop() {
    LOG(INFO) << "Stopping WebSocket server...";
    ioc_->stop();
}

} // namespace network
} // namespace picoradar
```

## 第三阶段：服务器主程序架构设计

### main.cpp的完整实现

我们的服务器入口体现了现代软件架构的最佳实践：

```cpp
// src/server_app/main.cpp
#include <iostream>
#include <memory>
#include <thread>
#include <csignal>
#include <glog/logging.h>
#include <boost/asio.hpp>
#include "player_registry.hpp"
#include "websocket_server.hpp"

namespace net = boost::asio;

// 全局变量用于信号处理
std::shared_ptr<net::io_context> g_ioc;

void signal_handler(int signal) {
    LOG(INFO) << "Received signal " << signal << ", initiating graceful shutdown";
    if (g_ioc) {
        g_ioc->stop();
    }
}

int main(int argc, char* argv[]) {
    // 1. 初始化日志系统
    google::InitGoogleLogging(argv[0]);
    google::SetLogDestination(google::GLOG_INFO, "logs/server.INFO");
    google::SetLogDestination(google::GLOG_ERROR, "logs/server.ERROR");
    google::SetLogDestination(google::GLOG_FATAL, "logs/server.FATAL");
    
    // 确保日志目录存在
    system("mkdir -p logs");
    
    LOG(INFO) << "=== PICO Radar Server Starting ===";
    LOG(INFO) << "Version: 1.0.0";
    LOG(INFO) << "Build: " << __DATE__ << " " << __TIME__;
    
    try {
        // 2. 创建核心业务逻辑（依赖注入的根）
        auto registry = std::make_shared<picoradar::core::PlayerRegistry>();
        LOG(INFO) << "PlayerRegistry created";
        
        // 3. 创建I/O上下文
        g_ioc = std::make_shared<net::io_context>();
        
        // 4. 创建WebSocket服务器，注入依赖
        auto server = std::make_shared<picoradar::network::WebsocketServer>(g_ioc, registry);
        LOG(INFO) << "WebSocket server created";
        
        // 5. 设置信号处理器实现优雅停机
        std::signal(SIGINT, signal_handler);
        std::signal(SIGTERM, signal_handler);
        
        // 6. 配置服务器参数
        const std::string address = "0.0.0.0";
        const unsigned short port = 9002;
        const int num_threads = std::max(1, static_cast<int>(std::thread::hardware_concurrency()));
        
        LOG(INFO) << "Server configuration:";
        LOG(INFO) << "  Address: " << address;
        LOG(INFO) << "  Port: " << port;
        LOG(INFO) << "  Worker threads: " << num_threads;
        LOG(INFO) << "  Hardware concurrency: " << std::thread::hardware_concurrency();
        
        // 7. 启动服务器（阻塞调用）
        LOG(INFO) << "Starting server...";
        server->run(address, port, num_threads);
        
    } catch (const std::exception& e) {
        LOG(FATAL) << "Server startup failed: " << e.what();
        return 1;
    }
    
    LOG(INFO) << "=== PICO Radar Server Shutdown Complete ===";
    return 0;
}
```

## 技术成果与里程碑

### 架构演进的成果

经过这个阶段，我们实现了从概念到可运行服务器的完整转变：

**之前（Day 2）**：
- 纯粹的业务逻辑（PlayerRegistry）
- 完整的单元测试覆盖
- 无网络能力

**现在（Day 3）**：
- 完整的WebSocket服务器
- 异步I/O架构
- 多线程支持
- 结构化日志
- 优雅停机
- 生产级错误处理

### 性能特征

我们的服务器现在具备了：

- **高并发能力**: 基于Asio的异步I/O可以处理数千个并发连接
- **CPU效率**: 线程池设计充分利用多核资源
- **内存安全**: 智能指针和RAII保证无内存泄漏
- **可观测性**: 完整的日志记录便于监控和调试

## 下一步计划

网络基础架构的完成为整个系统打下了坚实基础。在下一篇日志中，我们将探讨：

- 认证机制的具体实现
- WebSocket消息协议的设计
- Session管理和生命周期
- 客户端通信协议的建立

---

**技术栈深度总结**：
- **网络库**: Boost.Beast + Boost.Asio (异步I/O)
- **并发模型**: Proactor pattern with thread pool
- **依赖管理**: vcpkg manifest mode
- **日志系统**: Google glog with structured logging
- **内存管理**: Smart pointers + RAII
- **构建系统**: Modern CMake with target-based design

这个阶段的开发标志着PICO Radar从纸面设计转向了真正可运行的网络应用。每一行代码都体现了现代C++的最佳实践，为后续的功能开发奠定了坚实的基础。

**下期预告**: 《代码质量的守护者——Linter与开发工具链建设》
