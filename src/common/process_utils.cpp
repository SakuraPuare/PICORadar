#include "process_utils.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#endif

namespace picoradar::common {

#ifdef _WIN32
// --- Windows 实现 ---
bool is_process_running(ProcessId pid) {
  if (pid == 0) {
    return false;
  }
  HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, pid);
  if (process == NULL) {
    return false;
  }
  CloseHandle(process);
  return true;
}
#else
// --- POSIX 实现 ---
auto is_process_running(ProcessId pid) -> bool {
  if (pid <= 0) {
    return false;
  }
  // kill(pid, 0) 是一个标准的POSIX技巧，用于检查进程是否存在而不发送信号。
  // 如果调用返回0，进程存在。
  // 如果返回-1且errno为EPERM，表示进程存在但我们没有权限发信号给它（例如，它由另一个用户拥有），这也算作“正在运行”。
  // 如果返回-1且errno为ESRCH，表示进程不存在。
  if (kill(pid, 0) == 0) {
    return true;
  }
  return errno == EPERM;
}
#endif

}  // namespace picoradar::common
