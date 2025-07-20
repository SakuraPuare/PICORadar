# 开发日志#1：脚手架的搭建与依赖之战

**作者：书樱**
**日期：2025年7月21日**

> **摘要**: 从蓝图到代码的第一步，往往不是写功能，而是搭建一个能让功能茁壮成长的“脚手架”。在这篇文章里，我将分享PICO Radar项目从0到1的真实过程——它远非一帆风顺，我们陷入了一场与C++依赖管理的“战争”。这其中有坑，有妥协，但最终，我们胜利了。

---

大家好，我是书樱。

在上一篇日志里，我们为PICO Radar项目绘制了宏伟的蓝图。现在，是时候卷起袖子，把图纸变成现实了。我们的第一步，就是搭建项目的骨架：一个能编译、能运行、能管理所有第三方代码的健壮结构。

听起来很简单？嗯，C++的依赖管理总能给你带来“惊喜”。

### 最初的平静：创建目录与CMake骨架

这部分工作进行得非常顺利。我们按照`README.md`中的规划，使用`mkdir`创建了`src`, `test`, `proto`等一系列清晰的目录结构。

紧接着，我们编写了根目录以及各个子目录的`CMakeLists.txt`文件。这就像在盖房子之前打地基，我们定义了项目名称、C++标准(C++17)，并用`add_subdirectory`将各个模块（`server_app`, `core`, `network`等）串联起来。一切看起来都井然有序。

### 风暴的来临：迎战三大依赖

我们的项目依赖于三个关键的第三方库：
1.  **Protobuf**: 用于数据序列化。
2.  **GTest**: 用于单元测试。
3.  **WebSocketPP**: 用于实现WebSocket通信。

我们决定采用现代化的vcpkg清单模式（`vcpkg.json`）来管理它们。理论上，我们只需在这个JSON文件里声明我们想要的库，然后在运行CMake时，vcpkg就会自动下载、编译并配置好一切。

然而，当我们运行`cmake`时，第一个问题出现了：

`Could not find a package configuration file provided by "websocketpp"`

vcpkg根本不认识`websocketpp`！经过一番探查，我们发现这个曾经流行的库似乎已经不再被vcpkg官方的清单模式所直接支持。

### 第一次转进：拥抱 `FetchContent`

既然vcpkg的“自动挡”失灵了，我们决定对`websocketpp`采用“手动挡”。CMake从3.11版本开始提供了一个强大的内置模块——`FetchContent`。它允许CMake在配置阶段直接从Git仓库下载代码。

这看起来是一个更现代、更可靠的方案。我们修改了根`CMakeLists.txt`，加入了`FetchContent_Declare`来从`websocketpp`的官方GitHub仓库拉取代码。

我们满怀信心地再次运行`cmake`，然后……遇到了第二个错误。

`Compatibility with CMake < 3.5 has been removed from CMake.`

`FetchContent`成功地下载了代码，但`websocketpp`项目本身的`CMakeLists.txt`太老了，它声明的CMake最低版本（2.8.8）已经不被我们所用的新版CMake所兼容。

### 战争的泥潭：策略、属性与无尽的尝试

接下来，我们陷入了一场与CMake策略（Policy）的搏斗。我们尝试了各种方法，试图在CMake处理`websocketpp`的代码之前，告诉它“请用新的兼容模式来解释这个老旧的文件”：
-   我们尝试在`FetchContent_Declare`中使用`CMAKE_ARGS`注入策略，失败了。
-   我们尝试使用`FetchContent`的底层API，在`Populate`之后用`set_property`设置策略，也失败了。

每一次失败，`build`目录都被我们删了又建，建了又删。这虽然令人沮g丧，但也让我们对CMake的依赖管理机制有了前所未有的深刻理解。

### 最终的胜利：务实的“补丁”方案

在所有“优雅”的方案都宣告失败后，我们决定采取一种最直接、甚至有点“粗暴”，但保证有效的方法。

既然问题在于`FetchContent`下载下来的文件内容不对，那我们就在它被CMake处理之前，手动把它改对！

我们的最终工作流变成了这样：
1.  运行一次`cmake`，让它失败。这次失败是意料之中的，它的唯一目的就是让`FetchContent`把`websocketpp`的代码下载到`build/_deps`目录中。
2.  **执行一步`replace`命令，强行修改`build/_deps/websocketpp-src/CMakeLists.txt`文件**，把`cmake_minimum_required(VERSION 2.8.8)`更新为`cmake_minimum_required(VERSION 3.5)`。
3.  再次运行`cmake`。

这一次，屏幕上终于没有了刺眼的红色错误。所有依赖都已就位，构建文件生成成功。

这或许不是最“优雅”的解决方案，但它有效、可重复，并且将问题隔离在了项目配置这一个阶段。这本身就是一种工程上的智慧——在“纯粹”与“实用”之间，我们果断地选择了后者。

### 结语

现在，PICO Radar的脚手架已经稳固地搭建完毕。我们拥有了一个混合式的、健壮的依赖管理系统：vcpkg负责主流库，FetchContent + “补丁”负责处理“遗留”库。

虽然过程比预想的要曲折得多，但我们征服了这头拦路虎。现在，地基已经打好，钢筋骨架也已就位。

下一篇日志，我们将开始为这座建筑添砖加瓦——实现服务器的核心功能。
