# 📡 PICO Radar

![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)
![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20Windows-orange.svg)
![Build](https://img.shields.io/badge/build-CMake-green.svg)
![License](https://img.shields.io/badge/license-MIT-lightgrey.svg)
[![CI](https://github.com/sakurapuare/PicoRadar/actions/workflows/ci.yml/badge.svg)](https://github.com/sakurapuare/PicoRadar/actions/workflows/ci.yml)

**PICO Radar: 一个为多用户、共处一室的VR体验设计的实时、低延迟位置共享系统。**

其核心使命是通过在每位玩家的头显中精确渲染其他玩家的虚拟形象，来防止在大型共享物理空间中可能发生的物理碰撞。

---

## 核心特性

-   **多玩家支持**: 稳定支持最多20名玩家同时在线。
-   **低延迟传输**: 保证端到端延迟低于100毫秒，完全在局域网内部署。
-   **零配置连接**: 客户端通过UDP广播自动发现并连接服务器，无需手动配置IP。
-   **安全可靠**: 基于预共享令牌的客户端鉴权和优雅的断连处理。
-   **高性能核心**: 基于C++17和Boost.Asio构建的异步、多线程网络服务器。
-   **跨平台构建**: 使用现代CMake和vcpkg，确保在Linux和Windows上的可复现构建。

## 项目状态

**第一阶段已完成**

-   ✅ **服务端**: 功能完整，包括连接管理、安全鉴权、数据广播和自动发现。
-   ✅ **模拟客户端**: 功能完整，可用于完整的端到端集成测试。
-   ✅ **质量保障**: 拥有覆盖核心逻辑的单元测试和覆盖所有关键场景的集成测试，并已配置CI/CD流水线。
-   🚧 **客户端库 (`client_lib`)**: **正在进行中**。这是项目下一阶段的核心任务。

查阅我们的[**开发路线图 (ROADMAP.md)**](ROADMAP.md)以获取详细的开发计划。

## 快速上手

### 依赖

-   C++17 编译器 (GCC, Clang, MSVC)
-   CMake (3.16+)
-   Ninja (推荐)
-   Git

所有C++库依赖（Boost, Protobuf, glog, gtest）都通过`vcpkg`自动管理。

### 构建

```bash
# 1. 克隆仓库 (确保使用 --recursive 拉取子模块)
git clone --recursive https://github.com/YourUsername/PicoRadar.git
cd PicoRadar

# 2. 配置CMake (vcpkg将自动安装所有依赖)
#    首次配置会花费一些时间用于编译依赖库
cmake -B build -S . -G Ninja -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake

# 3. 构建项目
cmake --build build
```

### 运行服务器

```bash
# 运行服务器，使用默认端口9002
./build/src/server_app/server_app

# 或指定一个端口
./build/src/server_app/server_app 9999
```

## 文档

-   [**技术设计文档 (DESIGN.md)**](DESIGN.md): 深入了解系统架构、技术选型和实现细节。
-   [**开发路线图 (ROADMAP.md)**](ROADMAP.md): 查看项目的功能规划和当前进度。
-   [**开发日志 (blogs/)**](blogs/): 关注我们从零到一的完整开发心路历程。

## 覆盖率统计说明

本项目已统一采用 [gcovr](https://gcovr.com/) 工具进行代码覆盖率统计和报告生成，弃用 lcov。请使用如下命令生成覆盖率报告：

```bash
./scripts/generate_coverage_report.sh
```

该脚本会自动运行测试并生成 HTML、XML、JSON 和文本格式的覆盖率报告，报告文件位于 `coverage/` 目录下。

如需手动安装 gcovr：

```bash
pip install gcovr
# 或
sudo apt-get install gcovr
```

## 贡献

我们欢迎任何形式的贡献！请查阅[**贡献指南 (CONTRIBUTING.md)**](CONTRIBUTING.md)来开始。

## 许可

本项目采用 [MIT许可](LICENSE.txt)。
