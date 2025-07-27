#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "common/logging.hpp"
#include "common/single_instance_guard.hpp"

using picoradar::common::SingleInstanceGuard;

constexpr auto DEFAULT_LOCK_FILE_NAME = "pico_radar_test.pid";

/**
 * @brief 解析命令行参数中的锁文件名
 * @param args 命令行参数向量
 * @return 锁文件名，如果未指定则返回默认值
 */
auto parse_lock_file_arg(const std::vector<std::string>& args) -> std::string {
  for (const auto& arg : args) {
    if (arg.length() >= 7 && arg.substr(0, 7) == "--file=") {
      return arg.substr(7);  // 移除 "--file=" 前缀
    }
  }
  return DEFAULT_LOCK_FILE_NAME;
}

/**
 * @brief 模式1: 尝试获取锁并永久持有
 * 用于测试主要的锁持有场景
 */
void run_lock_mode(const std::string& lock_file) {
  try {
    SingleInstanceGuard guard(lock_file);
    LOG_INFO << "LOCK_ACQUIRED for file: " << lock_file;

    // 模拟一个正在运行的服务器
    while (true) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  } catch (const std::runtime_error& e) {
    LOG_ERROR << "LOCK_FAILED: " << e.what();
    exit(1);
  }
}

/**
 * @brief 模式2: 检查是否能获取锁（预期失败）
 * 用于验证锁竞争检测功能
 */
void run_check_mode(const std::string& lock_file) {
  try {
    SingleInstanceGuard guard(lock_file);
    // 如果我们能获取锁，说明测试失败了
    LOG_ERROR << "CHECK_FAILED: Successfully acquired lock when it should "
                 "have failed for file: "
              << lock_file;
    exit(1);
  } catch (const std::runtime_error& e) {
    // 成功捕获到异常，说明测试通过
    LOG_INFO << "CHECK_SUCCESS: Correctly failed to acquire lock for file: "
             << lock_file << " - " << e.what();
    exit(0);
  }
}

/**
 * @brief 模式3: 快速退出模式
 * 获取锁后短暂持有然后退出，用于测试锁的快速释放
 */
void run_quick_exit_mode(const std::string& lock_file) {
  try {
    SingleInstanceGuard guard(lock_file);
    LOG_INFO << "QUICK_EXIT: Lock acquired for file: " << lock_file
             << ", exiting in 100ms";
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    LOG_INFO << "QUICK_EXIT: Exiting now";
    exit(0);
  } catch (const std::runtime_error& e) {
    LOG_ERROR << "QUICK_EXIT_FAILED: " << e.what();
    exit(1);
  }
}

/**
 * @brief 模式4: 立即退出模式
 * 获取锁后立即退出，用于测试锁的RAII行为
 */
void run_exit_quickly_mode(const std::string& lock_file) {
  try {
    SingleInstanceGuard guard(lock_file);
    LOG_INFO << "EXIT_QUICKLY: Lock acquired for file: " << lock_file
             << ", exiting immediately";
    exit(0);
  } catch (const std::runtime_error& e) {
    LOG_ERROR << "EXIT_QUICKLY_FAILED: " << e.what();
    exit(1);
  }
}

/**
 * @brief 模式5: 测试模式
 * 获取锁，短暂持有，然后正常退出（用于性能测试）
 */
void run_test_mode(const std::string& lock_file) {
  try {
    SingleInstanceGuard guard(lock_file);
    LOG_INFO << "TEST_MODE: Lock acquired for file: " << lock_file;

    // 模拟一些工作
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    LOG_INFO << "TEST_MODE: Work completed, exiting normally";
    exit(0);
  } catch (const std::runtime_error& e) {
    LOG_ERROR << "TEST_MODE_FAILED: " << e.what();
    exit(1);
  }
}

/**
 * @brief 显示使用帮助
 */
void show_usage(const char* program_name) {
  std::cout << "Usage: " << program_name << " <mode> [--file=<lock_file>]\n\n";
  std::cout << "Modes:\n";
  std::cout << "  --lock        : Acquire lock and hold indefinitely\n";
  std::cout << "  --check       : Try to acquire lock (expect failure)\n";
  std::cout << "  --quick_exit  : Acquire lock, hold briefly, then exit\n";
  std::cout << "  --exit_quickly: Acquire lock and exit immediately\n";
  std::cout
      << "  --test        : Acquire lock, do minimal work, exit normally\n";
  std::cout << "  --help        : Show this help message\n\n";
  std::cout << "Options:\n";
  std::cout << "  --file=<name> : Use custom lock file name (default: "
            << DEFAULT_LOCK_FILE_NAME << ")\n\n";
  std::cout << "Examples:\n";
  std::cout << "  " << program_name << " --lock\n";
  std::cout << "  " << program_name << " --check --file=custom.pid\n";
  std::cout << "  " << program_name << " --test --file=test_lock.pid\n";
}

auto main(const int argc, char** argv) -> int {
  // 初始化日志系统
  logger::LogConfig config = logger::LogConfig::loadFromConfigManager();
  config.console_enabled = true;
  config.console_colored = true;
  config.file_enabled = false;
  logger::Logger::Init(argv[0], config);

  if (argc < 2) {
    LOG_ERROR << "Error: Missing required mode argument";
    show_usage(argv[0]);
    return 1;
  }

  // 将参数转换为vector以便处理
  std::vector<std::string> args;
  for (int i = 1; i < argc; ++i) {
    args.emplace_back(argv[i]);
  }

  const std::string mode = args[0];
  const std::string lock_file = parse_lock_file_arg(args);

  LOG_INFO << "Starting with mode: " << mode << ", lock file: " << lock_file;

  // 根据模式执行相应的操作
  if (mode == "--lock") {
    run_lock_mode(lock_file);
  } else if (mode == "--check") {
    run_check_mode(lock_file);
  } else if (mode == "--quick_exit") {
    run_quick_exit_mode(lock_file);
  } else if (mode == "--exit_quickly") {
    run_exit_quickly_mode(lock_file);
  } else if (mode == "--test") {
    run_test_mode(lock_file);
  } else if (mode == "--help" || mode == "-h") {
    show_usage(argv[0]);
    return 0;
  } else {
    LOG_ERROR << "Error: Unknown mode '" << mode << "'";
    show_usage(argv[0]);
    return 1;
  }

  return 0;
}
