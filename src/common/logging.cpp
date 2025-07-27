// Prevent Windows.h from defining problematic macros
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

#include "logging.hpp"

#include <glog/logging.h>

#include <filesystem>
#include <iostream>

namespace logger {

// Helper to convert our LogLevel to glog's severity
static google::LogSeverity ToGlogSeverity(const LogLevel level) {
  switch (level) {
    case LogLevel::TRACE:
      return google::GLOG_INFO;
    case LogLevel::DEBUG:
      return google::GLOG_INFO;
    case LogLevel::INFO:
      return google::GLOG_INFO;
    case LogLevel::WARNING:
      return google::GLOG_WARNING;
    case LogLevel::ERROR:
      return google::GLOG_ERROR;
    case LogLevel::FATAL:
      return google::GLOG_FATAL;
    default:
      return google::GLOG_INFO;
  }
}

LogLevel Logger::current_log_level_ = LogLevel::INFO;

void Logger::Init(const std::string& program_name, const std::string& log_dir,
                  const LogLevel min_log_level, const uint32_t max_log_size,
                  const bool log_to_stderr) {
  static std::once_flag once_flag;
  std::call_once(once_flag, [&] {
    // Create log directory if it doesn't exist
    try {
      std::filesystem::create_directories(log_dir);
    } catch (const std::exception& e) {
      std::cerr << "Warning: Failed to create log directory " << log_dir << ": "
                << e.what() << ". Logging to stderr only." << std::endl;
      // If we can't create the log directory, just log to stderr
      google::InitGoogleLogging(program_name.c_str());
      FLAGS_logtostderr = true;
      FLAGS_log_prefix = true;
      current_log_level_ = min_log_level;
      google::SetStderrLogging(ToGlogSeverity(min_log_level));
      return;
    }

    google::InitGoogleLogging(program_name.c_str());

    // Create log directory if it doesn't exist
    if (!google::IsGoogleLoggingInitialized() || log_dir != "./logs") {
      google::ShutdownGoogleLogging();
      google::InitGoogleLogging(program_name.c_str());
    }
    google::SetLogDestination(google::GLOG_INFO, (log_dir + "/INFO_").c_str());
    google::SetLogDestination(google::GLOG_WARNING,
                              (log_dir + "/WARNING_").c_str());
    google::SetLogDestination(google::GLOG_ERROR,
                              (log_dir + "/ERROR_").c_str());
    google::SetLogDestination(google::GLOG_FATAL,
                              (log_dir + "/FATAL_").c_str());

    current_log_level_ = min_log_level;
    google::SetStderrLogging(ToGlogSeverity(min_log_level));

    FLAGS_max_log_size = max_log_size;

    FLAGS_logtostderr = log_to_stderr;
    FLAGS_log_prefix = true;
    FLAGS_timestamp_in_logfile_name = true;
    // FLAGS_log_thread_id = true; // This flag might not be available in all
    // glog versions.

    google::InstallFailureSignalHandler();
    google::InstallFailureWriter([](const char* data, const size_t size) {
      GetInstance().WriteFatalLog(data, size);
    });
  });
}

Logger& Logger::GetInstance() {
  static Logger instance;
  return instance;
}

void Logger::SetLogLevel(const LogLevel level) {
  std::lock_guard lock(mutex_);
  current_log_level_ = level;
  google::SetStderrLogging(ToGlogSeverity(level));
}

LogLevel Logger::GetCurrentLogLevel() { return current_log_level_; }

Logger::LogStream::LogStream(const char* file, const int line,
                             const LogLevel level)
    : file_(file), line_(line), level_(level) {}

Logger::LogStream::~LogStream() {
  if (level_ >= GetCurrentLogLevel()) {
    std::lock_guard lock(GetInstance().mutex_);
    switch (level_) {
      case LogLevel::TRACE:
        google::LogMessage(file_, line_, google::GLOG_INFO).stream()
            << "[TRACE] " << stream_.str();
        break;
      case LogLevel::DEBUG:
        google::LogMessage(file_, line_, google::GLOG_INFO).stream()
            << "[DEBUG] " << stream_.str();
        break;
      case LogLevel::INFO:
        google::LogMessage(file_, line_, google::GLOG_INFO).stream()
            << stream_.str();
        break;
      case LogLevel::WARNING:
        google::LogMessage(file_, line_, google::GLOG_WARNING).stream()
            << stream_.str();
        break;
      case LogLevel::ERROR:
        google::LogMessage(file_, line_, google::GLOG_ERROR).stream()
            << stream_.str();
        break;
      case LogLevel::FATAL:
        google::LogMessage(file_, line_, google::GLOG_FATAL).stream()
            << stream_.str();
        break;
      default:
        break;
    }
  }
}

Logger::LogStreamIf::LogStreamIf(const char* file, const int line,
                                 const LogLevel level, const bool condition)
    : file_(file), line_(line), level_(level), condition_(condition) {}

Logger::LogStreamIf::~LogStreamIf() {
  if (condition_ && level_ >= GetCurrentLogLevel()) {
    std::lock_guard lock(GetInstance().mutex_);
    switch (level_) {
      case LogLevel::TRACE:
        google::LogMessage(file_, line_, google::GLOG_INFO).stream()
            << "[TRACE] " << stream_.str();
        break;
      case LogLevel::DEBUG:
        google::LogMessage(file_, line_, google::GLOG_INFO).stream()
            << "[DEBUG] " << stream_.str();
        break;
      case LogLevel::INFO:
        google::LogMessage(file_, line_, google::GLOG_INFO).stream()
            << stream_.str();
        break;
      case LogLevel::WARNING:
        google::LogMessage(file_, line_, google::GLOG_WARNING).stream()
            << stream_.str();
        break;
      case LogLevel::ERROR:
        google::LogMessage(file_, line_, google::GLOG_ERROR).stream()
            << stream_.str();
        break;
      case LogLevel::FATAL:
        google::LogMessage(file_, line_, google::GLOG_FATAL).stream()
            << stream_.str();
        break;
      default:
        break;
    }
  }
}

void Logger::WriteFatalLog(const char* data, const int size) {
  std::lock_guard lock(mutex_);
  google::LogMessageFatal(__FILE__, __LINE__).stream()
      << std::string(data, size);
}

void Logger::Flush() {
  std::lock_guard lock(mutex_);
  google::FlushLogFiles(google::GLOG_INFO);
  google::FlushLogFiles(google::GLOG_WARNING);
  google::FlushLogFiles(google::GLOG_ERROR);
  google::FlushLogFiles(google::GLOG_FATAL);
}

Logger::Logger() = default;

Logger::~Logger() { google::ShutdownGoogleLogging(); }

}  // namespace logger
