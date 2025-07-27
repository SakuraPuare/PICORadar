#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PICORadarComponent.generated.h"

// 用户位置数据结构
USTRUCT(BlueprintType)
struct PICORADAR_API FUserPositionData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString UserId;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FVector Position;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FRotator Rotation;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float Timestamp;

	FUserPositionData()
	{
		UserId = TEXT("");
		Position = FVector::ZeroVector;
		Rotation = FRotator::ZeroRotator;
		Timestamp = 0.0f;
	}

	FUserPositionData(const FString& InUserId, const FVector& InPosition, const FRotator& InRotation, float InTimestamp)
		: UserId(InUserId), Position(InPosition), Rotation(InRotation), Timestamp(InTimestamp)
	{
	}
};

// 连接状态枚举
UENUM(BlueprintType)
enum class EPICORadarConnectionStatus : uint8
{
	Disconnected,
	Connecting,
	Connected,
	Error
};

// 委托声明
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUserPositionUpdated, FUserPositionData, UserData);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUserConnected, FString, UserId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUserDisconnected, FString, UserId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnConnectionStatusChanged, EPICORadarConnectionStatus, Status);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PICORADAR_API UPICORadarComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPICORadarComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// 蓝图接口
	UFUNCTION(BlueprintCallable, Category = "PICO Radar")
	bool ConnectToServer(const FString& ServerAddress, int32 Port = 8080);

	UFUNCTION(BlueprintCallable, Category = "PICO Radar")
	void DisconnectFromServer();

	UFUNCTION(BlueprintCallable, Category = "PICO Radar")
	bool SendUserPosition(const FVector& Position, const FRotator& Rotation);

	UFUNCTION(BlueprintCallable, Category = "PICO Radar")
	TArray<FUserPositionData> GetAllUserPositions() const;

	UFUNCTION(BlueprintCallable, Category = "PICO Radar")
	bool GetUserPosition(const FString& UserId, FUserPositionData& OutUserData) const;

	UFUNCTION(BlueprintCallable, Category = "PICO Radar")
	EPICORadarConnectionStatus GetConnectionStatus() const;

	UFUNCTION(BlueprintCallable, Category = "PICO Radar")
	void SetUserId(const FString& NewUserId);

	UFUNCTION(BlueprintCallable, Category = "PICO Radar")
	FString GetUserId() const;

	// 事件委托
	UPROPERTY(BlueprintAssignable, Category = "PICO Radar Events")
	FOnUserPositionUpdated OnUserPositionUpdated;

	UPROPERTY(BlueprintAssignable, Category = "PICO Radar Events")
	FOnUserConnected OnUserConnected;

	UPROPERTY(BlueprintAssignable, Category = "PICO Radar Events")
	FOnUserDisconnected OnUserDisconnected;

	UPROPERTY(BlueprintAssignable, Category = "PICO Radar Events")
	FOnConnectionStatusChanged OnConnectionStatusChanged;

protected:
	// 配置参数
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO Radar Settings")
	FString UserId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO Radar Settings")
	float UpdateFrequency;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO Radar Settings")
	bool bAutoConnectOnBeginPlay;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO Radar Settings")
	FString DefaultServerAddress;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO Radar Settings")
	int32 DefaultServerPort;

private:
	// 内部状态
	EPICORadarConnectionStatus ConnectionStatus;
	TMap<FString, FUserPositionData> UserPositions;
	float LastUpdateTime;
	FString ServerAddress;
	int32 ServerPort;

	// 内部方法
	void UpdateConnectionStatus(EPICORadarConnectionStatus NewStatus);
	void ProcessIncomingData();
	bool ValidateConnection() const;
};
