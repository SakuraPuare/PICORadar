# DevLog-16-日志前缀：让服务端与客户端输出一目了然

> 作者：书樱

## 背景与需求

在PICO Radar的集成测试和压力测试中，服务端与多个客户端（mock_client）往往会同时输出大量日志。随着系统复杂度提升，日志分析和问题定位变得愈发困难。尤其是在CI/CD自动化测试环境下，所有进程的日志汇总在一起，若无法区分来源，调试体验极差。

因此，我们迫切需要一种机制，让每条日志都能明确标识其“身份”——究竟是服务端输出，还是客户端输出。

## 设计思路

我们采用了Google glog作为全局日志库。glog本身支持多级别日志、文件/控制台输出等，但并不直接支持“日志前缀”功能。我们希望：

- 不侵入现有LOG(INFO) << ... 代码风格。
- 能灵活为不同进程（如server/client）设置不同的前缀。
- 前缀只影响控制台输出，文件日志保持原样。

查阅glog源码后发现，可以通过自定义LogSink来“拦截”日志流，从而实现前缀功能。

## 技术实现

### 1. 扩展日志模块

在`src/common/logging.hpp/.cpp`中，我们新增了如下内容：

```cpp
class PrefixedLogSink : public google::LogSink {
 public:
  explicit PrefixedLogSink(std::string prefix);
  void send(google::LogSeverity severity, const char* full_filename,
            const char* base_filename, int line,
            const struct ::google::LogMessageTime& tm, const char* message,
            size_t message_len) override;
 private:
  const std::string prefix_;
};
```

实现上，我们在`send`方法中，先输出前缀，再用glog的LogMessage格式化剩余内容：

```cpp
void PrefixedLogSink::send(...) {
    std::cerr << prefix_;
    google::LogMessage(full_filename, line, severity).stream() << message;
}
```

### 2. setup_logging增强

`setup_logging`函数新增了`log_prefix`参数：

```cpp
void setup_logging(std::string_view app_name, bool log_to_file = true,
                   const std::string& log_dir = "./logs",
                   const std::string& log_prefix = "");
```

只要传入非空前缀，就会自动注册自定义LogSink，所有控制台日志都会带上前缀。

### 3. 应用到主程序

- 服务端（`src/server_app/main.cpp`）：
  ```cpp
  picoradar::common::setup_logging(argv[0], true, "./logs", "[SERVER] ");
  ```
- 客户端（`test/mock_client/main.cpp`）：
  ```cpp
  picoradar::common::setup_logging(argv[0], false, "", "[CLIENT] ");
  ```

这样，所有服务端日志前面都会有`[SERVER] `，客户端则是`[CLIENT] `。

## 遇到的坑与解决

最初尝试直接用glog内部的LogMessage构造函数和message()成员拼接日志，结果发现这些API并非公开接口，导致编译报错。最终采用了“前缀+LogMessage::stream()”的组合，既安全又兼容glog的格式。

## 效果与意义

现在，无论是本地测试还是CI流水线，日志输出如下：

```
[SERVER] I20240601 20:00:00.123456 12345 main.cpp:42] Server started successfully.
[CLIENT] I20240601 20:00:00.234567 12346 sync_client.cpp:88] Player connected: player_01
```

一眼就能分辨来源，极大提升了调试效率。

## 未来展望

- 可以进一步支持“多标签”或“动态标签”，如区分不同客户端ID。
- 日志前缀可用于区分不同测试用例、线程等，提升可观测性。
- 也可考虑将前缀写入日志文件，便于后期自动化分析。

## 总结

本次日志前缀功能的实现，虽小却极具工程价值。它让我们的测试和运维体验更上一层楼，也为后续的可观测性和自动化打下了坚实基础。

——书樱 