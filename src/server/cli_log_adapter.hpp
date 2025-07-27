#pragma once

#include <functional>
#include <memory>
#include <string>

namespace picoradar::server {

class CLIInterface;

/**
 * @brief 日志适配器，将日志输出重定向到CLI界面
 */
class CLILogAdapter {
 public:
  static void initialize(const std::shared_ptr<CLIInterface>& cli_interface);
  static void shutdown();

  // 设置额外的日志处理器（例如文件日志）
  static void setAdditionalHandler(
      const std::function<void(const std::string&, const std::string&)>&
          handler);

  // 手动添加日志条目
  static void addLogEntry(const std::string& level, const std::string& message);

 private:
  static std::shared_ptr<CLIInterface> cli_interface_;
  static std::function<void(const std::string&, const std::string&)>
      additional_handler_;
};

// 宏定义用于替换原有的LOG_*宏
#define CLI_LOG_INFO(message)                                         \
  do {                                                                \
    std::ostringstream oss;                                           \
    oss << (message);                                                 \
    picoradar::server::CLILogAdapter::addLogEntry("INFO", oss.str()); \
  } while (0)

#define CLI_LOG_WARNING(message)                                         \
  do {                                                                   \
    std::ostringstream oss;                                              \
    oss << (message);                                                    \
    picoradar::server::CLILogAdapter::addLogEntry("WARNING", oss.str()); \
  } while (0)

#define CLI_LOG_ERROR(message)                                         \
  do {                                                                 \
    std::ostringstream oss;                                            \
    oss << (message);                                                  \
    picoradar::server::CLILogAdapter::addLogEntry("ERROR", oss.str()); \
  } while (0)

#define CLI_LOG_DEBUG(message)                                         \
  do {                                                                 \
    std::ostringstream oss;                                            \
    oss << (message);                                                  \
    picoradar::server::CLILogAdapter::addLogEntry("DEBUG", oss.str()); \
  } while (0)

}  // namespace picoradar::server
