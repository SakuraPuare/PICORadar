#!/bin/bash

# PICORadar 跨平台编译测试脚本

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() {
    echo -e "${BLUE}[TEST]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[PASS]${NC} $1"
}

log_error() {
    echo -e "${RED}[FAIL]${NC} $1"
}

# 测试 MinGW-w64 安装
test_mingw() {
    log_info "测试 MinGW-w64 安装..."
    
    if command -v x86_64-w64-mingw32-gcc &> /dev/null; then
        local version=$(x86_64-w64-mingw32-gcc --version | head -n1)
        log_success "MinGW-w64 已安装: $version"
        return 0
    else
        log_error "MinGW-w64 未安装"
        echo "请运行: sudo apt-get install gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64"
        return 1
    fi
}

# 测试 CMake 预设
test_cmake_presets() {
    log_info "测试 CMake 预设..."
    
    cd "$PROJECT_ROOT"
    
    local presets=("linux-x64" "windows-x64" "linux-x64-ue" "windows-x64-ue")
    local failed=0
    
    for preset in "${presets[@]}"; do
        if cmake --list-presets | grep -q "$preset"; then
            log_success "预设 $preset 可用"
        else
            log_error "预设 $preset 不可用"
            failed=1
        fi
    done
    
    return $failed
}

# 测试简单的 Windows 交叉编译
test_cross_compile() {
    log_info "测试简单的 Windows 交叉编译..."
    
    local test_dir="/tmp/picoradar_test"
    mkdir -p "$test_dir"
    
    # 创建简单的测试程序
    cat > "$test_dir/test.cpp" << 'EOF'
#include <iostream>
#include <string>

int main() {
    std::cout << "Hello from PICORadar cross-compile test!" << std::endl;
    return 0;
}
EOF

    # 尝试编译
    cd "$test_dir"
    if x86_64-w64-mingw32-g++ -o test.exe test.cpp 2>/dev/null; then
        log_success "简单 Windows 交叉编译成功"
        local size=$(stat -f%z test.exe 2>/dev/null || stat -c%s test.exe)
        log_info "生成的 exe 文件大小: $size bytes"
        rm -rf "$test_dir"
        return 0
    else
        log_error "简单 Windows 交叉编译失败"
        rm -rf "$test_dir"
        return 1
    fi
}

# 测试 vcpkg 基本功能
test_vcpkg() {
    log_info "测试 vcpkg 基本功能..."
    
    cd "$PROJECT_ROOT"
    
    if [ ! -d "vcpkg" ]; then
        log_info "vcpkg 不存在，正在克隆..."
        git clone https://github.com/Microsoft/vcpkg.git vcpkg
    fi
    
    if [ ! -f "vcpkg/vcpkg" ]; then
        log_info "编译 vcpkg..."
        cd vcpkg
        ./bootstrap-vcpkg.sh
        cd ..
    fi
    
    if [ -f "vcpkg/vcpkg" ]; then
        log_success "vcpkg 可用"
        local version=$(./vcpkg/vcpkg version | head -n1)
        log_info "vcpkg 版本: $version"
        return 0
    else
        log_error "vcpkg 安装失败"
        return 1
    fi
}

# 测试脚本权限
test_scripts() {
    log_info "测试构建脚本..."
    
    local scripts=("cross-compile.sh" "build-for-ue.sh")
    local failed=0
    
    for script in "${scripts[@]}"; do
        local script_path="$PROJECT_ROOT/scripts/$script"
        if [ -f "$script_path" ] && [ -x "$script_path" ]; then
            log_success "脚本 $script 可执行"
        else
            log_error "脚本 $script 不可执行或不存在"
            failed=1
        fi
    done
    
    return $failed
}

# 运行完整测试
run_all_tests() {
    log_info "开始跨平台编译环境测试..."
    
    local total_tests=0
    local passed_tests=0
    
    # 测试列表
    local tests=(
        "test_mingw:MinGW-w64"
        "test_cmake_presets:CMake 预设"
        "test_cross_compile:交叉编译"
        "test_vcpkg:vcpkg"
        "test_scripts:构建脚本"
    )
    
    for test_entry in "${tests[@]}"; do
        local test_func="${test_entry%%:*}"
        local test_name="${test_entry##*:}"
        
        total_tests=$((total_tests + 1))
        
        echo
        log_info "运行测试: $test_name"
        
        if $test_func; then
            passed_tests=$((passed_tests + 1))
        fi
    done
    
    echo
    echo "=============================="
    log_info "测试完成"
    echo "总测试数: $total_tests"
    echo "通过数: $passed_tests"
    echo "失败数: $((total_tests - passed_tests))"
    
    if [ $passed_tests -eq $total_tests ]; then
        log_success "所有测试通过！跨平台编译环境已就绪"
        echo
        echo "您现在可以运行:"
        echo "  ./scripts/cross-compile.sh        # 构建所有平台"
        echo "  ./scripts/build-for-ue.sh         # 为 UE 构建"
        return 0
    else
        log_error "部分测试失败，请检查环境配置"
        return 1
    fi
}

# 显示帮助
show_help() {
    cat << EOF
PICORadar 跨平台编译测试脚本

用法: $0 [OPTION]

选项:
    test      运行所有测试（默认）
    mingw     仅测试 MinGW-w64
    cmake     仅测试 CMake 预设
    compile   仅测试交叉编译
    vcpkg     仅测试 vcpkg
    scripts   仅测试构建脚本
    -h        显示帮助

示例:
    $0              # 运行所有测试
    $0 mingw        # 仅测试 MinGW-w64
    $0 compile      # 仅测试交叉编译
EOF
}

# 主函数
main() {
    case "${1:-test}" in
        "test")
            run_all_tests
            ;;
        "mingw")
            test_mingw
            ;;
        "cmake")
            test_cmake_presets
            ;;
        "compile")
            test_cross_compile
            ;;
        "vcpkg")
            test_vcpkg
            ;;
        "scripts")
            test_scripts
            ;;
        "-h"|"--help"|"help")
            show_help
            ;;
        *)
            log_error "未知选项: $1"
            show_help
            exit 1
            ;;
    esac
}

main "$@"
