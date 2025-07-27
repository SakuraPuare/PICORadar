#pragma once

#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "PICORadarTypes.generated.h"

/**
 * PICORadar使用的向量结构体
 * 对应protobuf中的Vector3消息
 */
USTRUCT(BlueprintType)
struct PICORADAR_API FPICORadarVector3 {
  GENERATED_BODY()

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO Radar")
  float X = 0.0f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO Radar")
  float Y = 0.0f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO Radar")
  float Z = 0.0f;

  FPICORadarVector3() = default;

  FPICORadarVector3(float InX, float InY, float InZ) : X(InX), Y(InY), Z(InZ) {}

  explicit FPICORadarVector3(const FVector& UnrealVector)
      : X(UnrealVector.X), Y(UnrealVector.Y), Z(UnrealVector.Z) {}

  /** 转换为UE的FVector */
  FVector ToFVector() const { return FVector(X, Y, Z); }

  /** 从UE的FVector转换 */
  static FPICORadarVector3 FromFVector(const FVector& Vector) {
    return FPICORadarVector3(Vector.X, Vector.Y, Vector.Z);
  }
};

/**
 * PICORadar使用的四元数结构体
 * 对应protobuf中的Quaternion消息
 */
USTRUCT(BlueprintType)
struct PICORADAR_API FPICORadarQuaternion {
  GENERATED_BODY()

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO Radar")
  float X = 0.0f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO Radar")
  float Y = 0.0f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO Radar")
  float Z = 0.0f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO Radar")
  float W = 1.0f;

  FPICORadarQuaternion() = default;

  FPICORadarQuaternion(float InX, float InY, float InZ, float InW)
      : X(InX), Y(InY), Z(InZ), W(InW) {}

  explicit FPICORadarQuaternion(const FQuat& UnrealQuat)
      : X(UnrealQuat.X), Y(UnrealQuat.Y), Z(UnrealQuat.Z), W(UnrealQuat.W) {}

  /** 转换为UE的FQuat */
  FQuat ToFQuat() const { return FQuat(X, Y, Z, W); }

  /** 转换为UE的FRotator */
  FRotator ToFRotator() const { return ToFQuat().Rotator(); }

  /** 从UE的FQuat转换 */
  static FPICORadarQuaternion FromFQuat(const FQuat& Quat) {
    return FPICORadarQuaternion(Quat.X, Quat.Y, Quat.Z, Quat.W);
  }

  /** 从UE的FRotator转换 */
  static FPICORadarQuaternion FromFRotator(const FRotator& Rotator) {
    return FromFQuat(Rotator.Quaternion());
  }
};

/**
 * PICORadar玩家数据结构体
 * 对应protobuf中的PlayerData消息
 */
USTRUCT(BlueprintType)
struct PICORADAR_API FPICORadarPlayerData {
  GENERATED_BODY()

  /** 玩家唯一ID */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO Radar")
  FString PlayerId;

  /** 场景ID */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO Radar")
  FString SceneId;

  /** 世界坐标位置 */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO Radar")
  FPICORadarVector3 Position;

  /** 头部朝向（四元数） */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO Radar")
  FPICORadarQuaternion Rotation;

  /** 时间戳（毫秒） */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO Radar")
  int64 Timestamp = 0;

  FPICORadarPlayerData() = default;

  /** 从UE Transform创建 */
  static FPICORadarPlayerData FromTransform(const FString& InPlayerId,
                                            const FString& InSceneId,
                                            const FVector& InPosition,
                                            const FRotator& InRotation,
                                            int64 InTimestamp = 0) {
    FPICORadarPlayerData Data;
    Data.PlayerId = InPlayerId;
    Data.SceneId = InSceneId;
    Data.Position = FPICORadarVector3::FromFVector(InPosition);
    Data.Rotation = FPICORadarQuaternion::FromFRotator(InRotation);
    Data.Timestamp = InTimestamp > 0
                         ? InTimestamp
                         : FDateTime::Now().ToUnixTimestamp() * 1000;
    return Data;
  }

  /** 获取UE坐标系位置 */
  FVector GetUnrealPosition() const { return Position.ToFVector(); }

  /** 获取UE坐标系旋转 */
  FRotator GetUnrealRotation() const { return Rotation.ToFRotator(); }

  /** 获取UE Transform */
  FTransform GetUnrealTransform() const {
    return FTransform(Rotation.ToFQuat(), Position.ToFVector());
  }
};

/**
 * PICORadar连接状态枚举
 */
UENUM(BlueprintType)
enum class EPICORadarConnectionState : uint8 {
  /** 未连接 */
  Disconnected UMETA(DisplayName = "Disconnected"),

  /** 正在连接 */
  Connecting UMETA(DisplayName = "Connecting"),

  /** 已连接 */
  Connected UMETA(DisplayName = "Connected"),

  /** 认证中 */
  Authenticating UMETA(DisplayName = "Authenticating"),

  /** 已认证（完全可用） */
  Authenticated UMETA(DisplayName = "Authenticated"),

  /** 连接失败 */
  Failed UMETA(DisplayName = "Failed")
};

/**
 * PICORadar统计信息结构体
 */
USTRUCT(BlueprintType)
struct PICORADAR_API FPICORadarStats {
  GENERATED_BODY()

  /** 当前连接的玩家数量 */
  UPROPERTY(BlueprintReadOnly, Category = "PICO Radar")
  int32 ConnectedPlayers = 0;

  /** 发送的消息总数 */
  UPROPERTY(BlueprintReadOnly, Category = "PICO Radar")
  int64 MessagesSent = 0;

  /** 接收的消息总数 */
  UPROPERTY(BlueprintReadOnly, Category = "PICO Radar")
  int64 MessagesReceived = 0;

  /** 平均延迟（毫秒） */
  UPROPERTY(BlueprintReadOnly, Category = "PICO Radar")
  float AverageLatency = 0.0f;

  /** 连接时长（秒） */
  UPROPERTY(BlueprintReadOnly, Category = "PICO Radar")
  float ConnectionDuration = 0.0f;

  /** 上次更新时间 */
  UPROPERTY(BlueprintReadOnly, Category = "PICO Radar")
  FDateTime LastUpdateTime;
};
