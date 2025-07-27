#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
// Prevent Windows from defining ERROR macro that conflicts with logging
#ifdef ERROR
#undef ERROR
#endif
#endif

#include <atomic>
#include <chrono>
#include <csignal>
#include <memory>
#include <thread>

#include "cli_interface.hpp"
#include "cli_log_adapter.hpp"
#include "common/config_manager.hpp"
#include "common/constants.hpp"
#include "common/logging.hpp"
#include "common/single_instance_guard.hpp"
#include "server.hpp"

static std::atomic<bool> g_stop_signal(false);
static std::shared_ptr<picoradar::server::CLIInterface> g_cli_interface;

void signal_handler(const int signal) {
  if (signal == SIGINT) {
    picoradar::server::CLILogAdapter::addLogEntry(
        "INFO", "收到SIGINT信号，正在关闭...");
    g_stop_signal = true;
  }
}

auto main(const int argc, char* argv[]) -> int {
  logger::Logger::Init(argv[0], "./logs", logger::LogLevel::INFO);

  // 检查是否使用传统CLI模式
  bool use_traditional_cli = false;
  for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) == "--traditional" ||
        std::string(argv[i]) == "-t") {
      use_traditional_cli = true;
      break;
    }
  }

  // 创建CLI界面
  if (!use_traditional_cli) {
    g_cli_interface = std::make_shared<picoradar::server::CLIInterface>();
    picoradar::server::CLILogAdapter::initialize(g_cli_interface);

    // 设置额外的文件日志处理器
    picoradar::server::CLILogAdapter::setAdditionalHandler(
        [](const std::string&, const std::string&) {
          // 这里可以添加额外的日志处理，比如写入文件
          // 目前保持原有的日志系统处理
        });
  }

  try {
    auto guard = std::make_unique<picoradar::common::SingleInstanceGuard>(
        "PicoRadar.pid");
  } catch (const std::runtime_error& e) {
    if (use_traditional_cli) {
      LOG_ERROR << "Failed to start: " << e.what();
    } else {
      picoradar::server::CLILogAdapter::addLogEntry(
          "ERROR", std::string("启动失败: ") + e.what());
    }
    return 1;
  }

  std::signal(SIGINT, signal_handler);

  if (use_traditional_cli) {
    LOG_INFO << "PICO Radar Server Starting...";
  } else {
    picoradar::server::CLILogAdapter::addLogEntry("INFO",
                                                  "PICO Radar 服务器启动中...");
  }

  // 加载配置
  auto& config = picoradar::common::ConfigManager::getInstance();
  if (auto config_result = config.loadFromFile("config/server.json");
      !config_result.has_value()) {
    const std::string warning_msg =
        std::string("配置文件加载失败，使用默认配置: ") +
        config_result.error().message;
    if (use_traditional_cli) {
      LOG_WARNING << warning_msg;
    } else {
      picoradar::server::CLILogAdapter::addLogEntry("WARNING", warning_msg);
    }
  }

  // 从配置获取端口，或使用命令行参数/默认值
  uint16_t port = picoradar::config::kDefaultServicePort;
  if (argc > 1 && !use_traditional_cli) {
    // 跳过--traditional参数
    for (int i = 1; i < argc; ++i) {
      if (std::string(argv[i]) != "--traditional" &&
          std::string(argv[i]) != "-t") {
        try {
          port = std::stoi(argv[i]);
          picoradar::server::CLILogAdapter::addLogEntry(
              "INFO", "使用命令行指定端口: " + std::to_string(port));
          break;
        } catch (const std::exception&) {
          picoradar::server::CLILogAdapter::addLogEntry(
              "ERROR", "端口号无效，使用默认端口 " + std::to_string(port));
        }
      }
    }
  } else if (argc > 1 && use_traditional_cli) {
    try {
      port = std::stoi(argv[1]);
      LOG_INFO << "Using port from command line: " << port;
    } catch (const std::exception&) {
      LOG_ERROR << "Invalid port number provided. Using default " << port;
    }
  } else {
    // 使用配置管理器获取端口，自动处理后备默认值
    port = config.getServicePort();
    if (use_traditional_cli) {
      LOG_INFO << "Using port from config/default: " << port;
    } else {
      picoradar::server::CLILogAdapter::addLogEntry(
          "INFO", "使用配置/默认端口: " + std::to_string(port));
    }
  }

  // 启动CLI界面（如果使用）
  if (!use_traditional_cli) {
    g_cli_interface->updateServerStatus("正在启动服务器...");

    // 设置命令处理器
    g_cli_interface->setCommandHandler([&](const std::string& command) {
      if (command == "status") {
        picoradar::server::CLILogAdapter::addLogEntry(
            "INFO", "服务器状态: 运行中, 端口: " + std::to_string(port));
      } else if (command == "connections") {
        picoradar::server::CLILogAdapter::addLogEntry("INFO",
                                                      "当前连接数: 待实现");
      } else if (command == "restart") {
        picoradar::server::CLILogAdapter::addLogEntry("WARNING",
                                                      "重启功能待实现");
      } else if (command == "help") {
        picoradar::server::CLILogAdapter::addLogEntry(
            "INFO", "可用命令: status, connections, restart, help");
      } else if (command == "exit" || command == "quit") {
        g_stop_signal = true;
      } else {
        picoradar::server::CLILogAdapter::addLogEntry(
            "WARNING", "未知命令: " + command + " (输入 help 查看帮助)");
      }
    });

    g_cli_interface->start();
  }

  picoradar::server::Server server;
  server.start(port, 4);

  if (use_traditional_cli) {
    LOG_INFO << "Server started successfully. Press Ctrl+C to exit.";
  } else {
    g_cli_interface->updateServerStatus("运行中");
    picoradar::server::CLILogAdapter::addLogEntry(
        "INFO", "服务器启动成功，按 Ctrl+C 退出");
  }

  // 定期更新统计信息（仅在GUI模式下）
  const std::atomic<int> connection_count{0};
  const std::atomic<int> messages_received{0};
  const std::atomic<int> messages_sent{0};

  std::thread stats_thread;
  if (!use_traditional_cli) {
    stats_thread = std::thread([&] {
      while (!g_stop_signal) {
        // 这里应该从实际的服务器获取统计信息
        // 目前使用模拟数据
        g_cli_interface->updateConnectionCount(connection_count.load());
        g_cli_interface->updateMessageStats(messages_received.load(),
                                            messages_sent.load());

        std::this_thread::sleep_for(std::chrono::seconds(1));
      }
    });
  }

  while (!g_stop_signal) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  // 清理
  if (!use_traditional_cli) {
    g_cli_interface->updateServerStatus("正在关闭...");
    picoradar::server::CLILogAdapter::addLogEntry("INFO", "正在关闭服务器...");

    if (stats_thread.joinable()) {
      stats_thread.join();
    }

    g_cli_interface->stop();
    picoradar::server::CLILogAdapter::shutdown();
  }

  if (use_traditional_cli) {
    LOG_INFO << "Shutdown complete.";
  }
  return 0;
}