#include <gtest/gtest.h>
#include <boost/beast/core/error.hpp>
#include <boost/beast/websocket/error.hpp>
#include <boost/asio/error.hpp>
#include "network/error_context.hpp"

using namespace picoradar::network;

class ErrorContextTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up test environment
    }
    
    void TearDown() override {
        // Clean up test environment  
    }
};

TEST_F(ErrorContextTest, NetworkContextCreation) {
    NetworkContext ctx("test_operation", "127.0.0.1:8080");
    
    EXPECT_EQ(ctx.operation, "test_operation");
    EXPECT_EQ(ctx.endpoint, "127.0.0.1:8080");
    EXPECT_TRUE(ctx.player_id.empty());
    EXPECT_EQ(ctx.bytes_transferred, 0);
    EXPECT_GT(std::chrono::steady_clock::now(), ctx.start_time);
}

TEST_F(ErrorContextTest, ErrorHelperRetryableErrors) {
    // Test retryable errors
    boost::beast::error_code timeout_error = boost::beast::error::timeout;
    boost::beast::error_code connection_reset = boost::asio::error::connection_reset;
    boost::beast::error_code connection_aborted = boost::asio::error::connection_aborted;
    boost::beast::error_code eof_error = boost::asio::error::eof;
    
    EXPECT_TRUE(ErrorHelper::isRetryableError(timeout_error));
    EXPECT_TRUE(ErrorHelper::isRetryableError(connection_reset));
    EXPECT_TRUE(ErrorHelper::isRetryableError(connection_aborted));
    EXPECT_TRUE(ErrorHelper::isRetryableError(eof_error));
    
    // Test non-retryable error
    boost::beast::error_code operation_not_supported = boost::asio::error::operation_not_supported;
    EXPECT_FALSE(ErrorHelper::isRetryableError(operation_not_supported));
}

TEST_F(ErrorContextTest, ErrorHelperClientDisconnect) {
    // Test client disconnect errors
    boost::beast::error_code websocket_closed = boost::beast::websocket::error::closed;
    boost::beast::error_code connection_reset = boost::asio::error::connection_reset;
    boost::beast::error_code eof_error = boost::asio::error::eof;
    
    EXPECT_TRUE(ErrorHelper::isClientDisconnect(websocket_closed));
    EXPECT_TRUE(ErrorHelper::isClientDisconnect(connection_reset));
    EXPECT_TRUE(ErrorHelper::isClientDisconnect(eof_error));
    
    // Test non-disconnect error
    boost::beast::error_code timeout_error = boost::beast::error::timeout;
    EXPECT_FALSE(ErrorHelper::isClientDisconnect(timeout_error));
}

TEST_F(ErrorContextTest, ErrorHelperSeverity) {
    // Test client disconnect severity
    boost::beast::error_code websocket_closed = boost::beast::websocket::error::closed;
    EXPECT_EQ(ErrorHelper::getErrorSeverity(websocket_closed), "info");
    
    // Test retryable error severity
    boost::beast::error_code timeout_error = boost::beast::error::timeout;
    EXPECT_EQ(ErrorHelper::getErrorSeverity(timeout_error), "warning");
    
    // Test serious error severity
    boost::beast::error_code operation_not_supported = boost::asio::error::operation_not_supported;
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
    EXPECT_NO_THROW(ErrorLogger::logPerformanceWarning(ctx, "test metric", "test threshold"));
    EXPECT_NO_THROW(ErrorLogger::logOperationSuccess(ctx));
}

TEST_F(ErrorContextTest, PerformanceTimingAccuracy) {
    NetworkContext ctx("timing_test", "localhost:8080");
    
    // Simulate some work
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    auto duration = std::chrono::steady_clock::now() - ctx.start_time;
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
    
    // Should be at least 10ms but less than 100ms (allowing for system variance)
    EXPECT_GE(duration_ms.count(), 10);
    EXPECT_LT(duration_ms.count(), 100);
}
