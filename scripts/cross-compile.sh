#!/bin/bash

# PICORadar 跨平台编译脚本
# 用于在 Linux 环境下编译 Windows 和 Linux 版本

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"
ARTIFACTS_DIR="$PROJECT_ROOT/artifacts"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 检查依赖
check_dependencies() {
    log_info "检查依赖..."
    
    # 检查 CMake
    if ! command -v cmake &> /dev/null; then
        log_error "CMake 未安装"
        exit 1
    fi
    
    # 检查 Ninja
    if ! command -v ninja &> /dev/null; then
        log_error "Ninja 未安装"
        exit 1
    fi
    
    # 检查 MinGW-w64 (用于 Windows 交叉编译)
    if [ "$1" = "windows" ] || [ "$1" = "all" ]; then
        if ! command -v x86_64-w64-mingw32-gcc &> /dev/null; then
            log_error "MinGW-w64 未安装，无法进行 Windows 交叉编译"
            log_info "请运行: sudo apt-get install gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64"
            exit 1
        fi
    fi
    
    log_success "所有依赖检查通过"
}

# 设置 vcpkg
setup_vcpkg() {
    log_info "设置 vcpkg..."
    
    if [ ! -d "$PROJECT_ROOT/vcpkg" ]; then
        log_info "克隆 vcpkg..."
        git clone https://github.com/Microsoft/vcpkg.git "$PROJECT_ROOT/vcpkg"
    fi
    
    if [ ! -f "$PROJECT_ROOT/vcpkg/vcpkg" ]; then
        log_info "编译 vcpkg..."
        cd "$PROJECT_ROOT/vcpkg"
        ./bootstrap-vcpkg.sh
        cd "$PROJECT_ROOT"
    fi
    
    log_success "vcpkg 设置完成"
}

# 安装依赖包
install_dependencies() {
    local triplet=$1
    log_info "安装依赖包 (triplet: $triplet)..."
    
    cd "$PROJECT_ROOT"
    ./vcpkg/vcpkg install --triplet "$triplet"
    
    log_success "依赖包安装完成"
}

# 构建项目
build_project() {
    local preset=$1
    local target_name=$2
    
    log_info "构建项目 ($target_name)..."
    
    cd "$PROJECT_ROOT"
    
    # 清理之前的构建
    if [ -d "$BUILD_DIR" ]; then
        rm -rf "$BUILD_DIR"
    fi
    
    # 配置
    log_info "配置 CMake..."
    cmake --preset "$preset"
    
    # 构建
    log_info "编译项目..."
    cmake --build build --config Release --parallel $(nproc)
    
    log_success "$target_name 构建完成"
}

# 打包产物
package_artifacts() {
    local target_name=$1
    local output_suffix=$2
    
    log_info "打包产物 ($target_name)..."
    
    local target_dir="$ARTIFACTS_DIR/$output_suffix"
    mkdir -p "$target_dir"
    
    # 复制可执行文件和库
    if [ "$target_name" = "windows" ]; then
        find "$BUILD_DIR" -name "*.exe" -type f -exec cp {} "$target_dir/" \;
        find "$BUILD_DIR" -name "*.dll" -type f -exec cp {} "$target_dir/" \;
    else
        find "$BUILD_DIR" -executable -type f -not -path "*/CMakeFiles/*" -exec cp {} "$target_dir/" \;
        find "$BUILD_DIR" -name "*.so" -type f -exec cp {} "$target_dir/" \;
    fi
    
    # 复制配置文件
    if [ -d "$PROJECT_ROOT/config" ]; then
        cp -r "$PROJECT_ROOT/config" "$target_dir/"
    fi
    
    # 创建 README
    cat > "$target_dir/README.md" << EOF
# PICORadar $target_name Build

## 构建信息
- 构建时间: $(date)
- 目标平台: $target_name
- 构建类型: Release

## UE 集成说明
1. 将 \`lib\` 目录中的库文件复制到 UE 项目的 \`ThirdParty\` 目录
2. 将 \`include\` 目录中的头文件复制到对应位置
3. 在 UE 的 \`.Build.cs\` 文件中添加依赖引用

## 文件说明
- 可执行文件: 服务端和示例程序
- 库文件: 用于 UE 集成的静态/动态库
- 配置文件: 默认配置文件模板
EOF
    
    # 创建压缩包
    cd "$ARTIFACTS_DIR"
    tar -czf "$output_suffix.tar.gz" "$output_suffix"
    
    log_success "$target_name 产物打包完成: $ARTIFACTS_DIR/$output_suffix.tar.gz"
}

# 主函数
main() {
    local target="${1:-all}"
    
    log_info "开始跨平台编译 (目标: $target)"
    
    # 清理产物目录
    rm -rf "$ARTIFACTS_DIR"
    mkdir -p "$ARTIFACTS_DIR"
    
    case "$target" in
        "linux"|"all")
            log_info "=== 构建 Linux 版本 ==="
            check_dependencies "linux"
            setup_vcpkg
            install_dependencies "x64-linux"
            build_project "linux-x64" "linux"
            package_artifacts "linux" "picoradar-linux-x64"
            ;;
    esac
    
    case "$target" in
        "windows"|"all")
            log_info "=== 构建 Windows 版本 ==="
            check_dependencies "windows"
            setup_vcpkg
            install_dependencies "x64-windows"
            build_project "windows-x64" "windows"
            package_artifacts "windows" "picoradar-windows-x64"
            ;;
    esac
    
    if [ "$target" = "all" ]; then
        log_success "所有平台构建完成！"
        log_info "产物位置: $ARTIFACTS_DIR"
        ls -la "$ARTIFACTS_DIR"/*.tar.gz
    fi
}

# 显示帮助信息
show_help() {
    cat << EOF
用法: $0 [TARGET]

TARGET:
    linux     仅构建 Linux 版本
    windows   仅构建 Windows 版本 (交叉编译)
    all       构建所有平台版本 (默认)
    
示例:
    $0              # 构建所有平台
    $0 linux        # 仅构建 Linux 版本
    $0 windows      # 仅构建 Windows 版本

注意:
    - Windows 交叉编译需要安装 MinGW-w64
    - 首次运行会自动设置 vcpkg 和安装依赖
    - 构建产物将保存在 artifacts/ 目录中
EOF
}

# 参数解析
case "${1:-}" in
    "-h"|"--help"|"help")
        show_help
        exit 0
        ;;
    *)
        main "$@"
        ;;
esac
