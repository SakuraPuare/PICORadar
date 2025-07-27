#include "client_impl.hpp"

#include <sstream>

#include "client.pb.h"
#include "common/logging.hpp"
#include "common/platform_fixes.hpp"
#include "server.pb.h"

namespace picoradar::client {

Client::Impl::Impl()
    : ioc_(std::make_unique<net::io_context>()),
      resolver_(std::make_unique<tcp::resolver>(*ioc_)),
      state_(ClientState::Disconnected),
      write_in_progress_(false) {
  LOG_DEBUG << "Client::Impl created";
}

Client::Impl::~Impl() {
  LOG_DEBUG << "Client::Impl destroying";

  // 如果有pending的promise，先设置异常避免promise析构时的问题
  if (get_state() == ClientState::Connecting) {
    safe_set_promise_exception(std::make_exception_ptr(
        std::runtime_error("Client is being destroyed")));
  }

  disconnect();
}

void Client::Impl::setOnPlayerListUpdate(Client::PlayerListCallback callback) {
  std::lock_guard lock(state_mutex_);
  player_list_callback_ = std::move(callback);
  LOG_DEBUG << "Player list callback set";
}

std::future<void> Client::Impl::connect(const std::string& server_address,
                                        const std::string& player_id,
                                        const std::string& token) {
  std::lock_guard lock(state_mutex_);

  if (get_state() != ClientState::Disconnected) {
    auto promise = std::promise<void>();
    auto future = promise.get_future();
    promise.set_exception(std::make_exception_ptr(std::runtime_error(
        "Client is not in disconnected state. Call disconnect() first.")));
    return future;
  }

  // 确保之前的网络线程已经完全退出
  if (network_thread_.joinable()) {
    ioc_->stop();
    network_thread_.join();
  }

  // 重置连接状态
  connect_promise_ = std::promise<void>();
  connect_promise_set_ = false;
  auto future = connect_promise_.get_future();

  player_id_ = player_id;
  token_ = token;

  // 解析服务器地址
  auto [host, port] = parse_address(server_address);

  // 重新创建io_context和相关组件以确保状态清洁
  ioc_ = std::make_unique<net::io_context>();
  resolver_ = std::make_unique<tcp::resolver>(*ioc_);
  ws_ = std::make_unique<websocket::stream<beast::tcp_stream>>(*ioc_);

  // 设置 WebSocket 选项
  ws_->set_option(
      websocket::stream_base::timeout::suggested(beast::role_type::client));

  ws_->set_option(
      websocket::stream_base::decorator([](websocket::request_type& req) {
        req.set(beast::http::field::user_agent, "PICORadar-Client/1.0");
      }));

  set_state(ClientState::Connecting);

  // 启动网络线程
  network_thread_ = std::thread(&Client::Impl::run_network_thread, this);

  // 为DNS解析设置超时
  auto resolve_timer = std::make_shared<net::steady_timer>(*ioc_);
  resolve_timer->expires_after(std::chrono::seconds(3));
  resolve_timer->async_wait([this, resolve_timer](beast::error_code ec) {
    if (!ec && get_state() == ClientState::Connecting) {
      LOG_ERROR << "DNS resolution timeout";
      safe_set_promise_exception(std::make_exception_ptr(
          std::runtime_error("DNS resolution timeout")));
      if (ioc_) {
        ioc_->stop();
      }
    }
  });

  // 开始异步解析
  resolver_->async_resolve(
      host, port,
      [this, resolve_timer](beast::error_code ec,
                            tcp::resolver::results_type results) {
        resolve_timer->cancel();  // 取消超时定时器
        handle_resolve(ec, results);
      });

  LOG_INFO << "Starting connection to " << server_address;
  return future;
}

void Client::Impl::disconnect() {
  LOG_INFO << "Disconnecting client";

  // 如果正在连接过程中，设置连接promise为异常
  if (get_state() == ClientState::Connecting) {
    safe_set_promise_exception(std::make_exception_ptr(
        std::runtime_error("Connection cancelled by disconnect")));
  }

  set_state(ClientState::Disconnecting);

  if (ioc_) {
    // 在 io_context 中执行关闭操作，确保在正确的线程中执行
    net::post(*ioc_, [this] { close_connection(); });

    // 停止 io_context
    ioc_->stop();
  }

  // 等待网络线程退出
  if (network_thread_.joinable()) {
    network_thread_.join();
    LOG_DEBUG << "Network thread joined";
  }

  // 重置状态
  set_state(ClientState::Disconnected);

  // 清理资源
  ws_.reset();
  if (ioc_) {
    ioc_->restart();
  }

  LOG_INFO << "Client disconnected";
}

void Client::Impl::sendPlayerData(const PlayerData& data) {
  if (get_state() != ClientState::Connected) {
    // 静默忽略，如需求文档所述
    return;
  }

  // 创建客户端消息
  ClientToServer client_msg;
  *client_msg.mutable_player_data() = data;

  // 序列化
  std::string serialized;
  if (!client_msg.SerializeToString(&serialized)) {
    LOG_ERROR << "Failed to serialize PlayerData";
    return;
  }

  // 添加到写队列
  {
    std::lock_guard lock(write_queue_mutex_);
    write_queue_.push(std::move(serialized));
  }

  // 触发写操作
  if (ioc_) {
    net::post(*ioc_, [this] { do_write(); });
  }
}

bool Client::Impl::isConnected() const {
  return get_state() == ClientState::Connected;
}

void Client::Impl::run_network_thread() {
  LOG_DEBUG << "Network thread started";

  try {
    ioc_->run();
  } catch (const std::exception& e) {
    LOG_ERROR << "Exception in network thread: " << e.what();

    // 如果连接过程中出现异常，设置 promise
    try {
      safe_set_promise_exception(std::current_exception());
    } catch (...) {
      // Promise 可能已经被设置
    }
  }

  LOG_DEBUG << "Network thread finished";
}

void Client::Impl::handle_resolve(beast::error_code ec,
                                  tcp::resolver::results_type results) {
  try {
    if (ec) {
      LOG_ERROR << "Resolve failed: " << ec.message();
      safe_set_promise_exception(std::make_exception_ptr(
          std::runtime_error("DNS resolution failed: " + ec.message())));
      return;
    }

    LOG_DEBUG << "DNS resolution successful";

    // 为TCP连接设置超时
    auto connect_timer = std::make_shared<net::steady_timer>(*ioc_);
    connect_timer->expires_after(std::chrono::seconds(3));
    connect_timer->async_wait([this, connect_timer](beast::error_code ec) {
      if (!ec && get_state() == ClientState::Connecting) {
        LOG_ERROR << "TCP connection timeout";
        safe_set_promise_exception(std::make_exception_ptr(
            std::runtime_error("TCP connection timeout")));
        if (ioc_) {
          ioc_->stop();
        }
      }
    });

    // 开始连接
    beast::get_lowest_layer(*ws_).expires_after(std::chrono::seconds(3));
    beast::get_lowest_layer(*ws_).async_connect(
        results, [this, connect_timer](
                     beast::error_code ec,
                     tcp::resolver::results_type::endpoint_type endpoint) {
          connect_timer->cancel();  // 取消超时定时器
          handle_connect(ec, endpoint);
        });
  } catch (const std::exception& e) {
    LOG_ERROR << "Exception in handle_resolve: " << e.what();
    try {
      safe_set_promise_exception(std::current_exception());
    } catch (...) {
      // Promise 可能已经被设置
    }
  } catch (...) {
    LOG_ERROR << "Unknown exception in handle_resolve";
    try {
      safe_set_promise_exception(std::make_exception_ptr(
          std::runtime_error("Unknown exception in DNS resolution")));
    } catch (...) {
      // Promise 可能已经被设置
    }
  }
}

void Client::Impl::handle_connect(
    beast::error_code ec, tcp::resolver::results_type::endpoint_type endpoint) {
  try {
    if (ec) {
      LOG_ERROR << "TCP connect failed: " << ec.message();
      safe_set_promise_exception(std::make_exception_ptr(
          std::runtime_error("TCP connection failed: " + ec.message())));
      return;
    }

    LOG_DEBUG << "TCP connection established to " << endpoint;

    // 关闭超时
    beast::get_lowest_layer(*ws_).expires_never();

    // 设置WebSocket握手超时
    ws_->next_layer().expires_after(std::chrono::seconds(2));

    // 进行 WebSocket 握手
    ws_->async_handshake(
        endpoint.address().to_string() + ":" + std::to_string(endpoint.port()),
        "/", [this](beast::error_code ec) { handle_handshake(ec); });
  } catch (const std::exception& e) {
    LOG_ERROR << "Exception in handle_connect: " << e.what();
    try {
      safe_set_promise_exception(std::current_exception());
    } catch (...) {
      // Promise 可能已经被设置
    }
  } catch (...) {
    LOG_ERROR << "Unknown exception in handle_connect";
    try {
      safe_set_promise_exception(std::make_exception_ptr(
          std::runtime_error("Unknown exception in TCP connection")));
    } catch (...) {
      // Promise 可能已经被设置
    }
  }
}

void Client::Impl::handle_handshake(beast::error_code ec) {
  try {
    if (ec) {
      LOG_ERROR << "WebSocket handshake failed: " << ec.message();
      safe_set_promise_exception(std::make_exception_ptr(
          std::runtime_error("WebSocket handshake failed: " + ec.message())));
      return;
    }

    LOG_DEBUG << "WebSocket handshake successful";

    // 设置为二进制模式以处理Protocol Buffers数据
    ws_->binary(true);

    // 关闭超时
    ws_->next_layer().expires_never();

    // 开始读取消息（在发送认证请求之前）
    start_read();

    // 发送认证请求
    send_auth_request();
  } catch (const std::exception& e) {
    LOG_ERROR << "Exception in handle_handshake: " << e.what();
    try {
      safe_set_promise_exception(std::current_exception());
    } catch (...) {
      // Promise 可能已经被设置
    }
  } catch (...) {
    LOG_ERROR << "Unknown exception in handle_handshake";
    try {
      safe_set_promise_exception(std::make_exception_ptr(
          std::runtime_error("Unknown exception in WebSocket handshake")));
    } catch (...) {
      // Promise 可能已经被设置
    }
  }
}

void Client::Impl::send_auth_request() {
  LOG_DEBUG << "Sending authentication request";

  // 创建认证请求
  ClientToServer client_msg;
  auto* auth_req = client_msg.mutable_auth_request();
  auth_req->set_player_id(player_id_);
  auth_req->set_token(token_);

  // 序列化
  std::string serialized;
  if (!client_msg.SerializeToString(&serialized)) {
    LOG_ERROR << "Failed to serialize auth request";
    safe_set_promise_exception(std::make_exception_ptr(
        std::runtime_error("Failed to serialize authentication request")));
    return;
  }

  // 发送
  ws_->async_write(net::buffer(serialized),
                   [this](beast::error_code ec, std::size_t bytes_transferred) {
                     handle_auth_write(ec, bytes_transferred);
                   });
}

void Client::Impl::handle_auth_write(beast::error_code ec,
                                     std::size_t bytes_transferred) {
  if (ec) {
    LOG_ERROR << "Auth write failed: " << ec.message();
    safe_set_promise_exception(std::make_exception_ptr(std::runtime_error(
        "Failed to send authentication request: " + ec.message())));
    return;
  }

  LOG_DEBUG << "Authentication request sent (" << bytes_transferred
            << " bytes)";
}

void Client::Impl::start_read() {
  ws_->async_read(read_buffer_,
                  [this](beast::error_code ec, std::size_t bytes_transferred) {
                    handle_read(ec, bytes_transferred);
                  });
}

void Client::Impl::handle_read(beast::error_code ec,
                               std::size_t bytes_transferred) {
  try {
    if (ec) {
      if (ec == websocket::error::closed) {
        LOG_INFO << "WebSocket connection closed by server";
      } else {
        LOG_ERROR << "Read failed: " << ec.message();
      }

      // 如果还在连接过程中，设置异常
      if (get_state() == ClientState::Connecting) {
        try {
          safe_set_promise_exception(std::make_exception_ptr(std::runtime_error(
              "Connection lost during handshake: " + ec.message())));
        } catch (...) {
          // Promise 可能已经被设置
        }
      }

      return;
    }

    // 处理收到的消息
    std::string message = beast::buffers_to_string(read_buffer_.data());
    read_buffer_.consume(bytes_transferred);

    LOG_DEBUG << "Received message (" << bytes_transferred << " bytes)";

    process_server_message(message);

    // 继续读取
    if (get_state() != ClientState::Disconnecting) {
      start_read();
    }
  } catch (const std::exception& e) {
    LOG_ERROR << "Exception in handle_read: " << e.what();
    if (get_state() == ClientState::Connecting) {
      try {
        safe_set_promise_exception(std::current_exception());
      } catch (...) {
        // Promise 可能已经被设置
      }
    }
  } catch (...) {
    LOG_ERROR << "Unknown exception in handle_read";
    if (get_state() == ClientState::Connecting) {
      try {
        safe_set_promise_exception(std::make_exception_ptr(
            std::runtime_error("Unknown exception in message handling")));
      } catch (...) {
        // Promise 可能已经被设置
      }
    }
  }
}

void Client::Impl::process_server_message(const std::string& message) {
  ServerToClient server_msg;
  if (!server_msg.ParseFromString(message)) {
    LOG_ERROR << "Failed to parse server message";
    return;
  }

  if (server_msg.has_auth_response()) {
    const auto& auth_resp = server_msg.auth_response();
    LOG_DEBUG << "Received auth response: success=" << auth_resp.success()
              << ", message=" << auth_resp.message();

    if (auth_resp.success()) {
      set_state(ClientState::Connected);
      safe_set_promise_value();
      LOG_INFO << "Authentication successful";
    } else {
      set_state(ClientState::Disconnected);  // 设置为断开状态
      safe_set_promise_exception(std::make_exception_ptr(
          std::runtime_error("Authentication failed: " + auth_resp.message())));
      LOG_ERROR << "Authentication failed: " << auth_resp.message();
    }
  } else if (server_msg.has_player_list()) {
    if (get_state() == ClientState::Connected && player_list_callback_) {
      const auto& player_list = server_msg.player_list();
      std::vector<PlayerData> players(player_list.players().begin(),
                                      player_list.players().end());

      LOG_DEBUG << "Received player list with " << players.size() << " players";

      try {
        player_list_callback_(players);
      } catch (const std::exception& e) {
        LOG_ERROR << "Exception in player list callback: " << e.what();
      }
    }
  }
}

void Client::Impl::do_write() {
  std::string message;

  // 使用作用域控制锁的生命周期
  {
    std::lock_guard lock(write_queue_mutex_);

    if (write_in_progress_ || write_queue_.empty() ||
        get_state() != ClientState::Connected) {
      return;
    }

    write_in_progress_ = true;
    message = std::move(write_queue_.front());
    write_queue_.pop();
  }  // 锁在这里自动释放

  // 在锁释放后进行异步写操作
  ws_->async_write(net::buffer(message),
                   [this](beast::error_code ec, std::size_t bytes_transferred) {
                     handle_write(ec, bytes_transferred);
                   });
}

void Client::Impl::handle_write(beast::error_code ec,
                                std::size_t bytes_transferred) {
  {
    std::lock_guard lock(write_queue_mutex_);
    write_in_progress_ = false;
  }

  if (ec) {
    LOG_ERROR << "Write failed: " << ec.message();
    return;
  }

  LOG_DEBUG << "Message sent (" << bytes_transferred << " bytes)";

  // 继续处理队列中的消息
  do_write();
}

void Client::Impl::close_connection() {
  if (ws_) {
    try {
      if (ws_->is_open()) {
        LOG_DEBUG << "Closing WebSocket connection";

        // 使用异步关闭来避免阻塞
        ws_->async_close(
            websocket::close_code::normal, [](beast::error_code ec) {
              if (ec) {
                LOG_DEBUG << "WebSocket close completed with error: "
                          << ec.message();
              } else {
                LOG_DEBUG << "WebSocket closed successfully";
              }
            });
      }
    } catch (const std::exception& e) {
      LOG_ERROR << "Exception during WebSocket close: " << e.what();
    } catch (...) {
      LOG_ERROR << "Unknown exception during WebSocket close";
    }
  }
}

void Client::Impl::set_state(ClientState new_state) { state_.store(new_state); }

ClientState Client::Impl::get_state() const { return state_.load(); }

std::pair<std::string, std::string> Client::Impl::parse_address(
    const std::string& address) {
  auto colon_pos = address.find_last_of(':');
  if (colon_pos == std::string::npos) {
    throw std::invalid_argument(
        "Invalid server address format. Expected 'host:port'");
  }

  std::string host = address.substr(0, colon_pos);
  std::string port = address.substr(colon_pos + 1);

  if (host.empty() || port.empty()) {
    throw std::invalid_argument(
        "Invalid server address format. Host and port cannot be empty");
  }

  return {host, port};
}

template <typename T>
void Client::Impl::safe_set_promise_value(T&& value) {
  if (!connect_promise_set_.exchange(true)) {
    try {
      connect_promise_.set_value(std::forward<T>(value));
    } catch (...) {
      // Promise already set, ignore
    }
  }
}

void Client::Impl::safe_set_promise_value() {
  if (!connect_promise_set_.exchange(true)) {
    try {
      connect_promise_.set_value();
    } catch (...) {
      // Promise already set, ignore
    }
  }
}

void Client::Impl::safe_set_promise_exception(std::exception_ptr ex) {
  if (!connect_promise_set_.exchange(true)) {
    try {
      connect_promise_.set_exception(ex);
    } catch (...) {
      // Promise already set, ignore
    }
  }
}

}  // namespace picoradar::client
