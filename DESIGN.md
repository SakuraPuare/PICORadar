# PICO Radar: 技术设计文档

本文档深入探讨PICO Radar系统的技术架构、核心组件设计和关键实现决策，旨在为开发者提供一份清晰、全面的技术参考。

## 1. 系统架构概览

本系统采用经典的 **客户端/服务器 (Client/Server)** 模型，所有组件均基于 **C++17** 标准开发，并通过 **CMake** 进行构建。

-   **服务端 (`server_app`)**: 一个独立的C++可执行程序。作为权威中心，它负责管理所有客户端连接、处理玩家状态更新，并将完整的游戏世界状态广播给所有已连接的客户端。
-   **核心逻辑 (`core_logic`)**: 一个静态库，封装了与具体网络实现无关的核心业务逻辑，主要是对玩家列表（`PlayerRegistry`）的管理。
-   **网络层 (`network_lib`)**: 一个静态库，基于 **Boost.Beast** 实现，负责处理底层的WebSocket通信。
-   **协议定义 (`proto_gen`)**: 一个由 **Protocol Buffers** 定义的静态库，包含了所有网络消息的数据结构。

依赖关系清晰地组织为：`server_app` -> `network_lib` -> `core_logic` -> `proto_gen`。

## 2. 构建系统与依赖管理

### 2.1. 依赖管理器

本项目 **唯一指定 `vcpkg`** 作为第三方依赖管理器。所有必需的库（`Boost.Beast`, `glog`, `protobuf`, `gtest`）都在根目录的 `vcpkg.json` 文件中定义，确保了开发环境的一致性和可复现性。

### 2.2. Protobuf 集成 (CMake)

为了实现健壮、可维护的 `.proto` 文件编译，我们采用 **现代CMake** 推荐的 **`protobuf_generate()`** 函数。这种方法将Protobuf代码的生成过程抽象为一个标准的CMake目标（Target），极大地简化了依赖管理。

**核心模式如下 (`CMakeLists.txt`):**

```cmake
add_library(proto_gen STATIC "proto/player_data.proto")
protobuf_generate(TARGET proto_gen IMPORT_DIRS "...")
target_link_libraries(proto_gen PUBLIC protobuf::libprotobuf)
```

通过将下游目标（如 `core_logic`）链接到 `proto_gen`，它们会自动继承所需的包含路径和库依赖。

## 3. 网络层设计 (Boost.Beast)

### 3.1. 核心模型

网络层采用异步、非阻塞的设计，其核心是两个类：

-   **`Listener`**: 负责在指定的IP地址和端口上监听传入的TCP连接。一旦接受一个新连接，它会立即创建一个对应的`Session`对象来处理该连接，然后继续监听下一个连接。
-   **`Session`**: 代表一个独立的客户端WebSocket连接。每个`Session`对象拥有自己的`websocket::stream`和读写缓冲区。所有的异步操作都在该对象内部完成，其生命周期由`std::shared_from_this`管理。

### 3.2. 优雅关闭协议 (WebSocket)

我们严格遵循WebSocket的“优雅关闭”握手协议，以确保连接能够干净地双向关闭，避免资源泄漏或客户端死锁。

-   **客户端发起关闭**: 当客户端（如测试中的`mock_client`）完成其任务后，它会调用`ws.close()`发起关闭。随后，它必须进入一个读取循环，直到收到服务器的关闭回执帧（表现为`websocket::error::closed`错误），才算完成握手。
-   **服务器响应关闭**: 当服务器的`on_read`回调收到`websocket::error::closed`错误时，它会将其识别为客户端发起的正常关闭请求。作为响应，服务器会立即调用`ws.async_close()`来回送一个确认关闭帧，从而完成服务器端的握手。

## 4. 鉴权协议

为了确保服务器的安全，所有客户端在建立连接后必须立即进行身份验证。

1.  **发送鉴权请求**: 客户端发送的第一个消息必须是包含**预共享令牌**和**玩家ID**的`AuthRequest`。
2.  **服务器验证**: 服务器验证令牌的有效性和玩家ID的非空性。
3.  **响应与状态变更**: 服务器回送一个`AuthResponse`。如果成功，服务器内部会将该`Session`标记为“已认证”，并存储其`player_id`。如果失败，服务器在发送完失败响应后会立即关闭连接。
4.  **会话生命周期与身份绑定**: `player_id`与`Session`的生命周期绑定。当`Session`对象因任何原因（客户端断开、网络错误等）被销毁时，其析构函数会负责调用`PlayerRegistry::removePlayer`，确保玩家数据被自动清理，防止资源泄漏。

## 5. 并发模型与线程安全

系统设计了清晰的多线程模型，以充分利用多核CPU，并通过精确的同步机制保证线程安全。

### 5.1. 线程划分

-   **主线程**: 负责初始化，启动服务器。
-   **IO线程池**: 由`WebsocketServer`创建的一组线程（数量可配），它们共享同一个`net::io_context`。所有网络IO事件（连接、读、写）都在这个线程池上被并发地调度和执行。
-   **广播线程**: 一个由`WebsocketServer`创建的专用后台线程，负责以固定频率（20Hz）执行数据广播任务。

### 5.2. 线程安全机制

我们采用两种不同的策略来处理并发访问：

-   **粗粒度锁定 (Mutex)**: 对于需要被多个线程（IO线程、广播线程）访问的共享资源，我们使用`std::mutex`进行保护。
    -   **`PlayerRegistry`**: 内部有一个`std::mutex`，保护其`players_`哈希表。任何来自IO线程的`updatePlayer`/`removePlayer`调用，或来自广播线程的`getAllPlayers`调用，都会被互斥地执行。
    -   **`WebsocketServer`的会话列表**: 内部有一个`sessions_mutex_`，保护其`sessions_`集合。来自IO线程的`register_session`/`unregister_session`调用，或来自广播线程的遍历，都会被互斥地执行。

-   **细粒度串行化 (Strand)**: 对于`Session`对象本身，我们采用了更高效的`net::strand`机制，而不是在每个方法里都使用锁。
    -   **核心原则**: 我们严格遵循Beast异步模型的铁律——对于单个`Session`（即单个socket），所有异步操作必须串行化。
    -   **实现**: 每个`Session`在构造时都会创建一个`net::strand`。所有需要与该`Session`交互的事件，无论是来自IO线程的读/写回调，还是来自广播线程的`send`调用，都会被`net::post`到这个`strand`上。这确保了对于同一个`Session`，所有操作都会在一个逻辑队列中被依次执行，从根本上杜绝了并发访问其内部成员（如`write_queue_`）的风险。

## 6. 健壮性设计

### 6.1. 单例服务与陈旧锁处理

为了防止服务器多实例运行导致的端口冲突，我们实现了一个跨平台的`SingleInstanceGuard`。

-   **问题**: 当服务器异常崩溃时，可能会在临时目录中留下一个PID锁文件。这个“陈旧的”锁文件将阻止服务器下一次正常启动。
-   **自我修复机制**: 当一个新实例启动并发现锁文件已存在时，它不会立即失败。相反，它会：
    1.  从锁文件中读取旧的进程ID (PID)。
    2.  调用一个跨平台的辅助函数`is_process_running(pid)`来检查该进程是否仍在运行。
    3.  如果进程**已不存在**，新实例会判断这是一个陈旧锁，自动删除它，然后重新尝试获取锁并正常启动。
    4.  只有当旧进程**确实在运行**时，新实例才会报错并退出。
-   **跨平台实现**: `is_process_running`函数通过`#ifdef`实现了平台兼容：在POSIX系统上使用`kill(pid, 0)`技巧，在Windows上使用`OpenProcess`。

### 6.2. 自动化集成测试的可靠性

为了保证CI/CD流程的绝对可靠，我们为集成测试制定了核心原则：

-   **端口隔离**: 每个需要启动服务器的测试用例（`.sh`脚本）都必须使用一个**唯一的、硬编码的**网络端口。这彻底解决了因`TIME_WAIT`状态导致的端口占用问题，使得所有集成测试都可以安全地并行运行。