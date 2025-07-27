#pragma once

#include <memory>
#include <string>

// 前向声明
namespace picoradar::server {
class CLIInterface;
}

namespace picoradar::server {

/**
 * @brief 简化的CLI日志适配器
 *
 * 这个新版本大幅简化了原有的CLILogAdapter，
 * 将大部分逻辑集成到了Logger类中
 */
class CLILogAdapter {
 public:
  /**
   * @brief 初始化CLI日志输出
   * @param cli_interface CLI界面实例
   */
  static void initialize(const std::shared_ptr<CLIInterface>& cli_interface);

  /**
   * @brief 关闭CLI日志输出
   */
  static void shutdown();

  /**
   * @brief 检查CLI输出是否已启用
   * @return 是否启用
   */
  static auto isEnabled() -> bool;

 private:
  static bool enabled_;
};

}  // namespace picoradar::server
