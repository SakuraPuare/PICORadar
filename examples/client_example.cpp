#include <chrono>
#include <iostream>
#include <thread>

#include "client.hpp"
#include "common/logging.hpp"

using namespace picoradar::client;
using namespace picoradar;

int main() {
  // 初始化日志系统
  logger::LogConfig config = logger::LogConfig::loadFromConfigManager();
  config.log_directory = "./logs";
  config.global_level = logger::LogLevel::INFO;
  config.file_enabled = true;
  config.console_enabled = true;
  config.max_files = 10;
  logger::Logger::Init("client_example", config);

  std::cout << "=== PICORadar Client Library 使用示例 ===" << std::endl;

  // 创建客户端实例
  Client client;

  // 设置玩家列表更新回调
  client.setOnPlayerListUpdate([](const std::vector<PlayerData>& players) {
    std::cout << "收到玩家列表更新，玩家数量: " << players.size() << std::endl;

    for (const auto& player : players) {
      std::cout << "玩家 " << player.player_id() << " 位置: ("
                << player.position().x() << ", " << player.position().y()
                << ", " << player.position().z() << ")" << std::endl;
    }
  });

  try {
    std::cout << "正在连接到服务器..." << std::endl;

    // 异步连接到服务器
    auto future = client.connect("127.0.0.1:11451", "example_player",
                                 "pico_radar_secret_token");

    // 等待连接完成
    future.get();
    std::cout << "连接成功！" << std::endl;

    // 发送玩家数据
    for (int i = 0; i < 5; ++i) {
      PlayerData data;
      data.set_player_id("example_player");
      data.set_scene_id("example_scene");

      // 设置位置（模拟移动）
      auto* pos = data.mutable_position();
      pos->set_x(static_cast<float>(i));
      pos->set_y(0.0F);
      pos->set_z(0.0F);

      // 设置旋转
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
      std::cout << "发送位置数据: (" << pos->x() << ", " << pos->y() << ", "
                << pos->z() << ")" << std::endl;

      // 等待一段时间
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "等待 2 秒以接收更多数据..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // 断开连接
    client.disconnect();
    std::cout << "已断开连接" << std::endl;

  } catch (const std::exception& e) {
    std::cerr << "错误: " << e.what() << std::endl;
    return 1;
  }

  std::cout << "示例程序结束" << std::endl;
  return 0;
}
