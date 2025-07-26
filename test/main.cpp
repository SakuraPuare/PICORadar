#include <gtest/gtest.h>
#include <google/protobuf/stubs/common.h>

#include "common/logging.hpp"

auto main(int argc, char** argv) -> int {
  // 为所有测试统一初始化日志系统
  // 日志将被重定向到 ./logs/tests 目录下的文件
  logger::Logger::Init(argv[0], "./logs/tests", logger::LogLevel::DEBUG, 10, true);

  // 初始化 Google Test 框架
  ::testing::InitGoogleTest(&argc, argv);

  // 运行所有测试并返回结果
  const int result = RUN_ALL_TESTS();

  // 关闭 Protobuf 库，释放其分配的所有全局对象
  google::protobuf::ShutdownProtobufLibrary();

  return result;
}