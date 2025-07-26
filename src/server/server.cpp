#include "server.hpp"
#include "core/player_registry.hpp"
#include "network/websocket_server.hpp"
#include "common/constants.hpp"
#include "common/logging.hpp"


namespace picoradar::server {

Server::Server() {
    ioc_ = std::make_unique<net::io_context>();
    registry_ = std::make_shared<core::PlayerRegistry>();
    ws_server_ = std::make_shared<network::WebsocketServer>(*ioc_, *registry_);
}

Server::~Server() {
    stop();
}

void Server::start(uint16_t port, int thread_count) {
    const std::string address = "0.0.0.0";
    ws_server_->start(address, port, thread_count);
    LOG_INFO << "Test server started on port " << port;
}

void Server::stop() {
    ws_server_->stop();
    LOG_INFO << "Test server stopped.";
}

auto Server::getPlayerCount() -> size_t {
    return registry_->getPlayerCount();
}

} // namespace picoradar::server

