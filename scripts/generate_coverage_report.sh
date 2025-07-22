#!/bin/bash

# 覆盖率报告生成脚本
# 使用方法: ./scripts/generate_coverage_report.sh

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 项目根目录
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build-coverage"
COVERAGE_DIR="${PROJECT_ROOT}/coverage"

echo -e "${GREEN}开始生成覆盖率报告...${NC}"

# 检查构建目录是否存在
if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${RED}错误: 构建目录不存在: $BUILD_DIR${NC}"
    echo -e "${YELLOW}请先运行: cmake --preset coverage && cmake --build build-coverage${NC}"
    exit 1
fi

# 检查gcovr是否安装
if ! command -v gcovr &> /dev/null; then
    echo -e "${RED}错误: gcovr 未安装${NC}"
    echo -e "${YELLOW}请安装 gcovr: pip install gcovr 或 sudo apt-get install gcovr${NC}"
    exit 1
fi

# 创建覆盖率目录
mkdir -p "$COVERAGE_DIR"

# 切换到构建目录
cd "$BUILD_DIR"

echo -e "${GREEN}运行测试以收集覆盖率数据...${NC}"

# 运行测试
ctest --output-on-failure

echo -e "${GREEN}收集并生成覆盖率报告...${NC}"

# 使用 gcovr 生成 HTML、XML、JSON 和文本报告
# 过滤掉不需要的文件

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