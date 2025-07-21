#include "player_registry.hpp"

namespace picoradar::core {

PlayerRegistry::PlayerRegistry() = default;

PlayerRegistry::~PlayerRegistry() = default;

void PlayerRegistry::updatePlayer(const picoradar::PlayerData& data) {
  std::lock_guard<std::mutex> lock(mutex_);
  // unordered_map的operator[]会在key不存在时创建新元素，存在时则返回其引用
  // 这完美符合我们“添加或更新”的需求
  players_[data.player_id()] = data;
}

void PlayerRegistry::removePlayer(const std::string& playerId) {
  std::lock_guard<std::mutex> lock(mutex_);
  players_.erase(playerId);
}

auto PlayerRegistry::getAllPlayers() const
    -> std::vector<picoradar::PlayerData> {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<picoradar::PlayerData> allPlayers;
  allPlayers.reserve(players_.size());  // 预分配内存以提高效率
  for (const auto& pair : players_) {
    allPlayers.push_back(pair.second);
  }
  return allPlayers;
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
