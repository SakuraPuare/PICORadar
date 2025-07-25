# 开发日志 #3：网络基石——异步I/O与服务器架构设计

**作者：书樱**
**日期：2025年7月21日**

> **摘要**: 本文记录了PICO Radar项目从纯粹的本地核心逻辑，迈向一个功能性网络服务器的关键一步。我们将深入探讨为何从`websocketpp`迁移至`Boost.Beast`，并重点剖析其底层Boost.Asio库所采用的异步I/O模型（Proactor模式）。同时，我们将解构服务器主程序的架构，阐述依赖注入、I/O线程池、以及优雅停机等专业服务器设计的核心原则。

---

大家好，我是书樱。

在上一篇日志中，我们通过TDD为`PlayerRegistry`核心模块建立了坚实的、可验证的基础。它就像一台经过精密调校的引擎。现在，是时候为这台引擎构建传动系统和底盘，让它真正连接到外部世界。今天，我们将探讨如何构建一个功能完备的WebSocket服务器，将我们的核心逻辑暴露于网络之中。

### 技术栈演进：为何拥抱Boost.Beast与Asio？

我们最初的技术选型中包含了`websocketpp`。然而，在深入评估后，我们决定进行一次重要的技术升级，全面转向**Boost.Beast**。这一决策的核心驱动力在于其底层的**Boost.Asio**库。

Asio是C++异步编程的基石，也是事实上的标准。它完美地实现了**Proactor设计模式**：
-   **Proactor模式**: 与更传统的Reactor模式（通知你“I/O已就绪，请你来读/写”）不同，Proactor模式（通知你“I/O操作已完成，这是结果”）将I/O操作的发起和完成彻底解耦。开发者只需发起一个异步操作（如`async_read`）并提供一个完成处理器（Completion Handler，通常是一个lambda或函数对象）。
-   **`io_context`**: Asio的核心是一个`io_context`对象，它代表了操作系统的I/O事件队列。我们的应用程序只需启动一个或多个线程来运行`io_context::run()`，Asio就会在后台高效地处理所有I/O事件，并在操作完成时，在这些线程上调用我们提供的处理器。

这种模型让我们能够用少量线程处理海量的并发连接，极大地提升了服务器的伸缩性（Scalability），并与我们现代C++的设计哲学无缝对接。

### 服务器架构剖析：`main.cpp`的职责

我们的`server/main.cpp`已经从一个简单的测试脚本，演变为一个结构清晰的服务器入口。其设计遵循了几个关键的软件工程原则：

```cpp
// src/server/main.cpp (概念性展示)
int main(int argc, char* argv[]) {
    // 1. 初始化与配置 (日志, 命令行参数等)
    google::InitGoogleLogging(argv[0]);
    
    // 2. 依赖注入：创建核心业务逻辑模块
    auto registry = std::make_shared<picoradar::core::PlayerRegistry>();

    // 3. 创建I/O服务层，并将业务逻辑注入
    auto ioc = std::make_shared<net::io_context>();
    auto server = std::make_shared<picoradar::network::WebsocketServer>(ioc, registry);
    
    // 4. 启动I/O线程池
    const int num_threads = 4;
    server->run("0.0.0.0", 9002, num_threads);

    // 5. 设置信号处理器，实现优雅停机
    net::signal_set signals(*ioc, SIGINT, SIGTERM);
    signals.async_wait([&](auto, auto){
        server->stop();
    });

    // ... 运行并最终等待服务器关闭 ...
    return 0;
}
```

1.  **依赖注入 (Dependency Injection)**: `main`函数作为“装配车间”，首先创建了`PlayerRegistry`的实例。然后，它将这个实例作为依赖，通过构造函数**注入**给了`WebsocketServer`。这种方式彻底解耦了网络层与业务逻辑层，它们可以被独立开发、测试和替换。
2.  **I/O线程池**: `server->run()`会创建一个包含4个线程的线程池。所有这些线程都会调用`io_context::run()`，共同处理网络I/O事件。这使得我们的服务器能够充分利用现代多核CPU的并行处理能力，显著提高网络吞吐量。
3.  **优雅停机 (Graceful Shutdown)**: 这是一个专业服务器应用的标志。我们注册了一个信号处理器来监听`SIGINT` (Ctrl+C) 和 `SIGTERM`。当接收到这些信号时，我们不会粗暴地终止进程，而是会调用`server->stop()`。这个函数会停止接受新的连接，并向`io_context`发送一个停止信号，让事件循环自然退出。这确保了所有正在进行的任务都能被妥善处理，资源得到正确释放。
4.  **可观测性 (Observability)**: 我们用Google的`glog`库全面取代了`std::cout`。结构化、带时间戳和严重性级别的日志，是构建系统可观测性的第一步。没有它，在生产环境中进行故障排查和性能分析将是天方夜谭。

### 里程碑达成

随着这次提交，PICO Radar不再仅仅是存在于设计文档和单元测试中的概念。它已经是一个可以被编译、运行，并真正在网络上监听连接的实体应用。我们成功地将经过验证的核心逻辑引擎，与一个高性能、可扩展的异步网络底盘结合在了一起。

当然，服务器的内部逻辑还很空洞。它能接受连接，但还无法理解客户端的意图。接下来的任务将是填充这些细节：
-   实现基于预共享令牌的认证流程。
-   构建向已认证客户端进行数据广播的循环。
-   开发一个`mock_client`来作为我们服务器的第一个“对话者”。

每一步都将使PICO Radar变得更加完整。感谢关注，我们下次更新再见！

---
书樱
2025年7月21日
