#include "PICORadarComponent.h"

#include "Async/Async.h"
#include "Camera/CameraComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "PICORadarModule.h"
#include "PICORadarWorldSubsystem.h"

// 包含PICORadar客户端库头文件
#if WITH_PICORADAR_CLIENT
#include "client.hpp"
#include "player.pb.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogPICORadarComponent, Log, All);

UPICORadarComponent::UPICORadarComponent() {
  PrimaryComponentTick.bCanEverTick = true;
  PrimaryComponentTick.bStartWithTickEnabled = true;

  // 设置默认值
  SetComponentTickEnabled(true);

#if WITH_PICORADAR_CLIENT
  // 创建客户端实例
  Client = MakeUnique<picoradar::client::Client>();
#endif
}

UPICORadarComponent::~UPICORadarComponent() {
  if (IsConnectedToServer()) {
    DisconnectFromServer();
  }
}

void UPICORadarComponent::BeginPlay() {
  Super::BeginPlay();

  LogMessage(FString::Printf(
      TEXT("PICORadar Component BeginPlay - PlayerId: %s"), *PlayerId));

  // 注册到世界子系统
  if (UWorld* World = GetWorld()) {
    if (UPICORadarWorldSubsystem* Subsystem =
            World->GetSubsystem<UPICORadarWorldSubsystem>()) {
      Subsystem->RegisterComponent(this);
    }
  }

  // 如果启用自动连接，则尝试连接
  if (bAutoConnectOnBeginPlay && !ServerAddress.IsEmpty() &&
      !PlayerId.IsEmpty()) {
    ConnectToServer(ServerAddress, PlayerId, AuthToken);
  }
}

void UPICORadarComponent::EndPlay(const EEndPlayReason::Type EndPlayReason) {
  // 断开连接
  DisconnectFromServer();

  // 从世界子系统注销
  if (UWorld* World = GetWorld()) {
    if (UPICORadarWorldSubsystem* Subsystem =
            World->GetSubsystem<UPICORadarWorldSubsystem>()) {
      Subsystem->UnregisterComponent(this);
    }
  }

  Super::EndPlay(EndPlayReason);
}

void UPICORadarComponent::Tick(float DeltaTime) {
  Super::Tick(DeltaTime);

  // 检查连接任务状态
  if (ConnectionTask.IsValid() && ConnectionTask->IsComplete()) {
    if (ConnectionTask->IsSuccessful()) {
      bIsConnected = true;
      bIsConnecting = false;
      LogMessage(TEXT("Successfully connected to PICORadar server"));
      OnConnected.Broadcast();
    } else {
      bIsConnecting = false;
      FString ErrorMsg = ConnectionTask->GetErrorMessage();
      LogMessage(
          FString::Printf(TEXT("Failed to connect to PICORadar server: %s"),
                          *ErrorMsg),
          true);
      OnConnectionFailed.Broadcast(ErrorMsg);
    }
    ConnectionTask.Reset();
  }

  // 如果已连接，检查是否需要发送数据更新
  if (bIsConnected && ShouldSendUpdate()) {
    SendPlayerData();
  }
}

bool UPICORadarComponent::IsTickable() const {
  return IsValid(this) && !IsTemplate();
}

TStatId UPICORadarComponent::GetStatId() const {
  RETURN_QUICK_DECLARE_CYCLE_STAT(UPICORadarComponent, STATGROUP_Tickables);
}

void UPICORadarComponent::ConnectToServer(const FString& InServerAddress,
                                          const FString& InPlayerId,
                                          const FString& InAuthToken) {
  if (bIsConnecting || bIsConnected) {
    LogMessage(TEXT("Already connecting or connected"), true);
    return;
  }

  if (InServerAddress.IsEmpty() || InPlayerId.IsEmpty()) {
    LogMessage(TEXT("Server address or player ID is empty"), true);
    OnConnectionFailed.Broadcast(TEXT("Invalid connection parameters"));
    return;
  }

#if !WITH_PICORADAR_CLIENT
  LogMessage(TEXT("PICORadar client library not available"), true);
  OnConnectionFailed.Broadcast(TEXT("Client library not available"));
  return;
#endif

  // 更新配置
  ServerAddress = InServerAddress;
  PlayerId = InPlayerId;
  AuthToken = InAuthToken;

  bIsConnecting = true;
  LogMessage(FString::Printf(TEXT("Connecting to server: %s with PlayerId: %s"),
                             *ServerAddress, *PlayerId));

#if WITH_PICORADAR_CLIENT
  // 设置玩家列表更新回调
  if (Client.IsValid()) {
    Client->setOnPlayerListUpdate(
        [this](const std::vector<picoradar::PlayerData>& Players) {
          // 将C++数据转换为UE数据类型
          TArray<FPICORadarPlayerData> UEPlayers;
          for (const auto& Player : Players) {
            FPICORadarPlayerData UEPlayer;
            UEPlayer.PlayerId =
                FString(UTF8_TO_TCHAR(Player.player_id().c_str()));
            UEPlayer.SceneId =
                FString(UTF8_TO_TCHAR(Player.scene_id().c_str()));

            // 转换位置数据
            if (Player.has_position()) {
              const auto& Pos = Player.position();
              UEPlayer.Position.X = Pos.x();
              UEPlayer.Position.Y = Pos.y();
              UEPlayer.Position.Z = Pos.z();
            }

            // 转换旋转数据
            if (Player.has_rotation()) {
              const auto& Rot = Player.rotation();
              UEPlayer.Rotation.X = Rot.x();
              UEPlayer.Rotation.Y = Rot.y();
              UEPlayer.Rotation.Z = Rot.z();
              UEPlayer.Rotation.W = Rot.w();
            }

            UEPlayer.Timestamp = Player.timestamp();
            UEPlayers.Add(UEPlayer);
          }

          // 在游戏线程中处理回调
          AsyncTask(ENamedThreads::GameThread,
                    [this, UEPlayers] { HandlePlayerListUpdate(UEPlayers); });
        });

    // 创建异步连接任务
    ConnectionTask = MakeShared<FPICORadarConnectionTask>(this);
    ConnectionTask->StartConnection(ServerAddress, PlayerId, AuthToken);
  }
#endif
}

void UPICORadarComponent::DisconnectFromServer() {
  if (!bIsConnecting && !bIsConnected) {
    return;
  }

  LogMessage(TEXT("Disconnecting from PICORadar server"));

#if WITH_PICORADAR_CLIENT
  if (Client.IsValid()) {
    Client->disconnect();
  }
#endif

  bIsConnected = false;
  bIsConnecting = false;
  CurrentPlayers.Empty();

  if (ConnectionTask.IsValid()) {
    ConnectionTask.Reset();
  }

  OnDisconnected.Broadcast(TEXT("Manual disconnect"));
}

bool UPICORadarComponent::IsConnectedToServer() const {
#if WITH_PICORADAR_CLIENT
  return bIsConnected && Client.IsValid() && Client->isConnected();
#else
  return false;
#endif
}

void UPICORadarComponent::SendPlayerData() {
  if (!IsConnectedToServer()) {
    return;
  }

  FVector CurrentPosition;
  FRotator CurrentRotation;
  GetCurrentTransform(CurrentPosition, CurrentRotation);

#if WITH_PICORADAR_CLIENT
  if (Client.IsValid()) {
    // 创建protobuf消息
    picoradar::PlayerData PlayerDataMsg;
    PlayerDataMsg.set_player_id(TCHAR_TO_UTF8(*PlayerId));
    PlayerDataMsg.set_scene_id(TCHAR_TO_UTF8(*SceneId));

    // 设置位置
    auto* Position = PlayerDataMsg.mutable_position();
    Position->set_x(CurrentPosition.X);
    Position->set_y(CurrentPosition.Y);
    Position->set_z(CurrentPosition.Z);

    // 设置旋转（转换为四元数）
    FQuat Quat = CurrentRotation.Quaternion();
    auto* Rotation = PlayerDataMsg.mutable_rotation();
    Rotation->set_x(Quat.X);
    Rotation->set_y(Quat.Y);
    Rotation->set_z(Quat.Z);
    Rotation->set_w(Quat.W);

    // 设置时间戳
    PlayerDataMsg.set_timestamp(FDateTime::Now().ToUnixTimestamp() * 1000);

    // 发送数据
    Client->sendPlayerData(PlayerDataMsg);

    // 更新缓存
    LastSentPosition = CurrentPosition;
    LastSentRotation = CurrentRotation;
    LastSendTime = GetWorld()->GetTimeSeconds();

    if (bEnableVerboseLogging) {
      LogMessage(FString::Printf(
          TEXT("Sent player data: Pos(%.1f,%.1f,%.1f) Rot(%.1f,%.1f,%.1f)"),
          CurrentPosition.X, CurrentPosition.Y, CurrentPosition.Z,
          CurrentRotation.Pitch, CurrentRotation.Yaw, CurrentRotation.Roll));
    }
  }
#endif
}

TArray<FPICORadarPlayerData> UPICORadarComponent::GetCurrentPlayers() const {
  TArray<FPICORadarPlayerData> Players;
  CurrentPlayers.GenerateValueArray(Players);
  return Players;
}

bool UPICORadarComponent::GetPlayerData(
    const FString& InPlayerId, FPICORadarPlayerData& OutPlayerData) const {
  if (const FPICORadarPlayerData* FoundPlayer =
          CurrentPlayers.Find(InPlayerId)) {
    OutPlayerData = *FoundPlayer;
    return true;
  }
  return false;
}

void UPICORadarComponent::HandlePlayerListUpdate(
    const TArray<FPICORadarPlayerData>& Players) {
  // 检测新加入和离开的玩家
  TSet<FString> NewPlayerIds;
  TSet<FString> CurrentPlayerIds;

  // 构建新玩家ID集合
  for (const auto& Player : Players) {
    NewPlayerIds.Add(Player.PlayerId);
  }

  // 构建当前玩家ID集合
  for (const auto& Pair : CurrentPlayers) {
    CurrentPlayerIds.Add(Pair.Key);
  }

  // 检测新加入的玩家
  for (const auto& Player : Players) {
    if (!CurrentPlayerIds.Contains(Player.PlayerId)) {
      OnPlayerJoined.Broadcast(Player.PlayerId, Player);
      if (bEnableVerboseLogging) {
        LogMessage(
            FString::Printf(TEXT("Player joined: %s"), *Player.PlayerId));
      }
    }
  }

  // 检测离开的玩家
  for (const auto& PlayerId : CurrentPlayerIds) {
    if (!NewPlayerIds.Contains(PlayerId)) {
      OnPlayerLeft.Broadcast(PlayerId);
      if (bEnableVerboseLogging) {
        LogMessage(FString::Printf(TEXT("Player left: %s"), *PlayerId));
      }
    }
  }

  // 更新玩家数据
  CurrentPlayers.Empty();
  for (const auto& Player : Players) {
    CurrentPlayers.Add(Player.PlayerId, Player);
  }

  // 广播玩家列表更新事件
  OnPlayerListUpdated.Broadcast(Players);
}

bool UPICORadarComponent::ShouldSendUpdate() const {
  if (!IsConnectedToServer()) {
    return false;
  }

  // 检查发送频率
  float CurrentTime = GetWorld()->GetTimeSeconds();
  float TimeSinceLastSend = CurrentTime - LastSendTime;
  if (TimeSinceLastSend < (1.0f / SendFrequency)) {
    return false;
  }

  // 检查位置和旋转变化
  FVector CurrentPosition;
  FRotator CurrentRotation;
  GetCurrentTransform(CurrentPosition, CurrentRotation);

  float PositionDelta = FVector::Dist(CurrentPosition, LastSentPosition);
  float RotationDelta =
      FMath::Abs(
          FRotator::ClampAxis(CurrentRotation.Yaw - LastSentRotation.Yaw)) +
      FMath::Abs(
          FRotator::ClampAxis(CurrentRotation.Pitch - LastSentRotation.Pitch)) +
      FMath::Abs(
          FRotator::ClampAxis(CurrentRotation.Roll - LastSentRotation.Roll));

  return (PositionDelta >= PositionThreshold) ||
         (RotationDelta >= RotationThreshold);
}

void UPICORadarComponent::GetCurrentTransform(FVector& OutPosition,
                                              FRotator& OutRotation) const {
  if (AActor* Owner = GetOwner()) {
    // 优先使用相机组件的Transform
    if (UCameraComponent* Camera =
            Owner->FindComponentByClass<UCameraComponent>()) {
      OutPosition = Camera->GetComponentLocation();
      OutRotation = Camera->GetComponentRotation();
    }
    // 否则使用Actor的根组件Transform
    else if (USceneComponent* RootComp = Owner->GetRootComponent()) {
      OutPosition = RootComp->GetComponentLocation();
      OutRotation = RootComp->GetComponentRotation();
    }
    // 最后使用Actor Transform
    else {
      OutPosition = Owner->GetActorLocation();
      OutRotation = Owner->GetActorRotation();
    }
  } else {
    OutPosition = FVector::ZeroVector;
    OutRotation = FRotator::ZeroRotator;
  }
}

FPICORadarPlayerData UPICORadarComponent::CreatePlayerDataFromTransform(
    const FVector& Position, const FRotator& Rotation) const {
  return FPICORadarPlayerData::FromTransform(PlayerId, SceneId, Position,
                                             Rotation);
}

void UPICORadarComponent::LogMessage(const FString& Message,
                                     bool bIsError) const {
  if (bIsError) {
    UE_LOG(LogPICORadarComponent, Error, TEXT("[%s] %s"), *PlayerId, *Message);
  } else if (bEnableVerboseLogging) {
    UE_LOG(LogPICORadarComponent, Log, TEXT("[%s] %s"), *PlayerId, *Message);
  }
}

// FPICORadarConnectionTask 实现
FPICORadarConnectionTask::FPICORadarConnectionTask(UPICORadarComponent* InOwner)
    : Owner(InOwner) {}

FPICORadarConnectionTask::~FPICORadarConnectionTask() {
  if (ConnectionFuture.valid()) {
    try {
      ConnectionFuture.get();
    } catch (...) {
      // 忽略异常，析构函数中不应该抛出异常
    }
  }
}

void FPICORadarConnectionTask::StartConnection(const FString& ServerAddress,
                                               const FString& PlayerId,
                                               const FString& AuthToken) {
#if WITH_PICORADAR_CLIENT
  if (Owner.IsValid()) {
    auto ClientPtr = Owner->Client.Get();
    if (ClientPtr) {
      ConnectionFuture = std::async(
          std::launch::async,
          [this, ClientPtr, ServerAddress, PlayerId, AuthToken] {
            try {
              auto Future = ClientPtr->connect(TCHAR_TO_UTF8(*ServerAddress),
                                               TCHAR_TO_UTF8(*PlayerId),
                                               TCHAR_TO_UTF8(*AuthToken));
              Future.get();
              bIsSuccessful = true;
            } catch (const std::exception& e) {
              ErrorMessage = FString(UTF8_TO_TCHAR(e.what()));
              bIsSuccessful = false;
            } catch (...) {
              ErrorMessage = TEXT("Unknown connection error");
              bIsSuccessful = false;
            }
            bIsComplete = true;
          });
    }
  }
#endif
}

bool FPICORadarConnectionTask::IsComplete() const { return bIsComplete.load(); }

bool FPICORadarConnectionTask::IsSuccessful() const {
  return bIsSuccessful.load();
}

FString FPICORadarConnectionTask::GetErrorMessage() const {
  return ErrorMessage;
}
