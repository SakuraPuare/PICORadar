#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>

namespace picoradar::constants {

//-----------------------------------------------------------------------------
// 服务配置 (Service Configuration)
//-----------------------------------------------------------------------------

/// @brief WebSocket 服务默认端口
constexpr uint16_t kDefaultServicePort = 11451;

/// @brief UDP 发现服务默认端口
constexpr uint16_t kDefaultDiscoveryPort = 11452;

/// @brief UDP 发现请求消息
const std::string kDiscoveryRequest = "PICO_RADAR_DISCOVERY_REQUEST";

/// @brief UDP 发现响应消息前缀
const std::string kDiscoveryResponsePrefix = "PICORADAR_SERVER_AT_";

//-----------------------------------------------------------------------------
// 网络参数 (Network Parameters)
//-----------------------------------------------------------------------------

/// @brief 最大消息大小 (1MB)
constexpr std::size_t kMaxMessageSize = 1024 * 1024;

/// @brief 接收缓冲区大小
constexpr std::size_t kReceiveBufferSize = 1024;

/// @brief 最大并发连接数
constexpr std::size_t kMaxConnections = 1000;

/// @brief 客户端位置插值周期 (100ms)
constexpr auto kInterpolationPeriod = std::chrono::milliseconds(100);

//-----------------------------------------------------------------------------
// 超时与重试 (Timeouts & Retries)
//-----------------------------------------------------------------------------

/// @brief 默认连接超时时间
constexpr auto kDefaultConnectionTimeout = std::chrono::milliseconds(1000);

/// @brief 默认握手超时时间
constexpr auto kDefaultHandshakeTimeout = std::chrono::milliseconds(1000);

/// @brief Ping 消息发送间隔
constexpr auto kDefaultPingInterval = std::chrono::milliseconds(30000);

/// @brief Pong 消息响应超时时间
constexpr auto kDefaultPongTimeout = std::chrono::milliseconds(1000);

/// @brief UDP 发现广播间隔
constexpr auto kDiscoveryBroadcastInterval = std::chrono::milliseconds(5000);

/// @brief UDP 发现超时时间
constexpr auto kDiscoveryTimeout = std::chrono::milliseconds(10000);

/// @brief 最大重试次数
constexpr int kMaxRetryAttempts = 3;

/// @brief 初始重试延迟
constexpr auto kInitialRetryDelay = std::chrono::milliseconds(1000);

/// @brief 最大重试延迟
constexpr auto kMaxRetryDelay = std::chrono::milliseconds(30000);

//-----------------------------------------------------------------------------
// 性能与资源 (Performance & Resources)
//-----------------------------------------------------------------------------

/// @brief 消息队列最大长度
constexpr std::size_t kMaxMessageQueueSize = 100;

/// @brief 最大玩家数量
constexpr std::size_t kMaxPlayerCount = 64;

/// @brief 默认线程池线程数
constexpr int kDefaultThreadCount = 4;

/// @brief 最大线程池线程数
constexpr int kMaxThreadCount = 16;

}  // namespace picoradar::constants