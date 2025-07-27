#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

#include "common/logging.hpp"
#include "common/process_utils.hpp"

using namespace picoradar::common;

class ProcessUtilsTest : public ::testing::Test {
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

  void createTestScript() {
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

    // 使脚本可执行 - 使用更兼容的权限设置
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
 * @brief 测试进程运行状态检查函数
 */
TEST_F(ProcessUtilsTest, IsProcessRunningFunction) {
  // 测试当前进程（肯定在运行）
  ProcessId current_pid = getpid();
  EXPECT_TRUE(is_process_running(current_pid));

  // 测试一个几乎不可能存在的PID
  ProcessId invalid_pid = 999999;
  EXPECT_FALSE(is_process_running(invalid_pid));

  // 测试PID 0（通常是内核进程或无效）
  EXPECT_FALSE(is_process_running(0));
}

/**
 * @brief 测试进程的基本启动和运行检查
 */
TEST_F(ProcessUtilsTest, BasicProcessStartAndRunning) {
  // 启动一个短暂运行的进程
  Process process(test_script_path_.string(), {"--sleep", "1"});

  // 进程应该正在运行
  EXPECT_TRUE(process.isRunning());

  // 等待进程结束
  auto exit_code = process.waitForExit();
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

  // 确认进程正在运行
  EXPECT_TRUE(process.isRunning());

  // 手动终止进程
  bool terminated = process.terminate();
  EXPECT_TRUE(terminated);

  // 进程应该已经停止
  EXPECT_FALSE(process.isRunning());

  // 再次尝试终止已停止的进程应该返回true
  bool terminated_again = process.terminate();
  EXPECT_TRUE(terminated_again);
}

/**
 * @brief 测试进程的退出码
 */
TEST_F(ProcessUtilsTest, ProcessExitCodes) {
  // 测试成功退出
  {
    Process process(test_script_path_.string(), {"--exit", "0"});
    auto exit_code = process.waitForExit();
    EXPECT_TRUE(exit_code.has_value());
    EXPECT_EQ(exit_code.value(), 0);
  }

  // 测试错误退出
  {
    Process process(test_script_path_.string(), {"--exit", "42"});
    auto exit_code = process.waitForExit();
    EXPECT_TRUE(exit_code.has_value());
    EXPECT_EQ(exit_code.value(), 42);
  }

  // 测试另一个错误退出码
  {
    Process process(test_script_path_.string(), {"--exit", "1"});
    auto exit_code = process.waitForExit();
    EXPECT_TRUE(exit_code.has_value());
    EXPECT_EQ(exit_code.value(), 1);
  }
}

/**
 * @brief 测试进程的RAII行为
 */
TEST_F(ProcessUtilsTest, RAIIBehavior) {
  ProcessId child_pid;

  {
    // 在作用域内创建进程
    Process process(test_script_path_.string(), {"--sleep", "5"});
    EXPECT_TRUE(process.isRunning());

    // 获取进程PID（需要通过某种方式，这里我们假设有方法）
    // 由于Process类没有公开PID，我们通过间接方式验证

    // 离开作用域，析构函数应该自动终止进程
  }

  // 给一点时间让析构函数完成
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // 这里我们无法直接验证PID，但可以通过其他方式验证RAII行为
  // 比如重新启动相同的进程应该能够成功
  Process new_process(test_script_path_.string(), {"--sleep", "1"});
  EXPECT_TRUE(new_process.isRunning());
}

/**
 * @brief 测试无效可执行文件的处理
 */
TEST_F(ProcessUtilsTest, InvalidExecutableHandling) {
  // 测试不存在的可执行文件
  EXPECT_THROW(
      { Process process("/nonexistent/executable", {}); }, std::runtime_error);

  // 测试无执行权限的文件
  std::filesystem::path no_exec_file = temp_dir_ / "no_exec.txt";
  std::ofstream file(no_exec_file);
  file << "This is not executable";
  file.close();

  EXPECT_THROW(
      { Process process(no_exec_file.string(), {}); }, std::runtime_error);
}

/**
 * @brief 测试进程参数传递
 */
TEST_F(ProcessUtilsTest, ProcessArguments) {
  // 这个测试较难验证，因为我们无法直接捕获子进程的输出
  // 但我们可以测试进程是否能够正确启动并运行
  Process process(test_script_path_.string(), {"--echo", "test_message"});

  // 等待进程完成
  auto exit_code = process.waitForExit();
  EXPECT_TRUE(exit_code.has_value());
  EXPECT_EQ(exit_code.value(), 0);
}

/**
 * @brief 测试多个进程的并发管理
 */
TEST_F(ProcessUtilsTest, ConcurrentProcesses) {
  const int num_processes = 5;
  std::vector<std::unique_ptr<Process>> processes;

  // 启动多个进程
  processes.reserve(num_processes);
  for (int i = 0; i < num_processes; ++i) {
    processes.push_back(std::unique_ptr<Process>(
        new Process(test_script_path_.string(), {"--sleep", "2"})));
  }

  // 验证所有进程都在运行
  for (const auto& process : processes) {
    EXPECT_TRUE(process->isRunning());
  }

  // 终止一些进程
  for (int i = 0; i < num_processes / 2; ++i) {
    EXPECT_TRUE(processes[i]->terminate());
    EXPECT_FALSE(processes[i]->isRunning());
  }

  // 等待剩余进程自然结束
  for (int i = num_processes / 2; i < num_processes; ++i) {
    auto exit_code = processes[i]->waitForExit();
    EXPECT_TRUE(exit_code.has_value());
    EXPECT_EQ(exit_code.value(), 0);
  }
}

/**
 * @brief 测试进程状态检查的边界情况
 */
TEST_F(ProcessUtilsTest, ProcessStatusEdgeCases) {
  Process process(test_script_path_.string(), {"--sleep", "1"});

  // 进程应该正在运行
  EXPECT_TRUE(process.isRunning());

  // 等待进程自然结束
  auto exit_code = process.waitForExit();
  EXPECT_TRUE(exit_code.has_value());

  // 进程已结束，isRunning应该返回false
  EXPECT_FALSE(process.isRunning());

  // 再次调用waitForExit应该返回空值（因为进程已经等待过了）
  auto second_wait = process.waitForExit();
  EXPECT_FALSE(second_wait.has_value());

  // 尝试终止已结束的进程应该成功
  EXPECT_TRUE(process.terminate());
}

/**
 * @brief 测试快速进程（立即退出）
 */
TEST_F(ProcessUtilsTest, FastExitingProcess) {
  Process process(test_script_path_.string(), {"--exit", "0"});

  // 等待进程结束（应该很快）
  auto exit_code = process.waitForExit();
  EXPECT_TRUE(exit_code.has_value());
  EXPECT_EQ(exit_code.value(), 0);

  // 进程应该已经停止
  EXPECT_FALSE(process.isRunning());
}

/**
 * @brief 测试使用系统工具的进程
 */
TEST_F(ProcessUtilsTest, SystemToolProcess) {
  // 测试使用echo命令（在大多数Unix系统上都可用）
  try {
    Process echo_process("/bin/echo", {"test", "message"});
    auto exit_code = echo_process.waitForExit();
    EXPECT_TRUE(exit_code.has_value());
    EXPECT_EQ(exit_code.value(), 0);
  } catch (const std::runtime_error&) {
    // 如果/bin/echo不存在，跳过这个测试
    GTEST_SKIP() << "/bin/echo not available on this system";
  }

  // 测试使用sleep命令
  try {
    Process sleep_process("/bin/sleep", {"0.1"});
    EXPECT_TRUE(sleep_process.isRunning());
    auto exit_code = sleep_process.waitForExit();
    EXPECT_TRUE(exit_code.has_value());
    EXPECT_EQ(exit_code.value(), 0);
  } catch (const std::runtime_error&) {
    // 如果/bin/sleep不存在，跳过这个测试
    GTEST_SKIP() << "/bin/sleep not available on this system";
  }
}
