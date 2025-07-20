#!/bin/bash

# 这是一个由CTest调用的脚本，用于测试SingleInstanceGuard的单例行为。

# 设置颜色以便输出
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# 检查是否提供了构建目录参数
if [ -z "$1" ]; then
    echo -e "${RED}Error: Build directory not provided.${NC}"
    exit 1
fi

BUILD_DIR="$1"
TEST_EXECUTABLE="$BUILD_DIR/common_tests"
PID_FILE="/tmp/pico_radar_test.pid"

echo "--- SingleInstanceGuard Test Runner ---"
echo "Using executable: $TEST_EXECUTABLE"

# 1. 清理环境
echo "Step 1: Cleaning up previous run..."
rm -f "$PID_FILE"
# 杀死任何可能残留的旧进程
pkill -f "$TEST_EXECUTABLE --lock"

# 2. 在后台启动第一个实例（锁定者）
echo "Step 2: Starting the first instance in the background (--lock mode)..."
"$TEST_EXECUTABLE" --lock &
LOCKER_PID=$!
echo "Locker process started with PID: $LOCKER_PID"

# 3. 等待，确保锁定者有足够的时间来获取锁
sleep 1

# 4. 启动第二个实例（检查者），并捕获其输出和退出码
echo "Step 3: Starting the second instance to check the lock (--check mode)..."
CHECKER_OUTPUT=$("$TEST_EXECUTABLE" --check 2>&1)
CHECKER_EXIT_CODE=$?

# 5. 清理：杀死第一个实例
echo "Step 4: Cleaning up the locker process..."
kill "$LOCKER_PID"
wait "$LOCKER_PID" 2>/dev/null # 等待进程结束，并抑制"Terminated"消息

# 6. 验证结果
echo "Step 5: Verifying results..."
echo "Checker Exit Code: $CHECKER_EXIT_CODE"
echo "Checker Output: $CHECKER_OUTPUT"

if [ "$CHECKER_EXIT_CODE" -eq 0 ]; then
    echo -e "${GREEN}SUCCESS: The second instance correctly failed to acquire the lock and exited with 0.${NC}"
    exit 0
else
    echo -e "${RED}FAILURE: The second instance exited with a non-zero code, or acquired the lock unexpectedly.${NC}"
    exit 1
fi
