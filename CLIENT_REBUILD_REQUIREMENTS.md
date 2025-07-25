# PICO Radar 客户端重建需求分析 (V2 - 详细版)

本文档旨在明确定义 PICO Radar 客户端库 (`client_lib`) 重建的目标、功能需求、设计原则和质量标准。这是“凤凰计划”的基石，将指导后续所有的 API 设计、实现和测试工作。

## 1. 背景与动机

旧的客户端库在集成测试中反复出现难以诊断的并发问题（如 `std::future_error`、超时、段错误），其根本原因在于复杂的异步事件处理、状态管理和线程模型。零散的修复被证明是无效的。

因此，我们决定彻底重建客户端，目标是创建一个更简单、更健壮、更易于测试和维护的库。

## 2. 核心功能需求 (Functional Requirements)

新的客户端库必须提供以下核心功能：

-   **[F-1] 服务器发现**:
    -   能够向局域网广播 UDP 发现请求。
    -   能够接收并解析服务器的 UDP 响应，以获取服务器的 IP 地址和端口。
    -   该功能应该是可选的，用户也可以选择手动指定服务器地址。

-   **[F-2] 连接管理**:
    -   能够根据给定的 IP 地址和端口，与服务器建立 WebSocket 连接。
    -   能够处理连接失败（如服务器不可达、端口错误）的情况。
    -   提供明确的断开连接接口，并能优雅地关闭 WebSocket 连接。

-   **[F-3] 用户认证**:
    -   在 WebSocket 连接建立后，自动发送包含玩家 ID 和认证令牌的认证请求。
    -   能够处理认证成功和认证失败两种响应。

-   **[F-4] 数据发送**:
    -   在成功认证后，能够向服务器发送玩家的实时位置数据 (`PlayerData` protobuf 消息)。

-   **[F-5] 数据接收**:
    -   能够接收来自服务器的玩家列表广播 (`PlayerList` protobuf 消息)。
    -   提供机制（如回调函数或事件队列）让库的使用者可以访问到最新的玩家列表。

## 3. 非功能性需求 (Non-Functional Requirements)

除了核心功能外，客户端的实现还必须满足以下质量标准：

-   **[NF-1] API 简洁性**:
    -   提供一个高度封装、易于理解和使用的公共 API。
    -   隐藏所有内部的实现细节（如线程管理、Boost.Asio 的复杂性）。

-   **[NF-2] 完全异步**:
    -   所有网络操作（连接、读、写）必须是完全异步和非阻塞的。
    -   阻塞操作（如等待连接成功）应通过 `std::future` 或类似机制返回给调用者，而不是阻塞库的内部线程。

-   **[NF-3] 线程安全**:
    -   所有暴露给用户的公共方法都必须是线程安全的。
    -   库的内部实现需要有清晰的线程模型，避免数据竞争。

-   **[NF-4] 清晰的错误处理**:
    -   连接、认证等关键操作的成功或失败状态，必须通过返回的 `std::future` 来明确传达。
    -   失败时，`std::future` 应携带一个描述性异常。

-   **[NF-5] 明确的生命周期**:
    -   客户端对象必须具有清晰的生命周期。
    -   其构造函数应只进行最少的初始化，不应启动任何线程或网络活动。
    -   提供明确的 `connect()` 或 `start()` 方法来启动操作。
    -   其析构函数或 `disconnect()` 方法必须确保所有内部线程和网络活动都已完全停止和清理。

-   **[NF-6] 可测试性**:
    -   API 设计应便于编写单元测试和集成测试。
    -   关键依赖（如网络IO）应易于被模拟 (mock) 或隔离。

---

## 4. 接口设计草案 (Draft API)

```cpp
#include <future>
#include <functional>
#include <string>
#include <vector>

#include "proto/player_data.pb.h" // Protobuf-generated header

class Client {
public:
    // 定义接收到玩家列表时的回调函数类型
    using PlayerListCallback = std::function<void(const std::vector<picoradar::PlayerData>&)>;

    Client();
    ~Client();

    // 禁止拷贝和移动
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    void setOnPlayerListUpdate(PlayerListCallback callback);

    std::future<void> connect(const std::string& server_address,
                              const std::string& player_id,
                              const std::string& token);

    void disconnect();

    void sendPlayerData(const picoradar::PlayerData& data);
};
```

---

## 5. 详细 API 解读与使用示例

### 5.1 API 行为详解

-   **`Client()` (构造函数)**
    -   **行为**: 仅初始化客户端对象内部的状态变量。
    -   **保证**: **不**启动任何线程，**不**进行任何网络活动。构造函数是轻量且快速的。

-   **`~Client()` (析构函数)**
    -   **行为**: 自动调用 `disconnect()`。这是一个阻塞操作，它会等待所有内部资源被安全释放后才返回。
    -   **保证**: 确保客户端对象被销毁时，所有线程和网络连接都已完全关闭，无任何资源泄漏。

-   **`setOnPlayerListUpdate(PlayerListCallback callback)`**
    -   **行为**: 注册一个回调函数，当客户端从服务器接收到新的玩家列表时，该回调将被调用。
    -   **线程模型**: 为了简单和安全，此回调函数将在客户端的**内部网络线程**中被调用。
    -   **使用者注意**: 回调函数的实现**必须**是快速且非阻塞的，以避免阻塞整个客户端的网络处理。如果需要进行耗时操作，应将数据分发到其他工作线程处理。

-   **`connect(address, player_id, token)`**
    -   **行为**: 启动一个完全异步的连接和认证流程。此函数会**立即返回**一个 `std::future<void>`。
    -   **内部流程**:
        1.  启动内部 `io_context` 线程（如果尚未运行）。
        2.  解析服务器地址和端口。
        3.  发起异步 TCP 连接。
        4.  TCP 连接成功后，进行异步 WebSocket 握手。
        5.  握手成功后，发送认证请求。
        6.  等待认证响应：
            -   若成功，设置 `std::promise` 的值，使返回的 `future` 就绪。
            -   若失败，设置 `std::promise` 的异常，使 `future` 带有异常。
    -   **错误处理**: 在上述任何一步失败（如地址解析失败、连接被拒、认证令牌无效），返回的 `future` 都会包含一个描述性的异常 (`std::runtime_error` 或其子类)。
    -   **可重入性**: 在一次成功的 `connect` 后，必须先调用 `disconnect` 才能再次调用 `connect`。

-   **`disconnect()`**
    -   **行为**: 启动一个同步的关闭流程。此函数会阻塞，直到所有资源都被释放。
    -   **内部流程**:
        1.  向 `io_context` 提交一个关闭 WebSocket 的任务。
        2.  向 `io_context` 发出停止信号。
        3.  `join()` 内部网络线程，等待其完全退出。
    -   **保证**: 此函数返回后，客户端处于一个干净的、可被安全销毁或可重新 `connect` 的状态。

-   **`sendPlayerData(data)`**
    -   **行为**: 一个“即发即忘” (fire-and-forget) 的方法。它会将 `PlayerData` 序列化并通过网络发送出去。
    -   **状态依赖**: 只有在客户端处于“已连接并认证”的状态时，数据才会被实际发送。在其他任何状态下（如正在连接、已断开），调用此方法将被静默忽略。
    -   **线程安全**: 此方法是线程安全的。它会将数据提交到内部 `io_context` 线程进行实际的发送操作。

### 5.2 典型使用流程示例

```cpp
#include "client.hpp"
#include <iostream>
#include <thread>
#include <chrono>

void on_players_updated(const std::vector<picoradar::PlayerData>& players) {
    // 重要：此回调在网络线程中运行，保持简短！
    std::cout << "[CALLBACK] Player list updated. Total players: " << players.size() << std::endl;
    for (const auto& p : players) {
        // std::cout << "  - Player: " << p.player_id() << std::endl;
    }
}

int main() {
    try {
        Client client;

        // 1. 注册回调
        client.setOnPlayerListUpdate(on_players_updated);

        // 2. 异步连接
        std::cout << "Connecting to server..." << std::endl;
        auto connect_future = client.connect("127.0.0.1:11451", "my_player_id", "pico_radar_secret_token");

        // ... 在主线程中可以做其他事情 ...

        // 3. 等待连接结果
        connect_future.get(); // .get() 会阻塞直到 future 就绪，如果连接失败会在此处抛出异常
        std::cout << "Connection and authentication successful!" << std::endl;

        // 4. 定期发送数据
        for (int i = 0; i < 5; ++i) {
            picoradar::PlayerData my_data;
            my_data.set_player_id("my_player_id");
            my_data.mutable_position()->set_x(1.0f * i);
            my_data.mutable_position()->set_y(2.0f);
            my_data.mutable_position()->set_z(3.0f);

            client.sendPlayerData(my_data);
            std::cout << "Sent my position data." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        // 5. 完成后断开连接
        std::cout << "Disconnecting..." << std::endl;
        client.disconnect();
        std::cout << "Disconnected." << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "An error occurred: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
```

## 6. 内部架构草图

为了实现上述 API 和行为，客户端内部将大致包含以下组件：

-   `Client` **(公共外观类)**:
    -   作为用户交互的唯一入口。
    -   管理 `io_context` 和网络线程的生命周期。
    -   持有一个指向内部实现（Pimpl）的指针，或直接包含状态。
    -   将公共方法的调用 `post` 到 `io_context` 线程以确保线程安全。

-   `io_context` **(核心事件循环)**:
    -   所有异步操作的核心。
    -   运行在一个单独的、由 `Client` 管理的后台线程 (`network_thread_`) 上。

-   `websocket::stream` **(会话对象)**:
    -   代表与服务器的 WebSocket 连接。
    -   其所有的异步方法 (e.g., `async_connect`, `async_read`, `async_write`) 都在 `io_context` 上调度。

-   **状态管理**:
    -   使用一个 `std::atomic<State>` 变量来追踪客户端的当前状态（e.g., `Disconnected`, `Connecting`, `Connected`, `Disconnecting`）。
    -   这对于决定 `sendPlayerData` 等方法是否应该执行其操作至关重要。

-   **回调和 Promise**:
    -   `connect` 方法创建的 `std::promise<void>` 会被传递到异步操作链中，在最终认证步骤完成时被 fulfill。
    -   用户提供的 `PlayerListCallback` 被存储起来，在 `async_read` 收到玩家列表消息时被调用。

这种设计将所有复杂的异步逻辑和线程管理都安全地封装在 `Client` 内部，为库的使用者提供了一个极其简洁和健壮的接口。 