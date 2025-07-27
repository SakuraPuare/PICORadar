// 示例：VR玩家控制器，集成PICORadar位置共享
// 展示最佳实践的UE插件集成方法

#pragma once

#include "Camera/CameraComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "PICORadarComponent.h"  // PICORadar插件头文件
#include "PICORadarTypes.h"
#include "VRPlayerPawn.generated.h"

/**
 * VR玩家Pawn示例
 *
 * 展示如何在VR项目中正确集成PICORadar组件：
 * - 自动位置同步
 * - VR头显跟踪
 * - 手柄交互
 * - 玩家可视化
 *
 * 设计原则：
 * - 组件化架构
 * - 事件驱动
 * - 性能优化
 * - 错误处理
 */
UCLASS(BlueprintType, Blueprintable)
class PICORADAREXAMPLEPROJECT_API AVRPlayerPawn : public APawn {
  GENERATED_BODY()

 public:
  AVRPlayerPawn();

 protected:
  virtual void BeginPlay() override;
  virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
  virtual void Tick(float DeltaTime) override;
  virtual void SetupPlayerInputComponent(
      UInputComponent* PlayerInputComponent) override;

  // 组件定义
 protected:
  /** VR根组件 */
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR")
  class USceneComponent* VRRoot;

  /** VR相机 */
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR")
  class UCameraComponent* VRCamera;

  /** 玩家碰撞体 */
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR")
  class USphereComponent* CollisionSphere;

  /** 玩家可视化网格 */
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR")
  class UStaticMeshComponent* PlayerMesh;

  /** PICORadar组件 - 核心位置共享功能 */
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PICO Radar")
  class UPICORadarComponent* RadarComponent;

  // 配置属性
 public:
  /** 玩家唯一ID（自动生成或手动设置） */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO Radar|Player")
  FString PlayerId;

  /** PICORadar服务器地址 */
  UPROPERTY(EditAnywhere, BlueprintReadWrite,
            Category = "PICO Radar|Connection")
  FString ServerAddress = TEXT("127.0.0.1:11451");

  /** 认证令牌 */
  UPROPERTY(EditAnywhere, BlueprintReadWrite,
            Category = "PICO Radar|Connection")
  FString AuthToken = TEXT("default_token");

  /** 场景ID */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO Radar|Sync")
  FString SceneId = TEXT("VRScene");

  /** 是否在游戏开始时自动连接 */
  UPROPERTY(EditAnywhere, BlueprintReadWrite,
            Category = "PICO Radar|Connection")
  bool bAutoConnect = true;

  /** 是否显示其他玩家 */
  UPROPERTY(EditAnywhere, BlueprintReadWrite,
            Category = "PICO Radar|Visualization")
  bool bShowOtherPlayers = true;

  /** 其他玩家可视化材质 */
  UPROPERTY(EditAnywhere, BlueprintReadWrite,
            Category = "PICO Radar|Visualization")
  class UMaterialInterface* OtherPlayerMaterial;

  /** 移动速度 */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR|Movement")
  float MovementSpeed = 300.0f;

  /** 旋转速度 */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR|Movement")
  float RotationSpeed = 90.0f;

  // 事件处理
 public:
  /** PICORadar连接成功 */
  UFUNCTION(BlueprintImplementableEvent, Category = "PICO Radar|Events")
  void OnRadarConnected();

  /** PICORadar连接失败 */
  UFUNCTION(BlueprintImplementableEvent, Category = "PICO Radar|Events")
  void OnRadarConnectionFailed(const FString& ErrorMessage);

  /** 其他玩家加入 */
  UFUNCTION(BlueprintImplementableEvent, Category = "PICO Radar|Events")
  void OnOtherPlayerJoined(const FString& PlayerId,
                           const FPICORadarPlayerData& PlayerData);

  /** 其他玩家离开 */
  UFUNCTION(BlueprintImplementableEvent, Category = "PICO Radar|Events")
  void OnOtherPlayerLeft(const FString& PlayerId);

  // 公共接口
 public:
  /** 手动连接到PICORadar服务器 */
  UFUNCTION(BlueprintCallable, Category = "PICO Radar")
  void ConnectToRadarServer();

  /** 断开PICORadar连接 */
  UFUNCTION(BlueprintCallable, Category = "PICO Radar")
  void DisconnectFromRadarServer();

  /** 检查PICORadar连接状态 */
  UFUNCTION(BlueprintPure, Category = "PICO Radar")
  bool IsConnectedToRadarServer() const;

  /** 获取当前所有在线玩家 */
  UFUNCTION(BlueprintPure, Category = "PICO Radar")
  TArray<FPICORadarPlayerData> GetOnlinePlayers() const;

  /** 设置玩家可见性 */
  UFUNCTION(BlueprintCallable, Category = "PICO Radar")
  void SetPlayerVisibility(bool bVisible);

  /** 获取玩家距离 */
  UFUNCTION(BlueprintPure, Category = "PICO Radar")
  float GetDistanceToPlayer(const FString& OtherPlayerId) const;

 private:
  // 内部状态
  FVector LastKnownPosition = FVector::ZeroVector;
  FRotator LastKnownRotation = FRotator::ZeroRotator;

  /** 其他玩家可视化Actor映射 */
  UPROPERTY()
  TMap<FString, class APICORadarPlayerVisualization*> OtherPlayerVisualizations;

  // 输入处理
 private:
  /** 移动输入 */
  void MoveForward(float Value);
  void MoveRight(float Value);
  void Turn(float Value);
  void LookUp(float Value);

  /** VR控制器输入 */
  void TriggerPressed();
  void TriggerReleased();
  void GripPressed();
  void GripReleased();

  // PICORadar事件处理
 private:
  /** 处理PICORadar连接成功 */
  UFUNCTION()
  void HandleRadarConnected();

  /** 处理PICORadar连接失败 */
  UFUNCTION()
  void HandleRadarConnectionFailed(const FString& ErrorMessage);

  /** 处理PICORadar断开连接 */
  UFUNCTION()
  void HandleRadarDisconnected(const FString& Reason);

  /** 处理玩家列表更新 */
  UFUNCTION()
  void HandlePlayerListUpdated(const TArray<FPICORadarPlayerData>& Players);

  /** 处理玩家加入 */
  UFUNCTION()
  void HandlePlayerJoined(const FString& JoinedPlayerId,
                          const FPICORadarPlayerData& PlayerData);

  /** 处理玩家离开 */
  UFUNCTION()
  void HandlePlayerLeft(const FString& LeftPlayerId);

  // 内部方法
 private:
  /** 初始化组件 */
  void InitializeComponents();

  /** 设置PICORadar事件绑定 */
  void SetupRadarEventBindings();

  /** 生成唯一玩家ID */
  FString GenerateUniquePlayerId() const;

  /** 更新玩家可视化 */
  void UpdatePlayerVisualizations(const TArray<FPICORadarPlayerData>& Players);

  /** 创建玩家可视化 */
  class APICORadarPlayerVisualization* CreatePlayerVisualization(
      const FPICORadarPlayerData& PlayerData);

  /** 移除玩家可视化 */
  void RemovePlayerVisualization(const FString& PlayerId);

  /** 日志输出 */
  void LogMessage(const FString& Message, bool bIsError = false) const;

  // 调试功能
 public:
  /** 启用调试显示 */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO Radar|Debug")
  bool bShowDebugInfo = false;

  /** 调试信息显示时间 */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO Radar|Debug")
  float DebugInfoDisplayTime = 5.0f;

  /** 显示调试信息 */
  UFUNCTION(BlueprintCallable, Category = "PICO Radar|Debug")
  void ShowDebugInfo();

 private:
  /** 调试信息显示定时器 */
  FTimerHandle DebugInfoTimer;

  /** 清除调试信息显示 */
  void ClearDebugInfo();
};
