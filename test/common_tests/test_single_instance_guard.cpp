#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <thread>

#include "common/single_instance_guard.hpp"

using namespace picoradar::common;

class SingleInstanceGuardTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // 创建临时测试目录
    temp_dir_ = std::filesystem::temp_directory_path() / "picoradar_sig_test";
    std::filesystem::create_directories(temp_dir_);

    // 生成唯一的测试锁文件名
    test_lock_file_ =
        "test_sig_" +
        std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()) +
        ".pid";
  }

  void TearDown() override {
    // 清理测试锁文件
    cleanup_lock_file(test_lock_file_);

    // 清理临时目录
    if (std::filesystem::exists(temp_dir_)) {
      std::filesystem::remove_all(temp_dir_);
    }
  }

  static void cleanup_lock_file(const std::string& lock_file) {
    try {
      std::remove(lock_file.c_str());
    } catch (...) {
      // 忽略清理错误
    }
  }

  std::filesystem::path temp_dir_;
  std::string test_lock_file_;
};

/**
 * @brief 测试基本的单实例锁功能
 */
TEST_F(SingleInstanceGuardTest, BasicLockAcquisition) {
  // Act & Assert: 应该能够成功获取锁
  EXPECT_NO_THROW({
    SingleInstanceGuard guard(test_lock_file_);
    // 锁在作用域内被持有
  });  // 锁在这里被自动释放

  // 锁释放后，应该能够再次获取
  EXPECT_NO_THROW({ SingleInstanceGuard guard(test_lock_file_); });
}

/**
 * @brief 测试锁竞争 - 同一进程内的多个实例
 */
TEST_F(SingleInstanceGuardTest, LockContention) {
  // Arrange: 获取第一个锁
  SingleInstanceGuard first_guard(test_lock_file_);

  // Act & Assert: 尝试获取第二个锁应该失败
  EXPECT_THROW(
      { SingleInstanceGuard second_guard(test_lock_file_); },
      std::runtime_error)
      << "应该无法获取已被持有的锁";
}

/**
 * @brief 测试RAII行为 - 锁的自动释放
 */
TEST_F(SingleInstanceGuardTest, RAIIBehavior) {
  // Arrange & Act: 在作用域内获取锁
  {
    SingleInstanceGuard guard(test_lock_file_);

    // 验证锁被持有 - 尝试获取第二个锁应该失败
    EXPECT_THROW(
        { SingleInstanceGuard duplicate(test_lock_file_); },
        std::runtime_error);

    // 离开作用域，锁应该被自动释放
  }

  // Assert: 锁已释放，应该能够重新获取
  EXPECT_NO_THROW({ SingleInstanceGuard new_guard(test_lock_file_); })
      << "锁应该已被自动释放";
}

/**
 * @brief 测试陈旧锁文件的自动清理
 */
TEST_F(SingleInstanceGuardTest, StaleLockCleanup) {
  // Arrange: 创建包含无效PID的陈旧锁文件
  {
    std::ofstream stale_file(test_lock_file_);
    stale_file << "999999";  // 几乎不可能存在的PID
  }

  // Act & Assert: 应该能够清理陈旧锁并获取新锁
  EXPECT_NO_THROW({ SingleInstanceGuard guard(test_lock_file_); })
      << "应该能够清理陈旧锁文件并获取新锁";
}

/**
 * @brief 测试不同锁文件的并行使用
 */
TEST_F(SingleInstanceGuardTest, MultipleDifferentLocks) {
  const std::string lock1 = "test_lock1.pid";
  const std::string lock2 = "test_lock2.pid";

  // Cleanup
  cleanup_lock_file(lock1);
  cleanup_lock_file(lock2);

  try {
    // Act: 应该能够同时持有不同的锁
    SingleInstanceGuard guard1(lock1);
    SingleInstanceGuard guard2(lock2);

    // Assert: 验证两个锁都被正确持有
    EXPECT_THROW(
        { SingleInstanceGuard duplicate1(lock1); }, std::runtime_error)
        << "第一个锁应该被持有";

    EXPECT_THROW(
        { SingleInstanceGuard duplicate2(lock2); }, std::runtime_error)
        << "第二个锁应该被持有";

  } catch (const std::exception& e) {
    FAIL() << "应该能够同时持有不同的锁: " << e.what();
  }

  // Cleanup
  cleanup_lock_file(lock1);
  cleanup_lock_file(lock2);
}

/**
 * @brief 测试锁文件名的边界情况
 */
TEST_F(SingleInstanceGuardTest, LockFileNameEdgeCases) {
  // 测试带特殊字符的锁文件名
  const std::string special_name = "test-lock_file.123.pid";
  cleanup_lock_file(special_name);

  EXPECT_NO_THROW({ SingleInstanceGuard guard(special_name); })
      << "应该能够使用带特殊字符的锁文件名";

  cleanup_lock_file(special_name);

  // 测试空字符串（应该有合理的错误处理）
  EXPECT_NO_THROW({
    try {
      SingleInstanceGuard empty_guard("");
    } catch (const std::runtime_error&) {
      // 空字符串可能会抛出异常，这是可以接受的
    }
  }) << "空字符串参数不应该导致程序崩溃";
}

/**
 * @brief 测试并发场景下的锁行为 - 对同一个锁文件的竞争
 */
TEST_F(SingleInstanceGuardTest, ConcurrentLockAttempts) {
  const int num_threads = 5;
  const int attempts_per_thread = 20;
  const std::string shared_lock_name = "concurrent_shared_test.pid";

  std::atomic<int> successful_acquisitions{0};
  std::atomic<int> failed_acquisitions{0};

  std::vector<std::future<void>> futures;

  // 清理共享锁文件
  cleanup_lock_file(shared_lock_name);

  // 启动多个线程尝试获取同一个锁
  futures.reserve(num_threads);
  for (int i = 0; i < num_threads; ++i) {
    futures.push_back(std::async(std::launch::async, [&, i]() {
      for (int j = 0; j < attempts_per_thread; ++j) {
        try {
          SingleInstanceGuard guard(shared_lock_name);
          successful_acquisitions.fetch_add(1);

          // 短暂持有锁
          std::this_thread::sleep_for(std::chrono::milliseconds(1));

        } catch (const std::runtime_error&) {
          failed_acquisitions.fetch_add(1);
        }

        // 小的延迟以避免忙等待
        std::this_thread::sleep_for(std::chrono::microseconds(100));
      }
    }));
  }

  // 等待所有线程完成
  for (auto& future : futures) {
    future.wait();
  }

  // Assert: 验证所有尝试都得到了处理
  int total_attempts = num_threads * attempts_per_thread;
  int total_handled =
      successful_acquisitions.load() + failed_acquisitions.load();

  EXPECT_EQ(total_handled, total_attempts) << "所有锁获取尝试都应该得到处理";

  // 在并发竞争中，大部分尝试应该失败，只有少数能成功获取锁
  EXPECT_GT(failed_acquisitions.load(), successful_acquisitions.load())
      << "在并发竞争中，失败的尝试应该多于成功的尝试";

  // 但至少应该有一些成功的获取
  EXPECT_GT(successful_acquisitions.load(), 0) << "应该有至少一些成功的锁获取";

  // 清理
  cleanup_lock_file(shared_lock_name);
}

/**
 * @brief 测试不同锁文件的并发获取（应该都能成功）
 */
TEST_F(SingleInstanceGuardTest, ConcurrentDifferentLockFiles) {
  const int num_threads = 5;
  const int attempts_per_thread = 20;

  std::atomic<int> successful_acquisitions{0};
  std::atomic<int> failed_acquisitions{0};

  std::vector<std::future<void>> futures;

  // 启动多个线程，每个线程使用不同的锁文件
  futures.reserve(num_threads);
  for (int i = 0; i < num_threads; ++i) {
    futures.push_back(std::async(std::launch::async, [&, i]() {
      for (int j = 0; j < attempts_per_thread; ++j) {
        std::string lock_name = "different_locks_test_" + std::to_string(i) +
                                "_" + std::to_string(j) + ".pid";
        cleanup_lock_file(lock_name);

        try {
          SingleInstanceGuard guard(lock_name);
          successful_acquisitions.fetch_add(1);

          // 短暂持有锁
          std::this_thread::sleep_for(std::chrono::milliseconds(1));

        } catch (const std::runtime_error&) {
          failed_acquisitions.fetch_add(1);
        }

        cleanup_lock_file(lock_name);
      }
    }));
  }

  // 等待所有线程完成
  for (auto& future : futures) {
    future.wait();
  }

  // Assert: 验证所有尝试都得到了处理
  int total_attempts = num_threads * attempts_per_thread;
  int total_handled =
      successful_acquisitions.load() + failed_acquisitions.load();

  EXPECT_EQ(total_handled, total_attempts) << "所有锁获取尝试都应该得到处理";

  // 使用不同锁文件，所有尝试都应该成功
  EXPECT_EQ(successful_acquisitions.load(), total_attempts)
      << "使用不同锁文件时，所有锁获取都应该成功";

  EXPECT_EQ(failed_acquisitions.load(), 0)
      << "使用不同锁文件时，不应该有失败的锁获取";
}

/**
 * @brief 测试快速的锁获取和释放循环
 */
TEST_F(SingleInstanceGuardTest, RapidLockCycling) {
  const int num_cycles = 100;

  for (int i = 0; i < num_cycles; ++i) {
    // 快速获取和释放锁
    {
      SingleInstanceGuard guard(test_lock_file_);
      // 立即释放
    }

    // 验证能够立即重新获取
    { SingleInstanceGuard guard(test_lock_file_); }
  }

  // 最终验证锁仍然可用
  EXPECT_NO_THROW({ SingleInstanceGuard final_guard(test_lock_file_); })
      << "经过快速循环后，锁仍应可用";
}