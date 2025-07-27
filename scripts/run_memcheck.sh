#!/bin/bash

# PICORadar Valgrind 内存检查脚本
# 使用 Valgrind 检测内存泄漏、越界访问等问题

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

# 脚本所在目录
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build/memcheck"
REPORTS_DIR="$PROJECT_ROOT/memcheck_reports"

log_info "开始内存检查..."

# 检查 Valgrind 是否安装
if ! command -v valgrind &> /dev/null; then
    log_error "Valgrind 未安装"
    log_info "请安装 Valgrind: sudo apt-get install valgrind"
    exit 1
fi

# 创建报告目录
mkdir -p "$REPORTS_DIR"

# 清理之前的构建目录
if [ -d "$BUILD_DIR" ]; then
    log_info "清理之前的构建目录..."
    rm -rf "$BUILD_DIR"
fi

# 配置 CMake with Debug 模式
log_info "配置 CMake (Debug 模式)..."
cmake -B "$BUILD_DIR" -S "$PROJECT_ROOT" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_TOOLCHAIN_FILE="$PROJECT_ROOT/vcpkg/scripts/buildsystems/vcpkg.cmake" \
    -DVCPKG_TARGET_TRIPLET=x64-linux \
    -DPICORADAR_BUILD_TESTS=ON

# 构建项目
log_info "构建项目..."
cmake --build "$BUILD_DIR" -j$(nproc)

# 创建时间戳报告目录
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
CURRENT_REPORT_DIR="$REPORTS_DIR/$TIMESTAMP"
mkdir -p "$CURRENT_REPORT_DIR"

# 运行内存检查
log_info "运行内存检查 (CTest + Valgrind)..."
cd "$BUILD_DIR"

# 设置 Valgrind 选项
export CTEST_OUTPUT_ON_FAILURE=1

# 运行测试并生成内存检查报告
if ctest -T memcheck --output-on-failure; then
    log_success "内存检查完成"
    
    # 复制结果到报告目录
    if [ -d "Testing/Temporary" ]; then
        cp -r Testing/Temporary/* "$CURRENT_REPORT_DIR/" 2>/dev/null || true
    fi
    
    # 查找并显示 Valgrind 结果
    MEMCHECK_FILES=$(find Testing -name "MemoryChecker.*.log" 2>/dev/null || true)
    if [ -n "$MEMCHECK_FILES" ]; then
        log_info "Valgrind 报告位置:"
        echo "$MEMCHECK_FILES" | while read -r file; do
            echo "  - $file"
            cp "$file" "$CURRENT_REPORT_DIR/"
        done
        
        log_info "检查结果摘要:"
        echo "$MEMCHECK_FILES" | while read -r file; do
            if grep -q "ERROR SUMMARY: 0 errors" "$file" 2>/dev/null; then
                log_success "$(basename "$file"): 无内存错误"
            else
                log_warning "$(basename "$file"): 发现潜在内存问题"
            fi
        done
    else
        log_warning "未找到 Valgrind 报告文件"
    fi
    
    log_success "内存检查报告保存到: $CURRENT_REPORT_DIR"
else
    log_error "内存检查过程中出现错误"
    exit 1
fi 