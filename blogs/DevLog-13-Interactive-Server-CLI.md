
# 开发日志 #13：让服务器“开口说话”——实现交互式CLI

大家好，我是书樱！

随着我们的PICO Radar项目日益成熟，核心功能趋于稳定，我们开始将目光从“能用”转向“好用”和“易于维护”。之前，我们的服务器像一个沉默的黑盒子，勤勤恳懇地在后台运行，但我们对它内部的状态——比如当前有多少玩家在线——一无所知，除非去翻阅冗长的日志。这对于调试和未来的运维来说，显然是不够的。

所以，我们决定给这个“黑盒子”开一个窗口，让它能“开口说话”。我们的目标很明确：**为服务器添加一个简单的交互式命令行界面（CLI），让我们可以在不重启服务的情况下，实时查询其内部状态。**

## 思考：如何让主线程“一心二用”？

我们服务器的`main`函数结构很经典：初始化、启动服务、然后进入一个循环，等待一个`Ctrl+C`的退出信号。代码大致是这样：

```cpp
// 伪代码
int main() {
    server->start();

    // 主线程在这里“卡住”，直到用户按下Ctrl+C
    while (!g_stop_signal) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    server->stop();
    return 0;
}
```

这个模式的问题在于，主线程被`while`循环“阻塞”了，它除了等待，什么也做不了。我们既要它继续跑核心服务，又要它能接收我们的命令行输入。怎么办？答案是：**多线程**。

我们的想法是：
1.  将服务器的核心I/O服务（即`io_context.run()`）放到一个独立的后台线程中。
2.  解放主线程，让它专门负责监听和处理来自控制台的输入。

这样一来，两个线程各司其职，互不干扰，服务器的核心逻辑依然在后台线程稳定运行。

## 动手：改造主循环

基于这个思路，我们对`src/server/main.cpp`进行了改造。

首先，我们引入了`<thread>`和`<string>`头文件，并创建了一个新的线程`server_thread`，它的任务就是运行`ioc.run()`。

```cpp
// src/server/main.cpp

// ...

// 在一个后台线程中运行io_context
std::thread server_thread([&ioc] {
    try {
        ioc.run();
    } catch (const std::exception& e) {
        LOG(ERROR) << "Exception in server I/O context: " << e.what();
    }
});
```

`ioc.run()`是一个阻塞调用，它会一直运行直到I/O服务停止。把它放进新线程后，主线程就“自由”了。

接下来，我们重写了主线程的循环。它不再是盲目地`sleep`，而是变成了一个标准的输入处理循环，使用`std::getline(std::cin, line)`来读取用户输入的每一行指令。

```cpp
// src/server/main.cpp

// ...

// 主循环处理CLI输入
LOG(INFO) << "Server started successfully. Type 'status' for player count, or 'quit' to exit.";
std::string line;
while (!g_stop_signal && std::getline(std::cin, line)) {
    if (line == "status") {
        LOG(INFO) << "Current online players: " << registry->getPlayerCount();
    } else if (line == "quit" || line == "exit") {
        g_stop_signal = true;
    }
}
```

我们设计了两个简单的命令：
*   `status`: 调用我们之前在`PlayerRegistry`中准备好的`getPlayerCount()`方法，获取当前玩家数量并打印出来。
*   `quit`或`exit`: 修改全局信号量`g_stop_signal`，通知服务器是时候该优雅地关闭了。

最后，在程序退出前，我们必须确保后台的服务器线程也已经结束。我们调用`server->stop()`来停止I/O服务（这将导致`ioc.run()`返回），然后使用`server_thread.join()`来等待该线程执行完毕。

```cpp
// src/server/main.cpp

// ...

// 停止服务器
server->stop();
if(server_thread.joinable()) {
    server_thread.join();
}

LOG(INFO) << "Shutdown complete.";
```

## 小插曲：一个简单的拼写错误

在实现过程中，我犯了一个经典的错误。我在`main.cpp`里调用了`registry->get_player_count()`，而`PlayerRegistry`类里的函数名其实是驼峰式的`getPlayerCount()`。编译器立刻就报错了，提醒我`PlayerRegistry`类中没有名为`get_player_count`的成员。

这个小插曲也提醒了我，即使是看似微不足道的命名约定，在整个项目里保持一致性也是非常重要的！

## 成果

现在，我们的服务器启动后，不再是一个无法交互的黑洞。我们可以随时输入`status`来查看在线人数，就像这样：

```
I20231028 10:30:00.123456 main.cpp:80] Server started successfully. Type 'status' for player count, or 'quit' to exit.
status
I20231028 10:30:05.456789 main.cpp:83] Current online players: 5
```

这个小小的CLI功能，是项目从“能用”到“好用”迈出的一大步。它极大地提升了开发和调试的便利性。接下来，我们将基于这个健壮的基础，去探索和实现更多令人兴奋的功能！

感谢你的阅读，我们下次再见！

---
书樱
2023年10月28日 