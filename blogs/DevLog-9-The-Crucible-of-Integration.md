# 开发日志 #9：集成测试的熔炉——多组件系统的端到端验证

**作者：书樱**  
**日期：2025年7月21日**

> **核心技术**: 集成测试架构、测试替身模式、端到端自动化、系统级行为验证
> 
> **工程亮点**: 多协议集成测试、自动化测试管道、故障注入、性能基准验证

---

## 引言：从零件到整车的跨越

大家好，我是书樱！

经过前八篇开发日志的积累，PICO Radar项目已经拥有了众多经过单元测试验证的"零件"：
- PlayerRegistry (玩家状态管理)
- WebSocketServer (实时通信)
- DiscoveryServer (服务发现)
- SingleInstanceGuard (进程控制)
- ConfigManager (配置管理)

但正如汽车制造业的经验：**一堆能正常工作的零件，并不总能组装成一辆能正常行驶的汽车。** 

今天分享的内容，是我们如何构建一个全面的集成测试体系，确保这些独立的模块能够协同工作，提供稳定可靠的端到端服务。

## 集成测试的挑战：超越单元测试的边界

### 单元测试的局限性

```cpp
// 单元测试示例 - 只验证单一组件
TEST(PlayerRegistryTest, AddPlayerTest) {
    PlayerRegistry registry;
    auto player = std::make_shared<Player>("test_user", "ws_session_123");
    
    registry.addPlayer(player);
    
    EXPECT_EQ(registry.getPlayerCount(), 1);
    EXPECT_TRUE(registry.hasPlayer("test_user"));
}
```

这个测试通过了，但它无法回答以下关键问题：
- 当WebSocket连接断开时，Player能否正确从Registry中移除？
- 多个Player同时更新位置时，广播机制是否正常？
- 服务发现和WebSocket服务能否正确协同工作？

### 集成测试的核心价值

```
系统级行为验证：
┌─────────────────┐    UDP     ┌─────────────────┐
│  MockClient A   │──────────►│ DiscoveryServer │
│ (服务发现)       │◄──────────│  (端口9001)     │
└─────────────────┘   发现响应 └─────────────────┘
        │                              │
        │ WebSocket连接                │ 启动信息
        ▼                              ▼
┌─────────────────┐   数据广播 ┌─────────────────┐
│ WebSocketServer │◄──────────│ PlayerRegistry  │
│  (端口9000)     │────────►│  (状态管理)     │
└─────────────────┘   状态更新 └─────────────────┘
        │                              ▲
        │ 玩家数据                     │ 注册/注销
        ▼                              │
┌─────────────────┐            ┌─────────────────┐
│  MockClient B   │────────────│  MockClient C   │
│ (数据生产者)     │  互相接收  │ (数据消费者)     │
└─────────────────┘            └─────────────────┘
```

## 核心工具：Mock客户端设计

### MockClient架构设计

我们开发了一个功能完备的测试替身`MockClient`，它具备与真实客户端相同的能力：

```cpp
// test/mock_client/mock_client.hpp
class MockClient {
public:
    enum class Mode {
        DISCOVERY_ONLY,     // 仅测试服务发现
        AUTH_FAIL,          // 测试认证失败场景
        AUTH_SUCCESS,       // 测试认证成功场景
        DATA_SEEDER,        // 数据播种者模式
        DATA_LISTENER,      // 数据监听者模式
        STRESS_TEST         // 压力测试模式
    };
    
    explicit MockClient(Mode mode, const std::string& config_file = "");
    
    // 执行测试并返回结果
    int run();
    
private:
    Mode mode_;
    std::string server_host_;
    uint16_t server_port_;
    std::string auth_token_;
    
    // 核心功能模块
    std::unique_ptr<DiscoveryClient> discovery_client_;
    std::unique_ptr<WebSocketClient> ws_client_;
    std::unique_ptr<TestDataGenerator> data_generator_;
    
    // 测试模式执行器
    int runDiscoveryTest();
    int runAuthFailTest();
    int runAuthSuccessTest();
    int runDataSeederTest();
    int runDataListenerTest();
    int runStressTest();
};
```

### 服务发现集成测试

```cpp
int MockClient::runDiscoveryTest() {
    LOG_INFO << "Starting service discovery test...";
    
    auto start_time = std::chrono::steady_clock::now();
    
    // 步骤1: 尝试发现服务器
    auto server_info = discovery_client_->discoverServer(9001, std::chrono::seconds(5));
    
    auto discovery_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time);
    
    if (!server_info) {
        LOG_ERROR << "Service discovery failed within timeout";
        return 1;  // 测试失败
    }
    
    LOG_INFO << "Server discovered in " << discovery_time.count() << "ms";
    LOG_INFO << "Server info: " << server_info->host << ":" << server_info->port;
    
    // 步骤2: 验证发现的服务器信息格式
    if (server_info->host.empty() || server_info->port == 0) {
        LOG_ERROR << "Invalid server info received";
        return 1;
    }
    
    // 步骤3: 尝试连接到发现的服务器
    try {
        ws_client_ = std::make_unique<WebSocketClient>(server_info->host, server_info->port);
        
        if (!ws_client_->connect(std::chrono::seconds(3))) {
            LOG_ERROR << "Failed to connect to discovered server";
            return 1;
        }
        
        LOG_INFO << "Successfully connected to discovered server";
        ws_client_->disconnect();
        
    } catch (const std::exception& e) {
        LOG_ERROR << "Connection to discovered server failed: " << e.what();
        return 1;
    }
    
    LOG_INFO << "✅ Service discovery test passed";
    return 0;  // 测试成功
}
```

### 认证失败测试

```cpp
int MockClient::runAuthFailTest() {
    LOG_INFO << "Starting authentication failure test...";
    
    // 使用无效的认证令牌
    const std::string invalid_token = "INVALID_TOKEN_" + 
        std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    
    try {
        // 连接到服务器
        if (!ws_client_->connect(std::chrono::seconds(3))) {
            LOG_ERROR << "Failed to connect to server";
            return 1;
        }
        
        // 发送无效认证
        nlohmann::json auth_msg = {
            {"type", "auth"},
            {"token", invalid_token},
            {"player_name", "test_user_invalid"}
        };
        
        ws_client_->send(auth_msg.dump());
        
        // 等待服务器响应
        auto response = ws_client_->receive(std::chrono::seconds(5));
        
        if (!response) {
            LOG_INFO << "✅ Server correctly closed connection (no response)";
            return 0;  // 预期行为：服务器应该断开连接
        }
        
        // 如果收到响应，检查是否为错误消息
        try {
            auto json_response = nlohmann::json::parse(*response);
            
            if (json_response.contains("type") && 
                json_response["type"] == "error" &&
                json_response.contains("message")) {
                
                LOG_INFO << "✅ Server correctly rejected authentication: " 
                         << json_response["message"];
                return 0;
            }
            
        } catch (const std::exception& e) {
            LOG_WARNING << "Failed to parse server response: " << e.what();
        }
        
        LOG_ERROR << "❌ Server did not reject invalid authentication";
        return 1;
        
    } catch (const std::exception& e) {
        // 连接异常也是预期行为（服务器拒绝连接）
        LOG_INFO << "✅ Server correctly rejected connection: " << e.what();
        return 0;
    }
}
```

### 数据广播测试

```cpp
int MockClient::runDataSeederTest() {
    LOG_INFO << "Starting data seeder test...";
    
    try {
        // 1. 连接并认证
        if (!connectAndAuth("seeder_test_user")) {
            return 1;
        }
        
        // 2. 发送测试数据
        TestPlayerData test_data = {
            .player_name = "seeder_test_user",
            .position = {1.0f, 2.0f, 3.0f},
            .rotation = {0.0f, 90.0f, 0.0f},
            .timestamp = std::chrono::system_clock::now(),
            .test_marker = "SEEDER_DATA_MARKER_" + 
                          std::to_string(std::chrono::system_clock::now().time_since_epoch().count())
        };
        
        nlohmann::json data_msg = {
            {"type", "player_data"},
            {"player_name", test_data.player_name},
            {"position", {test_data.position.x, test_data.position.y, test_data.position.z}},
            {"rotation", {test_data.rotation.x, test_data.rotation.y, test_data.rotation.z}},
            {"test_marker", test_data.test_marker}
        };
        
        ws_client_->send(data_msg.dump());
        LOG_INFO << "Sent test data with marker: " << test_data.test_marker;
        
        // 3. 保持连接一段时间确保数据被处理
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        // 4. 验证数据是否被正确广播回来
        auto response = ws_client_->receive(std::chrono::seconds(3));
        if (response) {
            try {
                auto json_response = nlohmann::json::parse(*response);
                
                if (json_response.contains("type") && 
                    json_response["type"] == "player_list") {
                    
                    LOG_INFO << "Received player list broadcast";
                    
                    // 查找我们的测试标记
                    if (json_response.contains("players") && 
                        json_response["players"].is_array()) {
                        
                        for (const auto& player : json_response["players"]) {
                            if (player.contains("test_marker") && 
                                player["test_marker"] == test_data.test_marker) {
                                
                                LOG_INFO << "✅ Found our test data in broadcast";
                                return 0;
                            }
                        }
                    }
                }
                
            } catch (const std::exception& e) {
                LOG_WARNING << "Failed to parse broadcast response: " << e.what();
            }
        }
        
        LOG_ERROR << "❌ Test data not found in broadcast";
        return 1;
        
    } catch (const std::exception& e) {
        LOG_ERROR << "Seeder test failed: " << e.what();
        return 1;
    }
}

int MockClient::runDataListenerTest() {
    LOG_INFO << "Starting data listener test...";
    
    try {
        // 1. 连接并认证
        if (!connectAndAuth("listener_test_user")) {
            return 1;
        }
        
        // 2. 等待接收广播数据
        LOG_INFO << "Waiting for player data broadcasts...";
        
        auto timeout = std::chrono::seconds(10);
        auto start_time = std::chrono::steady_clock::now();
        
        while (std::chrono::steady_clock::now() - start_time < timeout) {
            auto response = ws_client_->receive(std::chrono::milliseconds(500));
            
            if (response) {
                try {
                    auto json_response = nlohmann::json::parse(*response);
                    
                    if (json_response.contains("type") && 
                        json_response["type"] == "player_list") {
                        
                        LOG_INFO << "Received player list broadcast";
                        
                        if (json_response.contains("players") && 
                            json_response["players"].is_array() && 
                            !json_response["players"].empty()) {
                            
                            LOG_INFO << "✅ Received non-empty player list with " 
                                     << json_response["players"].size() << " players";
                            
                            // 打印收到的数据以供调试
                            for (const auto& player : json_response["players"]) {
                                if (player.contains("player_name")) {
                                    LOG_INFO << "  Player: " << player["player_name"];
                                }
                            }
                            
                            return 0;  // 成功接收到数据
                        }
                    }
                    
                } catch (const std::exception& e) {
                    LOG_WARNING << "Failed to parse received message: " << e.what();
                    continue;
                }
            }
        }
        
        LOG_ERROR << "❌ No valid player data received within timeout";
        return 1;
        
    } catch (const std::exception& e) {
        LOG_ERROR << "Listener test failed: " << e.what();
        return 1;
    }
}
```

## 自动化测试管道：Shell脚本编排

### 端到端测试脚本

```bash
#!/bin/bash
# test/integration/run_full_integration_test.sh

set -e  # 遇到错误立即退出

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 确保构建是最新的
build_project() {
    log_info "Building project..."
    cd "$PROJECT_ROOT"
    cmake --build build --config Release --parallel
}

# 清理端口占用
cleanup_ports() {
    log_info "Cleaning up ports..."
    pkill -f "server" || true
    pkill -f "mock_client" || true
    sleep 2
}

# 启动服务器
start_server() {
    log_info "Starting server..."
    cd "$BUILD_DIR"
    
    # 使用测试配置启动服务器
    ./server ../config/test_server.json &
    SERVER_PID=$!
    
    # 等待服务器启动
    sleep 3
    
    # 验证服务器是否正常启动
    if ! kill -0 $SERVER_PID 2>/dev/null; then
        log_error "Server failed to start"
        return 1
    fi
    
    log_info "Server started with PID: $SERVER_PID"
}

# 停止服务器
stop_server() {
    if [ ! -z "$SERVER_PID" ]; then
        log_info "Stopping server (PID: $SERVER_PID)..."
        kill $SERVER_PID || true
        wait $SERVER_PID 2>/dev/null || true
    fi
}

# 运行单个测试
run_test() {
    local test_name=$1
    local test_mode=$2
    local timeout=${3:-30}
    
    log_info "Running $test_name..."
    
    cd "$BUILD_DIR"
    timeout $timeout ./mock_client --mode=$test_mode --config=../config/test_server.json
    local result=$?
    
    if [ $result -eq 0 ]; then
        log_info "✅ $test_name PASSED"
        return 0
    else
        log_error "❌ $test_name FAILED (exit code: $result)"
        return 1
    fi
}

# 运行组合测试（需要多个客户端协作）
run_broadcast_test() {
    log_info "Running broadcast test (seeder + listener)..."
    
    cd "$BUILD_DIR"
    
    # 启动监听者（后台运行）
    timeout 30 ./mock_client --mode=data_listener --config=../config/test_server.json &
    LISTENER_PID=$!
    
    # 等待监听者连接
    sleep 2
    
    # 启动播种者
    timeout 30 ./mock_client --mode=data_seeder --config=../config/test_server.json
    SEEDER_RESULT=$?
    
    # 等待监听者完成
    wait $LISTENER_PID
    LISTENER_RESULT=$?
    
    if [ $SEEDER_RESULT -eq 0 ] && [ $LISTENER_RESULT -eq 0 ]; then
        log_info "✅ Broadcast test PASSED"
        return 0
    else
        log_error "❌ Broadcast test FAILED (seeder: $SEEDER_RESULT, listener: $LISTENER_RESULT)"
        return 1
    fi
}

# 主测试流程
main() {
    log_info "Starting PICO Radar Integration Tests"
    
    local failed_tests=0
    local total_tests=0
    
    # 清理环境
    cleanup_ports
    
    # 构建项目
    build_project
    
    # 启动服务器
    if ! start_server; then
        log_error "Failed to start server, aborting tests"
        exit 1
    fi
    
    # 确保清理函数在脚本退出时执行
    trap 'stop_server; cleanup_ports' EXIT
    
    # 测试1: 服务发现
    total_tests=$((total_tests + 1))
    if ! run_test "Service Discovery Test" "discovery_only" 10; then
        failed_tests=$((failed_tests + 1))
    fi
    
    # 测试2: 认证失败
    total_tests=$((total_tests + 1))
    if ! run_test "Authentication Failure Test" "auth_fail" 10; then
        failed_tests=$((failed_tests + 1))
    fi
    
    # 测试3: 认证成功
    total_tests=$((total_tests + 1))
    if ! run_test "Authentication Success Test" "auth_success" 10; then
        failed_tests=$((failed_tests + 1))
    fi
    
    # 测试4: 数据广播
    total_tests=$((total_tests + 1))
    if ! run_broadcast_test; then
        failed_tests=$((failed_tests + 1))
    fi
    
    # 测试5: 压力测试
    total_tests=$((total_tests + 1))
    if ! run_test "Stress Test" "stress_test" 60; then
        failed_tests=$((failed_tests + 1))
    fi
    
    # 测试结果汇总
    log_info "======================"
    log_info "Integration Test Results:"
    log_info "Total tests: $total_tests"
    log_info "Passed: $((total_tests - failed_tests))"
    log_info "Failed: $failed_tests"
    
    if [ $failed_tests -eq 0 ]; then
        log_info "🎉 All integration tests PASSED!"
        exit 0
    else
        log_error "💥 $failed_tests test(s) FAILED!"
        exit 1
    fi
}

# 执行主函数
main "$@"
```

## 性能基准集成测试

### 并发连接压力测试

```cpp
int MockClient::runStressTest() {
    LOG_INFO << "Starting stress test...";
    
    const int NUM_CONCURRENT_CLIENTS = 50;
    const auto TEST_DURATION = std::chrono::seconds(30);
    
    std::vector<std::future<bool>> client_futures;
    std::atomic<int> successful_connections{0};
    std::atomic<int> message_sent{0};
    std::atomic<int> message_received{0};
    
    auto start_time = std::chrono::steady_clock::now();
    
    // 启动多个并发客户端
    for (int i = 0; i < NUM_CONCURRENT_CLIENTS; ++i) {
        client_futures.push_back(std::async(std::launch::async, [&, i]() {
            return runStressClient(i, TEST_DURATION, successful_connections, 
                                 message_sent, message_received);
        }));
    }
    
    // 等待所有客户端完成
    int successful_clients = 0;
    for (auto& future : client_futures) {
        if (future.get()) {
            successful_clients++;
        }
    }
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time);
    
    // 性能指标计算
    double connection_success_rate = (double)successful_connections / NUM_CONCURRENT_CLIENTS;
    double message_throughput = (double)message_sent / duration.count();
    double message_loss_rate = 1.0 - ((double)message_received / message_sent);
    
    LOG_INFO << "Stress Test Results:";
    LOG_INFO << "  Test Duration: " << duration.count() << " seconds";
    LOG_INFO << "  Concurrent Clients: " << NUM_CONCURRENT_CLIENTS;
    LOG_INFO << "  Successful Connections: " << successful_connections.load();
    LOG_INFO << "  Connection Success Rate: " << (connection_success_rate * 100) << "%";
    LOG_INFO << "  Messages Sent: " << message_sent.load();
    LOG_INFO << "  Messages Received: " << message_received.load();
    LOG_INFO << "  Message Throughput: " << message_throughput << " msg/sec";
    LOG_INFO << "  Message Loss Rate: " << (message_loss_rate * 100) << "%";
    
    // 性能基准验证
    bool performance_acceptable = 
        connection_success_rate >= 0.95 &&  // 95%连接成功率
        message_loss_rate <= 0.01 &&        // 1%消息丢失率
        message_throughput >= 100;           // 100消息/秒吞吐量
    
    if (performance_acceptable) {
        LOG_INFO << "✅ Stress test PASSED - Performance benchmarks met";
        return 0;
    } else {
        LOG_ERROR << "❌ Stress test FAILED - Performance below acceptable thresholds";
        return 1;
    }
}

bool MockClient::runStressClient(int client_id, 
                                std::chrono::seconds duration,
                                std::atomic<int>& successful_connections,
                                std::atomic<int>& message_sent,
                                std::atomic<int>& message_received) {
    try {
        // 1. 服务发现
        auto server_info = discovery_client_->discoverServer(9001, std::chrono::seconds(2));
        if (!server_info) {
            LOG_WARNING << "Client " << client_id << ": Discovery failed";
            return false;
        }
        
        // 2. 连接并认证
        auto ws_client = std::make_unique<WebSocketClient>(server_info->host, server_info->port);
        if (!ws_client->connect(std::chrono::seconds(3))) {
            LOG_WARNING << "Client " << client_id << ": Connection failed";
            return false;
        }
        
        // 认证
        nlohmann::json auth_msg = {
            {"type", "auth"},
            {"token", "valid_test_token"},
            {"player_name", "stress_test_user_" + std::to_string(client_id)}
        };
        
        ws_client->send(auth_msg.dump());
        successful_connections++;
        
        // 3. 持续发送和接收数据
        auto end_time = std::chrono::steady_clock::now() + duration;
        int local_sent = 0;
        int local_received = 0;
        
        while (std::chrono::steady_clock::now() < end_time) {
            // 发送位置数据
            nlohmann::json data_msg = {
                {"type", "player_data"},
                {"player_name", "stress_test_user_" + std::to_string(client_id)},
                {"position", {
                    static_cast<float>(std::rand() % 100), 
                    static_cast<float>(std::rand() % 100), 
                    static_cast<float>(std::rand() % 100)
                }},
                {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()}
            };
            
            ws_client->send(data_msg.dump());
            local_sent++;
            message_sent++;
            
            // 尝试接收消息
            auto response = ws_client->receive(std::chrono::milliseconds(100));
            if (response) {
                local_received++;
                message_received++;
            }
            
            // 控制发送频率
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        LOG_DEBUG << "Client " << client_id << ": Sent " << local_sent 
                 << ", Received " << local_received;
        
        return true;
        
    } catch (const std::exception& e) {
        LOG_WARNING << "Client " << client_id << " failed: " << e.what();
        return false;
    }
}
```

## CI/CD集成：持续验证

### GitHub Actions工作流

```yaml
# .github/workflows/integration-tests.yml
name: Integration Tests

on:
  push:
    branches: [ main, develop ]
  pull_request:
    branches: [ main ]

jobs:
  integration-test:
    runs-on: ubuntu-latest
    
    steps:
    - uses: actions/checkout@v3
    
    - name: Install dependencies
      run: |
        sudo apt-get update
        sudo apt-get install -y cmake ninja-build clang-12 libc++-12-dev libc++abi-12-dev
        
    - name: Setup vcpkg
      uses: lukka/run-vcpkg@v11
      with:
        vcpkgGitCommitId: '2024.05.24'
        
    - name: Configure CMake
      run: |
        cmake --preset=default
        
    - name: Build project
      run: |
        cmake --build build --config Release --parallel
        
    - name: Run integration tests
      run: |
        cd test/integration
        chmod +x run_full_integration_test.sh
        ./run_full_integration_test.sh
        
    - name: Upload test results
      if: always()
      uses: actions/upload-artifact@v3
      with:
        name: integration-test-results
        path: |
          logs/
          test/integration/results/
```

## 结语：质量保证的完整闭环

通过构建全面的集成测试体系，我们实现了PICO Radar系统级质量保证的完整闭环：

### 验证覆盖的核心场景
1. **服务发现流程**: UDP协议的端到端验证
2. **认证安全边界**: 有效/无效令牌的正确处理
3. **数据流管道**: 从客户端→服务器→其他客户端的完整数据路径
4. **并发处理能力**: 多客户端并发访问的稳定性
5. **性能基准验证**: 吞吐量、延迟、资源利用率

### 自动化质量门禁
- **构建时验证**: 每次代码变更自动触发集成测试
- **性能回归检测**: 自动化性能基准比较
- **故障快速定位**: 详细的测试日志和错误报告
- **多环境兼容性**: 跨平台测试验证

这个集成测试体系不仅是代码质量的守护者，更是我们向生产环境部署的信心来源。

---

**技术栈总结**:
- **测试架构**: Mock客户端、测试替身模式、端到端验证
- **自动化工具**: Shell脚本编排、CI/CD集成、性能基准测试
- **并发测试**: 多客户端压力测试、性能指标验证、资源监控
- **质量保证**: 功能验证、性能验证、安全验证、兼容性验证

**下一站**: DevLog-10 将探讨PICO Radar测试革命的完整历程，包括测试框架演进、代码覆盖率分析、以及企业级质量保证实践。

---

大家好，我是书樱！

在过去的系列日志中，我们从零开始，为PICO Radar项目设计了蓝图，搭建了构建系统，用TDD实现了核心逻辑，并攻克了网络编程中的种种难关。我们的代码库中，每一个独立的模块都经过了单元测试的验证。然而，软件开发的一个永恒真理是：**一堆能正常工作的零件，并不总能组装成一辆能正常行驶的汽车。**

今天，我想分享的，就是我们将这些“零件”组装起来，并投入“集成熔炉”进行全面锻造的经历。

### 超越单元测试：集成测试的必要性

我们遵循经典的**测试金字塔 (Testing Pyramid)** 模型。底层是大量的、快速的**单元测试**，它们验证了单个类或函数的正确性。但单元测试有其固有的局限性：它无法验证模块之间的交互。我们的`PlayerRegistry`单元测试通过了，`WebsocketServer`的框架也搭建好了，但它们组合在一起时，能正确地处理一个真实的客户端连接、认证、数据交换的全过程吗？

这就是**集成测试**的用武之地。它的使命，就是验证多个独立开发的模块在协同工作时的正确性。

### 关键工具：作为“测试替身”的`mock_client`

要对我们的`server`进行集成测试，我们需要一个能够与之交互的客户端。在等待真实的PICO硬件和Unreal Engine集成之前，我们开发了一个至关重要的工具：`mock_client`。

在测试理论中，`mock_client`是一个典型的**测试替身 (Test Double)**。它是一个功能齐全、但其行为完全由测试脚本控制的真实程序。它能精确地模拟真实客户端的各种行为：
-   通过UDP广播进行服务发现。
-   使用正确或错误的令牌发起认证。
-   上报模拟的位置数据。
-   监听并解析从服务器广播的数据。

通过命令行参数，我们可以指挥`mock_client`扮演不同的“角色”，这使其成为我们自动化集成测试的基石。

### 四大支柱：验证系统关键路径

我们围绕`mock_client`构建了四个核心的集成测试场景，每一个都由一个shell脚本驱动，实现了“启动服务器 -> 运行客户端 -> 验证结果 -> 清理环境”的完全自动化。

1.  **`AuthFailTest` (安全边界测试)**:
    此测试验证服务器能否正确处理提供了无效凭证的客户端。`mock_client`被设计为：只有当服务器因认证失败而**正确地**拒绝或断开连接时，它才会以成功码（0）退出。这验证了系统的安全边界和错误处理路径。

2.  **`AuthSuccessTest` (核心成功路径测试)**:
    与上一个测试相反，它验证了使用有效凭证的客户端能够顺利完成认证。这确保了系统的“正门”是畅通的。

3.  **`BroadcastTest` (核心业务流测试)**:
    这是一个多方交互测试，验证了系统的核心数据管道。
    -   一个`mock_client`扮演“播种者”（Seeder），连接服务器，发送一条数据，然后断开。
    -   另一个`mock_client`扮演“监听者”（Listener），连接服务器，并断言自己收到了一个包含“播种者”数据的**非空**玩家列表。
    这个测试的通过，证明了数据可以完整地从一个客户端流经服务器，再到达另一个客户端。

4.  **`DiscoveryTest` (多协议交互测试)**:
    此测试验证了我们的“零配置”承诺。`mock_client`在不知道服务器IP的情况下，通过UDP广播发现服务器，然后使用获取到的地址建立TCP WebSocket连接，并成功完成认证。它验证了系统在UDP和TCP两种不同协议间协同工作的能力。

### 结语：从“模块正确”到“系统可信”

通过这套严格的集成测试的锻造，PICO Radar项目完成了一次质的飞跃——从“一堆经过单元测试的、功能正确的模块”，演变为“一个其整体行为经过验证的、可信赖的系统”。

这个过程再次印证了一个核心工程理念：**测试不仅是开发的附属品，它本身就是设计和文档的一部分**。我们的测试套件，就是一份描述系统真实行为的、最精确的、可执行的“活文档”。

站在这坚实可靠的基石上，我们为第一阶段的开发画上了一个完美的句号。我们对系统的质量充满了信心，并已为下一阶段的挑战——构建供游戏引擎使用的`client_lib`——做好了充分的准备。

感谢您一路的关注与陪伴，PICO Radar的故事，未完待续。

---
书樱
2025年7月21日