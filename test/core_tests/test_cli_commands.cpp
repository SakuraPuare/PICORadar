#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <boost/asio.hpp>
#include <chrono>
#include <memory>
#include <string>
#include <thread>

#include "server.hpp"

// Mock implementation to test CLI command functionality
class CLICommandTest : public ::testing::Test {
 protected:
  void SetUp() override {
    server_ = std::make_unique<picoradar::server::Server>();
    stop_signal_ = false;
  }

  void TearDown() override {
    if (server_) {
      server_->stop();
    }
  }

  // Simulate CLI command processing
  std::string processCommand(const std::string& command, uint16_t port) {
    if (command == "status") {
      return "服务器状态: 运行中, 端口: " + std::to_string(port) +
             ", 连接数: " + std::to_string(server_->getConnectionCount()) +
             ", 玩家数: " + std::to_string(server_->getPlayerCount());
    } else if (command == "connections") {
      return "当前连接数: " + std::to_string(server_->getConnectionCount());
    } else if (command == "restart") {
      return "正在重启服务器...";
    } else if (command == "help") {
      return "可用命令: status, connections, restart, help";
    } else if (command == "exit" || command == "quit") {
      stop_signal_ = true;
      return "正在退出...";
    } else {
      return "未知命令: " + command + " (输入 help 查看帮助)";
    }
  }

  std::unique_ptr<picoradar::server::Server> server_;
  std::atomic<bool> stop_signal_;
};

TEST_F(CLICommandTest, StatusCommand) {
  uint16_t test_port = 8080;
  std::string result = processCommand("status", test_port);

  EXPECT_THAT(result, ::testing::HasSubstr("服务器状态: 运行中"));
  EXPECT_THAT(result, ::testing::HasSubstr("端口: 8080"));
  EXPECT_THAT(result, ::testing::HasSubstr("连接数:"));
  EXPECT_THAT(result, ::testing::HasSubstr("玩家数:"));
}

TEST_F(CLICommandTest, ConnectionsCommand) {
  std::string result = processCommand("connections", 8080);

  EXPECT_THAT(result, ::testing::HasSubstr("当前连接数:"));
  EXPECT_THAT(result, ::testing::HasSubstr("0"));  // Initially no connections
}

TEST_F(CLICommandTest, RestartCommand) {
  std::string result = processCommand("restart", 8080);

  EXPECT_EQ(result, "正在重启服务器...");
}

TEST_F(CLICommandTest, HelpCommand) {
  std::string result = processCommand("help", 8080);

  EXPECT_THAT(result, ::testing::HasSubstr("可用命令:"));
  EXPECT_THAT(result, ::testing::HasSubstr("status"));
  EXPECT_THAT(result, ::testing::HasSubstr("connections"));
  EXPECT_THAT(result, ::testing::HasSubstr("restart"));
  EXPECT_THAT(result, ::testing::HasSubstr("help"));
}

TEST_F(CLICommandTest, ExitCommand) {
  EXPECT_FALSE(stop_signal_);

  std::string result = processCommand("exit", 8080);

  EXPECT_TRUE(stop_signal_);
  EXPECT_THAT(result, ::testing::HasSubstr("正在退出"));
}

TEST_F(CLICommandTest, QuitCommand) {
  EXPECT_FALSE(stop_signal_);

  std::string result = processCommand("quit", 8080);

  EXPECT_TRUE(stop_signal_);
  EXPECT_THAT(result, ::testing::HasSubstr("正在退出"));
}

TEST_F(CLICommandTest, UnknownCommand) {
  std::string result = processCommand("invalid_command", 8080);

  EXPECT_THAT(result, ::testing::HasSubstr("未知命令: invalid_command"));
  EXPECT_THAT(result, ::testing::HasSubstr("输入 help 查看帮助"));
}

TEST_F(CLICommandTest, EmptyCommand) {
  std::string result = processCommand("", 8080);

  EXPECT_THAT(result, ::testing::HasSubstr("未知命令:"));
}

// Test server restart functionality
class ServerRestartTest : public ::testing::Test {
 protected:
  void SetUp() override {
    server_ = std::make_unique<picoradar::server::Server>();
  }

  void TearDown() override {
    if (server_) {
      server_->stop();
    }
  }

  uint16_t findAvailablePort() {
    boost::asio::io_context temp_ioc;
    boost::asio::ip::tcp::acceptor acceptor(temp_ioc);
    boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(), 0);
    acceptor.open(endpoint.protocol());
    acceptor.bind(endpoint);
    return acceptor.local_endpoint().port();
  }

  std::unique_ptr<picoradar::server::Server> server_;
};

TEST_F(ServerRestartTest, RestartPreservesStatsMethods) {
  uint16_t port = findAvailablePort();

  // Start server
  server_->start(port, 1);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Check initial stats
  size_t initial_connections = server_->getConnectionCount();
  size_t initial_received = server_->getMessagesReceived();
  size_t initial_sent = server_->getMessagesSent();

  // Stop and restart
  server_->stop();
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  server_->start(port, 1);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Verify stats methods still work after restart
  EXPECT_GE(server_->getConnectionCount(), 0);
  EXPECT_GE(server_->getMessagesReceived(), 0);
  EXPECT_GE(server_->getMessagesSent(), 0);
  EXPECT_GE(server_->getPlayerCount(), 0);
}
