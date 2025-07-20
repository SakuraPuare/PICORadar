#include "player_registry.hpp"

namespace picoradar {
namespace core {

PlayerRegistry::PlayerRegistry() {}

PlayerRegistry::~PlayerRegistry() {}

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

std::vector<picoradar::PlayerData> PlayerRegistry::getAllPlayers() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<picoradar::PlayerData> allPlayers;
  allPlayers.reserve(players_.size());  // 预分配内存以提高效率
  for (const auto& pair : players_) {
    allPlayers.push_back(pair.second);
  }
  return allPlayers;
}

std::unique_ptr<picoradar::PlayerData> PlayerRegistry::getPlayer(
    const std::string& playerId) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = players_.find(playerId);
  if (it != players_.end()) {
    return std::make_unique<picoradar::PlayerData>(it->second);
  }
  return nullptr;
}

size_t PlayerRegistry::getPlayerCount() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return players_.size();
}

}  // namespace core
}  // namespace picoradar
