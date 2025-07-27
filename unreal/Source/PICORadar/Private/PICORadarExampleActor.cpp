#include "PICORadarExampleActor.h"
#include "PICORadarComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

APICORadarExampleActor::APICORadarExampleActor()
{
	PrimaryActorTick.bCanEverTick = true;

	// 创建根组件
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));

	// 创建网格组件
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(RootComponent);

	// 尝试加载一个简单的网格（立方体）
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshAsset(TEXT("/Engine/BasicShapes/Cube"));
	if (CubeMeshAsset.Succeeded())
	{
		MeshComponent->SetStaticMesh(CubeMeshAsset.Object);
		MeshComponent->SetRelativeScale3D(FVector(0.5f, 0.5f, 0.5f));
	}

	// 创建PICORadar组件
	RadarComponent = CreateDefaultSubobject<UPICORadarComponent>(TEXT("RadarComponent"));
}

void APICORadarExampleActor::BeginPlay()
{
	Super::BeginPlay();
	
	UE_LOG(LogTemp, Log, TEXT("PICORadarExampleActor: BeginPlay"));

	// 绑定事件
	if (RadarComponent)
	{
		RadarComponent->OnUserPositionUpdated.AddDynamic(this, &APICORadarExampleActor::OnUserPositionUpdated);
		RadarComponent->OnUserConnected.AddDynamic(this, &APICORadarExampleActor::OnUserConnected);
		RadarComponent->OnUserDisconnected.AddDynamic(this, &APICORadarExampleActor::OnUserDisconnected);
		RadarComponent->OnConnectionStatusChanged.AddDynamic(this, &APICORadarExampleActor::OnConnectionStatusChanged);

		// 设置用户ID
		FString UniqueUserId = FString::Printf(TEXT("VRUser_%d"), FMath::RandRange(1000, 9999));
		RadarComponent->SetUserId(UniqueUserId);
	}
}

void APICORadarExampleActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void APICORadarExampleActor::StartRadarSystem()
{
	if (RadarComponent)
	{
		UE_LOG(LogTemp, Log, TEXT("PICORadarExampleActor: Starting radar system"));
		RadarComponent->ConnectToServer(TEXT("127.0.0.1"), 8080);
	}
}

void APICORadarExampleActor::StopRadarSystem()
{
	if (RadarComponent)
	{
		UE_LOG(LogTemp, Log, TEXT("PICORadarExampleActor: Stopping radar system"));
		RadarComponent->DisconnectFromServer();
		
		// 清理所有用户可视化Actor
		for (auto& Pair : UserVisualActors)
		{
			if (Pair.Value && IsValid(Pair.Value))
			{
				Pair.Value->Destroy();
			}
		}
		UserVisualActors.Empty();
	}
}

void APICORadarExampleActor::OnUserPositionUpdated(FUserPositionData UserData)
{
	UE_LOG(LogTemp, Log, TEXT("PICORadarExampleActor: User position updated - %s at %s"), 
		   *UserData.UserId, *UserData.Position.ToString());

	// 如果不是自己的数据，更新可视化
	if (RadarComponent && UserData.UserId != RadarComponent->GetUserId())
	{
		UpdateUserVisualActor(UserData.UserId, UserData.Position, UserData.Rotation);
	}
}

void APICORadarExampleActor::OnUserConnected(FString UserId)
{
	UE_LOG(LogTemp, Log, TEXT("PICORadarExampleActor: User connected - %s"), *UserId);
	
	// 在屏幕上显示消息
	if (GEngine)
	{
		FString Message = FString::Printf(TEXT("User Connected: %s"), *UserId);
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, Message);
	}
}

void APICORadarExampleActor::OnUserDisconnected(FString UserId)
{
	UE_LOG(LogTemp, Log, TEXT("PICORadarExampleActor: User disconnected - %s"), *UserId);
	
	// 移除用户可视化
	RemoveUserVisualActor(UserId);
	
	// 在屏幕上显示消息
	if (GEngine)
	{
		FString Message = FString::Printf(TEXT("User Disconnected: %s"), *UserId);
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, Message);
	}
}

void APICORadarExampleActor::OnConnectionStatusChanged(EPICORadarConnectionStatus Status)
{
	FString StatusString;
	FColor StatusColor = FColor::White;
	
	switch (Status)
	{
	case EPICORadarConnectionStatus::Disconnected:
		StatusString = TEXT("Disconnected");
		StatusColor = FColor::Red;
		break;
	case EPICORadarConnectionStatus::Connecting:
		StatusString = TEXT("Connecting...");
		StatusColor = FColor::Yellow;
		break;
	case EPICORadarConnectionStatus::Connected:
		StatusString = TEXT("Connected");
		StatusColor = FColor::Green;
		break;
	case EPICORadarConnectionStatus::Error:
		StatusString = TEXT("Connection Error");
		StatusColor = FColor::Orange;
		break;
	}

	UE_LOG(LogTemp, Log, TEXT("PICORadarExampleActor: Connection status - %s"), *StatusString);
	
	// 在屏幕上显示连接状态
	if (GEngine)
	{
		FString Message = FString::Printf(TEXT("PICO Radar Status: %s"), *StatusString);
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, StatusColor, Message);
	}
}

AActor* APICORadarExampleActor::CreateUserVisualActor(const FString& UserId, const FVector& Position)
{
	// 创建一个简单的Actor来表示其他用户
	AActor* UserActor = GetWorld()->SpawnActor<AActor>();
	if (UserActor)
	{
		// 添加一个网格组件
		UStaticMeshComponent* UserMesh = NewObject<UStaticMeshComponent>(UserActor);
		UserActor->SetRootComponent(UserMesh);
		
		// 使用球体网格来表示其他用户
		static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMeshAsset(TEXT("/Engine/BasicShapes/Sphere"));
		if (SphereMeshAsset.Succeeded())
		{
			UserMesh->SetStaticMesh(SphereMeshAsset.Object);
			UserMesh->SetRelativeScale3D(FVector(0.3f, 0.3f, 0.3f));
		}
		
		UserActor->SetActorLocation(Position);
		UserVisualActors.Add(UserId, UserActor);
		
		UE_LOG(LogTemp, Log, TEXT("PICORadarExampleActor: Created visual actor for user %s"), *UserId);
	}
	
	return UserActor;
}

void APICORadarExampleActor::UpdateUserVisualActor(const FString& UserId, const FVector& Position, const FRotator& Rotation)
{
	AActor** FoundActor = UserVisualActors.Find(UserId);
	AActor* UserActor = nullptr;
	
	if (FoundActor && *FoundActor && IsValid(*FoundActor))
	{
		UserActor = *FoundActor;
	}
	else
	{
		// 创建新的可视化Actor
		UserActor = CreateUserVisualActor(UserId, Position);
	}
	
	if (UserActor)
	{
		UserActor->SetActorLocation(Position);
		UserActor->SetActorRotation(Rotation);
	}
}

void APICORadarExampleActor::RemoveUserVisualActor(const FString& UserId)
{
	if (AActor** FoundActor = UserVisualActors.Find(UserId))
	{
		if (*FoundActor && IsValid(*FoundActor))
		{
			(*FoundActor)->Destroy();
		}
		UserVisualActors.Remove(UserId);
		
		UE_LOG(LogTemp, Log, TEXT("PICORadarExampleActor: Removed visual actor for user %s"), *UserId);
	}
}
