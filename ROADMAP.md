# PICO Radar: 开发路线图

本文档旨在通过分解项目为不同的阶段和关键里程碑，来跟踪PICO Radar项目的开发进度。

-   [x] 已完成
-   [ ] 待办
-   **>** 进行中

---

### 阶段 0：规划与奠基 (已完成)

*   [x] 敲定项目需求与范围。
*   [x] 定义系统架构与技术选型。
*   [x] 设计通讯协议与数据结构。
*   [x] 建立开发准则与工程标准。
*   [x] 创建作为项目章程的 `README.md`。
*   [x] 创建此 `ROADMAP.md` 文件。

---

### 阶段 1：项目设置与核心协议 (已完成)

*   [x] 根据 `README.md` 的定义创建项目目录结构。
*   [x] 初始化项目的 `CMakeLists.txt` 文件。
*   [x] **技术栈变更**: 依赖库从 `websocketpp` (FetchContent) 迁移至 `Boost.Beast` (vcpkg)。
*   [x] 设置依赖管理器 (vcpkg) 并获取 `protobuf`, `gtest`, `boost-beast`, `glog` 库。
*   [x] 实现 `proto/player_data.proto` 文件。
*   [x] 编写脚本将 `.proto` 文件编译为C++代码，并集成到构建流程中。
*   [x] 创建 `server_app` 可执行程序的初始框架。

---

### 阶段 2：服务端实现 (已完成)

*   [x] 实现用于管理玩家状态列表（增、删、改）的 `core` 核心逻辑。
*   [x] 配置并集成 GoogleTest 测试框架，为 `core` 模块编写单元测试。
*   [x] 使用 `Boost.Beast` 实现核心的WebSocket服务器逻辑（监听、接受连接）。
*   [x] 集成 `glog` 日志库，替换所有原生 `cout` 输出。
*   [x] 实现基于预共享令牌的鉴权机制。
*   [x] 实现完整的数据处理与广播循环（接收 -> 解析 -> 更新 -> 广播）。
*   [x] 实现用于处理断连的超时/心跳机制（通过WebSocket的内置功能）。
*   [x] 实现UDP服务发现协议的服务端逻辑。
*   [x] 开发一个基础的命令行界面（CLI）来监控服务器状态（如在线玩家数）。

---

### 阶段 3：客户端库与测试重构 (已完成)

*   [x] ~~开发 `mock_client` 测试程序，并用其驱动服务端功能的完善。~~ (已废弃)
*   [x] 创建 `client_lib` 静态库的初始框架。
*   [x] 在 `client_lib` 中实现WebSocket连接、鉴权、UDP发现等核心功能。
*   [x] **客户端库重构**: 将客户端库重构为完全异步的模式，以解决阻塞和竞态条件问题。
*   [x] **测试与构建系统重构**:
    *   [x] 将所有基于Shell脚本的测试 (`.sh`) 迁移到由GoogleTest驱动的C++集成测试。
    *   [x] 实现了一个用于管理子进程的`Process`辅助类。
    *   [x] 将服务端和模拟客户端重构为生命周期可控的、可被测试直接调用的库。
    *   [x] **移除 `mock_client`**: 所有集成测试 (`test_broadcast`, `test_discovery`) 直接依赖 `client_lib`，统一了测试客户端，消除了代码冗余。
    *   [x] **CMake 重构**: 统一了所有测试目标的GTest依赖和链接配置，创建了共享的`gtest_main_obj`对象库，简化了测试的创建和维护。
    *   [x] 实现服务端与客户端日志前缀区分，提升测试可观测性。
*   [x] 在 `client_lib` 中实现视觉平滑算法（插值）。

---

### **阶段 4: 健壮性与性能 (进行中)**

*   **>** [ ] 修复并增强压力测试 (`test_stress`)，确保其能稳定地在重构后的架构上运行。
*   [ ] `Client` 类中增加断线重连机制。
*   [ ] `Client` 类中增加对服务器主动断开的优雅处理。
*   [ ] 执行并撰写正式的性能基准测试，创建 `PERFORMANCE.md`。

---

### 阶段 5：Unreal Engine 集成

*   [ ] 基于C++客户端库，开发Unreal Engine插件。
*   [ ] 插件需要提供蓝图接口，方便在游戏引擎中调用。
*   [ ] 实现玩家虚拟形象的显示和更新。

---

### 阶段 6：文档与发布

*   [ ] 完善所有公开API的文档。
*   [ ] 撰写详细的 `DESIGN.md` 和 `USER_GUIDE.md`。
*   [ ] 使用 Doxygen 生成 `API_REFERENCE.md`。
*   [ ] 将服务端程序打包为Windows和Linux应用。
*   [ ] 提供一个集成了 `client_lib` 的Unreal Engine示例项目。
*   [ ] 对所有文档进行最终审校。
*   [ ] **项目完成。**

---

## 开发日志 (DevLog)

*   ...
*   [x] DevLog-17: The-Pressure-Test.md
*   [ ] **DevLog-18: 大重构：告别模拟客户端 (The Great Refactoring: Farewell, Mock Client)**
