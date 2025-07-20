#!/bin/bash

# 集成测试: 认证失败
# 这个脚本包含了重试逻辑，以处理由于TCP TIME_WAIT状态导致的端口绑定失败。

GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

BUILD_DIR="$1"
SERVER_EXE="$BUILD_DIR/src/server_app/server_app"
CLIENT_EXE="$BUILD_DIR/test/mock_client/mock_client"
PID_FILE="/tmp/PicoRadar.pid"
PORT=9003

echo "--- Integration Test: Authentication Failure (Port: $PORT) ---"

# 启动服务器（带重试逻辑）
SERVER_PID=0
for i in {1..5}; do
    echo "Attempt $i to start server..."
    # 清理
    rm -f "$PID_FILE"
    pkill -f "$SERVER_EXE $PORT"
    sleep 0.5

    # 启动
    "$SERVER_EXE" "$PORT" &
    SERVER_PID=$!
    
    # 检查服务器是否在运行并监听
    sleep 1
    if ps -p $SERVER_PID > /dev/null && netstat -lnt | grep -q ":$PORT"; then
        echo "Server started successfully with PID $SERVER_PID."
        break
    else
        echo "Server failed to start or listen, retrying..."
        SERVER_PID=0
    fi
done

if [ "$SERVER_PID" -eq 0 ]; then
    echo -e "${RED}FAILURE: Could not start server after multiple attempts.${NC}"
    exit 1
fi

# 运行客户端
echo "Running client with wrong token..."
timeout 10s "$CLIENT_EXE" 127.0.0.1 "$PORT" "wrong-token" "auth_fail_tester" --test-auth-fail
CLIENT_EXIT_CODE=$?

# 清理服务器
echo "Cleaning up server..."
kill "$SERVER_PID"
wait "$SERVER_PID" 2>/dev/null

# 验证结果
echo "Verifying result..."
if [ "$CLIENT_EXIT_CODE" -eq 0 ]; then
    echo -e "${GREEN}SUCCESS: Client exited with 0 as expected.${NC}"
    exit 0
else
    echo -e "${RED}FAILURE: Client exited with code $CLIENT_EXIT_CODE, expected 0.${NC}"
    exit 1
fi
