#pragma once

#include <atomic>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>

#include "client.hpp"

namespace beast = boost::beast;
namespace net = boost::asio;
namespace websocket = beast::websocket;
using tcp = net::ip::tcp;

namespace picoradar::client {

/**
 * @brief 客户端状态枚举
 */
enum class ClientState : std::uint8_t {
  Disconnected,  ///< 未连接
  Connecting,    ///< 正在连接
  Connected,     ///< 已连接并认证成功
  Disconnecting  ///< 正在断开连接
};

/**
 * @brief 客户端内部实现类
 *
 * 使用 Pimpl 模式隐藏复杂的异步逻辑和 Boost.Asio 依赖。
 * 所有网络操作都在单独的线程中运行，确保主线程不被阻塞。
 */
class Client::Impl {
 public:
  Impl();
  ~Impl();

  // 禁止拷贝和移动
  Impl(const Impl&) = delete;
  Impl& operator=(const Impl&) = delete;
  Impl(Impl&&) = delete;
  Impl& operator=(Impl&&) = delete;

  void setOnPlayerListUpdate(PlayerListCallback callback);
  std::future<void> connect(const std::string& server_address,
                            const std::string& player_id,
                            const std::string& token);
  void disconnect();
  void sendPlayerData(const PlayerData& data);
  bool isConnected() const;

 private:
  // 网络相关
  std::unique_ptr<net::io_context> ioc_;
  std::unique_ptr<websocket::stream<beast::tcp_stream>> ws_;
  std::unique_ptr<tcp::resolver> resolver_;

  // 线程管理
  std::thread network_thread_;
  mutable std::mutex state_mutex_;

  // 状态管理
  std::atomic<ClientState> state_;

  // 回调和 Promise
  PlayerListCallback player_list_callback_;
  std::promise<void> connect_promise_;
  std::atomic<bool> connect_promise_set_{false};

  // 消息队列和缓冲区
  beast::flat_buffer read_buffer_;
  std::queue<std::string> write_queue_;
  std::mutex write_queue_mutex_;
  bool write_in_progress_;

  // 认证信息
  std::string player_id_;
  std::string token_;

  // 内部方法
  void run_network_thread();
  void handle_resolve(beast::error_code ec,
                      tcp::resolver::results_type results);
  void handle_connect(beast::error_code ec,
                      tcp::resolver::results_type::endpoint_type endpoint);
  void handle_handshake(beast::error_code ec);
  void send_auth_request();
  void handle_auth_write(beast::error_code ec, std::size_t bytes_transferred);
  void start_read();
  void handle_read(beast::error_code ec, std::size_t bytes_transferred);
  void process_server_message(const std::string& message);
  void do_write();
  void handle_write(beast::error_code ec, std::size_t bytes_transferred);
  void close_connection();
  void set_state(ClientState new_state);
  ClientState get_state() const;

  // Promise安全设置辅助函数
  template <typename T>
  void safe_set_promise_value(T&& value);
  void safe_set_promise_value();  // 为void promise的特化版本
  void safe_set_promise_exception(std::exception_ptr ex);

  // 解析服务器地址
  std::pair<std::string, std::string> parse_address(const std::string& address);
};

}  // namespace picoradar::client
