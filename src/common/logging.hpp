#pragma once

#include <glog/logging.h>

#include <string>
#include <string_view>

namespace picoradar::common {

/**
 * @brief 一个自定义的glog LogSink，用于在每条日志消息前添加一个前缀。
 *
 * 这允许我们在测试中区分来自不同进程（如服务器和客户端）的日志输出。
 */
class PrefixedLogSink : public google::LogSink {
 public:
  /**
   * @brief 构造函数。
   * @param prefix 要添加到每条日志消息开头的字符串。
   */
  explicit PrefixedLogSink(std::string prefix);

  void send(google::LogSeverity severity, const char* full_filename,
            const char* base_filename, int line,
            const struct ::google::LogMessageTime& tm, const char* message,
            size_t message_len) override;

 private:
  const std::string prefix_;
};

/**
 * @brief 初始化应用程序的日志系统。
 *
 * 这个函数封装了 glog 的初始化和配置逻辑，确保了日志系统
 * 在整个应用程序中的行为一致性。它会根据指定的程序名称设置
 * 日志，并可以选择性地将日志输出到文件和/或标准错误流。
 *
 * @param app_name 应用程序的名称，通常是 argv[0]。
 *                 用于glog内部识别和日志文件命名。
 * @param log_to_file 是否将日志写入文件。默认为 true。
 * @param log_dir 日志文件存放的目录。默认为 "./logs"。
 *                如果目录不存在，该函数会自动创建。
 * @param log_prefix 如果提供，将在每条控制台日志消息前添加此文本前缀 (例如,
 * "[SERVER] ")。 这对于在集成测试中区分日志源特别有用。
 */
void setup_logging(std::string_view app_name, bool log_to_file = true,
                   const std::string& log_dir = "./logs",
                   const std::string& log_prefix = "");

}  // namespace picoradar::common