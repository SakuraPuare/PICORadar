#pragma once

/**
 * @file compile_time_constants.hpp
 * @brief 编译时常量定义
 * 
 * 这个文件包含项目中使用的编译时常量，
 * 用于集中管理配置参数和避免硬编码数值。
 */

#include <chrono>
#include <cstddef>

namespace picoradar::constants {

// 网络相关常量
constexpr std::size_t kMaxMessageSize = 4096;
constexpr std::size_t kReceiveBufferSize = 1024;
constexpr std::size_t kMaxConnections = 1000;

// 超时相关常量 (毫秒)
constexpr auto kDefaultConnectionTimeout = std::chrono::milliseconds(5000);
constexpr auto kDefaultHandshakeTimeout = std::chrono::milliseconds(3000);
constexpr auto kDefaultPingInterval = std::chrono::milliseconds(30000);
constexpr auto kDefaultPongTimeout = std::chrono::milliseconds(5000);

// 重试相关常量
constexpr int kMaxRetryAttempts = 3;
constexpr auto kInitialRetryDelay = std::chrono::milliseconds(1000);
constexpr auto kMaxRetryDelay = std::chrono::milliseconds(30000);

// 缓冲区和队列大小
constexpr std::size_t kMaxMessageQueueSize = 100;
constexpr std::size_t kMaxPlayerCount = 64;

// 线程池配置
constexpr int kDefaultThreadCount = 4;
constexpr int kMaxThreadCount = 16;

// 发现服务配置
constexpr auto kDiscoveryBroadcastInterval = std::chrono::milliseconds(5000);
constexpr auto kDiscoveryTimeout = std::chrono::milliseconds(10000);

} // namespace picoradar::constants
