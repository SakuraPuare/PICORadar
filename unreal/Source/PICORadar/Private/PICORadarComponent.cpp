#include "PICORadarComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "TimerManager.h"

UPICORadarComponent::UPICORadarComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.016f; // ~60 FPS

	// 默认设置
	UserId = TEXT("Player_") + FString::FromInt(FMath::RandRange(1000, 9999));
	UpdateFrequency = 30.0f; // 30 Hz
	bAutoConnectOnBeginPlay = false;
	DefaultServerAddress = TEXT("127.0.0.1");
	DefaultServerPort = 8080;

	// 初始状态
	ConnectionStatus = EPICORadarConnectionStatus::Disconnected;
	LastUpdateTime = 0.0f;
	ServerPort = DefaultServerPort;
}

void UPICORadarComponent::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Log, TEXT("PICORadarComponent: BeginPlay - UserId: %s"), *UserId);

	if (bAutoConnectOnBeginPlay && !DefaultServerAddress.IsEmpty())
	{
		ConnectToServer(DefaultServerAddress, DefaultServerPort);
	}
}

void UPICORadarComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DisconnectFromServer();
	Super::EndPlay(EndPlayReason);
}

void UPICORadarComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (ConnectionStatus == EPICORadarConnectionStatus::Connected)
	{
		// 处理接收到的数据
		ProcessIncomingData();

		// 定期发送位置更新
		LastUpdateTime += DeltaTime;
		if (LastUpdateTime >= (1.0f / UpdateFrequency))
		{
			if (AActor* Owner = GetOwner())
			{
				FVector Position = Owner->GetActorLocation();
				FRotator Rotation = Owner->GetActorRotation();
				SendUserPosition(Position, Rotation);
			}
			LastUpdateTime = 0.0f;
		}
	}
}

bool UPICORadarComponent::ConnectToServer(const FString& InServerAddress, int32 Port)
{
	if (ConnectionStatus == EPICORadarConnectionStatus::Connected)
	{
		UE_LOG(LogTemp, Warning, TEXT("PICORadarComponent: Already connected to server"));
		return true;
	}

	ServerAddress = InServerAddress;
	ServerPort = Port;

	UE_LOG(LogTemp, Log, TEXT("PICORadarComponent: Attempting to connect to %s:%d"), *ServerAddress, ServerPort);

	UpdateConnectionStatus(EPICORadarConnectionStatus::Connecting);

	// TODO: 实现实际的网络连接逻辑
	// 这里暂时模拟连接成功
	UpdateConnectionStatus(EPICORadarConnectionStatus::Connected);

	return true;
}

void UPICORadarComponent::DisconnectFromServer()
{
	if (ConnectionStatus == EPICORadarConnectionStatus::Disconnected)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("PICORadarComponent: Disconnecting from server"));

	// TODO: 实现实际的断开连接逻辑

	UserPositions.Empty();
	UpdateConnectionStatus(EPICORadarConnectionStatus::Disconnected);
}

bool UPICORadarComponent::SendUserPosition(const FVector& Position, const FRotator& Rotation)
{
	if (ConnectionStatus != EPICORadarConnectionStatus::Connected)
	{
		return false;
	}

	// TODO: 实现实际的数据发送逻辑
	// 这里暂时只是记录日志
	UE_LOG(LogTemp, VeryVerbose, TEXT("PICORadarComponent: Sending position - User: %s, Pos: %s, Rot: %s"), 
		   *UserId, *Position.ToString(), *Rotation.ToString());

	return true;
}

TArray<FUserPositionData> UPICORadarComponent::GetAllUserPositions() const
{
	TArray<FUserPositionData> Result;
	UserPositions.GenerateValueArray(Result);
	return Result;
}

bool UPICORadarComponent::GetUserPosition(const FString& InUserId, FUserPositionData& OutUserData) const
{
	if (const FUserPositionData* FoundData = UserPositions.Find(InUserId))
	{
		OutUserData = *FoundData;
		return true;
	}
	return false;
}

EPICORadarConnectionStatus UPICORadarComponent::GetConnectionStatus() const
{
	return ConnectionStatus;
}

void UPICORadarComponent::SetUserId(const FString& NewUserId)
{
	if (!NewUserId.IsEmpty())
	{
		UserId = NewUserId;
		UE_LOG(LogTemp, Log, TEXT("PICORadarComponent: UserId changed to: %s"), *UserId);
	}
}

FString UPICORadarComponent::GetUserId() const
{
	return UserId;
}

void UPICORadarComponent::UpdateConnectionStatus(EPICORadarConnectionStatus NewStatus)
{
	if (ConnectionStatus != NewStatus)
	{
		ConnectionStatus = NewStatus;
		OnConnectionStatusChanged.Broadcast(ConnectionStatus);

		FString StatusString;
		switch (ConnectionStatus)
		{
		case EPICORadarConnectionStatus::Disconnected:
			StatusString = TEXT("Disconnected");
			break;
		case EPICORadarConnectionStatus::Connecting:
			StatusString = TEXT("Connecting");
			break;
		case EPICORadarConnectionStatus::Connected:
			StatusString = TEXT("Connected");
			break;
		case EPICORadarConnectionStatus::Error:
			StatusString = TEXT("Error");
			break;
		}

		UE_LOG(LogTemp, Log, TEXT("PICORadarComponent: Connection status changed to: %s"), *StatusString);
	}
}

void UPICORadarComponent::ProcessIncomingData()
{
	// TODO: 实现实际的数据接收和处理逻辑
	// 这里暂时模拟接收其他用户的位置数据
	
	// 示例：模拟接收到另一个用户的数据
	static float SimulationTime = 0.0f;
	SimulationTime += GetWorld()->GetDeltaSeconds();
	
	if (FMath::Fmod(SimulationTime, 2.0f) < 0.016f) // 每2秒模拟一次新用户数据
	{
		FString MockUserId = TEXT("MockUser_123");
		FVector MockPosition = FVector(
			FMath::Sin(SimulationTime) * 100.0f,
			FMath::Cos(SimulationTime) * 100.0f,
			0.0f
		);
		FRotator MockRotation = FRotator(0.0f, SimulationTime * 30.0f, 0.0f);
		
		FUserPositionData MockData(MockUserId, MockPosition, MockRotation, SimulationTime);
		
		// 检查是否是新用户
		bool bIsNewUser = !UserPositions.Contains(MockUserId);
		
		UserPositions.Add(MockUserId, MockData);
		
		if (bIsNewUser)
		{
			OnUserConnected.Broadcast(MockUserId);
		}
		
		OnUserPositionUpdated.Broadcast(MockData);
	}
}

bool UPICORadarComponent::ValidateConnection() const
{
	return ConnectionStatus == EPICORadarConnectionStatus::Connected;
}
