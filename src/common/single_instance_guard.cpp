#include "single_instance_guard.hpp"

#include <fstream>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <system_error>
#include <unordered_set>

#include "process_utils.hpp"

#ifdef _WIN32
#include <processthreadsapi.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>

#include <cstdlib>
#endif

namespace picoradar::common {

// 进程内锁状态跟踪
static std::mutex process_locks_mutex;
static std::unordered_set<std::string> active_locks;

// ... (get_temp_dir_path 辅助函数保持不变) ...
auto get_temp_dir_path() -> std::string {
#ifdef _WIN32
  char temp_path[MAX_PATH];
  if (GetTempPathA(MAX_PATH, temp_path) == 0) {
    return ".";
  }
  return std::string(temp_path);
#else
  const char* temp_dir = getenv("TMPDIR");
  if (temp_dir == nullptr) {
    temp_dir = "/tmp";
  }
  return std::string(temp_dir);
#endif
}

// 辅助函数：从锁文件中读取PID
auto read_pid_from_lockfile(const std::string& path) -> ProcessId {
  std::ifstream file(path);
  if (!file.is_open()) {
    return 0;
  }
  ProcessId pid = 0;
  file >> pid;
  return pid;
}

#ifdef _WIN32
// --- Windows 实现 (带陈旧锁检测) ---
SingleInstanceGuard::SingleInstanceGuard(const std::string& lock_file_name) {
  lock_file_path_ = get_temp_dir_path() + "\\" + lock_file_name;

  // 首先检查进程内是否已经有这个锁
  {
    std::lock_guard<std::mutex> lock(process_locks_mutex);
    if (active_locks.find(lock_file_path_) != active_locks.end()) {
      throw std::runtime_error(
          "PICO Radar server is already running in this process.");
    }
  }

  for (int i = 0; i < 2; ++i) {  // 最多重试一次
    file_handle_ = CreateFileA(
        lock_file_path_.c_str(), GENERIC_WRITE,
        0,  // 独占访问
        NULL, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE,  // 自动删除
        NULL);

    if (file_handle_ != INVALID_HANDLE_VALUE) {
      // 成功获取锁
      std::string pid_str = std::to_string(GetCurrentProcessId());
      DWORD bytes_written;
      WriteFile(file_handle_, pid_str.c_str(), pid_str.length(), &bytes_written,
                NULL);

      // 添加到进程内活动锁集合
      {
        std::lock_guard<std::mutex> lock(process_locks_mutex);
        active_locks.insert(lock_file_path_);
      }

      return;  // 成功，退出构造函数
    }

    // 如果获取锁失败
    if (GetLastError() != ERROR_SHARING_VIOLATION) {
      throw std::runtime_error(
          "Failed to create lock file for unknown reason.");
    }

    // 文件已被锁定，检查PID是否为陈旧的
    ProcessId pid = read_pid_from_lockfile(lock_file_path_);
    if (pid > 0 && !is_process_running(pid)) {
      // 进程不存在，是陈旧锁
      DeleteFileA(lock_file_path_.c_str());  // 删除它然后重试
      continue;
    }

    // 进程仍在运行
    throw std::runtime_error("PICO Radar server is already running.");
  }
}

SingleInstanceGuard::~SingleInstanceGuard() {
  if (file_handle_ != INVALID_HANDLE_VALUE) {
    // 从进程内活动锁集合中移除
    {
      std::lock_guard<std::mutex> lock(process_locks_mutex);
      active_locks.erase(lock_file_path_);
    }

    CloseHandle(file_handle_);
    // FILE_FLAG_DELETE_ON_CLOSE 会自动处理删除
  }
}

#else
// --- POSIX 实现 (带陈旧锁检测) ---
SingleInstanceGuard::SingleInstanceGuard(const std::string& lock_file_name) {
  lock_file_path_ = get_temp_dir_path() + "/" + lock_file_name;

  // 首先检查进程内是否已经有这个锁
  {
    std::lock_guard<std::mutex> lock(process_locks_mutex);
    if (active_locks.find(lock_file_path_) != active_locks.end()) {
      throw std::runtime_error(
          "PICO Radar server is already running in this process.");
    }
  }

  for (int i = 0; i < 2; ++i) {  // 最多重试一次
    file_descriptor_ = open(lock_file_path_.c_str(), O_RDWR | O_CREAT, 0666);
    if (file_descriptor_ < 0) {
      throw std::system_error(errno, std::system_category(),
                              "Failed to open lock file");
    }

    struct flock lock_info = {0};
    lock_info.l_type = F_WRLCK;
    lock_info.l_whence = SEEK_SET;
    lock_info.l_start = 0;
    lock_info.l_len = 0;

    if (fcntl(file_descriptor_, F_SETLK, &lock_info) == 0) {
      // 成功获取锁
      const std::string pid_str = std::to_string(getpid());
      if (ftruncate(file_descriptor_, 0) != 0) { /* ignore */
      }
      if (write(file_descriptor_, pid_str.c_str(), pid_str.length()) ==
          -1) { /* ignore */
      }

      // 添加到进程内活动锁集合
      {
        std::lock_guard<std::mutex> lock(process_locks_mutex);
        active_locks.insert(lock_file_path_);
      }

      return;  // 成功，退出构造函数
    }

    // 如果获取锁失败
    if (errno != EWOULDBLOCK && errno != EAGAIN) {
      close(file_descriptor_);
      throw std::system_error(errno, std::system_category(),
                              "Failed to acquire lock");
    }

    // 文件已被锁定，检查PID是否为陈旧的
    const ProcessId pid = read_pid_from_lockfile(lock_file_path_);
    close(file_descriptor_);  // 关闭当前句柄，因为我们没有获得锁

    if (pid > 0 && !is_process_running(pid)) {
      // 进程不存在，是陈旧锁
      unlink(lock_file_path_.c_str());  // 删除它然后重试
      continue;                         // 继续重试
    }

    // 进程仍在运行，立即抛出异常
    throw std::runtime_error("PICO Radar server is already running.");
  }

  // 如果到这里，说明两次重试都失败了
  throw std::runtime_error("PICO Radar server is already running.");
}

SingleInstanceGuard::~SingleInstanceGuard() {
  if (file_descriptor_ != -1) {
    // 从进程内活动锁集合中移除
    {
      std::lock_guard<std::mutex> lock(process_locks_mutex);
      active_locks.erase(lock_file_path_);
    }

    struct flock lock_info = {0};
    lock_info.l_type = F_UNLCK;
    lock_info.l_whence = SEEK_SET;
    lock_info.l_start = 0;
    lock_info.l_len = 0;
    fcntl(file_descriptor_, F_SETLK, &lock_info);
    close(file_descriptor_);
    unlink(lock_file_path_.c_str());
  }
}
#endif

}  // namespace picoradar::common
