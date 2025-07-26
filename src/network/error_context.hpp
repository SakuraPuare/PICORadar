#pragma once

/**
 * @file error_context.hpp
 * @brief 增强的错误处理和上下文信息
 * 
 * 提供更丰富的错误信息和调试上下文，
 * 帮助快速定位和解决问题。
 */

#include <string>
#include <chrono>
#include <boost/beast/core/error.hpp>
#include <boost/beast/websocket/error.hpp>
#include <boost/asio/error.hpp>
#include "common/logging.hpp"

namespace picoradar::network {

/**
 * @brief 网络操作的上下文信息
 */
struct NetworkContext {
    std::string operation;      // 操作类型 (e.g., "accept", "read", "write")
    std::string endpoint;       // 客户端端点信息
    std::string player_id;      // 玩家ID (如果已认证)
    std::chrono::steady_clock::time_point start_time;  // 操作开始时间
    size_t bytes_transferred = 0;  // 传输的字节数
    
    NetworkContext(const std::string& op, const std::string& ep) 
        : operation(op), endpoint(ep), start_time(std::chrono::steady_clock::now()) {}
};

/**
 * @brief 增强的错误记录器
 */
class ErrorLogger {
public:
    /**
     * @brief 记录网络错误
     */
    static void logNetworkError(const NetworkContext& ctx, 
                               const boost::beast::error_code& ec,
                               const std::string& additional_info = "") {
        auto duration = std::chrono::steady_clock::now() - ctx.start_time;
        auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
        
        LOG_ERROR << "Network error in " << ctx.operation 
                  << " operation - Endpoint: " << ctx.endpoint
                  << ", Player: " << (ctx.player_id.empty() ? "unauthenticated" : ctx.player_id)
                  << ", Duration: " << duration_ms.count() << "ms"
                  << ", Bytes: " << ctx.bytes_transferred
                  << ", Error: " << ec.message()
                  << (additional_info.empty() ? "" : ", Info: " + additional_info);
    }
    
    /**
     * @brief 记录性能警告
     */
    static void logPerformanceWarning(const NetworkContext& ctx,
                                    const std::string& metric,
                                    const std::string& threshold_info) {
        auto duration = std::chrono::steady_clock::now() - ctx.start_time;
        auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
        
        LOG_WARNING << "Performance warning in " << ctx.operation
                    << " - " << metric
                    << ", Duration: " << duration_ms.count() << "ms"
                    << ", Threshold: " << threshold_info
                    << ", Endpoint: " << ctx.endpoint;
    }
    
    /**
     * @brief 记录成功的操作统计
     */
    static void logOperationSuccess(const NetworkContext& ctx) {
        auto duration = std::chrono::steady_clock::now() - ctx.start_time;
        auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
        
        if (duration_ms.count() > 100) {  // 记录超过100ms的操作
            LOG_INFO << "Long operation completed: " << ctx.operation
                     << ", Duration: " << duration_ms.count() << "ms"
                     << ", Bytes: " << ctx.bytes_transferred
                     << ", Player: " << ctx.player_id;
        }
    }
};

/**
 * @brief 错误类别辅助函数
 */
class ErrorHelper {
public:
    /**
     * @brief 判断错误是否可重试
     */
    static bool isRetryableError(const boost::beast::error_code& ec) {
        return ec == boost::beast::error::timeout ||
               ec == boost::asio::error::connection_reset ||
               ec == boost::asio::error::connection_aborted ||
               ec == boost::asio::error::eof;
    }
    
    /**
     * @brief 判断错误是否是客户端断开连接
     */
    static bool isClientDisconnect(const boost::beast::error_code& ec) {
        return ec == boost::beast::websocket::error::closed ||
               ec == boost::asio::error::connection_reset ||
               ec == boost::asio::error::eof;
    }
    
    /**
     * @brief 获取错误的严重程度
     */
    static std::string getErrorSeverity(const boost::beast::error_code& ec) {
        if (isClientDisconnect(ec)) {
            return "info";  // 客户端断开是正常的
        } else if (isRetryableError(ec)) {
            return "warning";  // 可重试的错误
        } else {
            return "error";  // 严重错误
        }
    }
};

} // namespace picoradar::network
