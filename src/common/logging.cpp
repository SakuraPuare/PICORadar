#include "logging.hpp"

#include <glog/logging.h>

#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

namespace picoradar::common {

// glog不拥有sink的所有权。我们需要自己管理其生命周期。
// 使用一个静态的unique_ptr确保它在程序的整个生命周期内都存在。
static std::unique_ptr<PrefixedLogSink> g_log_sink;

PrefixedLogSink::PrefixedLogSink(std::string prefix)
    : prefix_(std::move(prefix)) {}

void PrefixedLogSink::send(google::LogSeverity severity,
                           const char* full_filename, const char* base_filename,
                           int line, const struct ::google::LogMessageTime& tm,
                           const char* message, size_t message_len) {
  // 先将我们的自定义前缀写入stderr。
  std::cerr << prefix_;

  // 使用glog的内部函数来格式化并写入日志的其余部分。
  // 这确保了我们的输出与glog默认的stderr输出格式完全一致。
  google::LogMessage(full_filename, line, severity).stream() << message;
}

void setup_logging(std::string_view app_name, bool log_to_file,
                   const std::string& log_dir, const std::string& log_prefix) {
  google::InitGoogleLogging(app_name.data());

  // 默认情况下，glog会向stderr记录日志。
  // 如果我们使用自己的sink或要记录到文件，我们会禁用它。
  FLAGS_logtostderr = false;

  if (!log_prefix.empty()) {
    // 如果提供了前缀，我们就使用自定义的sink。
    g_log_sink = std::make_unique<PrefixedLogSink>(log_prefix);
    google::AddLogSink(g_log_sink.get());
  }

  if (log_to_file) {
    std::filesystem::path log_path = log_dir;
    FLAGS_log_dir = log_dir;
    if (!std::filesystem::exists(log_path)) {
      std::filesystem::create_directories(log_path);
    }
    // 如果没有前缀，我们希望在记录到文件的同时，也记录到stderr。
    if (log_prefix.empty()) {
      FLAGS_alsologtostderr = true;
    }
  } else if (log_prefix.empty()) {
    // 如果不记录到文件且没有前缀sink，则启用默认的stderr日志记录。
    FLAGS_logtostderr = true;
  }
}

}  // namespace picoradar::common