#include "logging.hpp"

#include <unistd.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <regex>

#include "common/config_manager.hpp"

namespace logger {

// ======================== LogConfig 实现 ========================

LogConfig LogConfig::loadFromConfigManager() {
  auto& config_manager = ::picoradar::common::ConfigManager::getInstance();
  LogConfig log_config;

  // 基础配置
  log_config.global_level =
      parseLogLevel(config_manager.template getWithDefault<std::string>(
          "logging.level", "INFO"));
  log_config.file_enabled = config_manager.template getWithDefault<bool>(
      "logging.file_enabled", true);
  log_config.console_enabled = config_manager.template getWithDefault<bool>(
      "logging.console_enabled", false);

  // 文件配置
  log_config.log_directory =
      config_manager.template getWithDefault<std::string>(
          "logging.file.directory", "./logs");
  log_config.filename_pattern =
      config_manager.template getWithDefault<std::string>(
          "logging.file.filename_pattern", "{program}.log");
  log_config.max_file_size_mb = config_manager.template getWithDefault<int>(
      "logging.file.max_size_mb", 10);
  log_config.max_files =
      config_manager.template getWithDefault<int>("logging.file.max_files", 10);
  log_config.single_file = config_manager.template getWithDefault<bool>(
      "logging.file.single_file", true);
  log_config.auto_flush = config_manager.template getWithDefault<bool>(
      "logging.file.auto_flush", true);

  // 控制台配置
  log_config.console_colored = config_manager.template getWithDefault<bool>(
      "logging.console.colored", true);
  log_config.console_min_level =
      parseLogLevel(config_manager.template getWithDefault<std::string>(
          "logging.console.min_level", "WARNING"));

  // CLI配置
  log_config.cli_enabled = config_manager.template getWithDefault<bool>(
      "logging.cli.enabled", false);
  log_config.cli_buffer_size = config_manager.template getWithDefault<int>(
      "logging.cli.buffer_size", 1000);

  // 格式配置
  log_config.format_pattern =
      config_manager.template getWithDefault<std::string>(
          "logging.format.pattern",
          "[{timestamp}] [{level}] [{location}] {message}");
  log_config.timestamp_format =
      config_manager.template getWithDefault<std::string>(
          "logging.format.timestamp_format", "%Y-%m-%d %H:%M:%S");
  log_config.include_location = config_manager.template getWithDefault<bool>(
      "logging.format.include_location", true);
  log_config.include_thread_id = config_manager.template getWithDefault<bool>(
      "logging.format.include_thread_id", false);

  // 性能配置
  log_config.async_logging = config_manager.template getWithDefault<bool>(
      "logging.performance.async_logging", false);
  log_config.buffer_size = config_manager.template getWithDefault<int>(
      "logging.performance.buffer_size", 1024);
  log_config.flush_interval_ms = config_manager.template getWithDefault<int>(
      "logging.performance.flush_interval_ms", 1000);

  // 加载模块级别配置
  if (config_manager.hasKey("logging.module_levels")) {
    auto config = config_manager.getConfig();
    if (config.contains("logging") &&
        config["logging"].contains("module_levels")) {
      auto& module_config = config["logging"]["module_levels"];
      for (auto& [module, level] : module_config.items()) {
        log_config.module_levels[module] =
            parseLogLevel(level.template get<std::string>());
      }
    }
  }

  return log_config;
}

void LogConfig::applyEnvironmentOverrides() {
  if (const char* env_level = std::getenv("PICO_LOG_LEVEL")) {
    global_level = parseLogLevel(env_level);
  }
  if (const char* env_dir = std::getenv("PICO_LOG_DIR")) {
    log_directory = env_dir;
  }
  if (const char* env_console = std::getenv("PICO_LOG_CONSOLE")) {
    console_enabled =
        (std::string(env_console) == "true" || std::string(env_console) == "1");
  }
  if (const char* env_colored = std::getenv("PICO_LOG_COLORED")) {
    console_colored =
        (std::string(env_colored) == "true" || std::string(env_colored) == "1");
  }
  if (const char* env_file = std::getenv("PICO_LOG_FILE")) {
    filename_pattern = env_file;
  }
}

LogLevel LogConfig::parseLogLevel(const std::string& level_str) {
  std::string upper_level = level_str;
  std::transform(upper_level.begin(), upper_level.end(), upper_level.begin(),
                 ::toupper);

  if (upper_level == "TRACE") return LogLevel::TRACE;
  if (upper_level == "DEBUG") return LogLevel::DEBUG;
  if (upper_level == "INFO") return LogLevel::INFO;
  if (upper_level == "WARNING" || upper_level == "WARN")
    return LogLevel::WARNING;
  if (upper_level == "ERROR") return LogLevel::ERROR;
  if (upper_level == "FATAL") return LogLevel::FATAL;

  return LogLevel::INFO;  // 默认值
}

// ======================== LogFormatter 实现 ========================

LogFormatter::LogFormatter(const std::string& pattern) : pattern_(pattern) {
  parsePattern();
}

std::string LogFormatter::format(const LogEntry& entry) const {
  std::string result;
  result.reserve(256);  // 预分配内存

  for (const auto& formatter : formatters_) {
    result += formatter(entry);
  }

  return result;
}

void LogFormatter::parsePattern() {
  std::regex placeholder_regex(R"(\{([^}]+)\})");
  std::sregex_iterator begin(pattern_.begin(), pattern_.end(),
                             placeholder_regex);
  std::sregex_iterator end;

  size_t last_pos = 0;

  for (auto it = begin; it != end; ++it) {
    const std::smatch& match = *it;

    // 添加占位符前的文本
    if (match.position() > last_pos) {
      std::string literal =
          pattern_.substr(last_pos, match.position() - last_pos);
      formatters_.push_back([literal](const LogEntry&) { return literal; });
    }

    // 处理占位符
    std::string placeholder = match[1].str();
    if (placeholder == "timestamp") {
      formatters_.push_back([](const LogEntry& entry) {
        auto time_t = std::chrono::system_clock::to_time_t(entry.timestamp);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      entry.timestamp.time_since_epoch()) %
                  1000;

        std::ostringstream oss;
        oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        oss << "." << std::setfill('0') << std::setw(3) << ms.count();
        return oss.str();
      });
    } else if (placeholder == "level") {
      formatters_.push_back([](const LogEntry& entry) {
        switch (entry.level) {
          case LogLevel::TRACE:
            return std::string("TRACE");
          case LogLevel::DEBUG:
            return std::string("DEBUG");
          case LogLevel::INFO:
            return std::string("INFO");
          case LogLevel::WARNING:
            return std::string("WARN");
          case LogLevel::ERROR:
            return std::string("ERROR");
          case LogLevel::FATAL:
            return std::string("FATAL");
          default:
            return std::string("UNKN");
        }
      });
    } else if (placeholder == "location") {
      formatters_.push_back([](const LogEntry& entry) {
        size_t pos = entry.file.find_last_of("/\\");
        std::string filename = (pos == std::string::npos)
                                   ? entry.file
                                   : entry.file.substr(pos + 1);
        return filename + ":" + std::to_string(entry.line);
      });
    } else if (placeholder == "function") {
      formatters_.push_back(
          [](const LogEntry& entry) { return entry.function; });
    } else if (placeholder == "thread") {
      formatters_.push_back([](const LogEntry& entry) {
        std::ostringstream oss;
        oss << entry.thread_id;
        return oss.str();
      });
    } else if (placeholder == "module") {
      formatters_.push_back([](const LogEntry& entry) {
        return entry.module.empty() ? std::string("")
                                    : "[" + entry.module + "]";
      });
    } else if (placeholder == "message") {
      formatters_.push_back(
          [](const LogEntry& entry) { return entry.message; });
    } else if (placeholder == "pid") {
      formatters_.push_back(
          [](const LogEntry&) { return std::to_string(getpid()); });
    } else {
      // 未知占位符，原样保留
      std::string literal = "{" + placeholder + "}";
      formatters_.push_back([literal](const LogEntry&) { return literal; });
    }

    last_pos = match.position() + match.length();
  }

  // 添加最后的文本
  if (last_pos < pattern_.length()) {
    std::string literal = pattern_.substr(last_pos);
    formatters_.push_back([literal](const LogEntry&) { return literal; });
  }
}

// ======================== LevelFilter 实现 ========================

void LevelFilter::setGlobalLevel(LogLevel level) {
  std::unique_lock lock(filter_mutex_);
  global_level_ = level;
}

void LevelFilter::setModuleLevel(const std::string& module, LogLevel level) {
  std::unique_lock lock(filter_mutex_);
  module_levels_[module] = level;
}

void LevelFilter::setFileLevel(const std::string& file_pattern,
                               LogLevel level) {
  std::unique_lock lock(filter_mutex_);
  file_levels_[file_pattern] = level;
}

bool LevelFilter::shouldLog(LogLevel level, const std::string& file,
                            const std::string& module) const {
  return level >= getEffectiveLevel(file, module);
}

LogLevel LevelFilter::getEffectiveLevel(const std::string& file,
                                        const std::string& module) const {
  std::shared_lock lock(filter_mutex_);

  // 1. 检查模块级别
  if (!module.empty()) {
    auto it = module_levels_.find(module);
    if (it != module_levels_.end()) {
      return it->second;
    }
  }

  // 2. 检查文件级别
  size_t pos = file.find_last_of("/\\");
  std::string filename =
      (pos == std::string::npos) ? file : file.substr(pos + 1);

  for (const auto& [pattern, level] : file_levels_) {
    if (filename.find(pattern) != std::string::npos) {
      return level;
    }
  }

  // 3. 返回全局级别
  return global_level_;
}

// ======================== FileLogStream 实现 ========================

FileLogStream::FileLogStream(const std::string& directory,
                             const std::string& filename_pattern,
                             size_t max_size_mb, size_t max_files,
                             bool auto_flush, const std::string& program_name)
    : directory_(directory),
      filename_pattern_(filename_pattern),
      program_name_(program_name),
      max_size_mb_(max_size_mb),
      max_files_(max_files),
      auto_flush_(auto_flush) {
  ensureDirectoryExists();
  current_filename_ = generateFilename();
  current_file_ =
      std::make_unique<std::ofstream>(current_filename_, std::ios::app);

  if (current_file_->is_open()) {
    current_file_->seekp(0, std::ios::end);
    current_size_ = current_file_->tellp();
  }
}

void FileLogStream::write(const LogEntry& entry, const std::string& formatted) {
  std::lock_guard lock(file_mutex_);

  if (!current_file_ || !current_file_->is_open()) {
    return;
  }

  *current_file_ << formatted << std::endl;
  current_size_ += formatted.length() + 1;  // +1 for newline

  if (auto_flush_) {
    current_file_->flush();
  }

  // 检查是否需要轮转
  if (current_size_ > max_size_mb_ * 1024 * 1024) {
    rotateFile();
  }
}

void FileLogStream::flush() {
  std::lock_guard lock(file_mutex_);
  if (current_file_ && current_file_->is_open()) {
    current_file_->flush();
  }
}

void FileLogStream::rotateFile() {
  if (!current_file_) return;

  current_file_->close();

  // 重命名现有文件
  for (int i = max_files_ - 1; i > 0; --i) {
    std::string old_name = current_filename_ + "." + std::to_string(i);
    std::string new_name = current_filename_ + "." + std::to_string(i + 1);

    if (std::filesystem::exists(old_name)) {
      std::filesystem::rename(old_name, new_name);
    }
  }

  // 将当前文件重命名为 .1
  std::string backup_name = current_filename_ + ".1";
  if (std::filesystem::exists(current_filename_)) {
    std::filesystem::rename(current_filename_, backup_name);
  }

  // 删除最老的文件
  std::string oldest_file =
      current_filename_ + "." + std::to_string(max_files_);
  if (std::filesystem::exists(oldest_file)) {
    std::filesystem::remove(oldest_file);
  }

  // 创建新文件
  current_file_ =
      std::make_unique<std::ofstream>(current_filename_, std::ios::app);
  current_size_ = 0;
}

std::string FileLogStream::generateFilename() const {
  std::string filename = filename_pattern_;

  // 替换 {program} 占位符
  size_t pos = filename.find("{program}");
  if (pos != std::string::npos) {
    filename.replace(pos, 9, program_name_);  // 使用实际程序名
  }

  // 替换 {date} 占位符
  pos = filename.find("{date}");
  if (pos != std::string::npos) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y%m%d");
    filename.replace(pos, 6, oss.str());
  }

  return (std::filesystem::path(directory_) / filename).string();
}

void FileLogStream::ensureDirectoryExists() {
  if (!std::filesystem::exists(directory_)) {
    std::filesystem::create_directories(directory_);
  }
}

// ======================== ConsoleLogStream 实现 ========================

ConsoleLogStream::ConsoleLogStream(bool colored, LogLevel min_level)
    : colored_(colored), min_level_(min_level) {}

void ConsoleLogStream::write(const LogEntry& entry,
                             const std::string& formatted) {
  std::lock_guard lock(console_mutex_);

  if (colored_) {
    std::cerr << getColorCode(entry.level) << formatted << getResetCode()
              << std::endl;
  } else {
    std::cerr << formatted << std::endl;
  }
}

void ConsoleLogStream::flush() {
  std::lock_guard lock(console_mutex_);
  std::cerr.flush();
}

std::string ConsoleLogStream::getColorCode(LogLevel level) const {
  if (!colored_) return "";

  switch (level) {
    case LogLevel::TRACE:
      return "\033[37m";  // White
    case LogLevel::DEBUG:
      return "\033[36m";  // Cyan
    case LogLevel::INFO:
      return "\033[32m";  // Green
    case LogLevel::WARNING:
      return "\033[33m";  // Yellow
    case LogLevel::ERROR:
      return "\033[31m";  // Red
    case LogLevel::FATAL:
      return "\033[35m";  // Magenta
    default:
      return "";
  }
}

std::string ConsoleLogStream::getResetCode() const {
  return colored_ ? "\033[0m" : "";
}

// ======================== CLIInterfaceLogStream 实现 ========================

// 前向声明CLI接口
namespace picoradar::server {
class CLIInterface;
}

CLIInterfaceLogStream::CLIInterfaceLogStream(
    std::shared_ptr<CLIOutput> cli_output)
    : cli_output_(cli_output) {}

void CLIInterfaceLogStream::write(const LogEntry& entry,
                                  const std::string& formatted) {
  if (auto cli = cli_output_.lock()) {
    // 使用CLIOutput的addLogEntry方法
    cli->addLogEntry(logLevelToString(entry.level), entry.message);
  }
}

void CLIInterfaceLogStream::flush() {
  // CLI 界面通常不需要显式刷新
}

std::string CLIInterfaceLogStream::logLevelToString(LogLevel level) const {
  switch (level) {
    case LogLevel::TRACE:
      return "TRACE";
    case LogLevel::DEBUG:
      return "DEBUG";
    case LogLevel::INFO:
      return "INFO";
    case LogLevel::WARNING:
      return "WARNING";
    case LogLevel::ERROR:
      return "ERROR";
    case LogLevel::FATAL:
      return "FATAL";
    default:
      return "UNKNOWN";
  }
}

// ======================== MemoryLogStream 实现 ========================

MemoryLogStream::MemoryLogStream(size_t max_entries)
    : max_entries_(max_entries) {}

void MemoryLogStream::write(const LogEntry& entry,
                            const std::string& formatted) {
  std::lock_guard lock(buffer_mutex_);

  entries_.push_back(formatted);

  // 限制缓冲区大小
  if (entries_.size() > max_entries_) {
    entries_.erase(entries_.begin());
  }
}

void MemoryLogStream::flush() {
  // 内存流不需要刷新
}

std::vector<std::string> MemoryLogStream::getEntries() const {
  std::lock_guard lock(buffer_mutex_);
  return entries_;
}

void MemoryLogStream::clear() {
  std::lock_guard lock(buffer_mutex_);
  entries_.clear();
}

// ======================== Logger 主类实现 ========================

Logger& Logger::getInstance() {
  static Logger instance;
  return instance;
}

void Logger::Init(const std::string& program_name, const LogConfig& config) {
  auto& instance = getInstance();
  std::lock_guard lock(instance.logger_mutex_);

  instance.program_name_ = program_name;
  instance.config_ = config;

  // 创建等级过滤器
  instance.level_filter_ = std::make_unique<LevelFilter>();
  instance.level_filter_->setGlobalLevel(config.global_level);

  for (const auto& [module, level] : config.module_levels) {
    instance.level_filter_->setModuleLevel(module, level);
  }

  // 创建格式化器
  instance.formatter_ = std::make_unique<LogFormatter>(config.format_pattern);

  // 清空现有输出流
  instance.output_streams_.clear();

  // 添加文件输出流
  if (config.file_enabled) {
    auto file_stream = std::make_unique<FileLogStream>(
        config.log_directory, config.filename_pattern, config.max_file_size_mb,
        config.max_files, config.auto_flush, program_name);
    instance.output_streams_.push_back(std::move(file_stream));
  }

  // 添加控制台输出流
  if (config.console_enabled) {
    auto console_stream = std::make_unique<ConsoleLogStream>(
        config.console_colored, config.console_min_level);
    instance.output_streams_.push_back(std::move(console_stream));
  }
}

void Logger::InitFromConfigManager(const std::string& program_name) {
  LogConfig config = LogConfig::loadFromConfigManager();
  config.applyEnvironmentOverrides();
  Init(program_name, config);
}

void Logger::addOutputStream(std::unique_ptr<LogOutputStream> stream) {
  auto& instance = getInstance();
  std::lock_guard lock(instance.logger_mutex_);
  instance.output_streams_.push_back(std::move(stream));
}

void Logger::removeOutputStream(LogOutputType type) {
  auto& instance = getInstance();
  std::lock_guard lock(instance.logger_mutex_);

  instance.output_streams_.erase(
      std::remove_if(
          instance.output_streams_.begin(), instance.output_streams_.end(),
          [type](const auto& stream) { return stream->getType() == type; }),
      instance.output_streams_.end());
}

std::vector<LogOutputType> Logger::getActiveStreams() {
  auto& instance = getInstance();
  std::lock_guard lock(instance.logger_mutex_);

  std::vector<LogOutputType> types;
  for (const auto& stream : instance.output_streams_) {
    types.push_back(stream->getType());
  }
  return types;
}

void Logger::setGlobalLevel(LogLevel level) {
  auto& instance = getInstance();
  std::lock_guard lock(instance.logger_mutex_);

  if (instance.level_filter_) {
    instance.level_filter_->setGlobalLevel(level);
  }
  instance.config_.global_level = level;
}

void Logger::setModuleLevel(const std::string& module, LogLevel level) {
  auto& instance = getInstance();
  std::lock_guard lock(instance.logger_mutex_);

  if (instance.level_filter_) {
    instance.level_filter_->setModuleLevel(module, level);
  }
  instance.config_.module_levels[module] = level;
}

LogLevel Logger::getEffectiveLevel(const std::string& file,
                                   const std::string& module) {
  auto& instance = getInstance();
  std::lock_guard lock(instance.logger_mutex_);

  if (instance.level_filter_) {
    return instance.level_filter_->getEffectiveLevel(file, module);
  }
  return LogLevel::INFO;
}

void Logger::enableCLIOutput(std::shared_ptr<CLIOutput> cli_output) {
  auto& instance = getInstance();
  std::lock_guard lock(instance.logger_mutex_);

  // 先移除现有的CLI输出流
  removeOutputStream(LogOutputType::CLI_INTERFACE);

  // 添加新的CLI输出流
  auto cli_stream = std::make_unique<CLIInterfaceLogStream>(cli_output);
  instance.output_streams_.push_back(std::move(cli_stream));

  instance.config_.cli_enabled = true;
}

void Logger::disableCLIOutput() {
  auto& instance = getInstance();
  std::lock_guard lock(instance.logger_mutex_);

  removeOutputStream(LogOutputType::CLI_INTERFACE);
  instance.config_.cli_enabled = false;
}

void Logger::setLogCallback(std::function<void(const LogEntry&)> callback) {
  auto& instance = getInstance();
  std::lock_guard lock(instance.logger_mutex_);
  instance.log_callback_ = std::move(callback);
}

void Logger::updateConfig(const LogConfig& config) {
  Init(getInstance().program_name_, config);
}

void Logger::reloadConfig() {
  InitFromConfigManager(getInstance().program_name_);
}

void Logger::flush() {
  auto& instance = getInstance();
  std::lock_guard lock(instance.logger_mutex_);

  for (auto& stream : instance.output_streams_) {
    stream->flush();
  }
}

void Logger::shutdown() {
  auto& instance = getInstance();
  std::lock_guard lock(instance.logger_mutex_);

  // 刷新所有输出流 (不调用公共flush方法避免死锁)
  for (auto& stream : instance.output_streams_) {
    stream->flush();
  }

  instance.output_streams_.clear();
  instance.level_filter_.reset();
  instance.formatter_.reset();
  instance.log_callback_ = nullptr;
}

void Logger::log(LogLevel level, const char* file, int line,
                 const char* function, const std::string& message,
                 const std::string& module) {
  auto& instance = getInstance();

  // 快速检查是否应该记录
  if (!shouldLog(level, file, module)) {
    return;
  }

  // 创建日志条目
  LogEntry entry;
  entry.timestamp = std::chrono::system_clock::now();
  entry.level = level;
  entry.file = file;
  entry.line = line;
  entry.function = function;
  entry.thread_id = std::this_thread::get_id();
  entry.module = module;
  entry.message = message;

  // 写入到输出流
  instance.writeToStreams(entry);

  // 调用自定义回调
  if (instance.log_callback_) {
    instance.log_callback_(entry);
  }
}

bool Logger::shouldLog(LogLevel level, const char* file,
                       const std::string& module) {
  auto& instance = getInstance();

  if (!instance.level_filter_) {
    return level >= LogLevel::INFO;
  }

  return instance.level_filter_->shouldLog(level, file, module);
}

void Logger::writeToStreams(const LogEntry& entry) {
  if (!formatter_) return;

  std::string formatted_message = formatter_->format(entry);

  std::lock_guard lock(logger_mutex_);
  for (auto& stream : output_streams_) {
    if (stream->shouldLog(entry.level)) {
      stream->write(entry, formatted_message);
    }
  }
}

std::string Logger::extractFilename(const std::string& filepath) const {
  size_t pos = filepath.find_last_of("/\\");
  return (pos == std::string::npos) ? filepath : filepath.substr(pos + 1);
}

std::string Logger::logLevelToString(LogLevel level) const {
  switch (level) {
    case LogLevel::TRACE:
      return "TRACE";
    case LogLevel::DEBUG:
      return "DEBUG";
    case LogLevel::INFO:
      return "INFO";
    case LogLevel::WARNING:
      return "WARNING";
    case LogLevel::ERROR:
      return "ERROR";
    case LogLevel::FATAL:
      return "FATAL";
    default:
      return "UNKNOWN";
  }
}

// ======================== LogStream 实现 ========================

Logger::LogStream::LogStream(const char* file, int line, const char* function,
                             LogLevel level, const std::string& module)
    : file_(file),
      line_(line),
      function_(function),
      level_(level),
      module_(module) {
  should_log_ = Logger::shouldLog(level, file, module);
}

Logger::LogStream::~LogStream() {
  if (should_log_) {
    Logger::log(level_, file_, line_, function_, stream_.str(), module_);
  }
}

// ======================== LogStreamIf 实现 ========================

Logger::LogStreamIf::LogStreamIf(const char* file, int line,
                                 const char* function, LogLevel level,
                                 bool condition, const std::string& module)
    : file_(file),
      line_(line),
      function_(function),
      level_(level),
      module_(module) {
  should_log_ = condition && Logger::shouldLog(level, file, module);
}

Logger::LogStreamIf::~LogStreamIf() {
  if (should_log_) {
    Logger::log(level_, file_, line_, function_, stream_.str(), module_);
  }
}

// ======================== LogStreamWithModule 实现 ========================

Logger::LogStreamWithModule::LogStreamWithModule(const char* file, int line,
                                                 const char* function,
                                                 LogLevel level,
                                                 const std::string& module)
    : file_(file),
      line_(line),
      function_(function),
      level_(level),
      module_(module) {
  should_log_ = Logger::shouldLog(level, file, module);
}

Logger::LogStreamWithModule::~LogStreamWithModule() {
  if (should_log_) {
    Logger::log(level_, file_, line_, function_, stream_.str(), module_);
  }
}

}  // namespace logger
