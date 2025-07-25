#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "player_data.pb.h"  // Protobuf 生成的代码

namespace picoradar::core {

class PlayerRegistry {
 public:
  PlayerRegistry();
  ~PlayerRegistry();

  // 禁止拷贝和赋值
  PlayerRegistry(const PlayerRegistry&) = delete;
  auto operator=(const PlayerRegistry&) -> PlayerRegistry& = delete;

  /**
   * @brief 添加或更新一个玩家的数据。
   *
   * 如果玩家ID已存在，则更新其数据；否则，添加为新玩家。
   * 此方法是线程安全的。
   * @param data 玩家数据
   */
  void updatePlayer(const std::string& playerId,
                    const picoradar::PlayerData& data);

  /**
   * @brief 移除一个玩家。
   *
   * @param playerId 要移除的玩家ID
   */
  void removePlayer(const std::string& playerId);

  /**
   * @brief 获取所有当前玩家数据的快照。
   *
   * @return 包含所有玩家数据的map引用
   */
  auto getAllPlayers() const
      -> const std::unordered_map<std::string, picoradar::PlayerData>&;

  /**
   * @brief 获取特定ID的玩家数据。
   *
   * @param playerId 玩家ID
   * @return 如果找到，返回玩家数据的unique_ptr；否则返回nullptr。
   */
  auto getPlayer(const std::string& playerId) const
      -> std::unique_ptr<picoradar::PlayerData>;

  /**
   * @brief 获取当前玩家数量。
   */
  auto getPlayerCount() const -> size_t;

 private:
  // 使用unordered_map以获得O(1)的平均查找效率
  std::unordered_map<std::string, picoradar::PlayerData> players_;

  // 使用mutable的mutex以允许在const成员函数中锁定
  mutable std::mutex mutex_;
};

}  // namespace picoradar::core
