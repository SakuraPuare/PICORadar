#include <glog/logging.h>
#include <google/protobuf/stubs/common.h>

#include <boost/asio/io_context.hpp>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <thread>

#include "common/constants.hpp"
#include "common/logging.hpp"
#include "common/single_instance_guard.hpp"
#include "core/player_registry.hpp"
#include "network/websocket_server.hpp"

// 创建一个全局的原子布尔值，用于优雅地处理Ctrl+C信号
static volatile std::atomic<bool> g_stop_signal(false);

// SIGINT (Ctrl+C) 的处理函数
void signal_handler(int signal) {
  if (signal == SIGINT) {
    LOG(INFO) << "Caught SIGINT, shutting down...";
    g_stop_signal = true;
  }
}

auto main(int argc, char* argv[]) -> int {
  // 初始化 glog，并为所有日志消息添加 "[SERVER] " 前缀
  picoradar::common::setup_logging(argv[0], true, "./logs", "[SERVER] ");

  // 确保只有一个实例在运行
  std::unique_ptr<picoradar::common::SingleInstanceGuard> guard;
  try {
    guard = std::make_unique<picoradar::common::SingleInstanceGuard>(
        "PicoRadar.pid");
  } catch (const std::runtime_error& e) {
    LOG(ERROR) << "Failed to start: " << e.what();
    return 1;
  }

  // 注册信号处理器
  std::signal(SIGINT, signal_handler);

  LOG(INFO) << "PICO Radar Server Starting...";
  LOG(INFO) << "============================";

  // 创建 Boost.Asio 的核心 I/O 上下文
  net::io_context ioc;

  // 创建核心模块
  auto registry = std::make_shared<picoradar::core::PlayerRegistry>();

  // 创建并运行网络服务器
  std::shared_ptr<picoradar::network::WebsocketServer> server;
  try {
    server =
        std::make_shared<picoradar::network::WebsocketServer>(ioc, *registry);
  } catch (const std::exception& e) {
    LOG(FATAL) << "Failed to create server: " << e.what();
    return 1;
  }

  // 监听所有网络接口
  const std::string address = "0.0.0.0";
  uint16_t port = picoradar::config::kDefaultServicePort;  // 默认端口
  if (argc > 1) {
    try {
      port = std::stoi(argv[1]);
    } catch (const std::exception& e) {
      LOG(ERROR) << "Invalid port number provided. Using default "
                 << picoradar::config::kDefaultServicePort;
    }
  }
  LOG(INFO) << "Server will listen on port " << port;

  const int threads = 4;  // 使用4个线程处理IO
  server->start(address, port, threads);

  // 在一个后台线程中运行io_context
  std::thread server_thread([&ioc] {
    try {
      ioc.run();
    } catch (const std::exception& e) {
      LOG(ERROR) << "Exception in server I/O context: " << e.what();
    }
  });

  // 主循环处理CLI输入
  LOG(INFO) << "Server started successfully. Type 'status' for player count, "
               "or 'quit' to exit.";
  std::string line;
  while (!g_stop_signal && std::getline(std::cin, line)) {
    if (line == "status") {
      LOG(INFO) << "Current online players: " << registry->getPlayerCount();
    } else if (line == "quit" || line == "exit") {
      g_stop_signal = true;
    }
  }

  // 停止服务器
  server->stop();
  if (server_thread.joinable()) {
    server_thread.join();
  }

  LOG(INFO) << "Shutdown complete.";
  google::protobuf::ShutdownProtobufLibrary();
  google::ShutdownGoogleLogging();

  return 0;
}
