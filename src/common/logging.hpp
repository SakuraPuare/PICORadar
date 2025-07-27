#pragma once

#include <atomic>
#include <chrono>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "platform_fixes.hpp"

// Ensure Windows macros are undefined after all includes
UNDEFINE_WINDOWS_MACROS()

namespace logger {

// 前向声明 - 抽象CLI接口
class CLIOutput {
 public:
  virtual ~CLIOutput() = default;
  virtual void addLogEntry(const std::string& level,
                           const std::string& message) = 0;
};

enum class LogLevel : std::uint8_t {
  TRACE = 0,
  DEBUG,
  INFO,
  WARNING,
  ERROR,
  FATAL,
  NUM_SEVERITIES
};

/**
 * @brief 日志条目结构
 */
struct LogEntry {
  std::chrono::system_clock::time_point timestamp;
  LogLevel level;
  std::string file;
  int line;
  std::string function;
  std::thread::id thread_id;
  std::string module;
  std::string message;
  std::map<std::string, std::string> extra_fields;
};

/**
 * @brief 日志配置结构
 */
struct LogConfig {
  // 基础配置
  LogLevel global_level = LogLevel::INFO;
  bool file_enabled = true;
  bool console_enabled = false;
  bool cli_enabled = false;

  // 文件输出配置
  std::string log_directory = "./logs";
  std::string filename_pattern = "{program}.log";
  size_t max_file_size_mb = 10;
  size_t max_files = 10;
  bool single_file = true;
  bool auto_flush = true;

  // 控制台输出配置
  bool console_colored = true;
  LogLevel console_min_level = LogLevel::WARNING;

  // CLI输出配置
  size_t cli_buffer_size = 1000;

  // 格式配置
  std::string format_pattern = "[{timestamp}] [{level}] [{location}] {message}";
  std::string timestamp_format = "%Y-%m-%d %H:%M:%S";
  bool include_location = true;
  bool include_thread_id = false;

  // 性能配置
  bool async_logging = false;
  size_t buffer_size = 1024;
  size_t flush_interval_ms = 1000;

  // 模块级别配置
  std::map<std::string, LogLevel> module_levels;

  // 从ConfigManager加载配置
  static LogConfig loadFromConfigManager();

  // 应用环境变量覆盖
  void applyEnvironmentOverrides();

 private:
  static LogLevel parseLogLevel(const std::string& level_str);
};

/**
 * @brief 日志输出流类型
 */
enum class LogOutputType : std::uint8_t {
  FILE,
  CONSOLE,
  CLI_INTERFACE,
  MEMORY_BUFFER,
  CUSTOM
};

/**
 * @brief 抽象日志输出流
 */
class LogOutputStream {
 public:
  virtual ~LogOutputStream() = default;
  virtual void write(const LogEntry& entry, const std::string& formatted) = 0;
  virtual void flush() = 0;
  virtual LogOutputType getType() const = 0;
  virtual bool shouldLog(LogLevel level) const { return true; }
};

/**
 * @brief 日志格式化器
 */
class LogFormatter {
 public:
  explicit LogFormatter(const std::string& pattern);
  std::string format(const LogEntry& entry) const;

 private:
  std::string pattern_;
  std::vector<std::function<std::string(const LogEntry&)>> formatters_;
  void parsePattern();
};

/**
 * @brief 等级过滤器
 */
class LevelFilter {
 public:
  void setGlobalLevel(LogLevel level);
  void setModuleLevel(const std::string& module, LogLevel level);
  void setFileLevel(const std::string& file_pattern, LogLevel level);

  bool shouldLog(LogLevel level, const std::string& file,
                 const std::string& module = "") const;

  LogLevel getEffectiveLevel(const std::string& file,
                             const std::string& module = "") const;

 private:
  LogLevel global_level_ = LogLevel::INFO;
  std::map<std::string, LogLevel> module_levels_;
  std::map<std::string, LogLevel> file_levels_;
  mutable std::shared_mutex filter_mutex_;
};

/**
 * @brief 文件日志输出流
 */
class FileLogStream : public LogOutputStream {
 public:
  FileLogStream(const std::string& directory,
                const std::string& filename_pattern, size_t max_size_mb,
                size_t max_files, bool auto_flush = true,
                const std::string& program_name = "picoradar");

  void write(const LogEntry& entry, const std::string& formatted) override;
  void flush() override;
  LogOutputType getType() const override { return LogOutputType::FILE; }

 private:
  std::string directory_;
  std::string filename_pattern_;
  std::string program_name_;
  size_t max_size_mb_;
  size_t max_files_;
  bool auto_flush_;

  std::unique_ptr<std::ofstream> current_file_;
  std::string current_filename_;
  size_t current_size_ = 0;
  std::mutex file_mutex_;

  void rotateFile();
  std::string generateFilename() const;
  void ensureDirectoryExists();
};

/**
 * @brief 控制台日志输出流
 */
class ConsoleLogStream : public LogOutputStream {
 public:
  explicit ConsoleLogStream(bool colored = true,
                            LogLevel min_level = LogLevel::INFO);

  void write(const LogEntry& entry, const std::string& formatted) override;
  void flush() override;
  LogOutputType getType() const override { return LogOutputType::CONSOLE; }
  bool shouldLog(LogLevel level) const override { return level >= min_level_; }

 private:
  bool colored_;
  LogLevel min_level_;
  std::mutex console_mutex_;

  std::string getColorCode(LogLevel level) const;
  std::string getResetCode() const;
};

/**
 * @brief CLI界面日志输出流
 */
class CLIInterfaceLogStream : public LogOutputStream {
 public:
  explicit CLIInterfaceLogStream(std::shared_ptr<CLIOutput> cli_output);

  void write(const LogEntry& entry, const std::string& formatted) override;
  void flush() override;
  LogOutputType getType() const override {
    return LogOutputType::CLI_INTERFACE;
  }

 private:
  std::weak_ptr<CLIOutput> cli_output_;
  std::string logLevelToString(LogLevel level) const;
};

/**
 * @brief 内存缓冲区日志输出流（用于测试）
 */
class MemoryLogStream : public LogOutputStream {
 public:
  explicit MemoryLogStream(size_t max_entries = 1000);

  void write(const LogEntry& entry, const std::string& formatted) override;
  void flush() override;
  LogOutputType getType() const override {
    return LogOutputType::MEMORY_BUFFER;
  }

  std::vector<std::string> getEntries() const;
  void clear();

 private:
  size_t max_entries_;
  std::vector<std::string> entries_;
  mutable std::mutex buffer_mutex_;
};

/**
 * @brief 主Logger类
 */
class Logger {
 public:
  // 初始化方法
  static void Init(const std::string& program_name, const LogConfig& config);
  static void InitFromConfigManager(const std::string& program_name);

  // 流管理
  static void addOutputStream(std::unique_ptr<LogOutputStream> stream);
  static void removeOutputStream(LogOutputType type);
  static std::vector<LogOutputType> getActiveStreams();

  // 等级管理
  static void setGlobalLevel(LogLevel level);
  static void setModuleLevel(const std::string& module, LogLevel level);
  static LogLevel getEffectiveLevel(const std::string& file,
                                    const std::string& module = "");

  // CLI集成
  static void enableCLIOutput(std::shared_ptr<CLIOutput> cli_output);
  static void disableCLIOutput();

  // 自定义回调
  static void setLogCallback(std::function<void(const LogEntry&)> callback);

  // 配置管理
  static void updateConfig(const LogConfig& config);
  static void reloadConfig();

  // 控制接口
  static void flush();
  static void shutdown();

  // 内部日志方法
  static void log(LogLevel level, const char* file, int line,
                  const char* function, const std::string& message,
                  const std::string& module = "");

  static bool shouldLog(LogLevel level, const char* file,
                        const std::string& module = "");

  // 获取实例（单例模式）
  static Logger& getInstance();

  // 流式日志类
  class LogStream {
   public:
    LogStream(const char* file, int line, const char* function, LogLevel level,
              const std::string& module = "");
    ~LogStream();

    template <typename T>
    LogStream& operator<<(const T& val) {
      if (should_log_) {
        stream_ << val;
      }
      return *this;
    }

   private:
    std::ostringstream stream_;
    const char* file_;
    int line_;
    const char* function_;
    LogLevel level_;
    std::string module_;
    bool should_log_;
  };

  // 条件日志类
  class LogStreamIf {
   public:
    LogStreamIf(const char* file, int line, const char* function,
                LogLevel level, bool condition, const std::string& module = "");
    ~LogStreamIf();

    template <typename T>
    LogStreamIf& operator<<(const T& val) {
      if (should_log_) {
        stream_ << val;
      }
      return *this;
    }

   private:
    std::ostringstream stream_;
    const char* file_;
    int line_;
    const char* function_;
    LogLevel level_;
    std::string module_;
    bool should_log_;
  };

  // 模块化日志类
  class LogStreamWithModule {
   public:
    LogStreamWithModule(const char* file, int line, const char* function,
                        LogLevel level, const std::string& module);
    ~LogStreamWithModule();

    template <typename T>
    LogStreamWithModule& operator<<(const T& val) {
      if (should_log_) {
        stream_ << val;
      }
      return *this;
    }

   private:
    std::ostringstream stream_;
    const char* file_;
    int line_;
    const char* function_;
    LogLevel level_;
    std::string module_;
    bool should_log_;
  };

 private:
  Logger() = default;
  ~Logger() = default;
  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;

  std::string program_name_;
  LogConfig config_;
  std::unique_ptr<LevelFilter> level_filter_;
  std::vector<std::unique_ptr<LogOutputStream>> output_streams_;
  std::unique_ptr<LogFormatter> formatter_;
  std::function<void(const LogEntry&)> log_callback_;

  mutable std::mutex logger_mutex_;

  void writeToStreams(const LogEntry& entry);
  std::string extractFilename(const std::string& filepath) const;
  std::string logLevelToString(LogLevel level) const;
};

// 方便使用的宏定义
#define LOG_TRACE                                               \
  ::logger::Logger::LogStream(__FILE__, __LINE__, __FUNCTION__, \
                              ::logger::LogLevel::TRACE)
#define LOG_DEBUG                                               \
  ::logger::Logger::LogStream(__FILE__, __LINE__, __FUNCTION__, \
                              ::logger::LogLevel::DEBUG)
#define LOG_INFO                                                \
  ::logger::Logger::LogStream(__FILE__, __LINE__, __FUNCTION__, \
                              ::logger::LogLevel::INFO)
#define LOG_WARNING                                             \
  ::logger::Logger::LogStream(__FILE__, __LINE__, __FUNCTION__, \
                              ::logger::LogLevel::WARNING)
#define LOG_ERROR                                               \
  ::logger::Logger::LogStream(__FILE__, __LINE__, __FUNCTION__, \
                              ::logger::LogLevel::ERROR)
#define LOG_FATAL                                               \
  ::logger::Logger::LogStream(__FILE__, __LINE__, __FUNCTION__, \
                              ::logger::LogLevel::FATAL)

// 模块化日志宏
#define LOG_MODULE(module, level)                                         \
  ::logger::Logger::LogStreamWithModule(__FILE__, __LINE__, __FUNCTION__, \
                                        level, module)

// 条件日志宏
#define LOG_IF_TRACE(condition)                                   \
  ::logger::Logger::LogStreamIf(__FILE__, __LINE__, __FUNCTION__, \
                                ::logger::LogLevel::TRACE, condition)
#define LOG_IF_DEBUG(condition)                                   \
  ::logger::Logger::LogStreamIf(__FILE__, __LINE__, __FUNCTION__, \
                                ::logger::LogLevel::DEBUG, condition)
#define LOG_IF_INFO(condition)                                    \
  ::logger::Logger::LogStreamIf(__FILE__, __LINE__, __FUNCTION__, \
                                ::logger::LogLevel::INFO, condition)
#define LOG_IF_WARNING(condition)                                 \
  ::logger::Logger::LogStreamIf(__FILE__, __LINE__, __FUNCTION__, \
                                ::logger::LogLevel::WARNING, condition)
#define LOG_IF_ERROR(condition)                                   \
  ::logger::Logger::LogStreamIf(__FILE__, __LINE__, __FUNCTION__, \
                                ::logger::LogLevel::ERROR, condition)
#define LOG_IF_FATAL(condition)                                   \
  ::logger::Logger::LogStreamIf(__FILE__, __LINE__, __FUNCTION__, \
                                ::logger::LogLevel::FATAL, condition)

// 频率限制日志宏
#define LOG_EVERY_N(level, n)                 \
  static std::atomic<int> LOG_OCCURRENCES(0); \
  if (LOG_OCCURRENCES.fetch_add(1) % (n) == 0) LOG_##level

// 第一次日志宏
#define LOG_FIRST_N(level, n)                 \
  static std::atomic<int> LOG_OCCURRENCES(0); \
  if (LOG_OCCURRENCES.fetch_add(1) < (n)) LOG_##level

}  // namespace logger
