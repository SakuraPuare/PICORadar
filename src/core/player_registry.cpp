#include "player_registry.hpp"

namespace picoradar::core {

using namespace picoradar::core;

PlayerRegistry::PlayerRegistry() = default;

PlayerRegistry::~PlayerRegistry() = default;

void PlayerRegistry::updatePlayer(std::string playerId,
                                  picoradar::PlayerData data) {
  std::lock_guard lock(mutex_);
  players_[std::move(playerId)] = std::move(data);
}

void PlayerRegistry::removePlayer(std::string playerId) {
  std::lock_guard lock(mutex_);
  players_.erase(playerId);
}

auto PlayerRegistry::getAllPlayers() const
    -> std::unordered_map<std::string, picoradar::PlayerData> {
  std::lock_guard lock(mutex_);
  return players_;  // 返回副本而非引用，线程安全
}

auto PlayerRegistry::getPlayer(const std::string& playerId) const
    -> std::unique_ptr<picoradar::PlayerData> {
  std::lock_guard lock(mutex_);
  auto it = players_.find(playerId);
  if (it != players_.end()) {
    return std::make_unique<picoradar::PlayerData>(it->second);
  }
  return nullptr;
}

auto PlayerRegistry::getPlayerCount() const -> size_t {
  std::lock_guard lock(mutex_);
  return players_.size();
}

}  // namespace picoradar::core
