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

# 检查lcov是否安装
if ! command -v lcov &> /dev/null; then
    echo -e "${RED}错误: lcov 未安装${NC}"
    echo -e "${YELLOW}请安装 lcov: sudo apt-get install lcov (Ubuntu/Debian)${NC}"
    exit 1
fi

# 检查genhtml是否安装
if ! command -v genhtml &> /dev/null; then
    echo -e "${RED}错误: genhtml 未安装${NC}"
    echo -e "${YELLOW}请安装 lcov: sudo apt-get install lcov (Ubuntu/Debian)${NC}"
    exit 1
fi

# 创建覆盖率目录
mkdir -p "$COVERAGE_DIR"

# 切换到构建目录
cd "$BUILD_DIR"

echo -e "${GREEN}运行测试以收集覆盖率数据...${NC}"

# 运行测试
ctest --output-on-failure

echo -e "${GREEN}收集覆盖率数据...${NC}"

# 使用lcov收集覆盖率数据
lcov --capture --directory . --output-file coverage.info

# 过滤掉不需要的文件
lcov --remove coverage.info \
    '/usr/*' \
    '*/test/*' \
    '*/mock_client/*' \
    '*/vcpkg/*' \
    '*/build*/*' \
    '*/CMakeFiles/*' \
    '*/proto/*.pb.*' \
    --output-file coverage_filtered.info

echo -e "${GREEN}生成HTML覆盖率报告...${NC}"

# 生成HTML报告
genhtml coverage_filtered.info --output-directory "$COVERAGE_DIR/html"

# 生成文本报告
lcov --list coverage_filtered.info > "$COVERAGE_DIR/coverage_summary.txt"

echo -e "${GREEN}覆盖率报告生成完成!${NC}"
echo -e "${GREEN}HTML报告位置: $COVERAGE_DIR/html/index.html${NC}"
echo -e "${GREEN}文本报告位置: $COVERAGE_DIR/coverage_summary.txt${NC}"

# 显示覆盖率摘要
echo -e "${YELLOW}覆盖率摘要:${NC}"
cat "$COVERAGE_DIR/coverage_summary.txt" 