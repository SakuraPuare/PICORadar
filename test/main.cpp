#include <gtest/gtest.h>
#include "logging.hpp"

auto main(int argc, char** argv) -> int {
    // 为所有测试统一初始化日志系统
    // 日志将被重定向到 ./logs/tests 目录下的文件
    picoradar::common::setup_logging(argv[0], true, "./logs/tests");

    // 初始化 Google Test 框架
    ::testing::InitGoogleTest(&argc, argv);

    // 运行所有测试并返回结果
    return RUN_ALL_TESTS();
} 