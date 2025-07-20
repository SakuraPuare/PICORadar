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

为了实现健壮、可维护的 `.proto` 文件编译，我们采用 **现代CMake** 推荐的 **`protobuf_generate()`** 函数，而不是过时的 `add_custom_command`。这种方法将Protobuf代码的生成过程抽象为一个标准的CMake目标（Target），极大地简化了依赖管理。

**核心模式如下 (`CMakeLists.txt`):**

```cmake
# 1. 创建一个库目标，并将 .proto 文件作为其“源”文件
add_library(proto_gen STATIC
    "proto/player_data.proto"
)

# 2. 调用 protobuf_generate 将生成的 .pb.cc 和 .pb.h 添加到该目标
protobuf_generate(
    TARGET proto_gen
    # 指定 .proto 文件的根目录，以便 import 正常工作
    IMPORT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/proto"
)

# 3. 将 protobuf 运行时库链接到我们的生成库
#    PUBLIC 关键字确保了该依赖项会传递给链接到 proto_gen 的任何目标
target_link_libraries(proto_gen PUBLIC protobuf::libprotobuf)
```

通过将下游目标（如 `core_logic`）链接到 `proto_gen`，它们会自动继承所需的包含路径和库依赖，从而避免了在多个`CMakeLists.txt`中手动管理路径的麻烦。

## 3. 网络层设计 (Boost.Beast)

### 3.1. 核心模型

网络层采用异步、非阻塞的设计，其核心是两个类：

-   **`Listener`**: 负责在指定的IP地址和端口上监听传入的TCP连接。一旦接受一个新连接，它会立即创建一个对应的`Session`对象来处理该连接，然后继续监听下一个连接，自身不参与任何I/O通信。
-   **`Session`**: 代表一个独立的客户端WebSocket连接。每个`Session`对象拥有自己的`websocket::stream`和读写缓冲区。所有的异步操作（握手、读、写）都在该对象内部完成，生命周期由`std::shared_from_this`管理。

### 3.2. 线程模型

服务器采用一个固定大小的线程池来处理所有异步I/O事件。

1.  `main`函数创建一个`net::io_context`实例。
2.  `Listener`和所有的`Session`对象都在这个唯一的`io_context`上调度它们的异步操作。
3.  `main`函数创建多个工作线程，每个线程都调用`ioc_.run()`。

这使得服务器能够高效地利用多核CPU处理大量并发连接，同时通过`net::strand`确保每个`Session`内部的操作是线程安全的，避免了显式加锁。

## 4. 鉴权协议

为了确保服务器的安全，所有客户端在建立连接后必须立即进行身份验证。

1.  **连接建立**: 客户端成功完成WebSocket握手。
2.  **发送鉴权请求**: 客户端发送的 **第一个** 二进制消息必须是一个序列化后的`ClientToServer`消息，且其`oneof`字段必须是`auth_request`。
3.  **服务器验证**:
    -   服务器解析收到的消息。如果不是合法的`AuthRequest`，或者`token`字段与服务器持有的预共享令牌不匹配，服务器将立即关闭该WebSocket连接，关闭代码为`policy_error`。
    -   如果令牌匹配，服务器会在对应的`Session`对象中将会话标记为“已认证”。
4.  **后续通信**:
    -   只有“已认证”的会话才能发送`PlayerData`消息并被服务器处理。
    -   未认证的会话如果发送除`AuthRequest`之外的任何消息，都将被视为违反协议并被断开连接。
    -   服务器不会向未认证的客户端广播任何数据。
