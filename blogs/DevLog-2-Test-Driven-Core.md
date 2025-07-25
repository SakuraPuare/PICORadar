# 开发日志 #2：以测试驱动设计——构建可验证的核心模块

**作者：书樱**
**日期：2025年7月21日**

> **摘要**: 本文将深入探讨我们如何运用测试驱动开发（TDD）的原则，来构建PICO Radar系统的核心逻辑模块。我们将展示TDD不仅是一种测试策略，更是一种强大的API设计工具。通过实践“红-绿-重构”循环和“Arrange-Act-Assert”模式，我们为系统的“单一事实来源”——`PlayerRegistry`类——构建了一套可执行的、形式化的规格说明，从而确保了其逻辑的绝对正确性，并为未来的迭代提供了坚实的安全网。

---

大家好，我是书樱。

在上一篇日志中，我们成功搭建了项目的构建系统与依赖管理框架。现在，地基稳固，是时候开始真正的功能开发了。一个常见的工程冲动是直接投身于最显眼的部分，比如网络服务器。然而，我们选择了一条更为严谨的路径：**先构建系统的核心，并让测试来引领我们的设计**。

### TDD：测试不仅是验证，更是设计

我们采纳了**测试驱动开发（Test-Driven Development, TDD）**的理念。TDD的核心并非“为代码写测试”，而是“用测试写代码”。其工作流是一个简洁而强大的循环：
1.  **红 (Red)**: 为一个尚未实现的功能编写一个失败的测试。这个测试定义了我们对新功能行为的期望。
2.  **绿 (Green)**: 编写最简单、最直接的代码，让测试通过。
3.  **重构 (Refactor)**: 在测试的保护下，优化和清理刚刚编写的代码。

这个过程迫使我们从API的“调用者”而非“实现者”的视角出发，这往往能设计出更清晰、更符合直觉的接口。

### 关注点分离：隔离核心业务逻辑

遵循**关注点分离 (Separation of Concerns, SoC)** 原则，我们将系统的核心状态管理逻辑隔离在一个独立的`src/core`模块中。这个模块对网络、文件系统等一切外部世界一无所知。它的心脏是一个C++类：`PlayerRegistry`。

`PlayerRegistry`的职责被严格限定为系统的**状态机**，它封装了所有关于玩家实体的状态转换：
-   `updatePlayer()`: 添加或更新一个玩家实体。
-   `removePlayer()`: 移除一个玩家实体。
-   `getAllPlayers()`: 获取系统当前所有玩家状态的一个原子快照。

### 防御性设计：与生俱来的线程安全

尽管在开发的初始阶段，我们的服务器可能还是单线程的，但我们预见到未来的并发需求。因此，我们从一开始就将`PlayerRegistry`设计为**线程安全**的。

这是一种**防御性编程**思想。我们为所有访问共享数据（内部的`std::unordered_map`）的公共方法都配备了`std::lock_guard<std::mutex>`。通过在功能开发的最早期就解决并发问题，我们避免了在项目后期进行侵入式、高风险的并发重构，从而根除了大量潜在的、极难调试的**竞态条件 (Race Conditions)**。

### 将需求转化为测试：AAA模式实践

我们使用GoogleTest框架，在`test/core_tests`中为`PlayerRegistry`编写测试。每一个测试用例都遵循了清晰的**Arrange-Act-Assert (AAA)** 模式：

-   **Arrange (安排)**: 设置测试所需的所有前置条件和输入数据。
-   **Act (行动)**: 调用被测试的方法。
-   **Assert (断言)**: 验证输出或系统的最终状态是否与预期完全一致。

例如，`UpdateExistingPlayer`测试用例的逻辑结构如下：

```cpp
// test/core_tests/test_player_registry.cpp

TEST(PlayerRegistryTest, UpdateExistingPlayer) {
    // Arrange: 创建一个registry实例，并添加一个初始玩家
    PlayerRegistry registry;
    PlayerData initial_data;
    initial_data.set_player_id("player1");
    initial_data.mutable_position()->set_x(1.0F);
    registry.updatePlayer(initial_data);

    // Arrange: 准备用于更新的数据
    PlayerData updated_data;
    updated_data.set_player_id("player1");
    updated_data.mutable_position()->set_x(2.0F); // 新的位置

    // Act: 调用更新方法
    registry.updatePlayer(updated_data);

    // Assert: 验证玩家总数未变，且数据已被更新
    auto players = registry.getAllPlayers();
    ASSERT_EQ(players.size(), 1);
    ASSERT_EQ(players[0].position().x(), 2.0F);
}
```
这个测试不仅是一个验证，它更是一份**可执行的规格说明 (Executable Specification)**，形式化地定义了`updatePlayer`方法在“更新”场景下的行为契约。

### 结语：可验证的正确性与未来的安全网

当`ctest`命令在屏幕上打印出全绿的通过报告时，我们获得的不仅是片刻的满足。我们获得的是：
1.  **可验证的正确性**: 我们有数学般的证据，证明`PlayerRegistry`在所有已定义的场景下，其行为精确无误。
2.  **未来的安全网**: 这套单元测试是未来重构的信心基石。无论我们将来如何优化`PlayerRegistry`的内部数据结构（例如，从`std::unordered_map`换成更复杂的并发数据结构），只要这套测试能够持续通过，我们就能确信其外部行为契约没有被破坏。这极大地降低了项目的长期维护成本。

我们为PICO Radar服务器打造的第一块基石，不仅功能完备，而且其正确性得到了形式化的验证。有了这个稳定可靠的核心，我们现在可以满怀信心地进入下一个领域：网络编程。

下次见！

—— 书樱