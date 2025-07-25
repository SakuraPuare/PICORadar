#pragma once

#include <atomic>  // Added for std::atomic
#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <memory>
#include <mutex>  // Added for std::mutex
#include <string>
#include <thread>

#include "common/constants.hpp"
#include "player_data.pb.h"

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;

namespace picoradar::client {

/**
 * @brief Represents the smoothed visual state of a player for interpolation.
 */
struct VisualPlayerState {
  std::string player_id;
  // TODO(sakurapuare): Use a proper 3D vector/quaternion library (e.g., GLM)y
  // (e.g., GLM)y (e.g., GLM) For now, using PlayerData sub-structs for position
  // and rotation
  Vector3 from_pos;
  Vector3 to_pos;
  Quaternion from_rot;
  Quaternion to_rot;

  double last_update_time_s = 0.0;  // Time of the last server update in seconds
  float interpolation_alpha = 0.0F;  // Current interpolation factor [0.0, 1.0]
};

class Client {
 public:
  Client();
  ~Client();

  // 禁止拷贝和赋值
  Client(const Client&) = delete;
  auto operator=(const Client&) -> Client& = delete;

  /**
   * @brief 初始化客户端并连接到服务器
   * @param host 服务器主机地址
   * @param port 服务器端口
   * @return 连接是否成功
   */
  void connect(const std::string& host, const std::string& port);

  /**
   * @brief 通过UDP广播发现服务器
   * @param discovery_port 发现端口，默认为 kDefaultDiscoveryPort
   * @return 发现的服务器地址，格式为"host:port"
   */
  static auto discover_server(
      uint16_t discovery_port = config::kDefaultDiscoveryPort) -> std::string;

  /**
   * @brief 断开与服务器的连接
   */
  void disconnect();

  /**
   * @brief 发送玩家数据到服务器
   * @param player_data 玩家数据
   * @return 发送是否成功
   */
  auto send_player_data(const picoradar::PlayerData& player_data) -> bool;

  /**
   * @brief 设置玩家ID
   * @param player_id 玩家ID
   */
  void set_player_id(const std::string& player_id);

  /**
   * @brief 获取玩家ID
   * @return 玩家ID字符串
   */
  auto get_player_id() const -> const std::string&;

  /**
   * @brief 设置认证令牌
   * @param token 认证令牌
   */
  void set_auth_token(const std::string& token);
  // 获取认证令牌
  auto get_auth_token() const -> const std::string&;

  // 获取玩家列表
  auto get_player_list() const -> const picoradar::PlayerList&;

  /**
   * @brief Updates the interpolation for all players' visual states.
   * @param delta_time_s Time elapsed since the last frame in seconds.
   */
  void update_visual_state(float delta_time_s);

  // 获取所有玩家的平滑可视化状态
  auto get_visual_players() const
      -> const std::map<std::string, VisualPlayerState>&;

  /**
   * @bri -> boolef 检查客户端是auto连接
   * @return -> bool否已连接
   */
  auto is_connected() const -> bool;

  /**
   * @brief 检查客户端是否已通过身份验证
   * @return 如果已经过验证则返回 true
   */
  auto is_authenticated() const -> bool;

 private:
  /**
   * @brief 开始读取来自服务器的消息
   */
  void start_read();

  /**
   * @brief 处理从服务器读取到的消息
   * @param ec 错误代码
   * @param bytes_transferred 传输的字节数
   */
  void on_read(beast::error_code ec, std::size_t bytes_transferred);

  /**
   * @brief 发送认证请求
   */
  void send_authentication_request();

  // 网络相关成员
  net::io_context ioc_;
  websocket::stream<tcp::socket> ws_;

  // 客户端状态
  std::string player_id_;
  std::string auth_token_;
  picoradar::PlayerList player_list_;
  std::atomic<bool> is_connected_;
  std::atomic<bool> is_authenticated_;

  // 线程安全
  mutable std::mutex player_list_mutex_;
  mutable std::mutex visual_players_mutex_;  // For the new visual players map

  // 异步处理
  std::thread io_thread_;
  std::unique_ptr<net::executor_work_guard<net::io_context::executor_type>>
      work_;

  // 读取缓冲区
  beast::flat_buffer read_buffer_;

  // 平滑处理
  std::map<std::string, VisualPlayerState> visual_players_;
};

}  // namespace picoradar::client