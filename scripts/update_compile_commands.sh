#!/bin/bash

# 更新编译数据库脚本
# 用于重新生成 compile_commands.json 文件

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
log_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

log_info "正在更新编译数据库..."

# 获取脚本目录和项目根目录
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# 确保在项目根目录
if [ ! -f "$PROJECT_ROOT/CMakeLists.txt" ]; then
    log_error "CMakeLists.txt 未找到，请检查项目结构"
    exit 1
fi

cd "$PROJECT_ROOT"

# 选择构建目录（使用 CMake presets）
BUILD_DIR="build/debug"

if [ ! -d "$BUILD_DIR" ]; then
    log_info "构建目录不存在，使用 CMake preset 配置..."
    if command -v cmake &> /dev/null; then
        cmake --preset dev-debug
    else
        log_error "CMake 未安装"
        exit 1
    fi
else
    log_info "重新配置现有构建目录..."
    cmake --preset dev-debug
fi

# 生成编译数据库
log_info "生成编译数据库..."
cmake --build "$BUILD_DIR" --target clean
cmake --build "$BUILD_DIR" -j$(nproc)

# 更新符号链接
if [ -L "compile_commands.json" ]; then
    rm compile_commands.json
fi

if [ -f "$BUILD_DIR/compile_commands.json" ]; then
    ln -sf "$BUILD_DIR/compile_commands.json" .
    log_success "编译数据库更新完成！"
    log_info "现在linter应该能够正确识别所有头文件了。"
else
    log_warning "编译数据库文件未生成，请检查构建过程"
fi 