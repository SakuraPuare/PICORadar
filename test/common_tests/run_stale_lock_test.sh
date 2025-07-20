#!/bin/bash

# 测试: 陈旧锁处理
# 1. 创建一个包含无效PID的锁文件。
# 2. 启动测试程序。
# 3. 验证程序是否能识别出这是一个陈旧锁，将其删除，并成功启动。

GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

BUILD_DIR="$1"
TEST_EXECUTABLE="$BUILD_DIR/common_tests"
PID_FILE="/tmp/pico_radar_test.pid"

echo "--- Stale Lock Test Runner ---"

# 1. 清理并创建陈旧锁文件
echo "Step 1: Creating a stale lock file..."
pkill -f "$TEST_EXECUTABLE --lock"
echo "999999" > "$PID_FILE" # 写入一个几乎不可能存在的PID

# 2. 在后台启动测试程序
echo "Step 2: Starting the test executable in --lock mode..."
"$TEST_EXECUTABLE" --lock &
LOCKER_PID=$!
sleep 1

# 3. 验证结果
echo "Step 3: Verifying the outcome..."
if ps -p $LOCKER_PID > /dev/null; then
    echo -e "${GREEN}SUCCESS: Process started and is running (PID: $LOCKER_PID). Stale lock was correctly handled.${NC}"
    # 清理
    kill "$LOCKER_PID"
    wait "$LOCKER_PID" 2>/dev/null
    exit 0
else
    echo -e "${RED}FAILURE: Process failed to start. Stale lock was not handled correctly.${NC}"
    exit 1
fi
