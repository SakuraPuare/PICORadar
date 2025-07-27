#ifdef _WIN32
    #include <windows.h>
    #include <conio.h>
#else
    #include <fcntl.h>
    #include <termios.h>
    #include <unistd.h>
#endif

#include <csignal>
#include <atomic>
#include <cctype>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <thread>

#include "client.hpp"
#include "common/logging.hpp"

using namespace picoradar::client;
using namespace picoradar;

// 全局变量用于控制程序运行
std::atomic<bool> g_running{true};
std::atomic<bool> g_connected{false};

// 玩家信息
std::string g_current_player_name;
std::atomic<float> g_player_x{0.0F};
std::atomic<float> g_player_y{0.0F};
std::atomic<float> g_player_z{0.0F};

// 其他玩家的信息
std::map<std::string, PlayerData> g_other_players;
std::mutex g_players_mutex;

// 终端控制相关
#ifndef _WIN32
struct termios g_old_tio;
#endif

// 信号处理函数
void signal_handler(int signal) {
  if (signal == SIGINT || signal == SIGTERM) {
    g_running = false;
  }
}

// 设置终端为非阻塞输入模式
void setup_terminal() {
#ifdef _WIN32
  // Windows: 设置控制台模式
  HANDLE hInput = GetStdHandle(STD_INPUT_HANDLE);
  DWORD mode = 0;
  GetConsoleMode(hInput, &mode);
  SetConsoleMode(hInput, mode & ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT));
#else
  struct termios new_tio;

  // 获取当前终端设置并修改
  new_tio = g_old_tio;

  // 设置为非标准模式，无回显
  new_tio.c_lflag &= ~(ICANON | ECHO);
  new_tio.c_cc[VMIN] = 0;
  new_tio.c_cc[VTIME] = 0;

  tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);

  // 设置标准输入为非阻塞
  int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
  fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
#endif
}

// 恢复终端设置
void restore_terminal() { 
#ifdef _WIN32
  // Windows: 恢复控制台模式
  HANDLE hInput = GetStdHandle(STD_INPUT_HANDLE);
  DWORD mode = 0;
  GetConsoleMode(hInput, &mode);
  SetConsoleMode(hInput, mode | ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT);
#else
  tcsetattr(STDIN_FILENO, TCSANOW, &g_old_tio);
#endif
}

// 清屏和移动光标到顶部
void clear_screen() { std::cout << "\033[2J\033[1;1H" << std::flush; }

// 移动光标到指定位置
void move_cursor(int row, int col) {
  std::cout << "\033[" << row << ";" << col << "H" << std::flush;
}

// 隐藏光标
void hide_cursor() { std::cout << "\033[?25l" << std::flush; }

// 显示光标
void show_cursor() { std::cout << "\033[?25h" << std::flush; }

// 获取用户输入的用户名
auto get_username() -> std::string {
  std::string username;

  // 确保终端处于正常输入模式
  clear_screen();
  std::cout << "🎮 PICO Radar WASD Game 🎮\n";
  std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
  std::cout << "\n";
  std::cout << "欢迎来到 PICO Radar 多人位置共享游戏！\n";
  std::cout << "\n";
  std::cout << "在这个游戏中，你可以:\n";
  std::cout << "• 使用 WASD 和 QE 键控制你的角色在 3D 空间中移动\n";
  std::cout << "• 实时看到其他在线玩家的位置\n";
  std::cout << "• 体验低延迟的多人位置同步\n";
  std::cout << "\n";
  std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
  std::cout << "\n";

  while (true) {
    std::cout << "请输入你的用户名 (3-16个字符，只能包含字母、数字和下划线): ";
    std::cout.flush();  // 确保提示信息被显示

    if (!std::getline(std::cin, username)) {
      std::cout << "\n❌ 输入错误，请重试。\n\n";
      std::cin.clear();
      continue;
    }

    // 验证用户名
    if (username.empty()) {
      std::cout << "❌ 用户名不能为空，请重新输入。\n\n";
      continue;
    }

    if (username.length() < 3) {
      std::cout << "❌ 用户名太短，至少需要3个字符，请重新输入。\n\n";
      continue;
    }

    if (username.length() > 16) {
      std::cout << "❌ 用户名太长，最多16个字符，请重新输入。\n\n";
      continue;
    }

    // 检查字符是否合法
    bool valid = true;
    for (char c : username) {
      if ((std::isalnum(c) == 0) && c != '_') {
        valid = false;
        break;
      }
    }

    if (!valid) {
      std::cout << "❌ 用户名只能包含字母、数字和下划线，请重新输入。\n\n";
      continue;
    }

    // 用户名有效
    break;
  }

  std::cout << "\n✅ 欢迎你，" << username << "！\n";
  std::cout << "正在准备游戏...\n";
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  return username;
}

// 格式化位置信息
auto format_position(float x, float y, float z) -> std::string {
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(1) << "(" << x << ", " << y << ", "
      << z << ")";
  return oss.str();
}

// 绘制游戏界面
void draw_game_ui() {
  clear_screen();

  std::cout << "🎮 PICO Radar WASD Game 🎮\n";
  std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
  std::cout << "\n";

  // 显示控制说明
  std::cout << "控制:\n";
  std::cout << "  W/S - 前进/后退 (Z轴)\n";
  std::cout << "  A/D - 左移/右移 (X轴)\n";
  std::cout << "  Q/E - 上升/下降 (Y轴)\n";
  std::cout << "  ESC 或 Ctrl+C - 退出游戏\n";
  std::cout << "\n";

  // 显示连接状态
  std::cout << "连接状态: ";
  if (g_connected) {
    std::cout << "🟢 已连接\n";
  } else {
    std::cout << "🔴 未连接\n";
  }
  std::cout << "\n";

  // 显示当前玩家信息
  std::cout << "👤 " << g_current_player_name << " 的位置: "
            << format_position(g_player_x.load(), g_player_y.load(),
                               g_player_z.load())
            << "\n";
  std::cout << "\n";

  // 显示其他玩家
  std::cout << "其他玩家:\n";
  std::cout << "────────────────────────────────────────────────\n";

  std::lock_guard<std::mutex> lock(g_players_mutex);
  if (g_other_players.empty()) {
    std::cout << "  (暂无其他玩家在线)\n";
  } else {
    for (const auto& [player_id, player_data] : g_other_players) {
      const auto& pos = player_data.position();
      std::cout << "  🤖 " << player_id << ": "
                << format_position(pos.x(), pos.y(), pos.z()) << "\n";
    }
  }

  std::cout << "\n";
  std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
  std::cout << "按 WASD/QE 移动，ESC 退出" << std::flush;
}

// 处理键盘输入
void handle_input() {
  const float move_speed = 1.0F;
  char ch;

#ifdef _WIN32
  // Windows: 使用 _kbhit() 和 _getch()
  while (_kbhit()) {
    ch = _getch();
#else
  // Linux/Unix: 使用 read()
  while (read(STDIN_FILENO, &ch, 1) > 0) {
#endif
    switch (ch) {
      case 'w':
      case 'W':
        g_player_z.store(g_player_z.load() + move_speed);
        break;
      case 's':
      case 'S':
        g_player_z.store(g_player_z.load() - move_speed);
        break;
      case 'a':
      case 'A':
        g_player_x.store(g_player_x.load() - move_speed);
        break;
      case 'd':
      case 'D':
        g_player_x.store(g_player_x.load() + move_speed);
        break;
      case 'q':
      case 'Q':
        g_player_y.store(g_player_y.load() + move_speed);
        break;
      case 'e':
      case 'E':
        g_player_y.store(g_player_y.load() - move_speed);
        break;
      case 27:  // ESC key
        g_running = false;
        break;
      case 3:  // Ctrl+C
        g_running = false;
        break;
    }
  }
}

// 发送玩家数据的线程
void player_data_sender_thread(Client& client, const std::string& player_id) {
  while (g_running && g_connected) {
    if (client.isConnected()) {
      PlayerData data;
      data.set_player_id(player_id);
      data.set_scene_id("wasd_game_scene");

      // 设置位置
      auto* pos = data.mutable_position();
      pos->set_x(g_player_x.load());
      pos->set_y(g_player_y.load());
      pos->set_z(g_player_z.load());

      // 设置旋转（固定值）
      auto* rot = data.mutable_rotation();
      rot->set_x(0.0F);
      rot->set_y(0.0F);
      rot->set_z(0.0F);
      rot->set_w(1.0F);

      // 设置时间戳
      data.set_timestamp(
          std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::system_clock::now().time_since_epoch())
              .count());

      // 发送数据
      client.sendPlayerData(data);
    }

    // 每秒发送20次（50ms间隔）
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
}

// UI 更新线程
void ui_update_thread() {
  while (g_running) {
    draw_game_ui();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));  // 10 FPS
  }
}

auto main(int argc, char* argv[]) -> int {
  // 初始化日志系统
  logger::Logger::Init("wasd_game", "./logs", logger::LogLevel::INFO, 10, true);

  // 设置信号处理
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);

  // 获取用户名（在设置终端模式之前）
  std::string username = get_username();
  g_current_player_name = username;  // 设置全局变量

  // 现在保存当前终端设置，然后设置为游戏模式
#ifndef _WIN32
  tcgetattr(STDIN_FILENO, &g_old_tio);
#endif
  setup_terminal();
  hide_cursor();

  // 在程序退出时恢复终端设置
  std::atexit([]() {
    show_cursor();
    restore_terminal();
  });

  // 解析命令行参数
  std::string server_address = "127.0.0.1:11451";
  std::string player_id = username;  // 使用用户输入的用户名作为玩家ID
  std::string token = "secure_production_token_change_me_2025";

  if (argc >= 2) {
    server_address = argv[1];
  }
  if (argc >= 3) {
    token = argv[2];  // 如果提供了命令行参数，第二个参数是token
  }

  // 显示启动信息
  clear_screen();
  std::cout << "🚀 启动 PICO Radar WASD Game...\n";
  std::cout << "玩家: " << username << "\n";
  std::cout << "服务器地址: " << server_address << "\n";
  std::cout << "正在连接...\n";

  try {
    // 创建客户端
    Client client;

    // 设置玩家列表更新回调
    client.setOnPlayerListUpdate(
        [&player_id](const std::vector<PlayerData>& players) {
          std::lock_guard<std::mutex> lock(g_players_mutex);
          g_other_players.clear();

          for (const auto& player : players) {
            // 不显示自己
            if (player.player_id() != player_id) {
              g_other_players[player.player_id()] = player;
            }
          }
        });

    // 连接到服务器
    auto future = client.connect(server_address, player_id, token);
    future.get();

    g_connected = true;

    std::cout << "✅ 连接成功！\n";
    std::cout << "启动游戏界面...\n";
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // 启动数据发送线程
    std::thread sender_thread(player_data_sender_thread, std::ref(client),
                              player_id);

    // 启动UI更新线程
    std::thread ui_thread(ui_update_thread);

    // 主游戏循环 - 处理输入
    while (g_running) {
      handle_input();
      std::this_thread::sleep_for(
          std::chrono::milliseconds(16));  // ~60 FPS input polling
    }

    // 清理
    g_connected = false;

    clear_screen();
    std::cout << "正在断开连接...\n";

    // 等待线程结束
    if (sender_thread.joinable()) {
      sender_thread.join();
    }
    if (ui_thread.joinable()) {
      ui_thread.join();
    }

    // 断开连接
    client.disconnect();

    std::cout << "游戏结束，感谢游玩！\n";

  } catch (const std::exception& e) {
    clear_screen();
    std::cerr << "❌ 错误: " << e.what() << '\n';
    std::cerr << "\n请确保:\n";
    std::cerr << "1. PICO Radar 服务器正在运行\n";
    std::cerr << "2. 服务器地址正确: " << server_address << "\n";
    std::cerr << "3. 网络连接正常\n";

    return 1;
  }

  return 0;
}
