#include <google/protobuf/stubs/common.h>
#include <gtest/gtest.h>

#include "common/logging.hpp"

auto main(int argc, char** argv) -> int {
  // 为所有测试统一初始化日志系统
  logger::LogConfig config = logger::LogConfig::loadFromConfigManager();
  config.log_directory = "./logs/tests";
  config.global_level = logger::LogLevel::DEBUG;
  config.file_enabled = true;
  config.console_enabled = true;
  config.max_files = 10;
  logger::Logger::Init(argv[0], config);

  // 初始化 Google Test 框架
  testing::InitGoogleTest(&argc, argv);

  // 运行所有测试并返回结果
  const int result = RUN_ALL_TESTS();

  // 关闭 Protobuf 库，释放其分配的所有全局对象
  google::protobuf::ShutdownProtobufLibrary();

  return result;
}