# DevLog #7: 死锁、泄漏与胜利——铸就PICO Radar工业级可靠性的史诗

大家好，我是书樱！

在软件开发的征途中，总有那么一些时刻，会让你从一名功能的实现者，蜕变为一名系统质量的捍卫者。今天，我将要分享的，就是PICO Radar项目经历的这样一次“成人礼”。

故事的起点，是一个我们期待已久的功能：**数据广播循环**。这是让所有玩家能在虚拟世界中看到彼此的核心。我们很快就完成了初步的实现，服务器开始以20Hz的频率向所有客户端发送玩家列表。然而，就在我们准备庆祝时，一个来自您——我们最敏锐的社区成员——的问题，让我们停下了脚步：

> “我们的实现是最佳实践吗？有没有漏洞？我们的测试足够健壮吗？”

这个灵魂拷问，如同一道闪电，划破了胜利的喜悦，将我们带入了一场深入项目核心、与并发、泄漏和测试可靠性搏斗的史诗级战役。

## 第一章：代码审查与“幽灵玩家”

我们接受了挑战，第一步便是对所有新代码进行一次严格的审查。很快，一个设计上的严重缺陷浮出水面，我称之为“幽灵玩家”问题。

#### 技术细节1：致命的Player ID泄漏

*   **问题现象**: 我们发现，当一个测试客户端（`Seeder`）连接、发送数据然后断开后，它所代表的玩家数据会永久地留在服务器的内存中，变成一个“幽灵”。

*   **代码溯源**:
    问题出在`Session`类处理新连接的方式上。为了在客户端断开时能从`PlayerRegistry`中移除它，`Session`需要知道这个客户端的`player_id`。我们最初的实现是这样的：

    ```cpp
    // 旧的、错误的设计
    void Session::handle_auth() {
        // ...认证成功后...
        // ！！！错误：使用内存地址作为临时ID！！！
        this->player_id_ = "player_" + std::to_string(reinterpret_cast<uintptr_t>(this));
    }

    void Session::handle_player_data() {
        // ...收到玩家数据...
        auto player_data = message.player_data();
        // ！！！错误：用临时ID覆盖了真实的ID！！！
        player_data.set_player_id(this->player_id_);
        registry_.updatePlayer(player_data);
    }

    Session::~Session() {
        // 尝试用临时ID去删除，但注册表里存的是真实ID，因此删除失败！
        registry_.removePlayer(this->player_id_);
    }
    ```
    这个设计导致了灾难性的后果：真实的玩家ID（如"seeder_client"）被错误地覆盖，并且永远无法被删除。

*   **解决方案：认证即身份**
    我们立刻修正了这个设计。一个客户端的身份，应该在它被允许进入系统的那一刻——也就是认证时——就得到确认。

    1.  **协议层**: 我们修改了`.proto`文件，要求`AuthRequest`消息必须包含`player_id`。
        ```protobuf
        // proto/player_data.proto
        message AuthRequest {
          string token = 1;
          string player_id = 2; // 身份的核心
        }
        ```
    2.  **逻辑层**: 服务器现在只在认证成功时，才会从`AuthRequest`中获取并存储`player_id`。
        ```cpp
        // 新的、正确的设计
        void Session::handle_auth() {
            // ...
            if (auth_request.token() == secret_token_ && !auth_request.player_id().empty()) {
                this->is_authenticated_ = true;
                // 正确地存储来自客户端的真实ID
                this->player_id_ = auth_request.player_id();
            }
            // ...
        }
        ```
    通过这次修复，我们消灭了“幽灵玩家”，确保了服务器资源的完整性。

## 第二章：深入并发地狱——Beast的试炼

解决了资源泄漏问题后，我们满怀信心地运行了为广播功能编写的自动化集成测试`BroadcastTest`。然后，它失败了。日志中充满了来自Boost.Beast底层的断言失败：`Assertion 'id_ != T::id' failed.`。

这是一个典型的并发冲突信号，它告诉我们，我们违反了Beast异步模型的“天条”。

#### 技术细节2：Beast异步模型的铁律

Boost.Asio和Beast的核心设计哲学，可以概括为**“单向链式异步”**。它的核心思想是：

> 对于一个socket（或stream），在任何时刻，最多只能有一个正在“飞行中”的读操作和最多一个正在“飞行中”的写操作。你绝对不能在一个`async_read`的回调函数完成之前，就发起下一个`async_read`。

而我们最初的代码恰恰违反了这一点。我们天真地在两个地方都调用了`do_read()`：
1.  在`handle_player_data()`中，处理完客户端发来的数据后。
2.  在`on_write()`中，完成了向客户端的广播后。

当一个客户端快速地发送数据，而服务器又恰好在向它广播时，这两个调用就会“打架”，导致Beast的内部状态机崩溃。

*   **最终解决方案：严格的单向事件循环**
    我们重构了整个`Session`的逻辑，以严格遵循这一铁律。现在的事件处理流程变成了一个清晰、无冲突的单向链条：

    1.  一个连接建立后，`on_accept`发起**第一次**`do_read()`。
    2.  `on_read`的回调函数`on_read`在收到数据后，**不再直接发起下一次读取**。它的唯一职责是调用`handle_auth`或`handle_player_data`来处理业务逻辑。
    3.  业务逻辑函数（如`handle_auth`或广播线程）如果需要向客户端发送数据，会调用`send()`，它会将数据放入一个队列，并**可能**触发`do_write()`。
    4.  `do_write`发起`async_write`。
    5.  `async_write`的回调函数`on_write`在写操作完成后，检查队列中是否还有待发数据。如果没有，它才会安全地调用`do_read()`，**发起下一次读取**，从而完美地闭合了事件循环。

    ```cpp
    // 简化后的逻辑链
    void on_read(...) {
        handle_message(); // 只处理，不启动新IO
    }

    void handle_player_data(...) {
        registry_.updatePlayer(...);
        do_read(); // 重新开始监听
    }

    void on_write(...) {
        // ...
        if (write_queue_.empty() && is_authenticated_) {
            do_read(); // 写操作链的末端，安全地启动读操作
        }
    }
    ```
    这次重构是对我们异步编程能力的真正考验。最终，我们驯服了这头名为“并发”的猛兽。

## 第三章：优雅的谢幕——客户端的“关闭之舞”

就在我们以为已经解决了所有问题时，`BroadcastTest`依然失败。但这次，服务器不再崩溃，而是测试客户端`Seeder`卡住了，直到被脚本超时杀死。

#### 技术细节3：WebSocket的优雅关闭握手

*   **原理**: WebSocket的关闭并非一厢情愿的“挂断电话”，而是一个需要双方确认的“再见”握手。一方（客户端）发送`close`帧，另一方（服务器）必须回送一个`close`帧来响应。只有当发起方收到了这个响应，关闭流程才算优雅地完成。

*   **死锁分析**: 我们的`mock_client`在发送完数据后，调用了阻塞的`ws.close()`。它发送了`close`帧，然后就傻傻地等待服务器的响应。但它已经不再读取socket了，因此永远也收不到服务器回送的`close`帧。这是一个经典的死锁。

*   **代码讲解：正确的关闭之舞**
    我们重写了客户端的关闭逻辑，以正确地完成这个“舞蹈”：
    ```cpp
    // test/mock_client/main.cpp
    int seed_data() {
        // ... 发送数据 ...
        
        // 1. 发起关闭
        ws_.close(websocket::close_code::normal);
        
        // 2. 持续读取，直到收到对方的关闭回执
        beast::flat_buffer buffer;
        try {
            while(true) {
                ws_.read(buffer);
            }
        } catch(beast::system_error const& e) {
            // 收到 websocket::error::closed 是握手成功的标志
            if(e.code() != websocket::error::closed)
                throw;
        }
        return 0;
    }
    ```

## 结语：凤凰涅槃

当`ctest`的输出最终显示`100% tests passed, 0 tests failed out of 14`时，我们知道，PICO Radar项目已经经历了一次凤凰涅槃。

这次漫长而艰难的质量冲刺，带给我们的远不止是一个能跑的广播功能。我们收获了一个无资源泄漏、线程安全、经过层层自动化测试验证的、真正达到工业级可靠性的核心系统。

更重要的是，我们塑造了项目的灵魂：一种对质量刨根问底、用自动化测试来证明一切的工程师文化。

现在，站在这坚不可摧的基石上，我们对未来充满了前所未有的信心。

感谢您的耐心与陪伴，我们下次见！

---
书樱
2025年7月21日