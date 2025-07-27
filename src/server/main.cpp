#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <memory>
#include <thread>

#include "cli_interface.hpp"
#include "cli_log_adapter.hpp"
#include "common/config_manager.hpp"
#include "common/constants.hpp"
#include "common/logging.hpp"
#include "common/platform_fixes.hpp"
#include "common/single_instance_guard.hpp"
#include "server.hpp"

static std::atomic<bool> g_stop_signal(false);
static std::shared_ptr<picoradar::server::CLIInterface> g_cli_interface;
static bool g_use_traditional_cli = false;

// 统一的日志输出函数
void logMessageHandler(const std::string& message, logger::LogLevel level) {
  if (g_use_traditional_cli) {
    std::cout << message << std::endl;
  } else if (g_cli_interface) {
    switch (level) {
      case logger::LogLevel::INFO:
        LOG_INFO << message;
        break;
      case logger::LogLevel::WARNING:
        LOG_WARNING << message;
        break;
      case logger::LogLevel::ERROR:
        LOG_ERROR << message;
        break;
      case logger::LogLevel::DEBUG:
        LOG_DEBUG << message;
        break;
      default:
        LOG_INFO << message;
        break;
    }
  }
}

void signalHandler(int signum) {
  if (signum == SIGINT) {
    logMessageHandler("收到SIGINT信号，正在关闭...", logger::LogLevel::INFO);
    g_stop_signal = true;

    // 立即停止CLI界面以防止阻塞
    if (g_cli_interface) {
      g_cli_interface->stop();
    }
  }
}

auto main(const int argc, char* argv[]) -> int {
  // 初始化日志系统
  logger::LogConfig config = logger::LogConfig::loadFromConfigManager();
  config.log_directory = "./logs";
  config.global_level = logger::LogLevel::INFO;
  config.file_enabled = true;
  config.console_enabled = true;
  logger::Logger::Init(argv[0], config);

  // 检查是否使用传统CLI模式
  for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) == "--traditional" ||
        std::string(argv[i]) == "-t") {
      g_use_traditional_cli = true;
      break;
    }
  }

  // 创建CLI界面
  if (!g_use_traditional_cli) {
    g_cli_interface = std::make_shared<picoradar::server::CLIInterface>();
    picoradar::server::CLILogAdapter::initialize(g_cli_interface);
  }

  try {
    auto guard = std::make_unique<picoradar::common::SingleInstanceGuard>(
        "PicoRadar.pid");
  } catch (const std::runtime_error& e) {
    logMessageHandler(std::string("启动失败: ") + e.what(),
                      logger::LogLevel::ERROR);
    return 1;
  }

  std::signal(SIGINT, signalHandler);

  logMessageHandler("PICO Radar 服务器启动中...", logger::LogLevel::INFO);

  // 加载配置
  auto& config_manager = picoradar::common::ConfigManager::getInstance();
  if (auto config_result = config_manager.loadFromFile("config/server.json");
      !config_result.has_value()) {
    const std::string warning_msg =
        std::string("配置文件加载失败，使用默认配置: ") +
        config_result.error().message;
    logMessageHandler(warning_msg, logger::LogLevel::WARNING);
  } else {
    // 配置加载成功，进行配置验证
    if (!config_manager.validateConfig()) {
      logMessageHandler("配置验证失败，某些配置项可能无效，请检查配置文件",
                        logger::LogLevel::WARNING);
    }
  }

  // 从配置或默认值获取端口
  uint16_t port = picoradar::constants::kDefaultServicePort;
  if (argc > 1) {
    // 跳过--traditional参数
    for (int i = 1; i < argc; ++i) {
      if (std::string(argv[i]) != "--traditional" &&
          std::string(argv[i]) != "-t") {
        try {
          port = std::stoi(argv[i]);
          logMessageHandler("使用命令行指定端口: " + std::to_string(port),
                            logger::LogLevel::INFO);
          break;
        } catch (const std::exception&) {
          logMessageHandler("端口号无效，使用默认端口 " + std::to_string(port),
                            logger::LogLevel::ERROR);
        }
      }
    }
  } else if (argc > 1 && g_use_traditional_cli) {
    try {
      port = std::stoi(argv[1]);
      logMessageHandler("使用命令行指定端口: " + std::to_string(port),
                        logger::LogLevel::INFO);
    } catch (const std::exception&) {
      logMessageHandler("端口号无效，使用默认端口 " + std::to_string(port),
                        logger::LogLevel::ERROR);
    }
  } else {
    // 使用配置管理器获取端口，自动处理后备默认值
    port = config_manager.getServicePort();
    logMessageHandler("使用配置/默认端口: " + std::to_string(port),
                      logger::LogLevel::INFO);
  }

  // 启动CLI界面（如果使用）
  if (!g_use_traditional_cli) {
    g_cli_interface->updateServerStatus("正在启动服务器...");
    g_cli_interface->start();
  }

  picoradar::server::Server server;
  server.start(port, 4);

  // 设置命令处理器（在服务器创建后）
  if (!g_use_traditional_cli) {
    g_cli_interface->setCommandHandler([&](const std::string& command) {
      if (command == "status") {
        logMessageHandler(
            "服务器状态: 运行中, 端口: " + std::to_string(port) +
                ", 连接数: " + std::to_string(server.getConnectionCount()) +
                ", 玩家数: " + std::to_string(server.getPlayerCount()),
            logger::LogLevel::INFO);
      } else if (command == "connections") {
        logMessageHandler(
            "当前连接数: " + std::to_string(server.getConnectionCount()),
            logger::LogLevel::INFO);
      } else if (command == "restart") {
        logMessageHandler("正在重启服务器...", logger::LogLevel::WARNING);
        // Stop the server
        server.stop();
        // Start again with same parameters
        server.start(port, 4);
        logMessageHandler("服务器重启完成", logger::LogLevel::INFO);
      } else if (command == "help") {
        logMessageHandler("可用命令: status, connections, restart, help",
                          logger::LogLevel::INFO);
      } else if (command == "exit" || command == "quit") {
        g_stop_signal = true;
      } else {
        logMessageHandler("未知命令: " + command + " (输入 help 查看帮助)",
                          logger::LogLevel::WARNING);
      }
    });
  }

  logMessageHandler("服务器启动成功，按 Ctrl+C 退出", logger::LogLevel::INFO);

  if (!g_use_traditional_cli) {
    g_cli_interface->updateServerStatus("运行中");
  }

  // 定期更新统计信息（仅在GUI模式下）
  std::thread stats_thread;
  if (!g_use_traditional_cli) {
    stats_thread = std::thread([&] {
      while (!g_stop_signal) {
        // 从实际的服务器获取统计信息
        g_cli_interface->updateConnectionCount(server.getConnectionCount());
        g_cli_interface->updateMessageStats(server.getMessagesReceived(),
                                            server.getMessagesSent());

        std::this_thread::sleep_for(std::chrono::seconds(1));
      }
    });
  }

  while (!g_stop_signal) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  // 清理
  logMessageHandler("正在关闭服务器...", logger::LogLevel::INFO);

  if (!g_use_traditional_cli) {
    g_cli_interface->updateServerStatus("正在关闭...");
  }

  // 停止服务器
  server.stop();

  if (!g_use_traditional_cli) {
    if (stats_thread.joinable()) {
      stats_thread.join();
    }

    g_cli_interface->stop();
    picoradar::server::CLILogAdapter::shutdown();
  }

  logMessageHandler("关闭完成", logger::LogLevel::INFO);
  return 0;
}