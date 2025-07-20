#include "websocket_server.hpp"

#include <glog/logging.h>

#include <boost/asio/post.hpp>
#include <boost/asio/signal_set.hpp>
#include <chrono>

#include "player_data.pb.h"

namespace picoradar {
namespace network {

void fail(beast::error_code ec, char const* what) {
  if (ec == net::error::operation_aborted || ec == websocket::error::closed ||
      ec == net::error::connection_reset) {
    return;
  }
  LOG(ERROR) << what << ": " << ec.message();
}

class Session : public std::enable_shared_from_this<Session> {
  WebsocketServer& server_;
  websocket::stream<beast::tcp_stream> ws_;
  beast::flat_buffer buffer_;
  std::vector<std::shared_ptr<const std::string>> write_queue_;
  core::PlayerRegistry& registry_;
  const std::string secret_token_;
  bool is_authenticated_ = false;
  std::string player_id_;

 public:
  Session(tcp::socket&& socket, core::PlayerRegistry& registry,
          std::string secret_token, WebsocketServer& server)
      : server_(server),
        ws_(std::move(socket)),
        registry_(registry),
        secret_token_(std::move(secret_token)) {}

  ~Session() {
    server_.unregister_session(shared_from_this());
    if (!player_id_.empty()) {
      registry_.removePlayer(player_id_);
    }
  }

  void run() {
    ws_.set_option(
        websocket::stream_base::timeout::suggested(beast::role_type::server));
    ws_.set_option(
        websocket::stream_base::decorator([](websocket::response_type& res) {
          res.set(http::field::server, std::string(BOOST_BEAST_VERSION_STRING) +
                                           " picoradar-websocket-server");
        }));
    ws_.async_accept(
        beast::bind_front_handler(&Session::on_accept, shared_from_this()));
  }

  void on_accept(beast::error_code ec) {
    if (ec) return fail(ec, "accept");
    do_read();
  }

  void do_read() {
    ws_.async_read(buffer_, beast::bind_front_handler(&Session::on_read,
                                                      shared_from_this()));
  }

  void on_read(beast::error_code ec, std::size_t) {
    if (ec) {
      return fail(ec, "read");
    }

    if (!is_authenticated_) {
      handle_auth();
    } else {
      handle_player_data();
    }
  }

  void handle_auth() {
    ClientToServer message;
    if (!message.ParseFromArray(buffer_.data().data(), buffer_.size())) {
      buffer_.consume(buffer_.size());
      do_read();
      return;
    }
    buffer_.consume(buffer_.size());

    if (!message.has_auth_request()) {
      do_read();
      return;
    }

    const auto& auth_request = message.auth_request();
    ServerToClient response;
    auto* auth_response = response.mutable_auth_response();

    if (auth_request.token() == secret_token_ && !auth_request.player_id().empty()) {
      is_authenticated_ = true;
      player_id_ = auth_request.player_id(); // Store the real player ID
      auth_response->set_success(true);
      LOG(INFO) << "Player " << player_id_ << " authenticated successfully.";
    } else {
      is_authenticated_ = false;
      auth_response->set_success(false);
    }
    auto ss = std::make_shared<const std::string>(response.SerializeAsString());
    send(ss);
  }

  void handle_player_data() {
    ClientToServer message;
    if (message.ParseFromArray(buffer_.data().data(), buffer_.size()) && message.has_player_data()) {
      auto player_data = message.player_data();
      player_data.set_player_id(player_id_);
      registry_.updatePlayer(player_data);
    }
    buffer_.consume(buffer_.size());
    do_read();
  }

  void send(std::shared_ptr<const std::string> const& ss) {
    net::post(ws_.get_executor(),
              beast::bind_front_handler(&Session::on_send, shared_from_this(),
                                        ss));
  }

 private:
  void on_send(std::shared_ptr<const std::string> const& ss) {
    write_queue_.push_back(ss);
    if (write_queue_.size() > 1) return;
    do_write();
  }

  void do_write() {
    if (write_queue_.empty()) return;
    ws_.binary(true);
    ws_.async_write(
        net::buffer(*write_queue_.front()),
        beast::bind_front_handler(&Session::on_write, shared_from_this()));
  }

  void on_write(beast::error_code ec, std::size_t) {
    if (ec) return fail(ec, "write");

    write_queue_.erase(write_queue_.begin());

    if (!write_queue_.empty()) {
      do_write();
    } else if (is_authenticated_) {
      // If we just finished writing and there's nothing else to write,
      // go back to reading.
      do_read();
    }
  }

 public:
  bool is_authenticated() const { return is_authenticated_; }
};

// ... (Listener and WebsocketServer remain the same) ...
class Listener : public std::enable_shared_from_this<Listener> {
  WebsocketServer& server_;
  net::io_context& ioc_;
  tcp::acceptor acceptor_;

 public:
  Listener(net::io_context& ioc, tcp::endpoint endpoint, WebsocketServer& server)
      : server_(server), ioc_(ioc), acceptor_(ioc) {
    beast::error_code ec;
    acceptor_.open(endpoint.protocol(), ec);
    if (ec) throw beast::system_error{ec};
    acceptor_.set_option(net::socket_base::reuse_address(true), ec);
    if (ec) throw beast::system_error{ec};
    acceptor_.bind(endpoint, ec);
    if (ec) throw beast::system_error{ec};
    acceptor_.listen(net::socket_base::max_listen_connections, ec);
    if (ec) throw beast::system_error{ec};
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
    auto session = std::make_shared<Session>(
        std::move(socket), server_.registry_, server_.secret_token_, server_);
    server_.register_session(session);
    session->run();
    do_accept();
  }
};

WebsocketServer::WebsocketServer(core::PlayerRegistry& registry,
                                 std::string secret_token)
    : registry_(registry), secret_token_(std::move(secret_token)) {}

WebsocketServer::~WebsocketServer() {
  if (is_running_.load()) {
    stop();
  }
}

void WebsocketServer::run(const std::string& address_str, uint16_t port,
                          int threads_count) {
  is_running_ = true;
  auto const address = net::ip::make_address(address_str);
  listener_ =
      std::make_shared<Listener>(ioc_, tcp::endpoint{address, port}, *this);
  listener_->run();
  LOG(INFO) << "Server started on " << address_str << ":" << port;
  threads_.reserve(threads_count);
  for (int i = 0; i < threads_count; ++i) {
    threads_.emplace_back([this] { ioc_.run(); });
  }
  broadcast_thread_ = std::thread(&WebsocketServer::broadcast_loop, this);
}

void WebsocketServer::stop() {
  is_running_ = false;
  ioc_.stop();
  if (broadcast_thread_.joinable()) {
    broadcast_thread_.join();
  }
  for (auto& thread : threads_) {
    if (thread.joinable()) {
      thread.join();
    }
  }
  LOG(INFO) << "Server stopped.";
}

void WebsocketServer::register_session(std::shared_ptr<Session> session) {
  std::lock_guard<std::mutex> lock(sessions_mutex_);
  sessions_.insert(session);
}

void WebsocketServer::unregister_session(std::shared_ptr<Session> session) {
  std::lock_guard<std::mutex> lock(sessions_mutex_);
  sessions_.erase(session);
}

void WebsocketServer::broadcast_loop() {
  while (is_running_) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto const players = registry_.getAllPlayers();
    if (players.empty()) {
      continue;
    }

    ServerToClient message;
    auto* player_list = message.mutable_player_list();
    for (const auto& player : players) {
      player_list->add_players()->CopyFrom(player);
    }
    
    auto const ss = std::make_shared<const std::string>(message.SerializeAsString());

    std::vector<std::shared_ptr<Session>> recipients;
    {
      std::lock_guard<std::mutex> lock(sessions_mutex_);
      recipients.reserve(sessions_.size());
      for (auto const& session : sessions_) {
        if (session->is_authenticated()) {
          recipients.push_back(session);
        }
      }
    }

    for (auto const& recipient : recipients) {
      recipient->send(ss);
    }
  }
}

}  // namespace network
}  // namespace picoradar
