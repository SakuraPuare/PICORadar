# DevLog #6: 铸就坚不可摧的基石：从“能跑”到“绝对可靠”的深度探索

大家好，我是书樱！

在软件开发的旅途中，有时最深刻的进步并非来自添加光鲜亮丽的新功能，而是源于一次对“为什么会这样？”的刨根问底。今天，我想与大家分享的，就是这样一个故事。它始于一个“卡死”的测试，最终却引领我们为PICO Radar项目铸就了前所未有的、坚不可摧的健壮基石。

## 第一幕：脆弱的测试与“端口幽灵”

在我们为`SingleInstanceGuard`编写了自动化测试后，一个诡异的现象出现了：在本地和CI服务器上，测试有时会莫名其妙地卡住，直到超时失败。日志显示了一个我们既熟悉又陌生的错误：`bind: Address already in use`。

我们最初的诊断是，`ctest`并行运行了两个需要占用端口的测试，导致了冲突。我们通过设置测试依赖，强制它们串行执行。然而，问题依旧！即使串行，前一个测试刚一结束，后一个测试的服务器就因为端口仍处于系统的`TIME_WAIT`状态而启动失败。

我们的测试脚本就像在和一个“端口幽灵”赛跑，时赢时输。这暴露了一个致命的缺陷：**我们的测试本身是脆弱的、不可靠的。** 一个依赖于时序和运气的测试套件，在工程上是不可接受的。

## 第二幕：釜底抽薪——端口隔离法则

我们意识到，试图通过“等待”和“重试”来解决资源竞争问题，只是在给问题贴上创可贴。真正的解决方案必须是釜底抽薪：**为每一个需要网络资源的集成测试分配一个完全独立的端口。**

这是保证测试稳定性的黄金法则。它确保了测试之间“零”干扰，无论它们是并行还是串行运行。

为此，我们采取了两个关键步骤：

**1. 让服务器端口可配置：**
我们修改了`server_app/main.cpp`，使其可以从命令行接收一个端口号参数。

```cpp
// src/server_app/main.cpp (部分)
int main(int argc, char* argv[]) {
  // ...
  uint16_t port = 9002;  // 默认端口
  if (argc > 1) {
    try {
      port = std::stoi(argv[1]);
    } catch (const std::exception& e) {
      // ... 错误处理 ...
    }
  }
  // ...
  server->run(address, port, threads);
  // ...
}
```

**2. 更新测试脚本以使用独立端口：**
我们的集成测试脚本现在会为服务器和客户端指定一个唯一的端口。

```bash
# test/integration_tests/run_auth_fail_test.sh (部分)
PORT=9003 # AuthFailTest 使用 9003
# ...
"$SERVER_EXE" "$PORT" &
# ...
"$CLIENT_EXE" 127.0.0.1 "$PORT" "wrong-token" --test-auth-fail
```

```bash
# test/integration_tests/run_auth_success_test.sh (部分)
PORT=9004 # AuthSuccessTest 使用 9004
# ...
```

就这样，通过简单的参数化，我们彻底消灭了“端口幽灵”。我们的CI流水线终于稳定地全线飘绿。

## 第三幕：灵魂拷问与终极进化

此时，我们本可以庆祝胜利并继续开发新功能。但一个更深层次的问题萦绕在我们心头，也是由您——我们最敏锐的观察者——所提出的：

> “我们只是让**测试**变得健壮了。但服务器**程序本身**在真实世界中足够健壮吗？如果服务器崩溃了，留下一个‘僵尸’锁文件，那它岂不是永远无法再次启动了？”

这是一个直击灵魂的拷问。它促使我们开启了本次征程中最有价值的终极进化：**赋予`SingleInstanceGuard`处理“陈旧锁”的自我修复能力。**

### 核心武器：跨平台的进程检查

要处理陈旧锁，我们首先需要一个可靠的方法来检查锁文件里记录的PID是否还对应着一个活着的进程。为此，我们创建了一个跨平台的辅助函数`is_process_running`。

**代码深度解析 (`src/common/process_utils.cpp`):**

*   **POSIX (Linux, macOS) 实现:**
    ```cpp
    // kill(pid, 0) 是一个标准的POSIX技巧，
    // 它不发送任何信号，仅仅检查进程是否存在。
    // 如果返回-1且errno为EPERM，表示进程存在但我们没权限，也算“正在运行”。
    if (kill(pid, 0) == 0) {
        return true;
    }
    return errno == EPERM;
    ```
    这是一种非常高效且优雅的实现，利用了操作系统API的特定行为。

*   **Windows 实现:**
    ```cpp
    // 在Windows上，我们尝试获取进程的句柄。
    // 如果能成功获取，即使只是获取同步权限，也意味着进程存在。
    HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, pid);
    if (process == NULL) {
        return false;
    }
    CloseHandle(process); // 别忘了释放句柄
    return true;
    ```

### 终极进化：`SingleInstanceGuard` 的新逻辑

装备了`is_process_running`这个武器后，我们重写了`SingleInstanceGuard`的构造函数。其核心逻辑可以用以下伪代码清晰地描述：

```
// SingleInstanceGuard 构造函数伪代码
function SingleInstanceGuard(lock_file):
  // 尝试第一次获取锁
  success = try_acquire_lock(lock_file)

  if success:
    // 成功，写入当前PID并返回
    write_current_pid(lock_file)
    return

  // 如果获取锁失败，说明可能已有实例在运行
  // 读取锁文件中的旧PID
  old_pid = read_pid_from(lock_file)

  // 检查旧PID是否还在运行
  if is_process_running(old_pid):
    // 是的，真的有实例在运行，抛出异常
    throw "Instance already running!"
  else:
    // 否，这是一个“陈旧锁”！
    // 删除陈旧的锁文件
    delete_lock_file(lock_file)
    // 再次尝试获取锁
    success = try_acquire_lock(lock_file)
    if not success:
      // 如果这次还失败，那就是真的出问题了
      throw "Could not acquire lock after cleaning stale lock!"
    // 成功，写入当前PID并返回
    write_current_pid(lock_file)
```

这个逻辑闭环，让我们的服务器在面对上次运行崩溃、留下垃圾文件的情况下，具备了**自我清理和修复**的能力。

### 最后的证明：为“陈旧锁”编写测试

为了证明我们的新机制万无一失，我们为它编写了专属的自动化测试`StaleLockTest`。测试脚本`run_stale_lock_test.sh`会故意创建一个包含无效PID的“陈旧锁”文件，然后启动我们的测试程序，并断言程序**必须成功启动**。

当`ctest`中代表这项测试的绿灯亮起时，我们知道，PICO Radar的基石，已经坚不可摧。

## 结语

这次漫长但收获颇丰的“绕道”，是我们项目成熟的标志。我们没有急于堆砌功能，而是选择了构建一个高度可靠、经过充分自动化测试的底层。现在，站在这坚实的基石之上，我们终于可以满怀信心地，去实现那些真正令人兴奋的核心功能了。

下一站，**数据广播循环**，我们来了！

感谢您的耐心与陪伴，我们下次见！

---
书樱
2025年7月21日