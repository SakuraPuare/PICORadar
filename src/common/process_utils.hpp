#pragma once

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

}  // namespace picoradar::common
