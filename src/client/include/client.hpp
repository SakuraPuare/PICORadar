#pragma once

#include <functional>
#include <future>
#include <memory>
#include <string>
#include <vector>

#include "player.pb.h"

namespace picoradar::client {

/**
 * @brief PICO Radar 客户端库
 *
 * 这是一个完全异步的客户端库，用于连接到 PICO Radar 服务器。
 * 所有网络操作都是非阻塞的，提供简洁而健壮的 API。
 *
 * 特性：
 * - 完全异步操作
 * - 线程安全的公共接口
 * - 自动服务器发现（可选）
 * - WebSocket 连接管理
 * - 自动认证处理
 * - 实时数据传输
 *
 * 使用示例：
 * @code
 * Client client;
 * client.setOnPlayerListUpdate([](const auto& players) {
 *     // 处理玩家列表更新
 * });
 *
 * auto future = client.connect("127.0.0.1:11451", "player1", "token");
 * future.get(); // 等待连接完成
 *
 * PlayerData data;
 * data.set_player_id("player1");
 * // ... 设置位置数据
 * client.sendPlayerData(data);
 *
 * client.disconnect();
 * @endcode
 */
class Client {
 public:
  /**
   * @brief 玩家列表更新回调函数类型
   *
   * 当从服务器收到新的玩家列表时，此回调函数将被调用。
   *
   * @warning 回调函数在内部网络线程中执行，必须保持简短且非阻塞。
   *          如需执行耗时操作，请将数据分发到其他线程处理。
   *
   * @param players 当前的玩家列表
   */
  using PlayerListCallback =
      std::function<void(const std::vector<PlayerData>&)>;

  /**
   * @brief 构造函数
   *
   * 仅初始化内部状态，不启动任何线程或网络活动。
   * 构造函数是轻量且快速的。
   */
  Client();

  /**
   * @brief 析构函数
   *
   * 自动调用 disconnect() 以确保所有资源被正确释放。
   * 这是一个阻塞操作，会等待所有内部线程安全退出。
   */
  ~Client();

  // 禁止拷贝和移动
  Client(const Client&) = delete;
  auto operator=(const Client&) -> Client& = delete;
  Client(Client&&) = delete;
  auto operator=(Client&&) -> Client& = delete;

  /**
   * @brief 设置玩家列表更新回调
   *
   * @param callback 当收到玩家列表更新时要调用的回调函数
   *
   * @note 此方法必须在调用 connect() 之前调用
   * @thread_safety 线程安全
   */
  void setOnPlayerListUpdate(PlayerListCallback callback);

  /**
   * @brief 异步连接到服务器
   *
   * 启动完全异步的连接和认证流程：
   * 1. 启动内部 io_context 线程
   * 2. 解析服务器地址
   * 3. 建立 TCP 连接
   * 4. 进行 WebSocket 握手
   * 5. 发送认证请求
   * 6. 等待认证响应
   *
   * @param server_address 服务器地址，格式为 "ip:port" (e.g.,
   * "127.0.0.1:11451")
   * @param player_id 玩家唯一标识符
   * @param token 认证令牌
   * @return std::future<void> 异步操作的 future，可用于等待连接完成或检查错误
   *
   * @throws std::runtime_error 通过 future 抛出，包含错误详情
   * @thread_safety 线程安全
   *
   * @note 在成功的 connect() 后，必须先调用 disconnect() 才能再次 connect()
   */
  auto connect(const std::string& server_address, const std::string& player_id,
               const std::string& token) const -> std::future<void>;

  /**
   * @brief 断开与服务器的连接
   *
   * 启动同步的断开流程：
   * 1. 向 io_context 提交关闭 WebSocket 的任务
   * 2. 停止 io_context
   * 3. 等待内部网络线程退出
   *
   * @thread_safety 线程安全
   *
   * @note 此方法是阻塞的，确保返回时所有资源都已释放
   */
  void disconnect() const;

  /**
   * @brief 发送玩家数据到服务器
   *
   * 这是一个"即发即忘"操作，将玩家数据异步发送到服务器。
   *
   * @param data 要发送的玩家数据
   *
   * @note 只有在客户端处于"已连接并认证"状态时，数据才会被实际发送。
   *       在其他状态下（如正在连接、已断开），调用将被静默忽略。
   * @thread_safety 线程安全
   */
  void sendPlayerData(const PlayerData& data);

  /**
   * @brief 检查客户端是否已连接
   *
   * @return true 如果客户端已连接并认证成功
   * @return false 如果客户端未连接或正在连接过程中
   *
   * @thread_safety 线程安全
   */
  [[nodiscard]] auto isConnected() const -> bool;

 private:
  class Impl;
  std::unique_ptr<Impl> pimpl_;
};

}  // namespace picoradar::client
