#!/bin/bash

# 集成测试: 数据广播
# 1. 启动服务器
# 2. 启动一个客户端(Seeder)发送数据
# 3. 启动另一个客户端(Listener)接收广播
# 4. 验证Listener是否收到了Seeder的数据

GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

BUILD_DIR="$1"
SERVER_EXE="$BUILD_DIR/src/server_app/server_app"
CLIENT_EXE="$BUILD_DIR/test/mock_client/mock_client"
PID_FILE="/tmp/PicoRadar.pid"
CORRECT_TOKEN="pico-radar-super-secret-token-!@#$"
PORT=9005

echo "--- Integration Test: Broadcast ---"

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

# 启动Seeder客户端
echo "Running Seeder client..."
timeout 10s "$CLIENT_EXE" 127.0.0.1 "$PORT" "$CORRECT_TOKEN" "seeder" --seed-data
SEEDER_EXIT_CODE=$?
if [ "$SEEDER_EXIT_CODE" -ne 0 ]; then
    echo -e "${RED}FAILURE: Seeder client failed to run (Exit code: $SEEDER_EXIT_CODE).${NC}"
    kill "$SERVER_PID"
    exit 1
fi
echo "Seeder client finished."

# 启动Listener客户端
echo "Running Listener client..."
timeout 10s "$CLIENT_EXE" 127.0.0.1 "$PORT" "$CORRECT_TOKEN" "listener" --test-broadcast
LISTENER_EXIT_CODE=$?

# 清理服务器
echo "Cleaning up server..."
kill "$SERVER_PID"
wait "$SERVER_PID" 2>/dev/null

# 验证结果
echo "Verifying result..."
if [ "$LISTENER_EXIT_CODE" -eq 0 ]; then
    echo -e "${GREEN}SUCCESS: Listener client received broadcast and exited with 0.${NC}"
    exit 0
else
    echo -e "${RED}FAILURE: Listener client exited with code $LISTENER_EXIT_CODE, expected 0.${NC}"
    exit 1
fi
