#pragma once

#include <atomic>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#include "core/player_registry.hpp"
#include "udp_discovery_server.hpp"

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

namespace picoradar {
namespace network {

class Listener;
class Session;

class WebsocketServer : public std::enable_shared_from_this<WebsocketServer> {
 public:
  WebsocketServer(core::PlayerRegistry& registry, std::string secret_token);
  ~WebsocketServer();

  WebsocketServer(const WebsocketServer&) = delete;
  WebsocketServer& operator=(const WebsocketServer&) = delete;

  void run(const std::string& address, uint16_t port, int threads);
  void stop();

  void register_session(std::shared_ptr<Session> session);
  void unregister_session(std::shared_ptr<Session> session);

 private:
  void broadcast_loop();

  friend class Listener;
  friend class Session;

  core::PlayerRegistry& registry_;
  net::io_context ioc_;
  std::shared_ptr<Listener> listener_;
  std::vector<std::thread> threads_;
  const std::string secret_token_;

  std::unique_ptr<UdpDiscoveryServer> discovery_server_;

  std::mutex sessions_mutex_;
  std::unordered_set<std::shared_ptr<Session>> sessions_;

  std::thread broadcast_thread_;
  std::atomic<bool> is_running_{false};
};

}  // namespace network
}  // namespace picoradar
