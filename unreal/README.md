# PICO Radar Unreal Engine Plugin

实时、低延迟的多用户VR位置共享系统的Unreal Engine插件。

## 功能特性

- 🎯 **实时位置共享**: 在多个VR用户之间实时共享位置和旋转数据
- 🚀 **低延迟**: 专为VR环境优化的高性能网络通信
- 🎮 **蓝图友好**: 完整的蓝图接口，无需编程知识
- 🔧 **易于集成**: 简单的组件化设计，快速集成到现有项目
- 📡 **自动发现**: 支持用户自动连接和断开检测
- 🎨 **可视化支持**: 内置用户位置可视化功能

## 快速开始

### 1. 安装插件

插件已经集成到项目中，确保在项目设置中启用 `PICORadar` 插件。

### 2. 基础使用

#### 方法一：使用组件（推荐）

1. 在您的Actor中添加 `PICORadarComponent`
2. 在蓝图或C++中配置连接参数
3. 调用 `ConnectToServer` 开始位置共享

```cpp
// C++ 示例
UCLASS()
class MYGAME_API AMyVRPlayer : public APawn
{
    GENERATED_BODY()

public:
    AMyVRPlayer()
    {
        // 添加PICORadar组件
        RadarComponent = CreateDefaultSubobject<UPICORadarComponent>(TEXT("RadarComponent"));
    }

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    class UPICORadarComponent* RadarComponent;

    virtual void BeginPlay() override
    {
        Super::BeginPlay();
        
        // 连接到服务器
        if (RadarComponent)
        {
            RadarComponent->SetUserId(TEXT("Player1"));
            RadarComponent->ConnectToServer(TEXT("127.0.0.1"), 8080);
        }
    }
};
```

#### 方法二：使用示例Actor

1. 在关卡中放置 `PICORadarExampleActor`
2. 调用 `StartRadarSystem()` 开始系统
3. 观察其他用户的位置可视化

### 3. 蓝图使用

#### 连接到服务器
```
RadarComponent -> Connect To Server
- Server Address: "127.0.0.1"
- Port: 8080
```

#### 监听用户事件
```
RadarComponent -> On User Position Updated -> (处理位置更新)
RadarComponent -> On User Connected -> (处理用户连接)
RadarComponent -> On User Disconnected -> (处理用户断开)
```

## API 参考

### UPICORadarComponent

#### 主要函数

- `ConnectToServer(ServerAddress, Port)`: 连接到PICORadar服务器
- `DisconnectFromServer()`: 断开服务器连接
- `SendUserPosition(Position, Rotation)`: 发送用户位置数据
- `GetAllUserPositions()`: 获取所有用户位置
- `GetUserPosition(UserId)`: 获取特定用户位置
- `SetUserId(UserId)`: 设置当前用户ID

#### 主要事件

- `OnUserPositionUpdated`: 当接收到用户位置更新时触发
- `OnUserConnected`: 当新用户连接时触发
- `OnUserDisconnected`: 当用户断开连接时触发
- `OnConnectionStatusChanged`: 当连接状态改变时触发

#### 配置参数

- `UserId`: 当前用户的唯一标识符
- `UpdateFrequency`: 位置更新频率（Hz）
- `bAutoConnectOnBeginPlay`: 是否在开始游戏时自动连接
- `DefaultServerAddress`: 默认服务器地址
- `DefaultServerPort`: 默认服务器端口

### FUserPositionData 结构

```cpp
USTRUCT(BlueprintType)
struct FUserPositionData
{
    UPROPERTY(BlueprintReadWrite)
    FString UserId;          // 用户ID
    
    UPROPERTY(BlueprintReadWrite)
    FVector Position;        // 3D位置
    
    UPROPERTY(BlueprintReadWrite)
    FRotator Rotation;       // 旋转
    
    UPROPERTY(BlueprintReadWrite)
    float Timestamp;         // 时间戳
};
```

## 高级配置

### 性能优化

1. **更新频率调整**: 根据需要调整 `UpdateFrequency`
   - VR应用建议: 30-60 Hz
   - 移动应用建议: 15-30 Hz

2. **网络优化**: 考虑使用UDP而非TCP以减少延迟

### 自定义可视化

继承 `APICORadarExampleActor` 或创建自己的可视化逻辑：

```cpp
UFUNCTION()
void OnUserPositionUpdated(FUserPositionData UserData)
{
    // 自定义可视化逻辑
    if (UserData.UserId != MyUserId)
    {
        UpdateUserAvatar(UserData.UserId, UserData.Position, UserData.Rotation);
    }
}
```

## 服务器配置

确保PICORadar服务器正在运行并监听正确的端口。参考主项目文档了解服务器配置详情。

## 故障排除

### 常见问题

1. **无法连接到服务器**
   - 检查服务器是否运行
   - 验证IP地址和端口
   - 检查防火墙设置

2. **位置数据不更新**
   - 确保 `UpdateFrequency` 设置合理
   - 检查组件的 `Tick` 是否启用
   - 验证Actor位置是否在变化

3. **用户可视化不显示**
   - 检查事件绑定是否正确
   - 确保材质和网格资源可用
   - 验证Actor生成位置

### 调试技巧

1. 启用详细日志:
```cpp
UE_LOG(LogTemp, VeryVerbose, TEXT("Debug message"));
```

2. 使用屏幕调试消息:
```cpp
GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("Debug info"));
```

## 路线图

- [ ] WebSocket支持
- [ ] 实际网络协议集成
- [ ] 动画数据同步
- [ ] 语音聊天集成
- [ ] 房间管理系统
- [ ] 数据压缩优化

## 许可证

此插件遵循与主PICORadar项目相同的许可证。

## 贡献

欢迎提交Issues和Pull Requests到主项目仓库。
