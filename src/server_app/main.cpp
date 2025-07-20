#include <glog/logging.h>

#include <csignal>
#include <iostream>

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

int main(int argc, char* argv[]) {
  // 初始化 glog
  google::InitGoogleLogging(argv[0]);
  // 将日志同时输出到标准错误流和文件
  FLAGS_logtostderr = false;
  FLAGS_alsologtostderr = true;
  // 设置日志文件目录
  FLAGS_log_dir = "./logs";

  // 注册信号处理器
  std::signal(SIGINT, signal_handler);

  LOG(INFO) << "PICO Radar Server Starting...";
  LOG(INFO) << "============================";

  // 定义预共享令牌
  // TODO: 未来从配置文件或环境变量加载
  const std::string secret_token = "pico-radar-super-secret-token-!@#$";
  LOG(INFO) << "Using secret token (first 8 chars): "
            << secret_token.substr(0, 8) << "...";

  // 创建核心模块
  auto registry = std::make_shared<picoradar::core::PlayerRegistry>();

  // 创建并运行网络服务器
  std::shared_ptr<picoradar::network::WebsocketServer> server;
  try {
    server = std::make_shared<picoradar::network::WebsocketServer>(
        *registry, secret_token);
  } catch (const std::exception& e) {
    LOG(FATAL) << "Failed to create server: " << e.what();
    return 1;
  }

  // 监听所有网络接口
  const std::string address = "0.0.0.0";
  const uint16_t port = 9002;
  const int threads = 4;  // 使用4个线程处理IO
  server->run(address, port, threads);

  // 等待停止信号
  while (!g_stop_signal) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  // 停止服务器
  server->stop();

  LOG(INFO) << "Shutdown complete.";
  google::ShutdownGoogleLogging();

  return 0;
}
