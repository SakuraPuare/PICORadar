#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/Engine.h"
#include "Tickable.h"
#include "PICORadarTypes.h"
#include "PICORadarComponent.generated.h"

// 前向声明
namespace picoradar::client {
    class Client;
}

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPICORadarConnected);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPICORadarDisconnected, const FString&, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPICORadarConnectionFailed, const FString&, ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerListUpdated, const TArray<FPICORadarPlayerData>&, Players);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPlayerJoined, const FString&, PlayerId, const FPICORadarPlayerData&, PlayerData);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerLeft, const FString&, PlayerId);

/**
 * PICO Radar组件
 * 
 * 这是PICORadar系统的核心UE组件，提供完整的实时位置共享功能。
 * 
 * 主要功能：
 * - 自动连接到PICORadar服务器
 * - 实时发送本地玩家位置数据
 * - 接收其他玩家的位置更新
 * - 完整的事件系统和蓝图支持
 * - 自动重连和错误处理
 * 
 * 使用方法：
 * 1. 将此组件添加到需要位置同步的Actor上
 * 2. 设置服务器地址、玩家ID和令牌
 * 3. 调用ConnectToServer()进行连接
 * 4. 绑定事件处理器处理玩家更新
 */
UCLASS(BlueprintType, Blueprintable, ClassGroup=(PICORadar), meta=(BlueprintSpawnableComponent))
class PICORADAR_API UPICORadarComponent : public UActorComponent, public FTickableGameObject
{
    GENERATED_BODY()

public:
    UPICORadarComponent();
    virtual ~UPICORadarComponent();

    // UActorComponent接口
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    
    // FTickableGameObject接口
    virtual void Tick(float DeltaTime) override;
    virtual bool IsTickable() const override;
    virtual TStatId GetStatId() const override;

    /**
     * 连接到PICORadar服务器
     * 
     * @param ServerAddress 服务器地址，格式为"ip:port"，例如"127.0.0.1:11451"
     * @param PlayerId 玩家唯一ID
     * @param AuthToken 认证令牌
     */
    UFUNCTION(BlueprintCallable, Category = "PICO Radar", CallInEditor = true)
    void ConnectToServer(const FString& ServerAddress, const FString& PlayerId, const FString& AuthToken);

    /**
     * 断开与服务器的连接
     */
    UFUNCTION(BlueprintCallable, Category = "PICO Radar")
    void DisconnectFromServer();

    /**
     * 检查是否已连接到服务器
     */
    UFUNCTION(BlueprintPure, Category = "PICO Radar")
    bool IsConnectedToServer() const;

    /**
     * 手动发送玩家数据
     * 通常不需要手动调用，组件会自动发送
     */
    UFUNCTION(BlueprintCallable, Category = "PICO Radar")
    void SendPlayerData();

    /**
     * 获取当前所有在线玩家列表
     */
    UFUNCTION(BlueprintPure, Category = "PICO Radar")
    TArray<FPICORadarPlayerData> GetCurrentPlayers() const;

    /**
     * 根据玩家ID获取特定玩家数据
     */
    UFUNCTION(BlueprintPure, Category = "PICO Radar")
    bool GetPlayerData(const FString& PlayerId, FPICORadarPlayerData& OutPlayerData) const;

    // 配置属性
public:
    /** 服务器地址 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO Radar|Connection")
    FString ServerAddress = TEXT("127.0.0.1:11451");

    /** 玩家ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO Radar|Connection")
    FString PlayerId = TEXT("Player1");

    /** 认证令牌 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO Radar|Connection")
    FString AuthToken = TEXT("default_token");

    /** 是否在BeginPlay时自动连接 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO Radar|Connection")
    bool bAutoConnectOnBeginPlay = true;

    /** 数据发送频率（Hz） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO Radar|Sync", meta = (ClampMin = "1", ClampMax = "60"))
    float SendFrequency = 20.0f;

    /** 位置变化阈值（厘米），低于此值不发送更新 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO Radar|Sync", meta = (ClampMin = "0.1", ClampMax = "10.0"))
    float PositionThreshold = 1.0f;

    /** 旋转变化阈值（度），低于此值不发送更新 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO Radar|Sync", meta = (ClampMin = "0.1", ClampMax = "10.0"))
    float RotationThreshold = 1.0f;

    /** 场景ID，用于区分不同的游戏场景 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO Radar|Sync")
    FString SceneId = TEXT("DefaultScene");

    /** 是否启用详细日志 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO Radar|Debug")
    bool bEnableVerboseLogging = false;

    // 事件
public:
    /** 成功连接到服务器时触发 */
    UPROPERTY(BlueprintAssignable, Category = "PICO Radar|Events")
    FOnPICORadarConnected OnConnected;

    /** 与服务器断开连接时触发 */
    UPROPERTY(BlueprintAssignable, Category = "PICO Radar|Events")
    FOnPICORadarDisconnected OnDisconnected;

    /** 连接失败时触发 */
    UPROPERTY(BlueprintAssignable, Category = "PICO Radar|Events")
    FOnPICORadarConnectionFailed OnConnectionFailed;

    /** 玩家列表更新时触发 */
    UPROPERTY(BlueprintAssignable, Category = "PICO Radar|Events")
    FOnPlayerListUpdated OnPlayerListUpdated;

    /** 新玩家加入时触发 */
    UPROPERTY(BlueprintAssignable, Category = "PICO Radar|Events")
    FOnPlayerJoined OnPlayerJoined;

    /** 玩家离开时触发 */
    UPROPERTY(BlueprintAssignable, Category = "PICO Radar|Events")
    FOnPlayerLeft OnPlayerLeft;

private:
    /** 内部C++客户端实例 */
    TUniquePtr<picoradar::client::Client> Client;

    /** 当前玩家数据缓存 */
    TMap<FString, FPICORadarPlayerData> CurrentPlayers;

    /** 上次发送的位置数据 */
    FVector LastSentPosition = FVector::ZeroVector;
    FRotator LastSentRotation = FRotator::ZeroRotator;

    /** 上次发送数据的时间 */
    float LastSendTime = 0.0f;

    /** 连接状态 */
    bool bIsConnected = false;
    bool bIsConnecting = false;

    /** 连接Future */
    TSharedPtr<class FPICORadarConnectionTask> ConnectionTask;

    // 内部方法
private:
    /** 处理玩家列表更新 */
    void HandlePlayerListUpdate(const TArray<FPICORadarPlayerData>& Players);

    /** 检查是否需要发送数据更新 */
    bool ShouldSendUpdate() const;

    /** 获取当前Actor的位置和旋转 */
    void GetCurrentTransform(FVector& OutPosition, FRotator& OutRotation) const;

    /** 转换UE坐标到PICORadar坐标系 */
    FPICORadarPlayerData CreatePlayerDataFromTransform(const FVector& Position, const FRotator& Rotation) const;

    /** 日志输出 */
    void LogMessage(const FString& Message, bool bIsError = false) const;
};

/**
 * 异步连接任务，用于处理非阻塞的服务器连接
 */
class PICORADAR_API FPICORadarConnectionTask
{
public:
    FPICORadarConnectionTask(UPICORadarComponent* InOwner);
    ~FPICORadarConnectionTask();

    /** 开始连接 */
    void StartConnection(const FString& ServerAddress, const FString& PlayerId, const FString& AuthToken);

    /** 检查连接是否完成 */
    bool IsComplete() const;

    /** 检查连接是否成功 */
    bool IsSuccessful() const;

    /** 获取错误信息 */
    FString GetErrorMessage() const;

private:
    TWeakObjectPtr<UPICORadarComponent> Owner;
    std::future<void> ConnectionFuture;
    std::atomic<bool> bIsComplete{false};
    std::atomic<bool> bIsSuccessful{false};
    FString ErrorMessage;
};
