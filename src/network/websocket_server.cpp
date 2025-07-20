#include "websocket_server.hpp"

#include <glog/logging.h>

#include <boost/asio/signal_set.hpp>

#include "player_data.pb.h"

namespace picoradar {
namespace network {

// 报告错误
void fail(beast::error_code ec, char const* what) {
  LOG(ERROR) << what << ": " << ec.message();
}

// 处理单个WebSocket会话
class Session : public std::enable_shared_from_this<Session> {
  websocket::stream<beast::tcp_stream> ws_;
  beast::flat_buffer buffer_;
  core::PlayerRegistry& registry_;
  const std::string secret_token_;
  bool is_authenticated_ = false;

 public:
  Session(tcp::socket&& socket, core::PlayerRegistry& registry,
          std::string secret_token)
      : ws_(std::move(socket)),
        registry_(registry),
        secret_token_(std::move(secret_token)) {}

  void run() {
    // 我们需要在strand上执行所有操作，以确保线程安全
    net::dispatch(
        ws_.get_executor(),
        beast::bind_front_handler(&Session::on_run, shared_from_this()));
  }

  void on_run() {
    // 设置建议的超时选项
    ws_.set_option(
        websocket::stream_base::timeout::suggested(beast::role_type::server));

    // 设置一个装饰器，用于添加服务器HTTP头
    ws_.set_option(
        websocket::stream_base::decorator([](websocket::response_type& res) {
          res.set(http::field::server, std::string(BOOST_BEAST_VERSION_STRING) +
                                           " picoradar-websocket-server");
        }));

    // 接受WebSocket握手
    ws_.async_accept(
        beast::bind_front_handler(&Session::on_accept, shared_from_this()));
  }

  void on_accept(beast::error_code ec) {
    if (ec) return fail(ec, "accept");

    LOG(INFO)
        << "Client connected: "
        << ws_.next_layer().socket().remote_endpoint().address().to_string();

    // 开始读取消息
    do_read();
  }

  void do_read() {
    ws_.async_read(buffer_, beast::bind_front_handler(&Session::on_read,
                                                      shared_from_this()));
  }

  void on_read(beast::error_code ec, std::size_t bytes_transferred) {
    boost::ignore_unused(bytes_transferred);

    if (ec == websocket::error::closed) {
      LOG(INFO) << "Client disconnected.";
      // TODO: 从 registry_ 中移除玩家
      return;
    }

    if (ec) return fail(ec, "read");

    // 检查是否已认证
    if (!is_authenticated_) {
      handle_auth();
    } else {
      handle_player_data();
    }
  }

  void handle_auth() {
    ClientToServer message;
    if (!message.ParseFromArray(buffer_.data().data(), buffer_.size())) {
      LOG(WARNING)
          << "Failed to parse message from client. Closing connection.";
      ws_.close(websocket::close_code::policy_error);
      return;
    }

    if (!message.has_auth_request()) {
      LOG(WARNING) << "Client sent non-auth message before authenticating. "
                      "Closing connection.";
      ws_.close(websocket::close_code::policy_error);
      return;
    }

    const auto& auth_request = message.auth_request();
    if (auth_request.token() == secret_token_) {
      LOG(INFO) << "Client authenticated successfully.";
      is_authenticated_ = true;

      // TODO: 发送 AuthResponse

      // 清空缓冲区并继续读取
      buffer_.consume(buffer_.size());
      do_read();
    } else {
      LOG(WARNING) << "Client sent invalid token. Closing connection.";
      // TODO: 发送失败的 AuthResponse
      ws_.close(websocket::close_code::policy_error);
    }
  }

  void handle_player_data() {
    // TODO:
    // 1. 反序列化 buffer_ 中的消息
    // 2. 处理消息
    // 3. 更新 registry_

    // 现在只做回显
    ws_.text(ws_.got_text());
    ws_.async_write(
        buffer_.data(),
        beast::bind_front_handler(&Session::on_write, shared_from_this()));
  }

  void on_write(beast::error_code ec, std::size_t bytes_transferred) {
    boost::ignore_unused(bytes_transferred);

    if (ec) return fail(ec, "write");

    // 清空缓冲区并继续读取
    buffer_.consume(buffer_.size());
    do_read();
  }
};

// 接受新的连接
class Listener : public std::enable_shared_from_this<Listener> {
  net::io_context& ioc_;
  tcp::acceptor acceptor_;
  core::PlayerRegistry& registry_;
  const std::string secret_token_;

 public:
  Listener(net::io_context& ioc, tcp::endpoint endpoint,
           core::PlayerRegistry& registry, std::string secret_token)
      : ioc_(ioc),
        acceptor_(ioc),
        registry_(registry),
        secret_token_(std::move(secret_token)) {
    beast::error_code ec;

    acceptor_.open(endpoint.protocol(), ec);
    if (ec) {
      fail(ec, "open");
      throw beast::system_error{ec};
    }

    acceptor_.set_option(net::socket_base::reuse_address(true), ec);
    if (ec) {
      fail(ec, "set_option");
      throw beast::system_error{ec};
    }

    acceptor_.bind(endpoint, ec);
    if (ec) {
      fail(ec, "bind");
      throw beast::system_error{ec};
    }

    acceptor_.listen(net::socket_base::max_listen_connections, ec);
    if (ec) {
      fail(ec, "listen");
      throw beast::system_error{ec};
    }
  }

  void run() { do_accept(); }

 private:
  void do_accept() {
    acceptor_.async_accept(
        net::make_strand(ioc_),
        beast::bind_front_handler(&Listener::on_accept, shared_from_this()));
  }

  void on_accept(beast::error_code ec, tcp::socket socket) {
    if (ec) return fail(ec, "accept");

    // 创建一个新的会话并运行它
    std::make_shared<Session>(std::move(socket), registry_, secret_token_)
        ->run();

    // 继续接受下一个连接
    do_accept();
  }
};

// --- WebsocketServer 实现 ---

WebsocketServer::WebsocketServer(core::PlayerRegistry& registry,
                                 std::string secret_token)
    : registry_(registry), secret_token_(std::move(secret_token)) {}

WebsocketServer::~WebsocketServer() {
  if (!ioc_.stopped()) {
    stop();
  }
}

void WebsocketServer::run(const std::string& address_str, uint16_t port,
                          int threads_count) {
  auto const address = net::ip::make_address(address_str);

  // 创建并运行 Listener
  listener_ = std::make_shared<Listener>(ioc_, tcp::endpoint{address, port},
                                         registry_, secret_token_);
  listener_->run();

  LOG(INFO) << "Server started on " << address_str << ":" << port;

  // 创建工作线程池
  threads_.reserve(threads_count);
  for (int i = 0; i < threads_count; ++i) {
    threads_.emplace_back([this] { ioc_.run(); });
  }
}

void WebsocketServer::stop() {
  LOG(INFO) << "Stopping server...";

  // 停止 io_context 将导致所有异步操作完成并退出 run()
  ioc_.stop();

  // 等待所有线程完成
  for (auto& thread : threads_) {
    if (thread.joinable()) {
      thread.join();
    }
  }

  LOG(INFO) << "Server stopped.";
}

}  // namespace network
}  // namespace picoradar
