# PICORadar 安装指南

本文档提供 PICORadar 项目的详细安装和构建说明。

## 系统要求

### 最低系统要求

- **操作系统**: Linux (Ubuntu 18.04+, CentOS 7+) 或 Windows 10/11
- **内存**: 至少 4GB RAM（推荐 8GB+）
- **磁盘空间**: 至少 2GB 可用空间（用于依赖库编译）
- **网络**: 稳定的互联网连接（首次构建时下载依赖）

### 必需的开发工具

#### Linux 环境

```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    git \
    pkg-config \
    curl \
    zip \
    unzip \
    tar

# CentOS/RHEL
sudo yum groupinstall "Development Tools"
sudo yum install cmake ninja-build git pkgconfig curl zip unzip tar
```

#### Windows 环境

- **Visual Studio 2019/2022** (或 Visual Studio Build Tools)
- **CMake 3.20+** - [下载链接](https://cmake.org/download/)
- **Git for Windows** - [下载链接](https://git-scm.com/download/win)
- **Ninja** (可选，推荐) - [下载链接](https://github.com/ninja-build/ninja/releases)

### 编译器要求

- **GCC**: 版本 7.0 或更高
- **Clang**: 版本 6.0 或更高
- **MSVC**: Visual Studio 2019 或更高 (支持 C++17)

## 安装步骤

### 1. 克隆仓库

```bash
# 使用 --recursive 确保同时克隆 vcpkg 子模块
git clone --recursive https://github.com/SakuraPuare/PicoRadar.git
cd PicoRadar

# 如果已经克隆但忘记使用 --recursive
git submodule update --init --recursive
```

### 2. 验证子模块

```bash
# 确认 vcpkg 子模块已正确下载
ls -la vcpkg/
# 应该看到 vcpkg 目录包含文件，而不是空目录
```

### 3. 配置构建环境

#### Linux/macOS

```bash
# 配置 CMake 项目（首次运行会自动安装所有依赖）
cmake -B build -S . -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake \
    -DCMAKE_BUILD_TYPE=Release

# 注意：首次配置可能需要 15-30 分钟，因为需要编译所有 C++ 依赖库
```

#### Windows (命令提示符)

```cmd
# 配置 CMake 项目
cmake -B build -S . -G "Visual Studio 16 2019" ^
    -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake

# 或使用 Ninja (推荐)
cmake -B build -S . -G Ninja ^
    -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake ^
    -DCMAKE_BUILD_TYPE=Release
```

### 4. 编译项目

```bash
# 编译所有组件
cmake --build build

# 或者指定并行编译线程数
cmake --build build -j$(nproc)  # Linux
cmake --build build -j%NUMBER_OF_PROCESSORS%  # Windows
```

### 5. 运行测试（可选但推荐）

```bash
# 进入构建目录并运行测试
cd build
ctest --output-on-failure

# 或者直接运行测试可执行文件
./test/test_core
./test/test_network
./test/test_integration
```

## 依赖管理

### vcpkg 依赖库

本项目使用 vcpkg 管理以下 C++ 依赖：

- **Boost.Beast** - WebSocket 和 HTTP 网络库
- **Protocol Buffers** - 数据序列化
- **Google Test** - 单元测试框架
- **glog** - 日志系统
- **nlohmann-json** - JSON 解析
- **tl-expected** - 现代错误处理
- **fmt** - 字符串格式化
- **FTXUI** - 终端用户界面
- **Google Benchmark** - 性能测试

### 清理和重建

如果遇到依赖问题，可以清理并重建：

```bash
# 删除构建目录
rm -rf build/

# 清理 vcpkg 缓存（慎用，会重新下载所有依赖）
rm -rf vcpkg/buildtrees/
rm -rf vcpkg/packages/

# 重新配置和构建
cmake -B build -S . -G Ninja -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build
```

## 构建选项

### CMake 配置选项

```bash
# 启用测试（默认开启）
-DPICORADAR_BUILD_TESTS=ON

# 启用服务端应用（默认开启）
-DPICORADAR_BUILD_SERVER=ON

# 启用代码覆盖率（仅 Debug 模式）
-DPICORADAR_ENABLE_COVERAGE=ON

# 使用 glog 日志系统（默认开启）
-DPICORADAR_USE_GLOG=ON
```

### 构建类型

```bash
# Release 模式（生产环境，默认）
-DCMAKE_BUILD_TYPE=Release

# Debug 模式（开发调试）
-DCMAKE_BUILD_TYPE=Debug

# RelWithDebInfo 模式（带调试信息的优化版本）
-DCMAKE_BUILD_TYPE=RelWithDebInfo
```

## 故障排除

### 常见问题

#### 1. vcpkg 下载失败

```bash
# 手动更新 vcpkg
cd vcpkg
git pull origin master
./bootstrap-vcpkg.sh  # Linux/macOS
.\bootstrap-vcpkg.bat  # Windows
```

#### 2. 编译器版本不兼容

检查编译器版本：

```bash
gcc --version    # 确保 >= 7.0
clang --version  # 确保 >= 6.0
```

#### 3. 内存不足

在资源有限的系统上：

```bash
# 限制并行编译线程数
cmake --build build -j2
```

#### 4. 权限问题 (Linux)

```bash
# 确保有足够的权限
sudo chown -R $USER:$USER .
```

### 获取帮助

如果遇到安装问题：

1. 查看详细的构建日志
2. 检查 [GitHub Issues](https://github.com/SakuraPuare/PicoRadar/issues)
3. 创建新的 Issue 并包含：
   - 操作系统和版本
   - 编译器版本
   - 完整的错误信息
   - 构建配置命令

## 验证安装

成功安装后，你应该能够：

```bash
# 运行服务器
./build/src/server/server

# 查看服务器帮助
./build/src/server/server --help

# 运行示例程序
./build/examples/client_example
./build/examples/wasd_game
```

如果所有步骤都成功完成，恭喜！你已经成功安装了 PICORadar。现在可以查看 [快速开始指南](README.md#快速上手) 了解如何使用系统。
