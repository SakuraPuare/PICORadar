# CMake 预设使用指南

## 概述

本项目使用 CMake 预设来简化不同场景下的构建配置。每个预设都针对特定的使用场景进行了优化。

## 可用预设

### 本地开发预设

#### `dev-debug` - 本地开发 Debug
```bash
cmake --preset dev-debug
cmake --build --preset dev-debug
```
- **用途**: 日常开发和调试
- **特性**: Debug 模式，启用测试，不启用覆盖率
- **构建目录**: `build/debug/`
- **推荐场景**: 日常开发、调试、单元测试

#### `dev-debug-coverage` - 本地开发 Debug + 覆盖率
```bash
cmake --preset dev-debug-coverage
cmake --build --preset dev-debug-coverage
```
- **用途**: 本地覆盖率测试
- **特性**: Debug 模式，启用测试和覆盖率
- **构建目录**: `build/coverage/`
- **推荐场景**: 检查测试覆盖率

### CI 预设

#### `ci-debug` - CI Debug
```bash
cmake --preset ci-debug
cmake --build --preset ci-debug
```
- **用途**: CI 环境下的测试构建
- **特性**: Debug 模式，启用测试和覆盖率
- **构建目录**: `build/`
- **使用场景**: CI 覆盖率测试

#### `ci-release` - CI Release
```bash
cmake --preset ci-release  
cmake --build --preset ci-release
```
- **用途**: CI 环境下的发布构建
- **特性**: Release 模式，启用测试，不启用覆盖率
- **构建目录**: `build/`
- **使用场景**: CI 性能测试、发布验证

### 交叉编译预设

#### `cross-windows` - Windows 交叉编译
```bash
cmake --preset cross-windows
cmake --build --preset cross-windows
```
- **用途**: Linux 下交叉编译 Windows 版本
- **特性**: Release 模式，不启用测试
- **构建目录**: `build/windows/`

### UE 集成预设

#### `ue-linux` - UE Linux 集成
```bash
cmake --preset ue-linux
cmake --build --preset ue-linux
```
- **用途**: Unreal Engine Linux 集成
- **特性**: Release 模式，静态库，位置无关代码
- **构建目录**: `build/ue-linux/`

#### `ue-windows` - UE Windows 集成
```bash
cmake --preset ue-windows
cmake --build --preset ue-windows
```
- **用途**: Unreal Engine Windows 集成
- **特性**: Release 模式，静态库，静态链接运行时
- **构建目录**: `build/ue-windows/`

## 使用建议

### 日常开发
```bash
# 普通开发
cmake --preset dev-debug && cmake --build --preset dev-debug

# 检查覆盖率时
cmake --preset dev-debug-coverage && cmake --build --preset dev-debug-coverage
```

### 运行测试
```bash
# 在相应的构建目录下运行测试
cd build/debug
ctest

# 或者使用 CMake 直接运行
cmake --build --preset dev-debug --target test
```

### 生成覆盖率报告
```bash
cmake --preset dev-debug-coverage
cmake --build --preset dev-debug-coverage
cd build/coverage
gcovr -r ../../ --html --html-details -o coverage_report.html
```

## 目录结构

不同预设使用不同的构建目录，避免冲突：

```
build/
├── debug/          # dev-debug
├── coverage/       # dev-debug-coverage  
├── windows/        # cross-windows
├── ue-linux/       # ue-linux
└── ue-windows/     # ue-windows
```

CI 预设使用根 `build/` 目录，与现有 CI 配置兼容。

## 注意事项

1. **第一次使用**: 需要确保 vcpkg 已正确设置
2. **磁盘空间**: 不同预设会创建独立的构建目录
3. **清理构建**: 可以安全删除 `build/` 下的子目录来清理特定预设的构建产物
4. **IDE 集成**: VS Code 和其他 IDE 可以直接识别和使用这些预设

## 故障排除

### 预设找不到
确保你的 CMake 版本 ≥ 3.20，并且在项目根目录执行命令。

### vcpkg 相关错误
确保 `vcpkg/` 目录存在且已正确初始化。

### 交叉编译失败
确保已安装 MinGW-w64 工具链：
```bash
sudo apt install mingw-w64
```
