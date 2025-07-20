#!/bin/bash

# 集成测试: UDP服务发现
# 1. 启动服务器
# 2. 启动客户端，使用 --discover 模式
# 3. 验证客户端是否能成功发现、连接并认证

GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

BUILD_DIR="$1"
SERVER_EXE="$BUILD_DIR/src/server_app/server_app"
CLIENT_EXE="$BUILD_DIR/test/mock_client/mock_client"
PID_FILE="/tmp/PicoRadar.pid"
CORRECT_TOKEN="pico-radar-super-secret-token-!@#$"
PORT=9006 # Use a new port for this test

echo "--- Integration Test: UDP Discovery (Port: $PORT) ---"

# 启动服务器（带重试逻辑）
SERVER_PID=0
for i in {1..5}; do
    echo "Attempt $i to start server..."
    rm -f "$PID_FILE"
    pkill -f "$SERVER_EXE $PORT"
    sleep 0.5
    "$SERVER_EXE" "$PORT" &
    SERVER_PID=$!
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
    echo -e "${RED}FAILURE: Could not start server.${NC}"
    exit 1
fi

# 运行客户端
echo "Running client in --discover mode..."
timeout 10s "$CLIENT_EXE" --discover "$CORRECT_TOKEN" "discovery_tester"
CLIENT_EXIT_CODE=$?

# 清理服务器
echo "Cleaning up server..."
kill "$SERVER_PID"
wait "$SERVER_PID" 2>/dev/null

# 验证结果
echo "Verifying result..."
if [ "$CLIENT_EXIT_CODE" -eq 0 ]; then
    echo -e "${GREEN}SUCCESS: Client discovered and authenticated successfully.${NC}"
    exit 0
else
    echo -e "${RED}FAILURE: Client exited with code $CLIENT_EXIT_CODE, expected 0.${NC}"
    exit 1
fi
