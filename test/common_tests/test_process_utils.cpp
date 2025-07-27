#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

#include "common/logging.hpp"
#include "common/process_utils.hpp"

using namespace picoradar::common;

class ProcessUtilsTest : public testing::Test {
 protected:
  void SetUp() override {
    // 创建测试可执行文件路径
    temp_dir_ =
        std::filesystem::temp_directory_path() / "picoradar_process_test";
    std::filesystem::create_directories(temp_dir_);

    // 创建一个简单的测试脚本
    test_script_path_ = temp_dir_ / "test_script.sh";
    createTestScript();
  }

  void TearDown() override {
    // 清理临时文件
    if (std::filesystem::exists(temp_dir_)) {
      std::filesystem::remove_all(temp_dir_);
    }
  }

  void createTestScript() const {
    std::ofstream script(test_script_path_);
    script << "#!/bin/bash\n";
    script << "if [ \"$1\" = \"--sleep\" ]; then\n";
    script << "    sleep \"$2\"\n";
    script << "elif [ \"$1\" = \"--exit\" ]; then\n";
    script << "    exit \"$2\"\n";
    script << "elif [ \"$1\" = \"--echo\" ]; then\n";
    script << "    echo \"$2\"\n";
    script << "else\n";
    script << "    echo \"Test script running\"\n";
    script << "fi\n";
    script.close();

    // 使脚本可执行
    std::filesystem::permissions(test_script_path_,
                                 std::filesystem::perms::owner_all |
                                     std::filesystem::perms::group_read |
                                     std::filesystem::perms::group_exec |
                                     std::filesystem::perms::others_read |
                                     std::filesystem::perms::others_exec,
                                 std::filesystem::perm_options::replace);
  }

  std::filesystem::path temp_dir_;
  std::filesystem::path test_script_path_;
};

/**
 * @brief 测试进程的基本启动和运行检查
 */
TEST_F(ProcessUtilsTest, BasicProcessStartAndRunning) {
  // 启动一个短暂运行的进程
  Process process(test_script_path_.string(), {"--sleep", "1"});

  // 进程应该正在运行
  EXPECT_TRUE(process.isRunning());

  // 等待进程结束
  const auto exit_code = process.waitForExit();
  EXPECT_TRUE(exit_code.has_value());
  EXPECT_EQ(exit_code.value(), 0);

  // 进程应该已经停止
  EXPECT_FALSE(process.isRunning());
}

/**
 * @brief 测试进程终止功能
 */
TEST_F(ProcessUtilsTest, ProcessTermination) {
  // 启动一个长时间运行的进程
  Process process(test_script_path_.string(), {"--sleep", "10"});

  // 进程应该正在运行
  EXPECT_TRUE(process.isRunning());

  // 终止进程
  EXPECT_TRUE(process.terminate());

  // 等待一小段时间让进程终止
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // 进程应该已经停止
  EXPECT_FALSE(process.isRunning());
}

/**
 * @brief 测试进程的退出码
 */
TEST_F(ProcessUtilsTest, ProcessExitCodes) {
  // 测试不同的退出码
  std::vector<int> exit_codes = {0, 1, 42, 127};

  for (int expected_exit_code : exit_codes) {
    Process process(test_script_path_.string(),
                    {"--exit", std::to_string(expected_exit_code)});

    auto exit_code = process.waitForExit();
    EXPECT_TRUE(exit_code.has_value());
    EXPECT_EQ(exit_code.value(), expected_exit_code);
    EXPECT_FALSE(process.isRunning());
  }
}

/**
 * @brief 测试进程参数传递
 */
TEST_F(ProcessUtilsTest, ProcessArguments) {
  // 测试无参数
  {
    Process process(test_script_path_.string(), {});
    auto exit_code = process.waitForExit();
    EXPECT_TRUE(exit_code.has_value());
    EXPECT_EQ(exit_code.value(), 0);
  }

  // 测试单个参数
  {
    Process process(test_script_path_.string(), {"--exit", "5"});
    auto exit_code = process.waitForExit();
    EXPECT_TRUE(exit_code.has_value());
    EXPECT_EQ(exit_code.value(), 5);
  }

  // 测试多个参数
  {
    Process process(test_script_path_.string(), {"--echo", "hello world"});
    auto exit_code = process.waitForExit();
    EXPECT_TRUE(exit_code.has_value());
    EXPECT_EQ(exit_code.value(), 0);
  }
}

/**
 * @brief 测试多个进程同时运行
 */
TEST_F(ProcessUtilsTest, MultipleProcesses) {
  std::vector<std::unique_ptr<Process>> processes;

  // 启动多个进程
  for (int i = 0; i < 5; ++i) {
    processes.push_back(std::make_unique<Process>(
        test_script_path_.string(), std::vector<std::string>{"--sleep", "1"}));
  }

  // 所有进程都应该在运行
  for (const auto& process : processes) {
    EXPECT_TRUE(process->isRunning());
  }

  // 等待所有进程完成
  for (auto& process : processes) {
    auto exit_code = process->waitForExit();
    EXPECT_TRUE(exit_code.has_value());
    EXPECT_EQ(exit_code.value(), 0);
  }
}

/**
 * @brief 压力测试：创建和销毁大量进程
 */
TEST_F(ProcessUtilsTest, StressTestProcessCreation) {
  constexpr int num_processes = 20;
  std::vector<std::unique_ptr<Process>> processes;

  auto start_time = std::chrono::steady_clock::now();

  // 创建大量短时间运行的进程
  for (int i = 0; i < num_processes; ++i) {
    try {
      processes.push_back(std::make_unique<Process>(
          test_script_path_.string(), std::vector<std::string>{"--exit", "0"}));
    } catch (const std::exception& e) {
      // 如果系统资源不足，记录但继续测试
      std::cout << "Failed to create process " << i << ": " << e.what()
                << std::endl;
    }
  }

  // 等待所有进程完成
  int completed_processes = 0;
  for (auto& process : processes) {
    if (process) {
      auto exit_code = process->waitForExit();
      if (exit_code.has_value()) {
        completed_processes++;
      }
    }
  }

  auto end_time = std::chrono::steady_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      end_time - start_time);

  // 大部分进程应该成功创建和完成
  EXPECT_GT(completed_processes, num_processes / 2);

  // 应该在合理时间内完成
  EXPECT_LT(duration.count(), 10000);  // 10秒内完成

  std::cout << "Created and completed " << completed_processes << "/"
            << num_processes << " processes in " << duration.count() << " ms"
            << std::endl;
}

/**
 * @brief 测试进程的重复终止调用
 */
TEST_F(ProcessUtilsTest, RepeatedTermination) {
  // 启动一个长时间运行的进程
  Process process(test_script_path_.string(), {"--sleep", "10"});

  // 确认进程正在运行
  EXPECT_TRUE(process.isRunning());

  // 手动终止进程
  const bool terminated = process.terminate();
  EXPECT_TRUE(terminated);

  // 等待终止完成
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // 进程应该已经停止
  EXPECT_FALSE(process.isRunning());

  // 再次尝试终止已停止的进程应该返回true
  const bool terminated_again = process.terminate();
  EXPECT_TRUE(terminated_again);
}
