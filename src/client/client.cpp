#include "client.hpp"

#include "common/logging.hpp"
#include "impl/client_impl.hpp"

namespace picoradar::client {

Client::Client() : pimpl_(std::make_unique<Impl>()) {
  LOG_DEBUG << "Client created";
}

Client::~Client() {
  LOG_DEBUG << "Client destroying";
  // pimpl_ 的析构函数会自动调用 disconnect()
}

void Client::setOnPlayerListUpdate(PlayerListCallback callback) {
  pimpl_->setOnPlayerListUpdate(std::move(callback));
}

std::future<void> Client::connect(const std::string& server_address,
                                  const std::string& player_id,
                                  const std::string& token) const {
  LOG_INFO << "Client connecting to " << server_address
           << " with player_id: " << player_id;

  return pimpl_->connect(server_address, player_id, token);
}

void Client::disconnect() const {
  LOG_INFO << "Client disconnecting";
  pimpl_->disconnect();
}

void Client::sendPlayerData(const PlayerData& data) {
  pimpl_->sendPlayerData(data);
}

bool Client::isConnected() const { return pimpl_->isConnected(); }

}  // namespace picoradar::client
