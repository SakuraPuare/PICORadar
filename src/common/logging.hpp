#pragma once

// Prevent Windows.h from defining min/max macros and other problematic macros
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
// Undefine problematic macros that might conflict with our enum
#ifdef ERROR
#undef ERROR
#endif
#ifdef FATAL
#undef FATAL
#endif
#ifdef WARNING
#undef WARNING
#endif
#ifdef INFO
#undef INFO
#endif
#ifdef DEBUG
#undef DEBUG
#endif
#ifdef TRACE
#undef TRACE
#endif
#endif

#include <glog/logging.h>
#include <glog/raw_logging.h>
#include <string>
#include <memory>
#include <mutex>
#include <sstream>
#include <iomanip>

namespace logger {

enum class LogLevel {
    TRACE = 0,
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    FATAL,
    NUM_SEVERITIES
};

class Logger {
public:
    // 初始化日志系统
    static void Init(const std::string& program_name,
                    const std::string& log_dir = "./logs",
                    LogLevel min_log_level = LogLevel::INFO,
                    uint32_t max_log_size = 10, // MB
                    bool log_to_stderr = false);
    
    // 获取单例
    static auto GetInstance() -> Logger&;

    // 设置日志级别
    void SetLogLevel(LogLevel level);
    
    // 流式日志输出
    class LogStream {
    public:
        LogStream(const char* file, int line, LogLevel level);
            
        ~LogStream();
        
        template<typename T>
        auto operator<<(const T& val) -> LogStream& {
            stream_ << val;
            return *this;
        }
        
    private:
        std::ostringstream stream_;
        const char* file_;
        int line_;
        LogLevel level_;
    };
    
    // 条件日志
    class LogStreamIf {
    public:
        LogStreamIf(const char* file, int line, LogLevel level, bool condition);
            
        ~LogStreamIf();
        
        template<typename T>
        auto operator<<(const T& val) -> LogStreamIf& {
            if (condition_) {
                stream_ << val;
            }
            return *this;
        }
        
    private:
        std::ostringstream stream_;
        const char* file_;
        int line_;
        LogLevel level_;
        bool condition_;
    };
    
    // 写入致命错误日志
    void WriteFatalLog(const char* data, int size);
    
    // 手动刷新日志
    void Flush();

    // 获取当前日志级别
    static auto GetCurrentLogLevel() -> LogLevel;
    
private:
    Logger();
    ~Logger();
    
    std::mutex mutex_;
    static LogLevel current_log_level_;
};

// 方便使用的宏定义
#define LOG_TRACE logger::Logger::LogStream(__FILE__, __LINE__, logger::LogLevel::TRACE)
#define LOG_DEBUG logger::Logger::LogStream(__FILE__, __LINE__, logger::LogLevel::DEBUG)
#define LOG_INFO logger::Logger::LogStream(__FILE__, __LINE__, logger::LogLevel::INFO)
#define LOG_WARNING logger::Logger::LogStream(__FILE__, __LINE__, logger::LogLevel::WARNING)
#define LOG_ERROR logger::Logger::LogStream(__FILE__, __LINE__, logger::LogLevel::ERROR)
#define LOG_FATAL logger::Logger::LogStream(__FILE__, __LINE__, logger::LogLevel::FATAL)

#define LOG_IF_TRACE(condition) logger::Logger::LogStreamIf(__FILE__, __LINE__, logger::LogLevel::TRACE, condition)
#define LOG_IF_DEBUG(condition) logger::Logger::LogStreamIf(__FILE__, __LINE__, logger::LogLevel::DEBUG, condition)
#define LOG_IF_INFO(condition) logger::Logger::LogStreamIf(__FILE__, __LINE__, logger::LogLevel::INFO, condition)
#define LOG_IF_WARNING(condition) logger::Logger::LogStreamIf(__FILE__, __LINE__, logger::LogLevel::WARNING, condition)
#define LOG_IF_ERROR(condition) logger::Logger::LogStreamIf(__FILE__, __LINE__, logger::LogLevel::ERROR, condition)
#define LOG_IF_FATAL(condition) logger::Logger::LogStreamIf(__FILE__, __LINE__, logger::LogLevel::FATAL, condition)

} // namespace logger
