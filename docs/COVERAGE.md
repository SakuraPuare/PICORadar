# 代码覆盖率配置和使用指南

## 概述

本项目支持代码覆盖率分析，使用 gcov 和 lcov 工具生成详细的覆盖率报告。

## 前置要求

### 安装必要的工具

在 Ubuntu/Debian 系统上：
```bash
sudo apt-get update
sudo apt-get install lcov
```

在 Arch Linux 系统上：
```bash
sudo pacman -S lcov
```

## 使用方法

### 方法1: 使用 CMake Preset (推荐)

1. **配置项目启用覆盖率**：
   ```bash
   cmake --preset coverage
   ```

2. **构建项目**：
   ```bash
   cmake --build build-coverage
   ```

3. **生成覆盖率报告**：
   ```bash
   ./scripts/generate_coverage_report.sh
   ```

### 方法2: 手动配置

1. **配置项目**：
   ```bash
   cmake -B build-coverage \
         -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake \
         -DENABLE_COVERAGE=ON \
         -DCMAKE_CXX_FLAGS="--coverage" \
         -DCMAKE_C_FLAGS="--coverage"
   ```

2. **构建项目**：
   ```bash
   cmake --build build-coverage
   ```

3. **运行测试**：
   ```bash
   cd build-coverage
   ctest --output-on-failure
   ```

4. **生成覆盖率报告**：
   ```bash
   ./scripts/generate_coverage_report.sh
   ```

## 覆盖率报告

生成的覆盖率报告包含：

- **HTML报告**: `coverage/html/index.html` - 详细的HTML格式覆盖率报告
- **文本摘要**: `coverage/coverage_summary.txt` - 文本格式的覆盖率摘要

### 查看HTML报告

在浏览器中打开 `coverage/html/index.html` 查看详细的覆盖率报告，包括：

- 总体覆盖率统计
- 按文件分类的覆盖率
- 代码行级别的覆盖率详情
- 未覆盖代码的高亮显示

## 覆盖率配置说明

### CMake配置

项目中的覆盖率相关配置：

- `ENABLE_COVERAGE`: 启用覆盖率功能的选项
- `CMAKE_CXX_FLAGS=--coverage`: C++编译器的覆盖率标志
- `CMAKE_C_FLAGS=--coverage`: C编译器的覆盖率标志
- `CMAKE_EXE_LINKER_FLAGS=--coverage`: 可执行文件的链接器覆盖率标志

### 过滤规则

覆盖率报告会自动过滤以下目录和文件：

- `/usr/*` - 系统头文件
- `*/test/*` - 测试文件
- `*/mock_client/*` - 模拟客户端文件
- `*/vcpkg/*` - vcpkg依赖文件
- `*/build*/*` - 构建目录
- `*/CMakeFiles/*` - CMake生成的文件
- `*/proto/*.pb.*` - Protobuf生成的文件

## 故障排除

### 常见问题

1. **lcov未找到**
   ```
   错误: lcov 未安装
   ```
   解决方案：安装lcov工具包

2. **构建目录不存在**
   ```
   错误: 构建目录不存在: build-coverage
   ```
   解决方案：先运行 `cmake --preset coverage` 配置项目

3. **覆盖率数据为空**
   - 确保运行了所有测试
   - 检查测试是否正常执行
   - 验证编译时启用了覆盖率标志

### 调试技巧

1. **检查覆盖率标志**：
   ```bash
   cd build-coverage
   grep -r "coverage" CMakeCache.txt
   ```

2. **手动收集覆盖率数据**：
   ```bash
   cd build-coverage
   lcov --capture --directory . --output-file manual_coverage.info
   lcov --list manual_coverage.info
   ```

3. **查看生成的.gcno文件**：
   ```bash
   find build-coverage -name "*.gcno" -type f
   ```

## 持续集成

在CI/CD环境中，可以添加以下步骤：

```yaml
# 示例GitHub Actions配置
- name: 构建覆盖率版本
  run: |
    cmake --preset coverage
    cmake --build build-coverage

- name: 运行测试
  run: |
    cd build-coverage
    ctest --output-on-failure

- name: 生成覆盖率报告
  run: ./scripts/generate_coverage_report.sh

- name: 上传覆盖率报告
  uses: codecov/codecov-action@v3
  with:
    file: ./coverage/coverage_filtered.info
    flags: unittests
    name: codecov-umbrella
```

## 最佳实践

1. **定期运行覆盖率分析**：建议在每次代码提交后运行覆盖率测试
2. **设置覆盖率目标**：为项目设置最低覆盖率要求（如80%）
3. **关注未覆盖代码**：重点分析未覆盖的代码路径
4. **集成到开发流程**：将覆盖率检查集成到CI/CD流程中 