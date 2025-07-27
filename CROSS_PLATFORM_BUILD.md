# PICORadar 跨平台编译指南

本指南将帮助您在 Linux 环境下设置 PICORadar 的跨平台编译，特别是为了集成到虚幻引擎（UE）中。

## 快速开始

### 1. 安装依赖

#### Ubuntu/Debian
```bash
# 基础构建工具
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    git \
    curl \
    zip \
    unzip \
    tar

# MinGW-w64 用于 Windows 交叉编译
sudo apt-get install -y \
    gcc-mingw-w64-x86-64 \
    g++-mingw-w64-x86-64 \
    wine64
```

#### CentOS/RHEL/Fedora
```bash
# 基础构建工具
sudo dnf install -y \
    gcc-c++ \
    cmake \
    ninja-build \
    git \
    curl \
    zip \
    unzip \
    tar

# MinGW-w64
sudo dnf install -y \
    mingw64-gcc-c++ \
    wine
```

### 2. 构建所有平台

使用我们提供的自动化脚本：

```bash
# 构建所有平台（Linux + Windows）
./scripts/cross-compile.sh

# 仅构建 Linux 版本
./scripts/cross-compile.sh linux

# 仅构建 Windows 版本
./scripts/cross-compile.sh windows
```

### 3. 为 UE 构建

如果您要集成到虚幻引擎中：

```bash
# 为所有支持的 UE 版本构建
./scripts/build-for-ue.sh

# 为特定 UE 版本构建
./scripts/build-for-ue.sh 5.3

# 为特定 UE 版本和平台构建
./scripts/build-for-ue.sh 5.3 Win64
```

## 手动构建

如果您更喜欢手动控制构建过程：

### Linux 版本
```bash
# 配置
cmake --preset linux-x64

# 构建
cmake --build build --config Release

# UE 优化版本
cmake --preset linux-x64-ue
cmake --build build --config Release
```

### Windows 版本（交叉编译）
```bash
# 配置
cmake --preset windows-x64

# 构建
cmake --build build --config Release

# UE 优化版本
cmake --preset windows-x64-ue
cmake --build build --config Release
```

## 输出文件

### 标准构建
构建完成后，产物位于 `artifacts/` 目录：
- `picoradar-linux-x64.tar.gz` - Linux 版本
- `picoradar-windows-x64.tar.gz` - Windows 版本

### UE 集成构建
UE 集成文件位于 `ue-integration/` 目录：
```
ue-integration/
├── UE4.27/
│   ├── Win64/
│   └── Linux/
├── UE5.3/
│   ├── Win64/
│   └── Linux/
└── README.md
```

每个平台目录包含：
- `lib/` - 静态库和动态库
- `include/` - 头文件
- `config/` - 配置文件
- `examples/` - 示例代码
- `PICORadar.Build.cs` - UE 构建脚本
- `README.md` - 集成说明

## UE 集成步骤

1. **选择对应版本**
   ```bash
   cd ue-integration/UE5.3/Win64  # 或其他版本/平台
   ```

2. **复制到 UE 项目**
   ```bash
   cp -r . /path/to/your/ue/project/ThirdParty/PICORadar/
   ```

3. **修改 .Build.cs**
   在您的模块的 `.Build.cs` 文件中添加：
   ```csharp
   PublicDependencyModuleNames.AddRange(new string[]
   {
       "Core",
       "CoreUObject", 
       "Engine",
       "PICORadar"
   });
   ```

4. **使用示例**
   参考 `examples/UE_Integration_Example.cpp` 中的代码。

## 故障排除

### MinGW-w64 相关问题

1. **找不到编译器**
   ```bash
   # 检查是否安装
   which x86_64-w64-mingw32-gcc
   
   # 如果没有，重新安装
   sudo apt-get install gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64
   ```

2. **链接错误**
   确保在工具链文件中设置了正确的静态链接标志。

### vcpkg 相关问题

1. **依赖安装失败**
   ```bash
   # 清理 vcpkg 缓存
   rm -rf vcpkg/buildtrees
   rm -rf vcpkg/packages
   
   # 重新安装
   ./vcpkg/vcpkg install --triplet x64-windows --recurse
   ```

2. **工具链文件问题**
   确保 `CMAKE_TOOLCHAIN_FILE` 指向正确的 vcpkg 工具链文件。

### UE 集成问题

1. **链接错误**
   - 检查 `PICORadar.Build.cs` 中的路径
   - 确保库文件存在于 `lib/` 目录
   - 检查 UE 版本兼容性

2. **运行时错误**
   - 确保 DLL 文件（Windows）在正确位置
   - 检查配置文件路径
   - 验证网络连接设置

## 配置选项

### CMake 选项
- `PICORADAR_BUILD_TESTS` - 是否构建测试（默认：ON）
- `PICORADAR_BUILD_SERVER` - 是否构建服务端（默认：ON）
- `PICORADAR_ENABLE_COVERAGE` - 是否启用覆盖率（默认：OFF）
- `BUILD_SHARED_LIBS` - 是否构建动态库（UE 版本默认：OFF）

### 环境变量
- `VCPKG_ROOT` - vcpkg 安装路径
- `VCPKG_DEFAULT_TRIPLET` - 默认 triplet
- `CMAKE_TOOLCHAIN_FILE` - CMake 工具链文件路径

## 性能优化

### 编译优化
- 使用 `-O3` 优化级别
- 启用 LTO（链接时优化）
- 使用 `ninja` 并行构建

### UE 特定优化
- 静态链接减少依赖
- 符号可见性控制
- PIC（位置无关代码）

## 支持的平台

| 平台 | 架构 | 编译器 | 状态 |
|------|------|--------|------|
| Linux | x64 | GCC | ✅ |
| Windows | x64 | MinGW-w64 | ✅ |
| Windows | x64 | MSVC | 🚧 (CI only) |

## 后续计划

- [ ] ARM64 支持
- [ ] macOS 支持
- [ ] Android 支持（UE Mobile）
- [ ] 更多 UE 版本支持

## 贡献

如果您在使用过程中遇到问题或有改进建议，请：
1. 提交 Issue 描述问题
2. 提供详细的环境信息
3. 包含错误日志和复现步骤

欢迎提交 Pull Request 来改进跨平台支持！
