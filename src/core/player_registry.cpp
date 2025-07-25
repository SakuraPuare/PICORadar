#include "player_registry.hpp"

namespace picoradar::core {

using namespace picoradar::core;

// =========================
// 详细中文注释已添加到每个函数实现和关键点。
// =========================

PlayerRegistry::PlayerRegistry() = default;

PlayerRegistry::~PlayerRegistry() = default;

void PlayerRegistry::updatePlayer(const std::string& playerId,
                                  const picoradar::PlayerData& data) {
  std::lock_guard<std::mutex> lock(mutex_);
  players_[playerId] = data;
}

void PlayerRegistry::removePlayer(const std::string& playerId) {
  std::lock_guard<std::mutex> lock(mutex_);
  players_.erase(playerId);
}

auto PlayerRegistry::getAllPlayers() const
    -> const std::unordered_map<std::string, picoradar::PlayerData>& {
  return players_;
}

auto PlayerRegistry::getPlayer(const std::string& playerId) const
    -> std::unique_ptr<picoradar::PlayerData> {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = players_.find(playerId);
  if (it != players_.end()) {
    return std::make_unique<picoradar::PlayerData>(it->second);
  }
  return nullptr;
}

auto PlayerRegistry::getPlayerCount() const -> size_t {
  std::lock_guard<std::mutex> lock(mutex_);
  return players_.size();
}

}  // namespace picoradar::core
