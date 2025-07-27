#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <thread>
#include <vector>

#include "common/config_manager.hpp"
#include "common/logging.hpp"

class LoggingTest : public testing::Test {
 protected:
  void SetUp() override {
    // 创建临时日志目录
    temp_log_dir_ =
        std::filesystem::temp_directory_path() / "picoradar_logging_test";
    std::filesystem::create_directories(temp_log_dir_);

    // 创建测试配置
    test_config_.global_level = logger::LogLevel::DEBUG;
    test_config_.file_enabled = true;
    test_config_.console_enabled = false;
    test_config_.log_directory = temp_log_dir_.string();
    test_config_.filename_pattern = "test_program.log";
    test_config_.max_file_size_mb = 1;
    test_config_.max_files = 5;
    test_config_.single_file = true;
    test_config_.auto_flush = true;
    test_config_.format_pattern = "[{timestamp}] [{level}] {message}";
  }

  void TearDown() override {
    // 关闭日志系统
    logger::Logger::shutdown();

    // 清理临时文件
    if (std::filesystem::exists(temp_log_dir_)) {
      std::filesystem::remove_all(temp_log_dir_);
    }
  }

  bool logFileExists(const std::string& filename) const {
    std::filesystem::path full_path = temp_log_dir_ / filename;
    return std::filesystem::exists(full_path);
  }

  size_t countLogFiles() const {
    size_t count = 0;
    if (std::filesystem::exists(temp_log_dir_)) {
      for (const auto& entry :
           std::filesystem::directory_iterator(temp_log_dir_)) {
        if (entry.is_regular_file()) {
          count++;
        }
      }
    }
    return count;
  }

  std::string readLogFileContent(const std::string& filename) const {
    std::filesystem::path full_path = temp_log_dir_ / filename;
    if (std::filesystem::exists(full_path)) {
      std::ifstream file(full_path);
      if (file.is_open()) {
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
      }
    }
    return "";
  }

  size_t countLinesInFile(const std::string& filename) const {
    std::string content = readLogFileContent(filename);
    if (content.empty()) return 0;

    size_t count = 0;
    for (char c : content) {
      if (c == '\n') count++;
    }
    return count;
  }

  std::filesystem::path temp_log_dir_;
  logger::LogConfig test_config_;
};

/**
 * @brief 测试新日志系统的基本初始化
 */
TEST_F(LoggingTest, NewBasicInitialization) {
  // 使用新的配置方式初始化
  EXPECT_NO_THROW(logger::Logger::Init("test_program", test_config_));

  // 写入一条测试消息
  LOG_INFO << "Test initialization message";

  // 强制刷新日志
  logger::Logger::flush();

  // 验证日志文件被创建
  EXPECT_TRUE(logFileExists("test_program.log"));

  // 验证文件内容
  std::string content = readLogFileContent("test_program.log");
  EXPECT_FALSE(content.empty());
  EXPECT_NE(content.find("Test initialization message"), std::string::npos);
}

/**
 * @brief 测试日志级别过滤
 */
TEST_F(LoggingTest, LogLevelFiltering) {
  // 设置ERROR级别
  test_config_.global_level = logger::LogLevel::ERROR;
  logger::Logger::Init("test_filter", test_config_);

  // 写入不同级别的日志
  LOG_DEBUG << "Debug message - should be filtered";
  LOG_INFO << "Info message - should be filtered";
  LOG_WARNING << "Warning message - should be filtered";
  LOG_ERROR << "Error message - should appear";
  LOG_FATAL << "Fatal message - should appear";

  logger::Logger::flush();

  // 验证只有ERROR和FATAL级别的消息被记录
  std::string content = readLogFileContent("test_filter.log");
  EXPECT_EQ(content.find("Debug message"), std::string::npos);
  EXPECT_EQ(content.find("Info message"), std::string::npos);
  EXPECT_EQ(content.find("Warning message"), std::string::npos);
  EXPECT_NE(content.find("Error message"), std::string::npos);
  EXPECT_NE(content.find("Fatal message"), std::string::npos);
}

/**
 * @brief 测试模块化日志
 */
TEST_F(LoggingTest, ModuleLogging) {
  // 设置不同模块的日志级别
  test_config_.module_levels["network"] = logger::LogLevel::DEBUG;
  test_config_.module_levels["database"] = logger::LogLevel::ERROR;
  test_config_.global_level = logger::LogLevel::INFO;

  logger::Logger::Init("test_module", test_config_);

  // 测试网络模块（DEBUG级别）
  LOG_MODULE("network", logger::LogLevel::DEBUG) << "Network debug message";
  LOG_MODULE("network", logger::LogLevel::INFO) << "Network info message";

  // 测试数据库模块（ERROR级别）
  LOG_MODULE("database", logger::LogLevel::DEBUG) << "DB debug - filtered";
  LOG_MODULE("database", logger::LogLevel::INFO) << "DB info - filtered";
  LOG_MODULE("database", logger::LogLevel::ERROR) << "DB error message";

  // 测试默认模块（INFO级别）
  LOG_DEBUG << "Default debug - filtered";
  LOG_INFO << "Default info message";

  logger::Logger::flush();

  std::string content = readLogFileContent("test_module.log");

  // 网络模块的DEBUG和INFO应该都出现
  EXPECT_NE(content.find("Network debug message"), std::string::npos);
  EXPECT_NE(content.find("Network info message"), std::string::npos);

  // 数据库模块只有ERROR应该出现
  EXPECT_EQ(content.find("DB debug - filtered"), std::string::npos);
  EXPECT_EQ(content.find("DB info - filtered"), std::string::npos);
  EXPECT_NE(content.find("DB error message"), std::string::npos);

  // 默认级别
  EXPECT_EQ(content.find("Default debug - filtered"), std::string::npos);
  EXPECT_NE(content.find("Default info message"), std::string::npos);
}

/**
 * @brief 测试条件日志
 */
TEST_F(LoggingTest, ConditionalLogging) {
  logger::Logger::Init("test_conditional", test_config_);

  bool condition_true = true;
  bool condition_false = false;

  LOG_IF_INFO(condition_true) << "Conditional message - should appear";
  LOG_IF_INFO(condition_false) << "Conditional message - should not appear";

  logger::Logger::flush();

  std::string content = readLogFileContent("test_conditional.log");
  EXPECT_NE(content.find("should appear"), std::string::npos);
  EXPECT_EQ(content.find("should not appear"), std::string::npos);
}

/**
 * @brief 测试文件轮转
 */
TEST_F(LoggingTest, FileRotation) {
  // 设置很小的文件大小以触发轮转
  test_config_.max_file_size_mb = 1;  // 1MB
  test_config_.max_files = 3;

  logger::Logger::Init("test_rotation", test_config_);

  // 写入大量日志以触发轮转
  for (int i = 0; i < 10000; ++i) {
    LOG_INFO << "Log message number " << i
             << " with some additional text to make it longer";
    if (i % 1000 == 0) {
      logger::Logger::flush();
    }
  }

  logger::Logger::flush();

  // 验证文件被创建
  EXPECT_TRUE(logFileExists("test_rotation.log"));

  // 检查文件大小合理
  std::filesystem::path log_path = temp_log_dir_ / "test_rotation.log";
  if (std::filesystem::exists(log_path)) {
    auto file_size = std::filesystem::file_size(log_path);
    EXPECT_GT(file_size, 0);
  }
}

/**
 * @brief 测试多输出流
 */
TEST_F(LoggingTest, MultipleOutputStreams) {
  // 启用文件和控制台输出
  test_config_.console_enabled = true;
  logger::Logger::Init("test_multi", test_config_);

  // 添加内存缓冲区用于测试
  auto memory_stream = std::make_unique<logger::MemoryLogStream>(100);
  auto* memory_ptr = memory_stream.get();
  logger::Logger::addOutputStream(std::move(memory_stream));

  LOG_INFO << "Test message for multiple streams";
  logger::Logger::flush();

  // 验证文件输出
  std::string file_content = readLogFileContent("test_multi.log");
  EXPECT_NE(file_content.find("Test message for multiple streams"),
            std::string::npos);

  // 验证内存缓冲区输出
  auto memory_entries = memory_ptr->getEntries();
  EXPECT_FALSE(memory_entries.empty());

  bool found_in_memory = false;
  for (const auto& entry : memory_entries) {
    if (entry.find("Test message for multiple streams") != std::string::npos) {
      found_in_memory = true;
      break;
    }
  }
  EXPECT_TRUE(found_in_memory);
}

/**
 * @brief 测试动态级别调整
 */
TEST_F(LoggingTest, DynamicLevelAdjustment) {
  logger::Logger::Init("test_dynamic", test_config_);

  // 初始级别是DEBUG
  LOG_DEBUG << "Debug message 1";
  LOG_INFO << "Info message 1";

  // 调整到INFO级别
  logger::Logger::setGlobalLevel(logger::LogLevel::INFO);

  LOG_DEBUG << "Debug message 2 - should be filtered";
  LOG_INFO << "Info message 2";

  // 调整模块级别
  logger::Logger::setModuleLevel("test", logger::LogLevel::DEBUG);
  LOG_MODULE("test", logger::LogLevel::DEBUG) << "Test module debug";

  logger::Logger::flush();

  std::string content = readLogFileContent("test_dynamic.log");

  EXPECT_NE(content.find("Debug message 1"), std::string::npos);
  EXPECT_NE(content.find("Info message 1"), std::string::npos);
  EXPECT_EQ(content.find("Debug message 2 - should be filtered"),
            std::string::npos);
  EXPECT_NE(content.find("Info message 2"), std::string::npos);
  EXPECT_NE(content.find("Test module debug"), std::string::npos);
}

/**
 * @brief 测试日志格式化
 */
TEST_F(LoggingTest, LogFormatting) {
  test_config_.format_pattern = "[{level}] {message}";
  logger::Logger::Init("test_format", test_config_);

  LOG_INFO << "Test formatting";
  logger::Logger::flush();

  std::string content = readLogFileContent("test_format.log");
  EXPECT_NE(content.find("[INFO] Test formatting"), std::string::npos);
}

/**
 * @brief 测试内存日志流
 */
TEST_F(LoggingTest, MemoryLogStream) {
  auto memory_stream = std::make_unique<logger::MemoryLogStream>(5);
  auto* memory_ptr = memory_stream.get();

  // 创建一个简单的日志条目用于测试
  logger::LogEntry entry;
  entry.timestamp = std::chrono::system_clock::now();
  entry.level = logger::LogLevel::INFO;
  entry.message = "Test memory message";

  // 写入消息
  memory_stream->write(entry, "Test memory message");

  auto entries = memory_ptr->getEntries();
  EXPECT_EQ(entries.size(), 1);
  EXPECT_EQ(entries[0], "Test memory message");

  // 测试缓冲区限制
  for (int i = 0; i < 10; ++i) {
    memory_stream->write(entry, "Message " + std::to_string(i));
  }

  entries = memory_ptr->getEntries();
  EXPECT_LE(entries.size(), 5);  // 不应该超过最大缓冲区大小

  // 测试清空
  memory_ptr->clear();
  entries = memory_ptr->getEntries();
  EXPECT_TRUE(entries.empty());
}
