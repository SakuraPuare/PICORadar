#include <gtest/gtest.h>

#include <boost/asio/error.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/websocket/error.hpp>

#include "network/error_context.hpp"

using namespace picoradar::network;

class ErrorContextTest : public testing::Test {
 protected:
  void SetUp() override {
    // Set up test environment
  }

  void TearDown() override {
    // Clean up test environment
  }
};

TEST_F(ErrorContextTest, NetworkContextCreation) {
  const NetworkContext ctx("test_operation", "127.0.0.1:8080");

  EXPECT_EQ(ctx.operation, "test_operation");
  EXPECT_EQ(ctx.endpoint, "127.0.0.1:8080");
  EXPECT_TRUE(ctx.player_id.empty());
  EXPECT_EQ(ctx.bytes_transferred, 0);
  EXPECT_GT(std::chrono::steady_clock::now(), ctx.start_time);
}

TEST_F(ErrorContextTest, ErrorHelperRetryableErrors) {
  // Test retryable errors
  const boost::beast::error_code timeout_error = boost::beast::error::timeout;
  const boost::beast::error_code connection_reset =
      boost::asio::error::connection_reset;
  const boost::beast::error_code connection_aborted =
      boost::asio::error::connection_aborted;
  const boost::beast::error_code eof_error = boost::asio::error::eof;

  EXPECT_TRUE(ErrorHelper::isRetryableError(timeout_error));
  EXPECT_TRUE(ErrorHelper::isRetryableError(connection_reset));
  EXPECT_TRUE(ErrorHelper::isRetryableError(connection_aborted));
  EXPECT_TRUE(ErrorHelper::isRetryableError(eof_error));

  // Test non-retryable error
  const boost::beast::error_code operation_not_supported =
      boost::asio::error::operation_not_supported;
  EXPECT_FALSE(ErrorHelper::isRetryableError(operation_not_supported));
}

TEST_F(ErrorContextTest, ErrorHelperClientDisconnect) {
  // Test client disconnect errors
  const boost::beast::error_code websocket_closed =
      boost::beast::websocket::error::closed;
  const boost::beast::error_code connection_reset =
      boost::asio::error::connection_reset;
  const boost::beast::error_code eof_error = boost::asio::error::eof;

  EXPECT_TRUE(ErrorHelper::isClientDisconnect(websocket_closed));
  EXPECT_TRUE(ErrorHelper::isClientDisconnect(connection_reset));
  EXPECT_TRUE(ErrorHelper::isClientDisconnect(eof_error));

  // Test non-disconnect error
  const boost::beast::error_code timeout_error = boost::beast::error::timeout;
  EXPECT_FALSE(ErrorHelper::isClientDisconnect(timeout_error));
}

TEST_F(ErrorContextTest, ErrorHelperSeverity) {
  // Test client disconnect severity
  const boost::beast::error_code websocket_closed =
      boost::beast::websocket::error::closed;
  EXPECT_EQ(ErrorHelper::getErrorSeverity(websocket_closed), "info");

  // Test retryable error severity
  const boost::beast::error_code timeout_error = boost::beast::error::timeout;
  EXPECT_EQ(ErrorHelper::getErrorSeverity(timeout_error), "warning");

  // Test serious error severity
  const boost::beast::error_code operation_not_supported =
      boost::asio::error::operation_not_supported;
  EXPECT_EQ(ErrorHelper::getErrorSeverity(operation_not_supported), "error");
}

TEST_F(ErrorContextTest, NetworkContextWithPlayerInfo) {
  NetworkContext ctx("authenticate", "192.168.1.100:9000");
  ctx.player_id = "test_player_123";
  ctx.bytes_transferred = 256;

  EXPECT_EQ(ctx.operation, "authenticate");
  EXPECT_EQ(ctx.endpoint, "192.168.1.100:9000");
  EXPECT_EQ(ctx.player_id, "test_player_123");
  EXPECT_EQ(ctx.bytes_transferred, 256);
}

TEST_F(ErrorContextTest, ErrorLoggerStaticMethods) {
  // Test that error logger methods don't crash with various inputs
  NetworkContext ctx("test_op", "test_endpoint");
  boost::beast::error_code test_error = boost::asio::error::connection_refused;

  // These should not throw exceptions
  EXPECT_NO_THROW(ErrorLogger::logNetworkError(ctx, test_error, "test info"));
  EXPECT_NO_THROW(
      ErrorLogger::logPerformanceWarning(ctx, "test metric", "test threshold"));
  EXPECT_NO_THROW(ErrorLogger::logOperationSuccess(ctx));
}

TEST_F(ErrorContextTest, PerformanceTimingAccuracy) {
  const NetworkContext ctx("timing_test", "localhost:8080");

  // Simulate some work
  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  const auto duration = std::chrono::steady_clock::now() - ctx.start_time;
  const auto duration_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(duration);

  // Should be at least 10ms but less than 100ms (allowing for system variance)
  EXPECT_GE(duration_ms.count(), 10);
  EXPECT_LT(duration_ms.count(), 100);
}

/**
 * @brief 测试空值和边界条件处理
 */
TEST_F(ErrorContextTest, EmptyAndBoundaryConditions) {
  // 测试空字符串
  NetworkContext empty_ctx("", "");
  EXPECT_EQ(empty_ctx.operation, "");
  EXPECT_EQ(empty_ctx.endpoint, "");
  EXPECT_TRUE(empty_ctx.player_id.empty());

  // 测试空错误码
  boost::beast::error_code empty_error;
  EXPECT_NO_THROW({
    ErrorLogger::logNetworkError(empty_ctx, empty_error, "");
    ErrorLogger::logPerformanceWarning(empty_ctx, "", "");
    ErrorLogger::logOperationSuccess(empty_ctx);
  });

  // 测试极大值
  NetworkContext boundary_ctx("boundary_test", "test:9999");
  boundary_ctx.bytes_transferred = std::numeric_limits<size_t>::max();

  EXPECT_NO_THROW({
    ErrorLogger::logNetworkError(boundary_ctx, empty_error, "Max bytes test");
  });
}

/**
 * @brief 测试特殊字符处理
 */
TEST_F(ErrorContextTest, SpecialCharacterHandling) {
  std::string special_operation = "op with spaces & special chars @#$%";
  std::string special_endpoint = "endpoint:8080 with 中文 🌟";
  std::string special_player = "player\"'&<>name";

  NetworkContext special_ctx(special_operation, special_endpoint);
  special_ctx.player_id = special_player;

  boost::beast::error_code error = boost::asio::error::connection_refused;

  EXPECT_NO_THROW({
    ErrorLogger::logNetworkError(special_ctx, error, "Special: \n\t\r chars");
    ErrorLogger::logPerformanceWarning(special_ctx, "metric with\nnewline",
                                       "threshold");
  });
}

/**
 * @brief 测试长字符串处理
 */
TEST_F(ErrorContextTest, LongStringHandling) {
  std::string long_string(2000, 'X');

  NetworkContext long_ctx(long_string, long_string);
  long_ctx.player_id = long_string;

  boost::beast::error_code error = boost::asio::error::connection_refused;

  EXPECT_NO_THROW({
    ErrorLogger::logNetworkError(long_ctx, error, long_string);
    ErrorLogger::logPerformanceWarning(long_ctx, long_string, long_string);
  });
}

/**
 * @brief 测试不同错误码的详细分类
 */
TEST_F(ErrorContextTest, DetailedErrorClassification) {
  // WebSocket特定错误
  std::vector<boost::beast::error_code> websocket_errors = {
      boost::beast::websocket::error::closed,
      boost::beast::websocket::error::buffer_overflow,
      boost::beast::websocket::error::partial_deflate_block};

  // 网络错误
  std::vector<boost::beast::error_code> network_errors = {
      boost::asio::error::network_down, boost::asio::error::network_unreachable,
      boost::asio::error::host_unreachable};

  // 连接错误
  std::vector<boost::beast::error_code> connection_errors = {
      boost::asio::error::connection_refused,
      boost::asio::error::connection_reset,
      boost::asio::error::connection_aborted};

  // 测试错误分类
  for (const auto& error : websocket_errors) {
    std::string severity = ErrorHelper::getErrorSeverity(error);
    EXPECT_TRUE(severity == "info" || severity == "warning" ||
                severity == "error")
        << "Invalid severity for WebSocket error: " << error.message();
  }

  for (const auto& error : network_errors) {
    std::string severity = ErrorHelper::getErrorSeverity(error);
    EXPECT_TRUE(severity == "info" || severity == "warning" ||
                severity == "error")
        << "Invalid severity for network error: " << error.message();
  }
}

/**
 * @brief 测试时间测量的一致性
 */
TEST_F(ErrorContextTest, TimeMeasurementConsistency) {
  constexpr int num_contexts = 100;
  std::vector<NetworkContext> contexts;

  // 创建多个上下文
  for (int i = 0; i < num_contexts; ++i) {
    contexts.emplace_back("test_" + std::to_string(i),
                          "endpoint_" + std::to_string(i));
    std::this_thread::sleep_for(std::chrono::microseconds(100));  // 小延迟
  }

  // 验证时间递增
  for (size_t i = 1; i < contexts.size(); ++i) {
    EXPECT_GE(contexts[i].start_time, contexts[i - 1].start_time)
        << "Context " << i << " should have later start time than " << (i - 1);
  }
}

/**
 * @brief 测试并发上下文创建
 */
TEST_F(ErrorContextTest, ConcurrentContextCreation) {
  constexpr int num_threads = 10;
  constexpr int contexts_per_thread = 50;
  std::vector<std::thread> threads;
  std::vector<std::vector<std::unique_ptr<NetworkContext>>> all_contexts(
      num_threads);

  for (int t = 0; t < num_threads; ++t) {
    threads.emplace_back([&, t] {
      for (int i = 0; i < contexts_per_thread; ++i) {
        all_contexts[t].push_back(std::make_unique<NetworkContext>(
            "thread_" + std::to_string(t) + "_op_" + std::to_string(i),
            "endpoint_" + std::to_string(t) + "_" + std::to_string(i)));
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }

  // 验证所有上下文都被正确创建
  for (int t = 0; t < num_threads; ++t) {
    EXPECT_EQ(all_contexts[t].size(), contexts_per_thread);
    for (int i = 0; i < contexts_per_thread; ++i) {
      EXPECT_FALSE(all_contexts[t][i]->operation.empty());
      EXPECT_FALSE(all_contexts[t][i]->endpoint.empty());
    }
  }
}

/**
 * @brief 测试错误分类的边界情况
 */
TEST_F(ErrorContextTest, ErrorClassificationEdgeCases) {
  // 成功状态（无错误）
  boost::beast::error_code success;
  EXPECT_FALSE(ErrorHelper::isRetryableError(success));
  EXPECT_FALSE(ErrorHelper::isClientDisconnect(success));
  EXPECT_EQ(ErrorHelper::getErrorSeverity(success), "error");

  // 自定义错误分类
  boost::beast::error_code custom_error(999, boost::system::generic_category());
  EXPECT_FALSE(ErrorHelper::isRetryableError(custom_error));
  EXPECT_FALSE(ErrorHelper::isClientDisconnect(custom_error));
  EXPECT_EQ(ErrorHelper::getErrorSeverity(custom_error), "error");
}

/**
 * @brief 测试操作持续时间计算精度
 */
TEST_F(ErrorContextTest, OperationDurationPrecision) {
  NetworkContext ctx("precision_test", "localhost:8080");

  // 等待精确的时间
  auto wait_duration = std::chrono::milliseconds(50);
  std::this_thread::sleep_for(wait_duration);

  auto measured_duration = std::chrono::steady_clock::now() - ctx.start_time;
  auto measured_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(measured_duration);

  // 应该在预期范围内（允许一些系统调度误差）
  EXPECT_GE(measured_ms.count(), 45);   // 至少45ms
  EXPECT_LE(measured_ms.count(), 100);  // 最多100ms
}

/**
 * @brief 性能测试：大量错误日志记录
 */
TEST_F(ErrorContextTest, PerformanceStressTest) {
  constexpr int num_logs = 1000;
  std::vector<NetworkContext> contexts;

  // 准备测试数据
  for (int i = 0; i < num_logs; ++i) {
    contexts.emplace_back("stress_test_" + std::to_string(i),
                          "endpoint_" + std::to_string(i % 100));
  }

  boost::beast::error_code test_error = boost::asio::error::connection_reset;

  auto start_time = std::chrono::high_resolution_clock::now();

  // 执行大量日志记录
  for (auto& ctx : contexts) {
    ErrorLogger::logNetworkError(ctx, test_error, "Stress test");
  }

  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      end_time - start_time);

  // 应该在合理时间内完成（根据实际性能调整）
  EXPECT_LT(duration.count(), 5000);  // 应该在5秒内完成

  std::cout << "Logged " << num_logs << " errors in " << duration.count()
            << " ms" << std::endl;
}
