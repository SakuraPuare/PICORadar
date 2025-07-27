#pragma once

#include "CoreMinimal.h"
#include "PICORadarTypes.h"
#include "PICORadarWorldSubsystem.generated.h"
#include "Subsystems/WorldSubsystem.h"

class UPICORadarComponent;
class APICORadarPlayerVisualization;

/**
 * PICORadar世界子系统
 *
 * 这个子系统管理整个世界中的PICORadar功能，包括：
 * - 全局玩家状态管理
 * - 自动玩家可视化
 * - 服务器发现
 * - 统计信息收集
 *
 * 作为单例存在于每个World中，为所有PICORadar组件提供协调服务。
 */
UCLASS(BlueprintType)
class PICORADAR_API UPICORadarWorldSubsystem : public UWorldSubsystem {
  GENERATED_BODY()

 public:
  // USubsystem接口
  virtual void Initialize(FSubsystemCollectionBase& Collection) override;
  virtual void Deinitialize() override;

  /** 获取世界子系统实例 */
  UFUNCTION(BlueprintPure, Category = "PICO Radar", CallInEditor = true,
            meta = (WorldContext = "WorldContext"))
  static UPICORadarWorldSubsystem* Get(const UObject* WorldContext);

  /**
   * 注册PICORadar组件
   */
  void RegisterComponent(UPICORadarComponent* Component);

  /**
   * 注销PICORadar组件
   */
  void UnregisterComponent(UPICORadarComponent* Component);

  /**
   * 获取所有注册的组件
   */
  UFUNCTION(BlueprintPure, Category = "PICO Radar")
  const TArray<UPICORadarComponent*>& GetRegisteredComponents() const {
    return RegisteredComponents;
  }

  /**
   * 获取全局玩家列表（来自所有组件的聚合）
   */
  UFUNCTION(BlueprintPure, Category = "PICO Radar")
  TArray<FPICORadarPlayerData> GetAllPlayers() const;

  /**
   * 根据玩家ID查找玩家数据
   */
  UFUNCTION(BlueprintPure, Category = "PICO Radar")
  bool FindPlayerById(const FString& PlayerId,
                      FPICORadarPlayerData& OutPlayerData) const;

  /**
   * 获取特定场景中的玩家列表
   */
  UFUNCTION(BlueprintPure, Category = "PICO Radar")
  TArray<FPICORadarPlayerData> GetPlayersInScene(const FString& SceneId) const;

  /**
   * 启用/禁用自动玩家可视化
   */
  UFUNCTION(BlueprintCallable, Category = "PICO Radar")
  void SetAutoVisualizationEnabled(bool bEnabled);

  /**
   * 检查自动可视化是否启用
   */
  UFUNCTION(BlueprintPure, Category = "PICO Radar")
  bool IsAutoVisualizationEnabled() const { return bAutoVisualizationEnabled; }

  /**
   * 为玩家创建可视化Actor
   */
  UFUNCTION(BlueprintCallable, Category = "PICO Radar")
  APICORadarPlayerVisualization* CreatePlayerVisualization(
      const FPICORadarPlayerData& PlayerData);

  /**
   * 移除玩家的可视化Actor
   */
  UFUNCTION(BlueprintCallable, Category = "PICO Radar")
  void RemovePlayerVisualization(const FString& PlayerId);

  /**
   * 获取玩家的可视化Actor
   */
  UFUNCTION(BlueprintPure, Category = "PICO Radar")
  APICORadarPlayerVisualization* GetPlayerVisualization(
      const FString& PlayerId) const;

  /**
   * 获取所有可视化Actor
   */
  UFUNCTION(BlueprintPure, Category = "PICO Radar")
  TArray<APICORadarPlayerVisualization*> GetAllPlayerVisualizations() const;

  /**
   * 清除所有可视化Actor
   */
  UFUNCTION(BlueprintCallable, Category = "PICO Radar")
  void ClearAllVisualizations();

  /**
   * 获取系统统计信息
   */
  UFUNCTION(BlueprintPure, Category = "PICO Radar")
  FPICORadarStats GetSystemStats() const;

  /**
   * 自动发现局域网中的PICORadar服务器
   */
  UFUNCTION(BlueprintCallable, Category = "PICO Radar")
  void StartServerDiscovery();

  /**
   * 停止服务器发现
   */
  UFUNCTION(BlueprintCallable, Category = "PICO Radar")
  void StopServerDiscovery();

  /**
   * 获取发现的服务器列表
   */
  UFUNCTION(BlueprintPure, Category = "PICO Radar")
  TArray<FString> GetDiscoveredServers() const { return DiscoveredServers; }

 public:
  // 配置属性

  /** 默认的玩家可视化类 */
  UPROPERTY(EditAnywhere, BlueprintReadWrite,
            Category = "PICO Radar|Visualization")
  TSubclassOf<APICORadarPlayerVisualization> DefaultVisualizationClass;

  /** 是否启用自动玩家可视化 */
  UPROPERTY(EditAnywhere, BlueprintReadWrite,
            Category = "PICO Radar|Visualization")
  bool bAutoVisualizationEnabled = true;

  /** 玩家可视化的默认生成位置偏移 */
  UPROPERTY(EditAnywhere, BlueprintReadWrite,
            Category = "PICO Radar|Visualization")
  FVector VisualizationSpawnOffset = FVector::ZeroVector;

  /** 是否自动清理离线玩家的可视化 */
  UPROPERTY(EditAnywhere, BlueprintReadWrite,
            Category = "PICO Radar|Visualization")
  bool bAutoCleanupVisualizations = true;

  /** 玩家离线后多久清理其可视化（秒） */
  UPROPERTY(EditAnywhere, BlueprintReadWrite,
            Category = "PICO Radar|Visualization", meta = (ClampMin = "1.0"))
  float VisualizationCleanupDelay = 5.0f;

  /** 服务器发现的UDP端口 */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO Radar|Discovery")
  int32 DiscoveryPort = 11450;

  /** 服务器发现超时时间（秒） */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO Radar|Discovery",
            meta = (ClampMin = "1.0"))
  float DiscoveryTimeout = 10.0f;

  /** 是否启用调试日志 */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO Radar|Debug")
  bool bEnableDebugLogging = false;

  // 委托事件
 public:
  DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGlobalPlayerJoined,
                                              const FPICORadarPlayerData&,
                                              PlayerData);
  DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGlobalPlayerLeft,
                                              const FString&, PlayerId);
  DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGlobalPlayerUpdated,
                                              const FPICORadarPlayerData&,
                                              PlayerData);
  DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnServerDiscovered,
                                              const FString&, ServerAddress);

  /** 全局玩家加入事件 */
  UPROPERTY(BlueprintAssignable, Category = "PICO Radar|Events")
  FOnGlobalPlayerJoined OnGlobalPlayerJoined;

  /** 全局玩家离开事件 */
  UPROPERTY(BlueprintAssignable, Category = "PICO Radar|Events")
  FOnGlobalPlayerLeft OnGlobalPlayerLeft;

  /** 全局玩家更新事件 */
  UPROPERTY(BlueprintAssignable, Category = "PICO Radar|Events")
  FOnGlobalPlayerUpdated OnGlobalPlayerUpdated;

  /** 服务器发现事件 */
  UPROPERTY(BlueprintAssignable, Category = "PICO Radar|Events")
  FOnServerDiscovered OnServerDiscovered;

 private:
  /** 注册的组件列表 */
  UPROPERTY()
  TArray<UPICORadarComponent*> RegisteredComponents;

  /** 玩家可视化Actor映射 */
  UPROPERTY()
  TMap<FString, APICORadarPlayerVisualization*> PlayerVisualizations;

  /** 全局玩家数据缓存 */
  TMap<FString, FPICORadarPlayerData> GlobalPlayerData;

  /** 离线玩家清理定时器 */
  TMap<FString, FTimerHandle> PlayerCleanupTimers;

  /** 发现的服务器列表 */
  TArray<FString> DiscoveredServers;

  /** 服务器发现定时器 */
  FTimerHandle ServerDiscoveryTimer;

  /** UDP Socket（用于服务器发现） */
  class FSocket* DiscoverySocket = nullptr;

  // 内部方法
 private:
  /** 处理组件的玩家列表更新 */
  UFUNCTION()
  void HandleComponentPlayerListUpdate(
      const TArray<FPICORadarPlayerData>& Players);

  /** 处理组件连接状态变化 */
  UFUNCTION()
  void HandleComponentConnectionChanged();

  /** 更新全局玩家数据 */
  void UpdateGlobalPlayerData();

  /** 处理玩家可视化自动管理 */
  void ManagePlayerVisualizations();

  /** 清理离线玩家 */
  void CleanupOfflinePlayer(const FString& PlayerId);

  /** 执行服务器发现 */
  void PerformServerDiscovery();

  /** 处理服务器发现响应 */
  void HandleDiscoveryResponse();

  /** 日志输出 */
  void LogMessage(const FString& Message, bool bIsError = false) const;
};
