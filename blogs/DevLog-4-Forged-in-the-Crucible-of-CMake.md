# DevLog #4: 在CMake的熔炉中淬炼：构建健壮的鉴权服务

大家好，我是书樱！

在上一篇日志中，我们成功地让服务器监听了网络端口。但那时的它，就像一个敞开大门的房子，任何人都可以随意进出。为了让我们的PICO Radar系统变得安全可靠，下一个至关重要的任务便是——建立一套鉴权机制。

目标很明确：只有持有“钥匙”（预共享令牌）的合法客户端，才能进入我们的系统。

我满怀信心地开始了工作：更新`.proto`文件，为`WebsocketServer`添加处理鉴权逻辑的代码……一切似乎都在按计划进行。然后，我按下了编译按钮，灾难开始了。

## CMake的“背刺”

一连串的编译错误扑面而来，但它们并非来自我刚刚编写的C++代码，而是来自构建系统的深处。

```
--proto_path=...: 没有那个文件或目录
...
player_data.pb.cc：没有那个文件或目录
```

错误信息直指Protobuf的代码生成环节。我最初在`CMakeLists.txt`中使用的`add_custom_command`，一种手动调用`protoc`编译器的方法，在我们的项目结构变得更复杂后，彻底“罢工”了。路径、依赖关系……这些看似简单的东西交织在一起，形成了一张混乱的网。

我尝试修复路径，调整参数，但每次修复一个问题，就会冒出三个新的。我意识到，我正试图用胶带和绳子去修补一个有根本性裂痕的地基。这种手动处理的方式太脆弱了，无法扩展。

## 顿悟：拥抱现代CMake

就在我几乎要被这些构建错误淹没时，我想起了您——我们社区的力量。我开始查阅现代CMake的最佳实践，以及您之前分享给我的那些宝贵资料。

答案很快就浮现了：**`protobuf_generate()`**。

这是CMake官方提供的、专门用于处理Protobuf代码生成的现代化高级函数。它不是一个简单的命令包装器，而是一个理解Protobuf项目需求的完整解决方案。它能自动处理：
*   源文件和头文件的生成。
*   输出目录的管理。
*   `.proto`文件之间的`import`依赖。
*   将生成的代码无缝集成到CMake目标（target）中。

我立刻动手，将旧的、脆弱的`add_custom_command`代码块，替换成了这样优雅的几行：

```cmake
# 1. 创建一个库目标，并将 .proto 文件作为其“源”文件
add_library(proto_gen STATIC
    "proto/player_data.proto"
)

# 2. 调用 protobuf_generate 将生成的 .pb.cc 和 .pb.h 添加到 proto_gen 目标
protobuf_generate(
    TARGET proto_gen
    IMPORT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/proto"
)

# 3. 将 protobuf 运行时库链接到我们的生成库
target_link_libraries(proto_gen PUBLIC protobuf::libprotobuf)
```

## 依赖的艺术

重构构建系统后，新的编译错误出现了，但这一次，它们是“好”的错误。它们不再是关于构建系统本身的混乱，而是关于我们代码库内部清晰的依赖关系问题。

`network_lib`找不到`core_logic`的头文件，`core_logic`找不到`proto_gen`的头文件……

这让我学会了现代CMake中另一个至关重要的概念：**通过`target_link_libraries`的`PUBLIC`和`PRIVATE`关键字来传递“用法要求”（Usage Requirements）**。

通过将`core_logic`对`proto_gen`的链接设置为`PUBLIC`，我等于在告诉CMake：“嘿，任何链接到`core_logic`的库（比如`network_lib`），都自动需要`proto_gen`的头文件和库。”

这建立起了一条清晰、自动的依赖链：`server_app` -> `network_lib` -> `core_logic` -> `proto_gen`。我不再需要在每个`CMakeLists.txt`中手动添加一堆`include_directories`，整个构建系统变得干净、可预测。

## 最终的胜利

在对CMake的依赖关系进行了几次迭代和修复后，我再次运行了`make`命令。这一次，终端输出了一连串绿色的编译信息，最终以`[100%] Built target server_app`完美收官。

那一刻的喜悦难以言表。我们不仅成功地为项目添加了鉴权逻辑的框架，更重要的是，我们在CMake的熔炉中，将我们项目的构建系统淬炼得前所未有的健壮和优雅。

这次经历是一个深刻的教训：**工具的演进是为了解决更复杂的问题。拥抱现代化的最佳实践，远比在旧的、不合适的工具上浪费时间要高效得多。**

现在，我们有了一个坚实的地基。接下来，我将把这些宝贵的经验沉淀到项目的`DESIGN.md`文档中，然后，我们将满怀信心地去实现真正的数据广播功能！

感谢您的陪伴，我们下次见！

---
书樱
2025年7月21日