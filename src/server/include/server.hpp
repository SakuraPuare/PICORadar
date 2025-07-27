#pragma once

#include <boost/asio/io_context.hpp>
#include <memory>
#include <thread>
#include <vector>

namespace net = boost::asio;

namespace picoradar {
namespace core {
class PlayerRegistry;
}
namespace network {
class WebsocketServer;
class UdpDiscoveryServer;
}  // namespace network

namespace server {

class Server {
 public:
  Server();
  ~Server();

  void start(uint16_t port, int thread_count);
  void stop() const;

  // Method to get player count for testing
  [[nodiscard]] auto getPlayerCount() const -> size_t;

 private:
  std::unique_ptr<net::io_context> ioc_;
  std::shared_ptr<core::PlayerRegistry> registry_;
  std::shared_ptr<network::WebsocketServer> ws_server_;
  std::shared_ptr<network::UdpDiscoveryServer> discovery_server_;
  std::vector<std::thread> server_threads_;
};

}  // namespace server
}  // namespace picoradar
