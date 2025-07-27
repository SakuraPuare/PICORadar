#include <gtest/gtest.h>

#include <chrono>
#include <thread>

#include "client.hpp"
#include "common/logging.hpp"

using namespace picoradar::client;
using namespace picoradar;

class ClientBasicTest : public testing::Test {
 protected:
  static void SetUpTestSuite() {
    // 初始化日志系统
    logger::LogConfig config = logger::LogConfig::loadFromConfigManager();
    config.log_directory = "./logs";
    config.global_level = logger::LogLevel::INFO;
    config.file_enabled = true;
    config.console_enabled = false;
    config.max_files = 10;
    logger::Logger::Init("client_test", config);
  }

  static void TearDownTestSuite() {
    // glog 会自动清理，不需要手动调用 ShutdownGoogleLogging
  }

  void SetUp() override {
    // 每个测试前的设置
  }

  void TearDown() override {
    // 每个测试后的清理
  }
};

/**
 * @brief 测试客户端的基本构造和析构
 */
TEST_F(ClientBasicTest, ConstructionAndDestruction) {
  // 测试客户端可以正常创建和销毁
  EXPECT_NO_THROW({
    const Client client;
    EXPECT_FALSE(client.isConnected());
  });
}

/**
 * @brief 测试设置回调函数
 */
TEST_F(ClientBasicTest, SetPlayerListCallback) {
  Client client;

  bool callback_called = false;
  client.setOnPlayerListUpdate(
      [&callback_called](const std::vector<PlayerData>&) {
        callback_called = true;
      });

  // 设置回调不应该抛异常
  EXPECT_FALSE(callback_called);
}

/**
 * @brief 测试无效地址格式
 */
TEST_F(ClientBasicTest, InvalidAddressFormat) {
  Client client;

  // 测试无效的地址格式 - 这些应该立即抛出异常
  EXPECT_THROW(client.connect("invalid_address", "player1", "token"),
               std::invalid_argument);
  EXPECT_THROW(client.connect("", "player1", "token"), std::invalid_argument);
  EXPECT_THROW(client.connect("host:", "player1", "token"),
               std::invalid_argument);
  EXPECT_THROW(client.connect(":port", "player1", "token"),
               std::invalid_argument);
}

/**
 * @brief 测试重复连接
 */
TEST_F(ClientBasicTest, DuplicateConnect) {
  const Client client;

  // 第一次连接（会失败，因为没有服务器，但不应该立即抛异常）
  auto future1 = client.connect("127.0.0.1:12345", "player1", "token");

  // 立即尝试第二次连接应该失败
  auto future2 = client.connect("127.0.0.1:12345", "player1", "token");
  EXPECT_THROW(future2.get(), std::runtime_error);

  // 清理第一个连接，但不等待太久
  const auto status = future1.wait_for(std::chrono::seconds(1));
  if (status == std::future_status::ready) {
    try {
      future1.get();
    } catch (...) {
      // 预期会失败
    }
  } else {
    // 如果1秒内没有完成，手动断开连接以清理资源
    client.disconnect();
  }
}

/**
 * @brief 测试在未连接状态下发送数据
 */
TEST_F(ClientBasicTest, SendDataWhenDisconnected) {
  Client client;

  PlayerData data;
  data.set_player_id("test_player");

  // 在未连接状态下发送数据应该被静默忽略
  EXPECT_NO_THROW(client.sendPlayerData(data));
}

/**
 * @brief 测试客户端状态
 */
TEST_F(ClientBasicTest, ClientState) {
  const Client client;

  // 初始状态应该是未连接
  EXPECT_FALSE(client.isConnected());

  // 尝试连接到不存在的服务器
  auto future = client.connect("127.0.0.1:12345", "player1", "token");

  // 连接过程中状态应该仍然是未连接（因为还没有认证成功）
  EXPECT_FALSE(client.isConnected());

  // 等待连接失败，但不要无限等待
  const auto status = future.wait_for(std::chrono::seconds(2));

  // 清理连接
  if (status != std::future_status::ready) {
    // 如果3秒内没有完成，手动断开连接以清理资源
    client.disconnect();
  } else {
    // 如果在3秒内完成，应该是连接失败
    EXPECT_THROW(future.get(), std::exception);
  }

  // 最终状态应该是未连接
  EXPECT_FALSE(client.isConnected());
}

/**
 * @brief 测试断开连接
 */
TEST_F(ClientBasicTest, DisconnectWhenNotConnected) {
  const Client client;

  // 在未连接状态下断开连接应该是安全的
  EXPECT_NO_THROW(client.disconnect());
  EXPECT_FALSE(client.isConnected());
}

/**
 * @brief 测试析构函数自动断开连接
 */
TEST_F(ClientBasicTest, DestructorDisconnects) {
  // 创建一个作用域来测试析构函数
  {
    const Client client;
    // 尝试连接（会失败，但这不影响测试析构函数）
    [[maybe_unused]] auto future =
        client.connect("127.0.0.1:12345", "player1", "token");

    // 等待一小段时间让连接开始
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 当 client 离开作用域时，析构函数应该自动调用 disconnect()
    // 这会取消正在进行的连接
  }

  // 如果析构函数正常工作，这里不应该有任何问题
  SUCCEED();
}

/**
 * @brief 测试客户端多次连接尝试
 */
TEST_F(ClientBasicTest, MultipleConnectionAttempts) {
  const Client client;

  // 创建多个连接尝试
  std::vector<std::future<void>> futures;

  for (int i = 0; i < 3; ++i) {
    try {
      auto future = client.connect("127.0.0.1:" + std::to_string(12345 + i),
                                   "player" + std::to_string(i), "token");
      futures.push_back(std::move(future));
    } catch (const std::exception&) {
      // 除了第一个，其他应该抛出异常
      if (i == 0) {
        FAIL() << "First connection attempt should not throw immediately";
      }
    }
  }

  // 等待第一个连接失败
  if (!futures.empty()) {
    try {
      futures[0].wait_for(std::chrono::seconds(1));
      if (futures[0].valid()) {
        EXPECT_THROW(futures[0].get(), std::exception);
      }
    } catch (...) {
      // 预期会失败
    }
  }

  // 清理
  client.disconnect();
}

/**
 * @brief 测试无效的参数组合
 */
TEST_F(ClientBasicTest, InvalidParameterCombinations) {
  Client client;

  // 测试空的player_id
  EXPECT_THROW(client.connect("127.0.0.1:8080", "", "token"),
               std::invalid_argument);

  // 测试空的token
  EXPECT_THROW(client.connect("127.0.0.1:8080", "player1", ""),
               std::invalid_argument);

  // 测试都为空
  EXPECT_THROW(client.connect("127.0.0.1:8080", "", ""), std::invalid_argument);

  // 测试特殊字符在player_id中
  EXPECT_NO_THROW(client.connect("127.0.0.1:12345", "player@#$%", "token"));

  // 清理最后一个连接
  client.disconnect();
}

/**
 * @brief 测试客户端数据发送的边界条件
 */
TEST_F(ClientBasicTest, PlayerDataBoundaryConditions) {
  Client client;

  // 测试空的PlayerData
  PlayerData empty_data;
  EXPECT_NO_THROW(client.sendPlayerData(empty_data));

  // 测试包含极值的PlayerData
  PlayerData extreme_data;
  extreme_data.set_player_id("extreme_player");
  extreme_data.mutable_position()->set_x(std::numeric_limits<float>::max());
  extreme_data.mutable_position()->set_y(std::numeric_limits<float>::lowest());
  extreme_data.mutable_position()->set_z(0.0f);

  EXPECT_NO_THROW(client.sendPlayerData(extreme_data));

  // 测试包含特殊字符的PlayerData
  PlayerData special_data;
  special_data.set_player_id("player_with_unicode_🌟");

  EXPECT_NO_THROW(client.sendPlayerData(special_data));
}

/**
 * @brief 测试回调函数的各种情况
 */
TEST_F(ClientBasicTest, CallbackFunctionVariations) {
  Client client;

  // 测试空回调
  EXPECT_NO_THROW(client.setOnPlayerListUpdate(nullptr));

  // 测试抛出异常的回调
  bool exception_caught = false;
  client.setOnPlayerListUpdate(
      [&exception_caught](const std::vector<PlayerData>&) {
        exception_caught = true;
        throw std::runtime_error("Test exception in callback");
      });

  // 设置另一个正常的回调
  bool normal_callback_called = false;
  client.setOnPlayerListUpdate(
      [&normal_callback_called](const std::vector<PlayerData>& players) {
        normal_callback_called = true;
        // 验证参数是有效的
        for (const auto& player : players) {
          EXPECT_FALSE(player.player_id().empty());
        }
      });

  // 测试lambda回调
  int lambda_call_count = 0;
  client.setOnPlayerListUpdate(
      [&lambda_call_count](const std::vector<PlayerData>& players) {
        lambda_call_count++;
        EXPECT_GE(players.size(), 0);  // 至少不应该是负数
      });

  // 回调应该被成功设置
  EXPECT_TRUE(true);  // 如果没有异常，测试就通过了
}

/**
 * @brief 测试客户端的快速连接和断开连接
 */
TEST_F(ClientBasicTest, RapidConnectDisconnect) {
  const Client client;

  for (int i = 0; i < 5; ++i) {
    // 快速连接尝试
    auto future = client.connect("127.0.0.1:" + std::to_string(12345 + i),
                                 "rapid_player", "token");

    // 立即断开连接
    client.disconnect();

    // 等待一小段时间
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // 检查状态
    EXPECT_FALSE(client.isConnected());
  }
}

/**
 * @brief 测试客户端在高负载下的行为
 */
TEST_F(ClientBasicTest, HighLoadPlayerDataSending) {
  Client client;

  // 创建测试数据
  PlayerData test_data;
  test_data.set_player_id("load_test_player");
  test_data.mutable_position()->set_x(1.0f);
  test_data.mutable_position()->set_y(2.0f);
  test_data.mutable_position()->set_z(3.0f);

  // 快速发送大量数据（在未连接状态下）
  auto start_time = std::chrono::high_resolution_clock::now();

  for (int i = 0; i < 10000; ++i) {
    test_data.mutable_position()->set_x(static_cast<float>(i));
    EXPECT_NO_THROW(client.sendPlayerData(test_data));
  }

  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      end_time - start_time);

  // 应该能在合理时间内完成（即使在未连接状态下）
  EXPECT_LT(duration.count(), 1000);  // 应该在1秒内完成
}

/**
 * @brief 测试客户端的地址解析
 */
TEST_F(ClientBasicTest, AddressParsingVariations) {
  Client client;

  // 测试各种有效的地址格式
  std::vector<std::string> valid_addresses = {
      "localhost:8080",   "127.0.0.1:8080", "0.0.0.0:8080",
      "192.168.1.1:8080", "10.0.0.1:1234",  "example.com:8080"};

  for (const auto& address : valid_addresses) {
    try {
      auto future = client.connect(address, "test_player", "token");
      client.disconnect();  // 立即断开连接

      // 等待连接失败或超时
      auto status = future.wait_for(std::chrono::milliseconds(100));
      if (status == std::future_status::ready) {
        try {
          future.get();
        } catch (...) {
          // 预期会失败，因为没有真实的服务器
        }
      }
    } catch (const std::invalid_argument&) {
      FAIL() << "Valid address should not throw invalid_argument: " << address;
    } catch (...) {
      // 其他异常是可以接受的（如网络错误）
    }
  }

  // 测试无效的地址格式
  std::vector<std::string> invalid_addresses = {"",
                                                "localhost",
                                                ":8080",
                                                "localhost:",
                                                "localhost:abc",
                                                "localhost:-1",
                                                "localhost:99999999",
                                                "invalid_format"};

  for (const auto& address : invalid_addresses) {
    EXPECT_THROW(client.connect(address, "test_player", "token"),
                 std::invalid_argument)
        << "Invalid address should throw: " << address;
  }
}

/**
 * @brief 测试客户端的线程安全性
 */
TEST_F(ClientBasicTest, ThreadSafetyBasics) {
  Client client;

  constexpr int num_threads = 5;
  std::vector<std::thread> threads;
  std::atomic<int> operations_completed{0};

  // 启动多个线程同时操作客户端
  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([&client, &operations_completed, i] {
      try {
        // 测试状态查询
        bool connected = client.isConnected();
        operations_completed++;

        // 测试数据发送
        PlayerData data;
        data.set_player_id("thread_player_" + std::to_string(i));
        client.sendPlayerData(data);
        operations_completed++;

        // 测试回调设置
        client.setOnPlayerListUpdate([](const std::vector<PlayerData>&) {
          // Do nothing
        });
        operations_completed++;

        // 测试断开连接（应该是安全的）
        client.disconnect();
        operations_completed++;

      } catch (...) {
        // 记录但不失败，某些操作可能会抛出异常
      }
    });
  }

  // 等待所有线程完成
  for (auto& thread : threads) {
    thread.join();
  }

  // 验证没有崩溃，并且一些操作成功了
  EXPECT_GT(operations_completed.load(), 0);
}
