#pragma once

#include <optional>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/types.h>

#endif

namespace picoradar::common {

#ifdef _WIN32
using ProcessId = DWORD;
#else
using ProcessId = pid_t;
#endif

/**
 * @brief 检查具有给定ID的进程当前是否正在运行。
 * @param pid 要检查的进程ID。
 * @return 如果进程正在运行，则为true；否则为false。
 */
auto is_process_running(ProcessId pid) -> bool;

/**
 * @class Process
 * @brief 以跨平台的方式管理子进程的生命周期。
 *
 * 此类为启动和停止外部可执行文件提供了RAII风格的包装器。
 * 创建Process对象时，它会启动指定的程序。
 * 当对象被销毁时，它会自动终止子进程以防止产生孤儿进程。
 */
class Process {
 public:
  /**
   * @brief 构造一个Process对象并启动可执行文件。
   * @param executable_path 要运行的可执行文件的路径。
   * @param args 要传递给可执行文件的字符串参数向量。
   */
  Process(const std::string& executable_path,
          const std::vector<std::string>& args);

  /**
   * @brief 析构函数。确保被管理的进程被终止。
   */
  ~Process();

  // 禁用复制和移动语义以防止对进程所有权的混淆。
  Process(const Process&) = delete;
  Process(Process&&) = delete;
  auto operator=(const Process&) -> Process& = delete;
  auto operator=(Process&&) -> Process& = delete;

  /**
   * @brief 检查被管理的进程当前是否正在运行。
   * @return 如果进程正在运行，则为true；否则为false。
   */
  [[nodiscard]] auto isRunning() const -> bool;

  /**
   * @brief 强制终止被管理的进程。
   * @return 如果终止信号发送成功，则为true；否则为false。
   */
  [[nodiscard]] auto terminate() -> bool;

  /**
   * @brief 等待被管理的进程执行完毕，并获取其退出码。
   * @return
   * 进程的退出码。如果进程仍在运行或无法获取退出码，则返回std::nullopt。
   */
  auto waitForExit() -> std::optional<int>;

 private:
  std::optional<ProcessId> pid_;
#ifdef _WIN32
  HANDLE process_handle_{nullptr};
#endif
};

}  // namespace picoradar::common
