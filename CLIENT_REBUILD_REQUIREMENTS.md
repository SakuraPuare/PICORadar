# PICO Radar 客户端重建需求分析

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
    -   其析构函数或 `stop()` 方法必须确保所有内部线程和网络活动都已完全停止和清理。

-   **[NF-6] 可测试性**:
    -   API 设计应便于编写单元测试和集成测试。
    -   关键依赖（如网络IO）应易于被模拟 (mock) 或隔离。

## 4. 接口设计草案 (Draft API)

基于以上需求，提出以下 `PicoRadarClient` 类的接口草案作为讨论起点：

```cpp
#include <future>
#include <functional>
#include <string>
#include <vector>

#include "player_data.pb.h" // Protobuf-generated header

class PicoRadarClient {
public:
    // 定义接收到玩家列表时的回调函数类型
    using PlayerListCallback = std::function<void(const std::vector<picoradar::PlayerData>&)>;

    PicoRadarClient();
    ~PicoRadarClient();

    // 禁止拷贝和移动
    PicoRadarClient(const PicoRadarClient&) = delete;
    PicoRadarClient& operator=(const PicoRadarClient&) = delete;

    /**
     * @brief 设置当接收到服务器的玩家列表更新时要调用的回调函数。
     */
    void setOnPlayerListUpdate(PlayerListCallback callback);

    /**
     * @brief 异步连接到服务器并进行认证。
     *
     * @param server_address 服务器的 IP 地址 (例如 "192.168.1.100:11451")。
     * @param player_id 本客户端的玩家ID。
     * @param token 用于认证的令牌。
     * @return 一个 std::future<void>。
     *         如果连接和认证成功，future 将就绪。
     *         如果失败，future 将包含一个异常。
     */
    std::future<void> connect(const std::string& server_address,
                              const std::string& player_id,
                              const std::string& token);

    /**
     * @brief 异步断开与服务器的连接。
     */
    void disconnect();

    /**
     * @brief 发送玩家的最新数据到服务器。
     *
     * 这是一个“即发即忘”的操作。
     * 只有在成功连接和认证后，此方法才会实际发送数据。
     * @param data 要发送的玩家数据。
     */
    void sendPlayerData(const picoradar::PlayerData& data);
};
``` 