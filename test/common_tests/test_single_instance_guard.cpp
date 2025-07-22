#include <gtest/gtest.h>
#include "process_utils.hpp"
#include <fstream>
#include <thread>
#include <chrono>

using namespace picoradar;

// 定义测试常量
const std::string LOCK_FILE_NAME = "pico_radar_test.pid";
// 这个路径需要与 CMakeLists.txt 中定义的目标名称匹配
const std::string LOCK_HOLDER_EXE = "./common_tests_locker"; 

class SingleInstanceGuardTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 每个测试前都清理环境
        remove(LOCK_FILE_NAME.c_str());
    }

    void TearDown() override {
        remove(LOCK_FILE_NAME.c_str());
    }
};

TEST_F(SingleInstanceGuardTest, StaleLockShouldBeRemoved) {
    // Arrange: 创建一个包含无效PID的陈旧锁文件
    {
        std::ofstream lock_file(LOCK_FILE_NAME);
        ASSERT_TRUE(lock_file.is_open());
        lock_file << "999999"; // 一个几乎不可能存在的PID
    }

    // Act: 启动一个进程，它应该能够识别并移除陈旧锁，然后成功获取锁
    common::Process locker_process(LOCK_HOLDER_EXE, {"--lock"});
    
    // 等待一小段时间让进程启动
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Assert: 验证进程是否仍在运行
    EXPECT_TRUE(locker_process.isRunning()) << "进程在遇到陈旧锁后未能成功启动。";
    
    // Cleanup 由 locker_process 的析构函数自动完成
}

TEST_F(SingleInstanceGuardTest, SecondInstanceShouldFail) {
    // Arrange: 启动第一个实例（锁定者）
    common::Process locker_process(LOCK_HOLDER_EXE, {"--lock"});
    ASSERT_TRUE(locker_process.isRunning()) << "第一个（锁定者）进程未能启动。";
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Act: 启动第二个实例，它应该会因为锁已被持有而失败
    common::Process second_instance(LOCK_HOLDER_EXE, {"--check"});
    auto exit_code = second_instance.waitForExit();

    // Assert: 验证第二个实例的退出码
    ASSERT_TRUE(exit_code.has_value()) << "未能获取第二个实例的退出码。";
    EXPECT_EQ(exit_code.value(), 0) << "第二个实例在锁被持有时，应以退出码0（表示预期的失败）退出。";
    
    // 验证第一个实例仍然在运行
    EXPECT_TRUE(locker_process.isRunning()) << "第一个（锁定者）进程在第二个实例运行后意外退出了。";
} 