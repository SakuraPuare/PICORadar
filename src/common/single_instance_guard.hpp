#pragma once

#include <string>

#ifdef _WIN32
#include <windows.h>
#else

#endif

namespace picoradar::common {

/**
 * @brief 一个用于确保应用程序只有一个实例在运行的类。
 *
 * 该类使用文件锁来实现单例保证，并且是跨平台的。
 * 它遵循RAII（资源获取即初始化）原则：
 * -
 * 在构造时，它会尝试锁定一个PID文件。如果锁定失败（意味着另一个实例已在运行），
 *   它会抛出一个异常。
 * - 在析构时，它会自动释放锁并删除PID文件。
 *
 * 用法:
 * int main() {
 *   try {
 *     SingleInstanceGuard guard("my_app.pid");
 *     // ... 应用程序的其余逻辑 ...
 *   } catch (const std::runtime_error& e) {
 *     std::cerr << e.what() << std::endl;
 *     return 1;
 *   }
 *   return 0;
 * }
 */
class SingleInstanceGuard {
 public:
  /**
   * @brief 构造函数，尝试获取实例锁。
   * @param lock_file_name PID文件的名称。它将被创建在临时目录中。
   * @throws std::runtime_error 如果另一个实例已在运行。
   */
  explicit SingleInstanceGuard(const std::string& lock_file_name);

  ~SingleInstanceGuard();

  // 禁止拷贝和赋值
  SingleInstanceGuard(const SingleInstanceGuard&) = delete;
  auto operator=(const SingleInstanceGuard&) -> SingleInstanceGuard& = delete;

 private:
  std::string lock_file_path_;

#ifdef _WIN32
  HANDLE file_handle_ = INVALID_HANDLE_VALUE;
#else
  int file_descriptor_ = -1;
#endif
};

}  // namespace picoradar::common
