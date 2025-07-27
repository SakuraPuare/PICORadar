#include "process_utils.hpp"

#include <array>
#include <stdexcept>
#include <string>
#include <vector>

#include "logging.hpp"
#include "platform_fixes.hpp"

#ifdef _WIN32
#include <tlhelp32.h>
#else
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstring>
#endif

namespace picoradar::common {

auto is_process_running(ProcessId pid) -> bool {
#ifdef _WIN32
  HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, pid);
  if (process == nullptr) {
    return false;
  }
  DWORD ret = WaitForSingleObject(process, 0);
  CloseHandle(process);
  return ret == WAIT_TIMEOUT;
#else
  if (pid == 0) {
    return false;
  }
  return kill(pid, 0) == 0;
#endif
}

#ifdef _WIN32
Process::Process(const std::string& executable_path,
                 const std::vector<std::string>& args) {
  std::string command_line = "\"" + executable_path + "\"";
  for (const auto& arg : args) {
    command_line += " " + arg;
  }

  STARTUPINFOA si;
  PROCESS_INFORMATION pi;

  ZeroMemory(&si, sizeof(si));
  si.cb = sizeof(si);
  ZeroMemory(&pi, sizeof(pi));

  if (!CreateProcessA(nullptr, &command_line[0], nullptr, nullptr, FALSE, 0,
                      nullptr, nullptr, &si, &pi)) {
    LOG_ERROR << "CreateProcess failed (" << GetLastError() << ").";
    throw std::runtime_error("Failed to create process");
  }

  process_handle_ = pi.hProcess;
  pid_ = pi.dwProcessId;
  CloseHandle(pi.hThread);
  LOG_INFO << "Process started. PID: " << *pid_;
}

Process::~Process() {
  if (isRunning()) {
    (void)terminate();
  }
}

auto Process::isRunning() const -> bool {
  if (!pid_) {
    return false;
  }
  return is_process_running(*pid_);
}

auto Process::terminate() -> bool {
  if (!isRunning()) {
    return true;
  }
  if (TerminateProcess(process_handle_, 1)) {
    LOG_INFO << "Process terminated. PID: " << *pid_;
    WaitForSingleObject(process_handle_, INFINITE);
    CloseHandle(process_handle_);
    pid_.reset();
    process_handle_ = nullptr;
    return true;
  }
  LOG_ERROR << "Failed to terminate process. PID: " << *pid_
            << ", Error: " << GetLastError();
  return false;
}

auto Process::waitForExit() -> std::optional<int> {
  if (!pid_) {
    return std::nullopt;
  }

  if (process_handle_ != nullptr) {
    WaitForSingleObject(process_handle_, INFINITE);
    DWORD exit_code;
    if (GetExitCodeProcess(process_handle_, &exit_code)) {
      CloseHandle(process_handle_);
      process_handle_ = nullptr;
      pid_.reset();
      return static_cast<int>(exit_code);
    }
    CloseHandle(process_handle_);
    process_handle_ = nullptr;
    pid_.reset();
  }
  return std::nullopt;
}

#else  // POSIX implementation
Process::Process(const std::string& executable_path,
                 const std::vector<std::string>& args) {
  std::array<int, 2> pipefd;
  if (pipe(pipefd.data()) == -1) {
    LOG_ERROR << "pipe() failed.";
    throw std::runtime_error("Failed to create pipe for process creation");
  }

  pid_ = fork();

  if (pid_ < 0) {
    LOG_ERROR << "fork() failed.";
    close(pipefd[0]);
    close(pipefd[1]);
    throw std::runtime_error("Failed to fork process");
  }

  if (pid_ == 0) {                          // Child process
    close(pipefd[0]);                       // Close read end in child
    fcntl(pipefd[1], F_SETFD, FD_CLOEXEC);  // Close pipe on exec

    std::vector<char*> c_args;
    c_args.push_back(const_cast<char*>(executable_path.c_str()));
    for (const auto& arg : args) {
      c_args.push_back(const_cast<char*>(arg.c_str()));
    }
    c_args.push_back(nullptr);

    execv(executable_path.c_str(), c_args.data());
    // If execv returns, it's an error
    const int error_code = errno;
    (void)write(pipefd[1], &error_code, sizeof(error_code));
    close(pipefd[1]);
    _exit(127);  // Standard exit code for command not found/failed exec
  }
  // Parent process
  close(pipefd[1]);  // Close write end in parent

  int error_code = 0;
  const ssize_t bytes_read = read(pipefd[0], &error_code, sizeof(error_code));
  close(pipefd[0]);

  if (bytes_read > 0) {
    // Child failed to exec
    pid_.reset();  // The process did not start successfully
    throw std::runtime_error("Failed to execute: " +
                             std::string(strerror(error_code)));
  }

  LOG_INFO << "Process started. PID: " << *pid_;
}

Process::~Process() {
  if (isRunning()) {
    (void)terminate();
  }
}

auto Process::isRunning() const -> bool {
  if (!pid_) {
    return false;
  }
  return is_process_running(*pid_);
}

auto Process::terminate() -> bool {
  if (!isRunning()) {
    return true;
  }

  if (kill(*pid_, SIGKILL) == 0) {
    LOG_INFO << "Process terminated. PID: " << *pid_;
    int status;
    waitpid(*pid_, &status, 0);  // Clean up zombie process
    pid_.reset();
    return true;
  }

  LOG_ERROR << "Failed to kill process with PID: " << *pid_;
  return false;
}

auto Process::waitForExit() -> std::optional<int> {
  if (!pid_) {
    return std::nullopt;
  }

  int status;
  if (waitpid(*pid_, &status, 0) != -1) {
    pid_.reset();
    if (WIFEXITED(status)) {
      return WEXITSTATUS(status);
    }
  } else {
    LOG_ERROR << "waitpid() failed for PID: " << *pid_;
  }

  return std::nullopt;
}
#endif

}  // namespace picoradar::common
