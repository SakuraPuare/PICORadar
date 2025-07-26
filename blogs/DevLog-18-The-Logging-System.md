# DevLog-18: 铸造信息熔炉——构建可观测性基石

嗨，我是书樱！

随着 PICO Radar 项目的日益复杂，我们越来越深刻地认识到，一个强大、可靠且易于使用的日志系统是项目的“中枢神经系统”。它不仅是调试的利器，更是我们理解系统行为、诊断线上问题的眼睛和耳朵。之前，我们直接使用 `glog`，虽然功能强大，但在整个项目中风格不一，配置分散，缺乏统一的管理。

是时候铸造我们自己的信息熔炉了——一个为 PICO Radar 量身定制的日志库。

## 为什么重复造轮子？

“不要重复造轮子”是软件工程的黄金法则，但有时，为了特定的需求，我们需要为轮子装上定制的“轮毂”。我们决定封装 `glog` 而不是完全从零开始，旨在：

1.  **统一接口**：提供一套简洁、易用的宏 (`LOG_INFO`, `LOG_ERROR` 等)，屏蔽底层 `glog` 的复杂性，确保整个项目日志风格的一致性。
2.  **简化配置**：将日志的初始化和配置（如日志级别、文件路径、滚动大小）集中管理，避免在代码库中散落各种初始化逻辑。
3.  **增强可扩展性**：封装提供了一个完美的扩展点。未来，如果我们想增加新的日志目标（比如网络日志、数据库日志）或改变日志格式，只需修改封装层，而无需触及成百上千的日志调用点。
4.  **提升安全性**：通过封装，我们可以更好地控制日志行为，例如，确保在析构时自动关闭和刷新日志，防止日志丢失。

## 设计蓝图：一个完备日志库的自我修养

在动手之前，我们绘制了新日志库的蓝图。一个现代化的 C++ 日志库应该具备以下品质：

*   **多级别日志**：从细致的 `TRACE` 到致命的 `FATAL`，满足不同场景的需求。
*   **线程安全**：在多线程环境中也能安心使用。
*   **高性能**：底层依赖 `glog` 的异步和缓冲机制。
*   **灵活输出**：支持输出到控制台和文件。
*   **日志滚动**：能按大小自动分割日志文件。
*   **条件日志**：`LOG_IF_INFO(condition) << "..."` 这样的语法，让日志记录更加智能。
*   **崩溃处理**：在程序崩溃时，能捕获最后的信息。

## 核心实现：`logger::Logger`

我们的实现围绕一个名为 `logger::Logger` 的单例类展开。

### 初始化与配置

我们将所有 `glog` 的初始化逻辑都收敛到了 `Logger::Init`静态函数中。

```cpp
// src/common/logging.hpp
class Logger {
public:
    static void Init(const std::string& program_name,
                    const std::string& log_dir = "./logs",
                    LogLevel min_log_level = LogLevel::INFO,
                    uint32_t max_log_size = 10, // MB
                    bool log_to_stderr = false);
    // ...
};

// src/common/logging.cpp
void Logger::Init(...) {
    static std::once_flag once_flag;
    std::call_once(once_flag, [&]() {
        google::InitGoogleLogging(program_name.c_str());
        google::SetLogDestination(google::INFO, (log_dir + "/INFO_").c_str());
        // ... 其他级别的目标设置 ...
        
        google::SetStderrLogging(static_cast<int>(min_log_level));
        google::SetMaxLogSize(max_log_size);
        FLAGS_logtostderr = log_to_stderr;
        
        google::InstallFailureSignalHandler();
    });
}
```

通过 `std::call_once`，我们确保了初始化逻辑在整个程序生命周期中只执行一次，既简单又线程安全。现在，在 `server` 和 `client` 的入口，我们只需要一行代码就能完成日志系统的全部设置：

```cpp
// src/server/main.cpp
logger::Logger::Init(argv[0], "./logs", logger::LogLevel::INFO);
```

### 流式输出的魔法：`LogStream`

为了实现 `LOG_INFO << "Hello, " << 123;` 这样自然的流式语法，我们设计了一个小巧的 `LogStream` 类。它的核心思想是利用 RAII（资源获取即初始化）模式。

```cpp
// src/common/logging.hpp
class LogStream {
public:
    LogStream(const char* file, int line, LogLevel level);
    ~LogStream(); // 关键所在！

    template<typename T>
    LogStream& operator<<(const T& val) {
        stream_ << val;
        return *this;
    }
private:
    std::ostringstream stream_;
    // ... 其他元数据
};
```

当 `LOG_INFO << "..."` 这行代码被执行时：
1.  `LogStream` 的一个临时对象被创建。
2.  `operator<<` 被连续调用，将所有日志内容写入内部的 `std::ostringstream`。
3.  在这行代码结束时，临时对象即将被销毁，此时它的**析构函数 `~LogStream()`** 被调用。
4.  在析构函数中，我们才真正地将拼接好的完整日志消息一次性地写入 `glog`。

```cpp
// src/common/logging.cpp
Logger::LogStream::~LogStream() {
    if (level_ >= current_level_) { // 检查日志级别
        std::lock_guard<std::mutex> lock(Logger::GetInstance().mutex_);
        switch (level_) {
            case LogLevel::INFO:
                google::LogMessage(file_, line_, google::GLOG_INFO).stream() 
                    << stream_.str();
                break;
            // ... 其他级别
        }
    }
}
```

这种设计的精妙之处在于，它将日志消息的构建和实际写入操作分离开来，并且只有在日志级别满足条件时，才会发生真正的 I/O，极大地提升了效率。

### 宏：连接用户与库的桥梁

最后，我们定义了一系列宏，作为提供给用户的最终接口。

```cpp
// src/common/logging.hpp
#define LOG_INFO logger::Logger::LogStream(__FILE__, __LINE__, logger::LogLevel::INFO)
#define LOG_ERROR logger::Logger::LogStream(__FILE__, __LINE__, logger::LogLevel::ERROR)
// ...

#define LOG_IF_INFO(condition) logger::Logger::LogStreamIf(__FILE__, __LINE__, logger::LogLevel::INFO, condition)
```

这些宏自动捕获了文件名 (`__FILE__`) 和行号 (`__LINE__`)，极大地简化了调用。

## 重构的成果

在完成了新的日志库后，我们对整个项目进行了重构。所有 `LOG(INFO)` 风格的调用都被替换为我们新的 `LOG_INFO` 宏。

**之前 (分散在各处):**
```cpp
// 在 main.cpp
google::InitGoogleLogging(argv[0]);
FLAGS_logtostderr = true;
LOG(INFO) << "Server starting...";

// 在其他文件
LOG(ERROR) << "Something went wrong.";
```

**之后 (统一且简洁):**
```cpp
// 在 main.cpp
logger::Logger::Init(argv[0], "./logs", logger::LogLevel::DEBUG);
LOG_INFO << "Server starting...";

// 在其他文件
LOG_ERROR << "Something went wrong.";
```

这次重构不仅仅是一次代码替换。它为 PICO Radar 项目建立了一个坚实、可观测的基石。现在，我们拥有了一个统一、可控、可扩展的日志系统，它将像一座忠实的灯塔，在未来开发的漫漫长夜中，为我们指引方向。

下一次，我们将回到客户端的开发上，是时候让它“浴火重生”了！

—— 书樱
2025年7月26日
