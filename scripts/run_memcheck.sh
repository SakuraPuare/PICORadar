#!/bin/bash
set -e

#
#
#
#
#
#
#
#

# 脚本所在目录
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
PROJECT_ROOT="$SCRIPT_DIR/.."
BUILD_DIR="$PROJECT_ROOT/build_memcheck"

# 清理之前的构建目录
if [ -d "$BUILD_DIR" ]; then
    echo "--- Cleaning up previous build directory ---"
    rm -rf "$BUILD_DIR"
fi

# 配置 CMake, 启用 Valgrind
echo "--- Configuring CMake with Valgrind enabled ---"
cmake -B "$BUILD_DIR" -S "$PROJECT_ROOT" -DCMAKE_BUILD_TYPE=Debug -DUSE_VALGRIND=ON

# 构建项目
echo "--- Building project ---"
cmake --build "$BUILD_DIR"

# 运行内存检查
echo "--- Running memory checks with CTest ---"
cd "$BUILD_DIR"
ctest --output-on-failure -T memcheck

echo "--- Memory checks completed ---" 