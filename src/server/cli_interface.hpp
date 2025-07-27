#pragma once

#include <atomic>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "ftxui/component/captured_mouse.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/component_base.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"

namespace picoradar::server {

/**
 * @brief CLI用户界面管理器
 *
 * 提供一个类似GUI的终端界面，包含：
 * - 服务器状态显示区域
 * - 实时日志输出区域
 * - 命令输入区域
 * - 统计信息显示区域
 */
class CLIInterface {
 public:
  explicit CLIInterface();
  ~CLIInterface();

  // 启动界面
  void start();

  // 停止界面
  void stop();

  // 添加日志条目
  void addLogEntry(const std::string& level, const std::string& message);

  // 更新服务器状态
  void updateServerStatus(const std::string& status);

  // 更新连接数
  void updateConnectionCount(int count);

  // 更新消息统计
  void updateMessageStats(int received, int sent);

  // 设置命令处理回调
  void setCommandHandler(std::function<void(const std::string&)> handler);

 private:
  struct LogEntry {
    std::string timestamp;
    std::string level;
    std::string message;
  };

  // UI组件
  ftxui::Component createUI();
  ftxui::Element renderStatus();
  ftxui::Element renderLogs();
  ftxui::Element renderStats();

  // 数据成员
  std::atomic<bool> running_{false};
  std::unique_ptr<std::thread> ui_thread_;

  // 状态数据
  std::string server_status_{"启动中..."};
  int connection_count_{0};
  int messages_received_{0};
  int messages_sent_{0};

  // 日志数据
  std::deque<LogEntry> log_entries_;
  std::mutex log_mutex_;
  static constexpr size_t MAX_LOG_ENTRIES = 1000;

  // 命令输入
  std::string command_input_;
  std::function<void(const std::string&)> command_handler_;

  // UI控制
  std::mutex ui_mutex_;
  std::atomic<bool> needs_refresh_{false};
};

}  // namespace picoradar::server
