#include "server.hpp"

#include "common/config_manager.hpp"
#include "common/constants.hpp"
#include "common/logging.hpp"
#include "core/player_registry.hpp"
#include "network/udp_discovery_server.hpp"
#include "network/websocket_server.hpp"

namespace picoradar::server {

Server::Server() {
  ioc_ = std::make_unique<net::io_context>();
  registry_ = std::make_shared<core::PlayerRegistry>();
  ws_server_ = std::make_shared<network::WebsocketServer>(*ioc_, *registry_);
}

Server::~Server() { stop(); }

void Server::start(uint16_t port, const int thread_count) {
  const auto& config = common::ConfigManager::getInstance();

  // Get configuration values with fallbacks to constants
  const std::string address =
      config.getWithDefault("server.host", std::string("0.0.0.0"));
  uint16_t discovery_port = config.getDiscoveryPort();

  // Create and start UDP discovery server
  discovery_server_ = std::make_shared<network::UdpDiscoveryServer>(
      *ioc_, discovery_port, port, address);
  discovery_server_->start();

  // Start WebSocket server
  ws_server_->start(address, port, thread_count);
  LOG_INFO << "Server started - WebSocket on port " << port
           << ", UDP Discovery on port " << discovery_port;
}

void Server::stop() const {
  if (discovery_server_) {
    discovery_server_->stop();
  }
  ws_server_->stop();
  LOG_INFO << "Server stopped.";
}

auto Server::getPlayerCount() const -> size_t {
  return registry_->getPlayerCount();
}

auto Server::getConnectionCount() const -> size_t {
  return ws_server_ ? ws_server_->getConnectionCount() : 0;
}

auto Server::getMessagesReceived() const -> size_t {
  return ws_server_ ? ws_server_->getMessagesReceived() : 0;
}

auto Server::getMessagesSent() const -> size_t {
  return ws_server_ ? ws_server_->getMessagesSent() : 0;
}

}  // namespace picoradar::server
