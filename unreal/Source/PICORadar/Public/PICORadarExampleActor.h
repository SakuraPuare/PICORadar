#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PICORadarExampleActor.generated.h"

// 前向声明
class UPICORadarComponent;
struct FUserPositionData;
enum class EPICORadarConnectionStatus : uint8;

UCLASS()
class PICORADAR_API APICORadarExampleActor : public AActor
{
	GENERATED_BODY()
	
public:	
	APICORadarExampleActor();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;

	// 蓝图可调用的函数
	UFUNCTION(BlueprintCallable, Category = "PICO Radar Example")
	void StartRadarSystem();

	UFUNCTION(BlueprintCallable, Category = "PICO Radar Example")
	void StopRadarSystem();

protected:
	// PICORadar组件
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PICO Radar")
	class UPICORadarComponent* RadarComponent;

	// 可视化网格
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visualization")
	class UStaticMeshComponent* MeshComponent;

	// 事件处理函数
	UFUNCTION()
	void OnUserPositionUpdated(FUserPositionData UserData);

	UFUNCTION()
	void OnUserConnected(FString UserId);

	UFUNCTION()
	void OnUserDisconnected(FString UserId);

	UFUNCTION()
	void OnConnectionStatusChanged(EPICORadarConnectionStatus Status);

private:
	// 用于显示其他用户的Actor映射
	UPROPERTY()
	TMap<FString, AActor*> UserVisualActors;

	// 创建用户可视化Actor
	AActor* CreateUserVisualActor(const FString& UserId, const FVector& Position);
	
	// 更新用户可视化Actor
	void UpdateUserVisualActor(const FString& UserId, const FVector& Position, const FRotator& Rotation);
	
	// 移除用户可视化Actor
	void RemoveUserVisualActor(const FString& UserId);
};
