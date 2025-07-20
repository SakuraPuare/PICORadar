# Linter 设置指南

## 问题描述

当CMake项目配置正常但linter仍然报错 `'core/player_registry.hpp' file not found` 时，通常是因为缺少 `compile_commands.json` 文件或protobuf生成文件。

## 解决方案

### 1. 自动解决方案

运行以下脚本来自动修复linter问题：

```bash
./scripts/update_compile_commands.sh
```

### 2. 手动解决方案

#### 步骤1：确保CMake生成编译数据库

在 `CMakeLists.txt` 中添加：

```cmake
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
```

#### 步骤2：重新配置项目

```bash
cd build
cmake ..
```

#### 步骤3：生成protobuf文件

```bash
make generate_proto
```

#### 步骤4：创建符号链接

```bash
cd ..
ln -sf build/compile_commands.json .
```

### 3. 验证修复

检查以下文件是否存在：

- `compile_commands.json` (项目根目录)
- `build/generated/player_data.pb.h`
- `build/generated/player_data.pb.cc`

### 4. IDE配置

#### VS Code

确保安装了以下扩展：
- C/C++
- clangd

#### 其他IDE

确保IDE能够找到 `compile_commands.json` 文件。

## 常见问题

### Q: 为什么需要 `compile_commands.json`？

A: `compile_commands.json` 是linter（如clangd）用来理解项目结构、包含路径和编译选项的关键文件。没有这个文件，linter无法正确解析相对路径的include语句。

### Q: 为什么protobuf文件很重要？

A: 项目中的 `player_registry.hpp` 包含了 `#include "player_data.pb.h"`，如果protobuf文件没有生成，linter会报错找不到这个头文件。

### Q: 每次修改CMakeLists.txt后都需要重新生成吗？

A: 是的，如果修改了包含路径、编译选项或添加了新的源文件，都需要重新运行 `./scripts/update_compile_commands.sh`。

## 故障排除

如果问题仍然存在：

1. 检查 `compile_commands.json` 中是否包含了正确的包含路径
2. 确认protobuf文件已正确生成
3. 重启IDE或linter服务
4. 检查 `.clangd` 配置文件是否正确 