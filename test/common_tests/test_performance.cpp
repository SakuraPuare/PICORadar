#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <random>
#include <thread>
#include <vector>

#include "common/config_manager.hpp"
#include "common/logging.hpp"
#include "common/process_utils.hpp"
#include "common/single_instance_guard.hpp"

using namespace picoradar::common;
using namespace std::chrono;

class PerformanceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    temp_dir_ = std::filesystem::temp_directory_path() / "picoradar_perf_test";
    std::filesystem::create_directories(temp_dir_);
  }

  void TearDown() override {
    if (std::filesystem::exists(temp_dir_)) {
      std::filesystem::remove_all(temp_dir_);
    }
  }

  std::filesystem::path temp_dir_;
};

/**
 * @brief 测试ConfigManager的读取性能
 */
TEST_F(PerformanceTest, ConfigManagerReadPerformance) {
  // 创建一个包含大量配置项的文件
  std::filesystem::path config_file = temp_dir_ / "large_config.json";
  std::ofstream file(config_file);

  file << "{\n";
  for (int i = 0; i < 1000; ++i) {
    file << "  \"key_" << i << "\": \"value_" << i << "\"";
    if (i < 999) {
      file << ",";
    }
    file << "\n";
  }
  file << "}\n";
  file.close();

  ConfigManager& config = ConfigManager::getInstance();

  // 测试加载时间
  auto start = high_resolution_clock::now();
  auto result = config.loadFromFile(config_file.string());
  auto end = high_resolution_clock::now();

  EXPECT_TRUE(result.has_value());
  auto load_duration = duration_cast<milliseconds>(end - start);
  LOG_INFO << "Config load time: " << load_duration.count() << "ms";

  // 测试读取性能
  start = high_resolution_clock::now();
  constexpr int read_iterations = 1000;
  for (int i = 0; i < read_iterations; ++i) {
    auto value = config.getString("key_" + std::to_string(i));
    EXPECT_TRUE(value.has_value());
  }
  end = high_resolution_clock::now();

  auto read_duration = duration_cast<microseconds>(end - start);
  LOG_INFO << read_iterations << " config reads time: " << read_duration.count()
           << "μs";
  EXPECT_LT(read_duration.count(),
            read_iterations * 500);  // 应该在500ms内完成（为调试版本留余量）
}

/**
 * @brief 测试ConfigManager的并发读取性能
 */
TEST_F(PerformanceTest, ConfigManagerConcurrentReadPerformance) {
  // 创建配置
  std::filesystem::path config_file = temp_dir_ / "concurrent_config.json";
  std::ofstream file(config_file);
  file << R"({
        "test_string": "test_value",
        "test_int": 42,
        "test_bool": true,
        "nested": {
            "deep_value": "deep_test",
            "numbers": [1, 2, 3, 4, 5]
        }
    })";
  file.close();

  ConfigManager& config = ConfigManager::getInstance();
  auto result = config.loadFromFile(config_file.string());
  EXPECT_TRUE(result.has_value());

  const int num_threads = 10;
  const int reads_per_thread = 1000;
  std::vector<std::thread> threads;
  std::atomic<int> total_operations{0};

  auto start = high_resolution_clock::now();

  // 启动多个线程同时读取
  threads.reserve(num_threads);
  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([&config, &total_operations, reads_per_thread]() {
      for (int j = 0; j < reads_per_thread; ++j) {
        auto str_val = config.getString("test_string");
        auto int_val = config.getInt("test_int");
        auto bool_val = config.getBool("test_bool");
        auto nested_val = config.getString("nested.deep_value");

        if (str_val.has_value() && int_val.has_value() &&
            bool_val.has_value() && nested_val.has_value()) {
          total_operations.fetch_add(1);
        }
      }
    });
  }

  // 等待所有线程完成
  for (auto& thread : threads) {
    thread.join();
  }

  auto end = high_resolution_clock::now();
  auto duration = duration_cast<milliseconds>(end - start);

  EXPECT_EQ(total_operations.load(), num_threads * reads_per_thread);
  LOG_INFO << "Concurrent reads (" << num_threads << " threads, "
           << reads_per_thread << " reads each) time: " << duration.count()
           << "ms";

  // 计算每秒操作数
  double ops_per_second = (total_operations.load() * 1000.0) / duration.count();
  LOG_INFO << "Operations per second: " << ops_per_second;
  EXPECT_GT(ops_per_second, 10000);  // 期望每秒至少10000次操作
}

/**
 * @brief 测试SingleInstanceGuard的创建和销毁性能
 */
TEST_F(PerformanceTest, SingleInstanceGuardPerformance) {
  const int num_iterations = 100;
  std::vector<duration<double, std::milli>> durations;

  for (int i = 0; i < num_iterations; ++i) {
    std::string lock_file = "perf_test_" + std::to_string(i) + ".pid";

    auto start = high_resolution_clock::now();
    {
      SingleInstanceGuard guard(lock_file);
      // 锁被持有的时间很短
    }
    auto end = high_resolution_clock::now();

    durations.push_back(
        duration_cast<duration<double, std::milli>>(end - start));

    // 清理
    remove(lock_file.c_str());
  }

  // 计算统计信息
  double total_time = 0;
  double min_time = durations[0].count();
  double max_time = durations[0].count();

  for (const auto& d : durations) {
    total_time += d.count();
    min_time = std::min(min_time, d.count());
    max_time = std::max(max_time, d.count());
  }

  double avg_time = total_time / num_iterations;

  LOG_INFO << "SingleInstanceGuard performance (" << num_iterations
           << " iterations):";
  LOG_INFO << "  Average: " << avg_time << "ms";
  LOG_INFO << "  Min: " << min_time << "ms";
  LOG_INFO << "  Max: " << max_time << "ms";

  // 性能断言
  EXPECT_LT(avg_time, 10.0);  // 平均时间应该小于10ms
  EXPECT_LT(max_time, 50.0);  // 最大时间应该小于50ms
}

/**
 * @brief 测试Process启动和终止性能
 */
TEST_F(PerformanceTest, ProcessCreationPerformance) {
  // 创建一个简单的测试脚本
  std::filesystem::path script_path = temp_dir_ / "perf_script.sh";
  std::ofstream script(script_path);
  script << "#!/bin/bash\n";
  script << "sleep 0.1\n";
  script.close();

  std::filesystem::permissions(script_path,
                               std::filesystem::perms::owner_exec |
                                   std::filesystem::perms::owner_read |
                                   std::filesystem::perms::owner_write);

  const int num_processes = 20;
  std::vector<duration<double, std::milli>> creation_times;
  std::vector<duration<double, std::milli>> termination_times;

  for (int i = 0; i < num_processes; ++i) {
    // 测试进程创建时间
    auto start = high_resolution_clock::now();
    Process process(script_path.string(), {});
    auto creation_end = high_resolution_clock::now();

    // 等待进程启动
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_TRUE(process.isRunning());

    // 测试进程终止时间
    auto term_start = high_resolution_clock::now();
    bool terminated = process.terminate();
    auto term_end = high_resolution_clock::now();

    EXPECT_TRUE(terminated);

    creation_times.push_back(
        duration_cast<duration<double, std::milli>>(creation_end - start));
    termination_times.push_back(
        duration_cast<duration<double, std::milli>>(term_end - term_start));
  }

  // 计算创建时间统计
  double avg_creation = 0;
  for (const auto& d : creation_times) {
    avg_creation += d.count();
  }
  avg_creation /= num_processes;

  // 计算终止时间统计
  double avg_termination = 0;
  for (const auto& d : termination_times) {
    avg_termination += d.count();
  }
  avg_termination /= num_processes;

  LOG_INFO << "Process performance (" << num_processes << " processes):";
  LOG_INFO << "  Average creation time: " << avg_creation << "ms";
  LOG_INFO << "  Average termination time: " << avg_termination << "ms";

  // 性能断言
  EXPECT_LT(avg_creation, 100.0);    // 平均创建时间应该小于100ms
  EXPECT_LT(avg_termination, 50.0);  // 平均终止时间应该小于50ms
}

/**
 * @brief 测试is_process_running函数的性能
 */
TEST_F(PerformanceTest, IsProcessRunningPerformance) {
  const int num_checks = 10000;
  ProcessId current_pid = getpid();
  ProcessId invalid_pid = 999999;

  // 测试有效PID检查性能
  auto start = high_resolution_clock::now();
  for (int i = 0; i < num_checks; ++i) {
    bool running = is_process_running(current_pid);
    EXPECT_TRUE(running);
  }
  auto end = high_resolution_clock::now();

  auto valid_duration = duration_cast<microseconds>(end - start);
  LOG_INFO << "Valid PID checks (" << num_checks
           << "): " << valid_duration.count() << "μs";

  // 测试无效PID检查性能
  start = high_resolution_clock::now();
  for (int i = 0; i < num_checks; ++i) {
    bool running = is_process_running(invalid_pid);
    EXPECT_FALSE(running);
  }
  end = high_resolution_clock::now();

  auto invalid_duration = duration_cast<microseconds>(end - start);
  LOG_INFO << "Invalid PID checks (" << num_checks
           << "): " << invalid_duration.count() << "μs";

  // 性能断言 - 每次检查应该很快
  double avg_valid = valid_duration.count() / static_cast<double>(num_checks);
  double avg_invalid =
      invalid_duration.count() / static_cast<double>(num_checks);

  LOG_INFO << "Average time per valid PID check: " << avg_valid << "μs";
  LOG_INFO << "Average time per invalid PID check: " << avg_invalid << "μs";

  EXPECT_LT(avg_valid, 10.0);  // 每次检查应该小于10微秒
  EXPECT_LT(avg_invalid, 10.0);
}

/**
 * @brief 压力测试：同时使用所有组件
 */
TEST_F(PerformanceTest, IntegratedStressTest) {
  // 创建配置文件
  std::filesystem::path config_file = temp_dir_ / "stress_config.json";
  std::ofstream file(config_file);
  file << R"({
        "stress_test": true,
        "thread_count": 5,
        "operation_count": 100
    })";
  file.close();

  ConfigManager& config = ConfigManager::getInstance();
  auto result = config.loadFromFile(config_file.string());
  EXPECT_TRUE(result.has_value());

  const int num_threads = config.getWithDefault("thread_count", 5);
  const int operations = config.getWithDefault("operation_count", 100);

  std::vector<std::thread> threads;
  std::atomic<int> successful_operations{0};
  std::atomic<int> failed_operations{0};

  auto start = high_resolution_clock::now();

  threads.reserve(num_threads);
  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([&, i]() {
      for (int j = 0; j < operations; ++j) {
        try {
          // 配置读取操作
          auto stress_flag = config.getBool("stress_test");
          if (!stress_flag.has_value() || !stress_flag.value()) {
            failed_operations.fetch_add(1);
            continue;
          }

          // 锁操作
          std::string lock_name =
              "stress_lock_" + std::to_string(i) + "_" + std::to_string(j);
          {
            SingleInstanceGuard guard(lock_name);
            std::this_thread::sleep_for(std::chrono::microseconds(100));
          }

          // PID检查操作
          ProcessId current = getpid();
          if (!is_process_running(current)) {
            failed_operations.fetch_add(1);
            continue;
          }

          successful_operations.fetch_add(1);

        } catch (const std::exception& e) {
          failed_operations.fetch_add(1);
        }
      }
    });
  }

  // 等待所有线程完成
  for (auto& thread : threads) {
    thread.join();
  }

  auto end = high_resolution_clock::now();
  auto duration = duration_cast<milliseconds>(end - start);

  int total_expected = num_threads * operations;
  int total_actual = successful_operations.load() + failed_operations.load();

  LOG_INFO << "Integrated stress test results:";
  LOG_INFO << "  Total expected operations: " << total_expected;
  LOG_INFO << "  Successful operations: " << successful_operations.load();
  LOG_INFO << "  Failed operations: " << failed_operations.load();
  LOG_INFO << "  Total time: " << duration.count() << "ms";
  LOG_INFO << "  Operations per second: "
           << (successful_operations.load() * 1000.0) / duration.count();

  EXPECT_EQ(total_actual, total_expected);
  EXPECT_GT(successful_operations.load(),
            total_expected * 0.95);    // 至少95%成功率
  EXPECT_LT(duration.count(), 10000);  // 应该在10秒内完成
}
