# PICORadar Unreal Engine Integration

这个目录包含了PICORadar系统的Unreal Engine 5插件实现。

## 目录结构

```
unreal-plugin/           # 完整的插件（包含构建产物）
├── Binaries/           # 编译后的二进制文件
├── Intermediate/       # 构建中间文件  
├── Source/            # C++ 源代码
├── PICORadar.uplugin  # 插件配置文件
└── README.md          # 插件使用文档

unreal-plugin-source/   # 纯源码版本（适合分发）
├── Source/            # C++ 源代码
├── PICORadar.uplugin  # 插件配置文件
├── README.md          # 插件使用文档
└── .gitignore         # Git忽略文件
```

## 功能特性

- 🎯 **实时位置共享**: 多用户VR位置同步
- 🚀 **低延迟**: 针对VR优化的网络通信
- 🎮 **蓝图友好**: 完整的可视化编程接口
- 🔧 **组件化**: 易于集成的Actor组件
- 📡 **事件驱动**: 用户连接/断开/位置更新事件
- 🎨 **可视化**: 内置的用户位置可视化

## 快速集成

### 安装插件
1. 将 `unreal` 内容复制到您的UE项目的 `Plugins/PICORadar/` 目录
2. 重新生成项目文件
3. 编译项目
4. 在项目设置中启用PICORadar插件

### 基础使用
```cpp
// 添加到您的Actor
UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
class UPICORadarComponent* RadarComponent;

// 在BeginPlay中初始化
RadarComponent = CreateDefaultSubobject<UPICORadarComponent>(TEXT("RadarComponent"));
RadarComponent->SetUserId(TEXT("Player1"));
RadarComponent->ConnectToServer(TEXT("127.0.0.1"), 8080);
```

## 网络集成计划

目前插件使用模拟数据进行演示。下一步将集成实际的PICORadar C++客户端库：

1. **网络层**: 集成 `src/client/` 中的C++客户端
2. **协议**: 使用gRPC与PICORadar服务器通信
3. **序列化**: protobuf消息序列化
4. **多平台**: 支持Windows、Linux、Mac

## 开发状态

- ✅ 基础插件架构
- ✅ UE组件系统集成
- ✅ 蓝图接口
- ✅ 事件系统
- ✅ 可视化演示
- ⏳ 网络协议集成
- ⏳ VR头显支持
- ⏳ 多平台构建

## 贡献

此插件是PICORadar项目的一部分。欢迎提交Issues和Pull Requests。

## 许可证

遵循主项目相同的许可证条款。
