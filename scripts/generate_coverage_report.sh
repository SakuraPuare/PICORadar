#!/bin/bash

# PICORadar 覆盖率报告生成脚本
# 使用方法: ./scripts/generate_coverage_report.sh

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

# 项目根目录
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${PROJECT_ROOT}/build/coverage"
COVERAGE_DIR="${PROJECT_ROOT}/coverage"

log_info "开始生成覆盖率报告..."

# 检查构建目录是否存在
if [ ! -d "$BUILD_DIR" ]; then
    log_error "构建目录不存在: $BUILD_DIR"
    log_info "请先运行: cmake --preset dev-debug-coverage && cmake --build build/coverage"
    exit 1
fi

# 检查gcovr是否安装
if ! command -v gcovr &> /dev/null; then
    log_error "gcovr 未安装"
    log_info "请安装 gcovr: pip install gcovr 或 sudo apt-get install gcovr"
    exit 1
fi

# 创建覆盖率目录
mkdir -p "$COVERAGE_DIR"

# 切换到构建目录
cd "$BUILD_DIR"

log_info "运行测试以收集覆盖率数据..."

# 运行测试
if ctest --output-on-failure; then
    log_success "测试运行完成"
else
    log_warning "部分测试失败，但继续生成覆盖率报告"
fi

log_info "收集并生成覆盖率报告..."

# 生成时间戳
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
REPORT_DIR="$COVERAGE_DIR/$TIMESTAMP"
mkdir -p "$REPORT_DIR"

# 使用 gcovr 生成多种格式的报告
# 过滤规则：包含 src/ 目录，排除测试和第三方代码
FILTER_ARGS=(
    --filter "$PROJECT_ROOT/src/"
    --exclude "$PROJECT_ROOT/build/"
    --exclude "$PROJECT_ROOT/vcpkg/"
    --exclude "$PROJECT_ROOT/vcpkg_installed/"
    --exclude "$PROJECT_ROOT/test/"
    --exclude ".*test.*"
    --exclude ".*_test\.cpp"
)

# 生成 HTML 报告
log_info "生成 HTML 覆盖率报告..."
gcovr "${FILTER_ARGS[@]}" \
    --html-details "$REPORT_DIR/coverage.html" \
    --html-title "PICORadar Coverage Report" \
    .

# 生成 XML 报告 (用于CI/CD)
log_info "生成 XML 覆盖率报告..."
gcovr "${FILTER_ARGS[@]}" \
    --xml "$REPORT_DIR/coverage.xml" \
    .

# 生成 JSON 报告
log_info "生成 JSON 覆盖率报告..."
gcovr "${FILTER_ARGS[@]}" \
    --json "$REPORT_DIR/coverage.json" \
    .

# 生成文本报告并显示
log_info "生成文本覆盖率报告..."
gcovr "${FILTER_ARGS[@]}" \
    --txt "$REPORT_DIR/coverage.txt" \
    . | tee "$REPORT_DIR/coverage_summary.txt"

# 创建符号链接到最新报告
cd "$COVERAGE_DIR"
rm -f latest
ln -sf "$TIMESTAMP" latest

log_success "覆盖率报告生成完成！"
log_info "报告位置:"
log_info "  HTML: $REPORT_DIR/coverage.html"
log_info "  XML:  $REPORT_DIR/coverage.xml"
log_info "  JSON: $REPORT_DIR/coverage.json"
log_info "  文本: $REPORT_DIR/coverage.txt"
log_info ""
log_info "最新报告链接: $COVERAGE_DIR/latest/"

# 显示覆盖率摘要
if [ -f "$REPORT_DIR/coverage_summary.txt" ]; then
    log_info "覆盖率摘要:"
    cat "$REPORT_DIR/coverage_summary.txt"
fi

# HTML 报告
GCOVR_EXCLUDES=(
    '--exclude' '../test/.*'
    '--exclude' '../mock_client/.*'
    '--exclude' '../vcpkg/.*'
    '--exclude' '../build.*/.*'
    '--exclude' '../CMakeFiles/.*'
    '--exclude' '.*proto/.*\\.pb\\..*'
    '--exclude' '/usr/.*'
)

gcovr . \
    --root .. \
    "${GCOVR_EXCLUDES[@]}" \
    --html-details "$COVERAGE_DIR/html/index.html" \
    --html-title "PICORadar Coverage Report" \
    --xml "$COVERAGE_DIR/coverage.xml" \
    --json "$COVERAGE_DIR/coverage.json" \
    --print-summary > "$COVERAGE_DIR/coverage_summary.txt"

echo -e "${GREEN}覆盖率报告生成完成!${NC}"
echo -e "${GREEN}HTML报告位置: $COVERAGE_DIR/html/index.html${NC}"
echo -e "${GREEN}XML报告位置: $COVERAGE_DIR/coverage.xml${NC}"
echo -e "${GREEN}JSON报告位置: $COVERAGE_DIR/coverage.json${NC}"
echo -e "${GREEN}文本报告位置: $COVERAGE_DIR/coverage_summary.txt${NC}"

# 显示覆盖率摘要
echo -e "${YELLOW}覆盖率摘要:${NC}"
cat "$COVERAGE_DIR/coverage_summary.txt" 