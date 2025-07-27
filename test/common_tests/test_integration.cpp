#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <thread>
#include <vector>

#include "common/config_manager.hpp"
#include "common/logging.hpp"
#include "common/process_utils.hpp"
#include "common/single_instance_guard.hpp"

using namespace picoradar::common;

class IntegrationTest : public testing::Test {
 protected:
  void SetUp() override {
    temp_dir_ =
        std::filesystem::temp_directory_path() / "picoradar_integration_test";
    std::filesystem::create_directories(temp_dir_);

    // 创建测试配置文件
    config_file_ = temp_dir_ / "integration_config.json";
    createIntegrationConfig();

    // 创建测试脚本
    test_script_ = temp_dir_ / "integration_script.sh";
    createIntegrationScript();
  }

  void TearDown() override {
    if (std::filesystem::exists(temp_dir_)) {
      std::filesystem::remove_all(temp_dir_);
    }
  }

  void createIntegrationConfig() const {
    std::ofstream file(config_file_);
    file << R"({
            "application": {
                "name": "PICORadar",
                "version": "1.0.0",
                "instance_lock": "picoradar_integration.pid"
            },
            "server": {
                "port": 8080,
                "host": "localhost",
                "max_connections": 100
            },
            "processes": {
                "worker_count": 4,
                "timeout_seconds": 30,
                "restart_on_failure": true
            },
            "logging": {
                "level": "INFO",
                "file": "integration.log"
            }
        })";
    file.close();
  }

  void createIntegrationScript() const {
    std::ofstream script(test_script_);
    script << "#!/bin/bash\n";
    script << "case $1 in\n";
    script << "  --config-test)\n";
    script << "    echo \"Config test mode\"\n";
    script << "    sleep 2\n";
    script << "    exit 0\n";
    script << "    ;;\n";
    script << "  --lock-test)\n";
    script << "    echo \"Lock test mode\"\n";
    script << "    sleep 5\n";
    script << "    exit 0\n";
    script << "    ;;\n";
    script << "  --quick)\n";
    script << "    echo \"Quick mode\"\n";
    script << "    exit 0\n";
    script << "    ;;\n";
    script << "  --fail)\n";
    script << "    echo \"Failure mode\"\n";
    script << "    exit 1\n";
    script << "    ;;\n";
    script << "  *)\n";
    script << "    echo \"Default mode\"\n";
    script << "    sleep 1\n";
    script << "    exit 0\n";
    script << "    ;;\n";
    script << "esac\n";
    script.close();

    std::filesystem::permissions(test_script_,
                                 std::filesystem::perms::owner_exec |
                                     std::filesystem::perms::owner_read |
                                     std::filesystem::perms::owner_write);
  }

  std::filesystem::path temp_dir_;
  std::filesystem::path config_file_;
  std::filesystem::path test_script_;
};

/**
 * @brief 测试配置驱动的应用程序实例管理
 */
TEST_F(IntegrationTest, ConfigDrivenInstanceManagement) {
  ConfigManager& config = ConfigManager::getInstance();
  auto load_result = config.loadFromFile(config_file_.string());
  ASSERT_TRUE(load_result.has_value());

  // 从配置中获取锁文件名
  auto lock_file = config.getString("application.instance_lock");
  ASSERT_TRUE(lock_file.has_value());

  // 测试第一个实例能够成功获取锁
  std::unique_ptr<SingleInstanceGuard> guard;
  EXPECT_NO_THROW(
      { guard = std::make_unique<SingleInstanceGuard>(lock_file.value()); });

  // 验证配置中的其他值
  auto app_name = config.getString("application.name");
  EXPECT_TRUE(app_name.has_value());
  EXPECT_EQ(app_name.value(), "PICORadar");

  auto server_port = config.getInt("server.port");
  EXPECT_TRUE(server_port.has_value());
  EXPECT_EQ(server_port.value(), 8080);

  // 尝试创建第二个实例应该失败
  EXPECT_THROW(
      { SingleInstanceGuard second_guard(lock_file.value()); },
      std::runtime_error);

  // 释放锁
  guard.reset();

  // 现在应该能够创建新实例
  EXPECT_NO_THROW(
      { guard = std::make_unique<SingleInstanceGuard>(lock_file.value()); });
}

/**
 * @brief 测试配置驱动的进程管理
 */
TEST_F(IntegrationTest, ConfigDrivenProcessManagement) {
  ConfigManager& config = ConfigManager::getInstance();
  auto load_result = config.loadFromFile(config_file_.string());
  ASSERT_TRUE(load_result.has_value());

  // 从配置中获取工作进程数量
  auto worker_count = config.getInt("processes.worker_count");
  ASSERT_TRUE(worker_count.has_value());

  auto timeout_seconds = config.getInt("processes.timeout_seconds");
  ASSERT_TRUE(timeout_seconds.has_value());

  // 创建配置指定数量的工作进程
  std::vector<std::unique_ptr<Process>> workers;
  workers.reserve(worker_count.value());
  for (int i = 0; i < worker_count.value(); ++i) {
    workers.push_back(std::unique_ptr<Process>(
        new Process(test_script_.string(), {"--config-test"})));
  }

  // 验证所有进程都在运行
  for (const auto& worker : workers) {
    EXPECT_TRUE(worker->isRunning());
  }

  // 等待所有进程完成
  for (const auto& worker : workers) {
    auto exit_code = worker->waitForExit();
    EXPECT_TRUE(exit_code.has_value());
    EXPECT_EQ(exit_code.value(), 0);
  }
}

/**
 * @brief 测试组合场景：单实例应用程序启动多个工作进程
 */
TEST_F(IntegrationTest, SingleInstanceMultipleWorkers) {
  ConfigManager& config = ConfigManager::getInstance();
  auto load_result = config.loadFromFile(config_file_.string());
  ASSERT_TRUE(load_result.has_value());

  auto lock_file = config.getString("application.instance_lock");
  auto worker_count = config.getInt("processes.worker_count");
  ASSERT_TRUE(lock_file.has_value());
  ASSERT_TRUE(worker_count.has_value());

  // 主应用程序获取单实例锁
  SingleInstanceGuard app_guard(lock_file.value());

  // 启动工作进程
  std::vector<std::unique_ptr<Process>> workers;
  for (int i = 0; i < worker_count.value(); ++i) {
    workers.push_back(std::unique_ptr<Process>(
        new Process(test_script_.string(), {"--lock-test"})));
    EXPECT_TRUE(workers.back()->isRunning());
  }

  // 验证无法启动第二个主应用程序实例
  EXPECT_THROW(
      { SingleInstanceGuard duplicate_guard(lock_file.value()); },
      std::runtime_error);

  // 但可以检查工作进程的状态
  for (const auto& worker : workers) {
    EXPECT_TRUE(worker->isRunning());
  }

  // 终止一些工作进程
  for (size_t i = 0; i < workers.size() / 2; ++i) {
    EXPECT_TRUE(workers[i]->terminate());
  }

  // 等待剩余工作进程完成
  for (size_t i = workers.size() / 2; i < workers.size(); ++i) {
    auto exit_code = workers[i]->waitForExit();
    EXPECT_TRUE(exit_code.has_value());
    EXPECT_EQ(exit_code.value(), 0);
  }
}

/**
 * @brief 测试配置热重载场景
 */
TEST_F(IntegrationTest, ConfigurationReloading) {
  ConfigManager& config = ConfigManager::getInstance();

  // 加载初始配置
  auto load_result = config.loadFromFile(config_file_.string());
  ASSERT_TRUE(load_result.has_value());

  auto initial_port = config.getInt("server.port");
  EXPECT_TRUE(initial_port.has_value());
  EXPECT_EQ(initial_port.value(), 8080);

  // 修改配置文件
  std::ofstream file(config_file_);
  file << R"({
        "application": {
            "name": "PICORadar",
            "version": "1.0.1",
            "instance_lock": "picoradar_integration.pid"
        },
        "server": {
            "port": 9090,
            "host": "0.0.0.0",
            "max_connections": 200
        },
        "processes": {
            "worker_count": 8,
            "timeout_seconds": 60,
            "restart_on_failure": false
        }
    })";
  file.close();

  // 重新加载配置
  auto reload_result = config.loadFromFile(config_file_.string());
  ASSERT_TRUE(reload_result.has_value());

  // 验证配置已更新
  auto new_port = config.getInt("server.port");
  EXPECT_TRUE(new_port.has_value());
  EXPECT_EQ(new_port.value(), 9090);

  auto new_worker_count = config.getInt("processes.worker_count");
  EXPECT_TRUE(new_worker_count.has_value());
  EXPECT_EQ(new_worker_count.value(), 8);

  auto new_version = config.getString("application.version");
  EXPECT_TRUE(new_version.has_value());
  EXPECT_EQ(new_version.value(), "1.0.1");
}

/**
 * @brief 测试故障恢复场景
 */
TEST_F(IntegrationTest, FailureRecoveryScenario) {
  ConfigManager& config = ConfigManager::getInstance();
  auto load_result = config.loadFromFile(config_file_.string());
  ASSERT_TRUE(load_result.has_value());

  auto restart_on_failure = config.getBool("processes.restart_on_failure");
  ASSERT_TRUE(restart_on_failure.has_value());

  if (restart_on_failure.value()) {
    // 模拟故障进程
    Process failing_process(test_script_.string(), {"--fail"});

    // 等待进程失败
    const auto exit_code = failing_process.waitForExit();
    EXPECT_TRUE(exit_code.has_value());
    EXPECT_NE(exit_code.value(), 0);  // 应该以非零退出码失败

    // 模拟重启
    Process restart_process(test_script_.string(), {"--quick"});
    const auto restart_exit = restart_process.waitForExit();
    EXPECT_TRUE(restart_exit.has_value());
    EXPECT_EQ(restart_exit.value(), 0);  // 重启应该成功
  }
}

/**
 * @brief 测试资源清理场景
 */
TEST_F(IntegrationTest, ResourceCleanupScenario) {
  ConfigManager& config = ConfigManager::getInstance();
  auto load_result = config.loadFromFile(config_file_.string());
  ASSERT_TRUE(load_result.has_value());

  auto lock_file = config.getString("application.instance_lock");
  ASSERT_TRUE(lock_file.has_value());

  // 确保锁文件在测试开始前不存在
  std::string full_lock_path =
      std::string(std::getenv("TMPDIR") ? std::getenv("TMPDIR") : "/tmp") +
      "/" + lock_file.value();
  std::filesystem::remove(full_lock_path);

  std::vector<ProcessId> process_pids;

  {
    // 在作用域内创建资源
    SingleInstanceGuard guard(lock_file.value());

    std::vector<std::unique_ptr<Process>> processes;
    processes.reserve(3);
    for (int i = 0; i < 3; ++i) {
      processes.push_back(std::unique_ptr<Process>(
          new Process(test_script_.string(), {"--config-test"})));
    }

    // 验证资源被正确创建
    for (const auto& process : processes) {
      EXPECT_TRUE(process->isRunning());
    }

    // 离开作用域，资源应该被自动清理
  }

  // 等待锁被完全释放
  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  // 验证锁已被释放 - 应该能够创建新的守护
  EXPECT_NO_THROW({
    SingleInstanceGuard new_guard(lock_file.value());
    // 立即销毁以释放锁
  });
}

/**
 * @brief 测试配置验证和错误处理
 */
TEST_F(IntegrationTest, ConfigurationValidationAndErrorHandling) {
  ConfigManager& config = ConfigManager::getInstance();

  // 测试加载无效配置
  std::filesystem::path invalid_config = temp_dir_ / "invalid_config.json";
  std::ofstream file(invalid_config);
  file << "{ invalid json content }";
  file.close();

  auto invalid_result = config.loadFromFile(invalid_config.string());
  EXPECT_FALSE(invalid_result.has_value());

  // 测试加载不存在的配置文件
  auto missing_result = config.loadFromFile("/nonexistent/config.json");
  EXPECT_FALSE(missing_result.has_value());

  // 加载有效配置
  auto valid_result = config.loadFromFile(config_file_.string());
  ASSERT_TRUE(valid_result.has_value());

  // 测试访问不存在的键
  auto missing_key = config.getString("nonexistent.key");
  EXPECT_FALSE(missing_key.has_value());

  // 测试类型不匹配
  auto type_mismatch =
      config.getInt("application.name");  // name是字符串，不是整数
  EXPECT_FALSE(type_mismatch.has_value());
}

/**
 * @brief 测试并发场景下的组件交互
 */
TEST_F(IntegrationTest, ConcurrentComponentInteraction) {
  ConfigManager& config = ConfigManager::getInstance();
  auto load_result = config.loadFromFile(config_file_.string());
  ASSERT_TRUE(load_result.has_value());

  constexpr int num_threads = 5;
  constexpr int operations_per_thread = 20;

  std::vector<std::thread> threads;
  std::atomic<int> successful_operations{0};
  std::atomic<int> failed_operations{0};

  threads.reserve(num_threads);
  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([&, i] {
      for (int j = 0; j < operations_per_thread; ++j) {
        try {
          // 配置读取
          if (auto app_name = config.getString("application.name");
              !app_name.has_value()) {
            failed_operations.fetch_add(1);
            continue;
          }

          // 短暂的锁操作
          {
            std::string lock_name = "concurrent_lock_" + std::to_string(i) +
                                    "_" + std::to_string(j);
            SingleInstanceGuard guard(lock_name);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
          }

          // 进程检查
          if (!is_process_running(getpid())) {
            failed_operations.fetch_add(1);
            continue;
          }

          successful_operations.fetch_add(1);

        } catch (const std::exception&) {
          failed_operations.fetch_add(1);
        }
      }
    });
  }

  // 等待所有线程完成
  for (auto& thread : threads) {
    thread.join();
  }

  const int total_operations = num_threads * operations_per_thread;
  EXPECT_EQ(successful_operations.load() + failed_operations.load(),
            total_operations);
  EXPECT_GT(successful_operations.load(),
            total_operations * 0.9);  // 至少90%成功率

  LOG_INFO << "Concurrent integration test results:";
  LOG_INFO << "  Successful operations: " << successful_operations.load();
  LOG_INFO << "  Failed operations: " << failed_operations.load();
  LOG_INFO << "  Success rate: "
           << successful_operations.load() * 100.0 / total_operations << "%";
}
