# 开发日志 #17: 极限施压——20个客户端并发的严峻考验

你好，我是书樱！

在PICO Radar的开发旅程中，我们始终将“稳定支持20名玩家”作为核心的承诺。理论分析和单元测试为我们构建了坚实的信心，但没有什么比一场真实、残酷的压力测试更能检验系统的真正能耐。今天，我们就将系统推向极限，模拟20个客户端在同一时间疯狂地连接、交互，以此来验证我们架构的稳定性和性能。

## 为什么压力测试至关重要？

在软件工程中，并发问题是出了名的棘手。当多个线程或进程在同一时刻争抢资源时，理论上完美的代码也可能暴露出隐藏的缺陷，如：

-   **竞态条件 (Race Conditions)**：两个或更多的线程访问共享数据，而最终的结果依赖于线程调度的精确时序。
-   **死锁 (Deadlocks)**：两个线程互相等待对方释放锁，导致程序永久挂起。
-   **资源耗尽**：连接数的激增可能迅速耗尽服务器的文件描述符、内存或CPU资源。

我们的服务器基于异步IO和多线程模型，`PlayerRegistry` 和会话列表都是被多个IO线程和广播线程共享的关键资源。只有通过高并发测试，我们才能确保为其设计的 `std::mutex` 和 `net::strand` 保护措施是真正有效的。

## 测试方案的设计

为了模拟真实世界的使用场景，我们精心设计了一个全新的集成测试：`StressIntegrationTest`。它的目标非常明确：在短时间内创建20个模拟客户端，让它们同时连接到服务器，持续交换数据，最后验证服务器能否自始至终保持稳定，并在结束后正确清理所有资源。

### 模拟客户端的“压力模式”

首先，我们必须让模拟客户端 (`SyncClient`) 的行为更接近一个真正的PICO设备。一个真实的客户端在连接后，会持续地做两件事：

1.  向服务器高频发送自己的位置数据。
2.  接收服务器广播的其他所有玩家的数据。

为此，我们在 `SyncClient` 中增加了一个新的 `--test-stress` 模式。

```cpp
// test/mock_client/sync_client.cpp

void SyncClient::stress_test(const std::string& player_id) {
  LOG(INFO) << "Starting stress test for player " << player_id;
  beast::flat_buffer buffer;
  beast::error_code ec;

  // 1. 读取线程
  std::thread read_thread([this, &buffer, &ec]() {
    while (ws_ && ws_->is_open()) {
      // ... 非阻塞地读取服务器广播的数据 ...
    }
  });

  // 2. 写入线程
  std.thread write_thread([this, &player_id, &ec]() {
    while (ws_ && ws_->is_open()) {
      try {
        send_test_data(player_id); // 持续发送自己的位置
        std::this_thread::sleep_for(100ms);
      } catch (const std::exception& e) {
        // ... 异常处理 ...
      }
    }
  });

  read_thread.join();
  write_thread.join();
}
```

当以这个模式运行时，每个 `SyncClient` 内部会启动两个并行的线程，完美地模拟了真实客户端的收发行为。

### 压力测试的主体 (`test_stress.cpp`)

这是我们执行测试的核心。代码结构清晰地遵循了“Arrange-Act-Assert”模式。

```cpp
// test/integration_tests/test_stress.cpp

TEST_F(StressIntegrationTest, Handle20ConcurrentClients) {
  // Arrange: 准备客户端线程容器
  std::vector<std::thread> client_threads;
  client_threads.reserve(NUM_CLIENTS);

  LOG(INFO) << "Starting " << NUM_CLIENTS << " client threads...";

  // Act (Part 1): 启动20个客户端线程
  for (int i = 0; i < NUM_CLIENTS; ++i) {
    std::string player_id = "stress_player_" + std::to_string(i);
    client_threads.emplace_back(run_client_task, player_id, port_);
    // 短暂间隔以错开连接高峰，模拟真实世界中的随机性
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  LOG(INFO) << "All " << NUM_CLIENTS << " clients are connecting...";

  // Assert (Part 1): 验证所有客户端是否都已成功连接并注册
  std::this_thread::sleep_for(std::chrono::seconds(5));
  EXPECT_EQ(registry_->getPlayerCount(), NUM_CLIENTS);

  // 让测试运行一段时间，观察稳定性
  std::this_thread::sleep_for(TEST_DURATION);

  // Act (Part 2): 停止服务器，这将触发所有客户端断开连接
  server_->stop();

  // 等待所有客户端线程执行完毕
  for (auto& t : client_threads) {
    if (t.joinable()) {
      t.join();
    }
  }

  // Assert (Part 2): 验证所有玩家数据是否已被正确清理
  ASSERT_EQ(registry_->getPlayerCount(), 0);
}
```

这个测试的精妙之处在于：

1.  **分阶段断言**：它先验证了所有客户端都能成功“进入”系统，再验证它们在系统关闭后都能被优雅地“清理”出去。
2.  **资源清理验证**：断言 `registry_->getPlayerCount()` 最终为0，这直接证明了 `Session` 的析构函数与 `PlayerRegistry::removePlayer` 之间的联动是线程安全的，没有一个玩家数据因为并发访问而被遗漏。
3.  **模拟真实性**：通过微小的延时 (`50ms`) 分散了连接请求，避免了瞬间的TCP连接风暴，更贴近现实。

## 结果与展望

测试结果令人振奋——**成功通过**！

在GitHub Actions的CI环境中，这个测试稳定地运行，服务器在4个IO线程的配置下，从容地处理了20个客户端的并发连接和持续数据交换，没有出现任何崩溃、死锁或数据不一致的情况。最后，所有资源都被完美释放。

这次成功的压力测试是我们项目的一个重要里程碑。它用实际数据证明了我们选择的技术栈（Boost.Asio）和我们设计的并发模型是正确且可靠的。我们现在可以满怀信心地说，PICO Radar已经为迎接真实世界的多人VR体验做好了准备。

下一站，我们将继续完善客户端库，并准备与Unreal Engine进行最终的集成！

感谢你的关注，我们下次开发日志再见！

——书樱 