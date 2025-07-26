#pragma once

/**
 * @file types.hpp
 * @brief 通用类型定义和前向声明
 * 
 * 这个文件包含项目中使用的通用类型定义和前向声明，
 * 用于减少头文件间的依赖关系和编译时间。
 */

#include <cstdint>
#include <string>
#include <memory>
#include <functional>

// 网络相关类型
namespace picoradar {
    using Port = std::uint16_t;
    using ThreadCount = int;
    using PlayerId = std::string;
    using AuthToken = std::string;
    using ServerAddress = std::string;
}

// Protobuf 前向声明
namespace picoradar {
    class PlayerData;
}

// 回调函数类型定义
namespace picoradar::client {
    using PlayerListCallback = std::function<void(const std::vector<PlayerData>&)>;
    using ConnectionCallback = std::function<void(bool success, const std::string& message)>;
    using ErrorCallback = std::function<void(const std::string& error)>;
}

// 结果类型
namespace picoradar {
    enum class ConnectionState {
        Disconnected,
        Connecting, 
        Connected,
        Reconnecting,
        Failed
    };
    
    enum class OperationResult {
        Success,
        Failed,
        Timeout,
        InvalidInput,
        NetworkError
    };
}
