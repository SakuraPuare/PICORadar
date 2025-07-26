#pragma once

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <memory>
#include <queue>
#include <set>
#include <string>
#include <thread>
#include <utility>
#include "core/player_registry.hpp"
#include "player.pb.h"

namespace beast = boost::beast;
namespace net = boost::asio;
namespace websocket = beast::websocket;
using tcp = net::ip::tcp;

namespace picoradar::network {

class WebsocketServer;  // Forward declaration

// Handles a single WebSocket connection
class Session : public std::enable_shared_from_this<Session> {
  websocket::stream<beast::tcp_stream> ws_;
  beast::flat_buffer buffer_;
  WebsocketServer& server_;
  std::string player_id_;
  std::queue<std::string> write_queue_;
  net::strand<net::any_io_executor> strand_;

 public:
  Session(tcp::socket&& socket, WebsocketServer& server);

  // Start the asynchronous operation
  void run();

  void do_read();
  void on_read(beast::error_code ec, std::size_t bytes_transferred);
  void on_accept(beast::error_code ec);
  void on_close(beast::error_code ec);
  void close();

  // Method to send a message to the client
  void send(const std::string& message);
  void on_write(beast::error_code ec, std::size_t bytes_transferred);

  // Getters and setters for player_id
  auto getPlayerId() const -> const std::string& { return player_id_; }
  void setPlayerId(const std::string& id) { player_id_ = id; }

 private:
  void do_write();
  void do_accept();
};

// Accepts incoming connections and launches the sessions
class Listener : public std::enable_shared_from_this<Listener> {
  net::io_context& ioc_;
  tcp::acceptor acceptor_;
  tcp::socket socket_;
  WebsocketServer& server_;

 public:
  Listener(net::io_context& ioc, const tcp::endpoint& endpoint,
           WebsocketServer& server)
      : ioc_(ioc), acceptor_(ioc), socket_(ioc), server_(server) {
    beast::error_code ec;
    acceptor_.open(endpoint.protocol(), ec);
    acceptor_.set_option(net::socket_base::reuse_address(true), ec);
    acceptor_.bind(endpoint, ec);
    acceptor_.listen(net::socket_base::max_listen_connections, ec);
  }

  void run() { do_accept(); }

  void stop() { acceptor_.close(); }

 private:
  void do_accept() {
    acceptor_.async_accept(
        socket_,
        beast::bind_front_handler(&Listener::on_accept, shared_from_this()));
  }

  void on_accept(beast::error_code ec);
};

class WebsocketServer {
public:
    WebsocketServer(net::io_context& ioc, core::PlayerRegistry& registry);
    ~WebsocketServer();

    void start(const std::string& address, uint16_t port, int thread_count);
    void stop();

    void onSessionOpened(const std::shared_ptr<Session>& session);
    void onSessionClosed(const std::shared_ptr<Session>& session);
    void processMessage(const std::shared_ptr<Session>& session,
                      const std::string& message);
    void broadcastPlayerList();

private:
    net::io_context& ioc_;
    core::PlayerRegistry& registry_;
    std::shared_ptr<Listener> listener_;
    std::set<std::shared_ptr<Session>> sessions_;
    std::vector<std::thread> threads_;
    bool is_running_ = false;
};

}  // namespace picoradar::network
