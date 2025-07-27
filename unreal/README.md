# PICORadar Unreal Engine Plugin

## 概述

PICORadar UE插件是一个为Unreal Engine 5设计的高性能、企业级实时位置共享系统。该插件完全集成了PICORadar的C++客户端库，为VR/AR应用提供低延迟、高精度的多用户位置同步功能。

## 🏗️ 架构设计

### 核心组件

1. **FPICORadarModule** - 插件主模块
   - 负责插件生命周期管理
   - C++客户端库的初始化和清理
   - 全局配置和资源管理

2. **UPICORadarComponent** - 核心Actor组件
   - 实时位置数据同步
   - WebSocket连接管理
   - 事件驱动的状态更新
   - 完整的蓝图支持

3. **UPICORadarWorldSubsystem** - 世界子系统
   - 全局玩家状态管理
   - 自动服务器发现
   - 玩家可视化协调
   - 统计信息收集

4. **APICORadarPlayerVisualization** - 玩家可视化Actor
   - 3D空间中的玩家位置渲染
   - 平滑插值动画
   - 距离衰减和LOD优化
   - 可自定义的视觉表现

5. **FPICORadarTypes** - 类型定义
   - UE和Protobuf数据结构转换
   - 线程安全的数据类型
   - 蓝图友好的接口

### 设计原则

#### 1. 现代C++最佳实践
```cpp
// RAII资源管理
TUniquePtr<picoradar::client::Client> Client;

// 智能指针和自动内存管理
TSharedPtr<FPICORadarConnectionTask> ConnectionTask;

// 类型安全的枚举
enum class EPICORadarConnectionState : uint8

// const正确性
[[nodiscard]] auto IsConnectedToServer() const -> bool;
```

#### 2. UE5模块化架构
```cpp
// 模块接口实现
class PICORADAR_API FPICORadarModule : public IModuleInterface

// 组件系统集成
UCLASS(BlueprintType, Blueprintable, ClassGroup=(PICORadar), 
       meta=(BlueprintSpawnableComponent))

// 子系统模式
class PICORADAR_API UPICORadarWorldSubsystem : public UWorldSubsystem
```

#### 3. 线程安全设计
```cpp
// 异步任务处理
AsyncTask(ENamedThreads::GameThread, [this, UEPlayers]()
{
    HandlePlayerListUpdate(UEPlayers);
});

// 原子操作
std::atomic<bool> bIsComplete{false};

// 游戏线程同步
void HandlePlayerListUpdate(const TArray<FPICORadarPlayerData>& Players);
```

#### 4. 事件驱动架构
```cpp
// 多播委托
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerListUpdated, 
    const TArray<FPICORadarPlayerData>&, Players);

// 蓝图事件
UPROPERTY(BlueprintAssignable, Category = "PICO Radar|Events")
FOnPlayerListUpdated OnPlayerListUpdated;

// C++回调集成
Client->setOnPlayerListUpdate([this](const auto& players) {
    // 异步处理...
});
```

## 🚀 核心特性

### 1. 零配置集成
```cpp
// 自动初始化
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO Radar|Connection")
bool bAutoConnectOnBeginPlay = true;

// 智能服务器发现
UFUNCTION(BlueprintCallable, Category = "PICO Radar")
void StartServerDiscovery();
```

### 2. 高性能数据同步
```cpp
// 智能更新阈值
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO Radar|Sync", 
          meta = (ClampMin = "0.1", ClampMax = "10.0"))
float PositionThreshold = 1.0f;

// 自适应发送频率
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO Radar|Sync", 
          meta = (ClampMin = "1", ClampMax = "60"))
float SendFrequency = 20.0f;
```

### 3. 企业级错误处理
```cpp
// 强类型错误状态
enum class EPICORadarConnectionState : uint8

// 详细的错误信息
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPICORadarConnectionFailed, 
    const FString&, ErrorMessage);

// 自动重连机制
void HandleConnectionFailure(const FString& Error);
```

### 4. 可视化系统
```cpp
// LOD优化
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO Radar|LOD")
float MaxVisibilityDistance = 5000.0f;

// 平滑插值
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO Radar|Animation")
float PositionInterpSpeed = 10.0f;

// 材质动态控制
UMaterialInstanceDynamic* DynamicPlayerMaterial;
```

## 📋 使用指南

### 基础集成

1. **添加组件**
```cpp
// C++方式
UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PICO Radar")
class UPICORadarComponent* RadarComponent;

// 构造函数中初始化
RadarComponent = CreateDefaultSubobject<UPICORadarComponent>(TEXT("RadarComponent"));
```

2. **蓝图配置**
```cpp
// 在BeginPlay中连接
RadarComponent->ConnectToServer(TEXT("127.0.0.1:11451"), TEXT("Player1"), TEXT("token"));

// 绑定事件
RadarComponent->OnPlayerListUpdated.AddDynamic(this, &AMyActor::HandlePlayerUpdate);
```

### 高级功能

1. **自定义可视化**
```cpp
// 创建自定义可视化类
UCLASS(BlueprintType)
class AMyPlayerVisualization : public APICORadarPlayerVisualization
{
    // 自定义渲染逻辑
    virtual void UpdateVisualization(float DeltaTime) override;
};

// 设置自定义可视化
Subsystem->DefaultVisualizationClass = AMyPlayerVisualization::StaticClass();
```

2. **服务器发现**
```cpp
// 自动发现服务器
UPICORadarWorldSubsystem* Subsystem = GetWorld()->GetSubsystem<UPICORadarWorldSubsystem>();
Subsystem->StartServerDiscovery();

// 监听发现事件
Subsystem->OnServerDiscovered.AddDynamic(this, &AMyActor::OnServerFound);
```

3. **统计监控**
```cpp
// 获取系统统计
FPICORadarStats Stats = Subsystem->GetSystemStats();
UE_LOG(LogGame, Log, TEXT("Connected Players: %d, Average Latency: %.1fms"), 
       Stats.ConnectedPlayers, Stats.AverageLatency);
```

## 🔧 构建配置

### Build.cs最佳实践

```csharp
public class MyProject : ModuleRules
{
    public MyProject(ReadOnlyTargetRules Target) : base(Target)
    {
        // 添加PICORadar依赖
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "PICORadar"  // <- 添加插件依赖
        });
        
        // 启用C++20（匹配PICORadar要求）
        CppStandard = CppStandardVersion.Cpp20;
    }
}
```

### 环境变量配置

```bash
# 设置PICORadar项目根目录
export PICORADAR_ROOT="/path/to/PICORadar"

# 或在Windows中
set PICORADAR_ROOT=C:\path\to\PICORadar
```

## 🧪 测试与验证

### 单元测试示例
```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPICORadarComponentTest, 
    "PICORadar.Component.BasicConnection", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FPICORadarComponentTest::RunTest(const FString& Parameters)
{
    // 创建测试组件
    UPICORadarComponent* Component = NewObject<UPICORadarComponent>();
    
    // 测试连接
    Component->ConnectToServer(TEXT("127.0.0.1:11451"), TEXT("TestPlayer"), TEXT("test_token"));
    
    // 验证状态
    TestTrue("Component should be connecting", Component->IsConnectedToServer());
    
    return true;
}
```

### 性能基准测试
```cpp
// 延迟测试
FDateTime StartTime = FDateTime::Now();
Component->SendPlayerData();
// 测量回调接收时间...

// 内存使用测试
SIZE_T MemoryBefore = FPlatformMemory::GetStats().UsedPhysical;
// 执行大量操作...
SIZE_T MemoryAfter = FPlatformMemory::GetStats().UsedPhysical;
```

## 🔍 故障排除

### 常见问题

1. **编译错误**
```bash
# 检查环境变量
echo $PICORADAR_ROOT

# 重新生成项目文件
./GenerateProjectFiles.sh
```

2. **运行时错误**
```cpp
// 启用详细日志
Component->bEnableVerboseLogging = true;

// 检查模块状态
if (!FPICORadarModule::IsAvailable())
{
    UE_LOG(LogGame, Error, TEXT("PICORadar module not available"));
}
```

3. **连接问题**
```cpp
// 测试网络连通性
Component->OnConnectionFailed.AddDynamic(this, &AMyActor::HandleConnectionError);

void AMyActor::HandleConnectionError(const FString& Error)
{
    UE_LOG(LogGame, Warning, TEXT("Connection failed: %s"), *Error);
}
```

## 📈 性能优化

### 内存管理
```cpp
// 使用对象池
TArray<APICORadarPlayerVisualization*> VisualizationPool;

// 智能清理
UPROPERTY(EditAnywhere, Category = "PICO Radar|Performance")
float VisualizationCleanupDelay = 5.0f;
```

### 网络优化
```cpp
// 批量更新
TArray<FPICORadarPlayerData> PendingUpdates;

// 压缩传输
bool bEnableDataCompression = true;
```

### 渲染优化
```cpp
// LOD系统
float GetLODLevel(float Distance) const
{
    return FMath::Clamp(Distance / MaxVisibilityDistance, 0.0f, 1.0f);
}

// 视锥体剔除
bool IsInViewFrustum(const FVector& Position) const;
```

## 🏆 最佳实践总结

1. **始终使用智能指针进行内存管理**
2. **在游戏线程中处理UE对象操作**
3. **使用UE的委托系统而非原生C++回调**
4. **实现完整的错误处理和日志记录**
5. **提供丰富的蓝图接口**
6. **遵循UE的命名约定和代码风格**
7. **使用子系统模式管理全局状态**
8. **实现适当的LOD和性能优化**

这个插件展示了如何将高性能的C++库正确集成到UE5中，同时保持代码的可维护性、可扩展性和性能。
