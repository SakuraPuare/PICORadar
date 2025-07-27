#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "PICORadarTypes.h"
#include "PICORadarPlayerVisualization.generated.h"

/**
 * 玩家位置可视化Actor
 * 
 * 用于在3D空间中可视化其他玩家的位置和朝向。
 * 这个Actor会根据从PICORadar系统接收到的玩家数据自动更新位置和旋转。
 * 
 * 特性：
 * - 自动跟随玩家位置和旋转
 * - 可自定义的可视化网格和材质
 * - 玩家名称显示
 * - 平滑的插值动画
 * - 距离衰减和遮挡处理
 */
UCLASS(BlueprintType, Blueprintable)
class PICORADAR_API APICORadarPlayerVisualization : public AActor
{
    GENERATED_BODY()

public:
    APICORadarPlayerVisualization();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

public:
    /**
     * 设置要可视化的玩家数据
     */
    UFUNCTION(BlueprintCallable, Category = "PICO Radar")
    void SetPlayerData(const FPICORadarPlayerData& PlayerData);

    /**
     * 获取当前玩家数据
     */
    UFUNCTION(BlueprintPure, Category = "PICO Radar")
    const FPICORadarPlayerData& GetPlayerData() const { return CurrentPlayerData; }

    /**
     * 获取玩家ID
     */
    UFUNCTION(BlueprintPure, Category = "PICO Radar")
    FString GetPlayerId() const { return CurrentPlayerData.PlayerId; }

    /**
     * 设置可视化是否启用
     */
    UFUNCTION(BlueprintCallable, Category = "PICO Radar")
    void SetVisualizationEnabled(bool bEnabled);

    /**
     * 检查可视化是否启用
     */
    UFUNCTION(BlueprintPure, Category = "PICO Radar")
    bool IsVisualizationEnabled() const { return bIsVisualizationEnabled; }

    /**
     * 强制立即更新位置（跳过插值）
     */
    UFUNCTION(BlueprintCallable, Category = "PICO Radar")
    void ForceUpdatePosition();

protected:
    // 可视化组件
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* PlayerMeshComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UTextRenderComponent* PlayerNameComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* DirectionArrowComponent;

    // 配置属性
public:
    /** 用于显示玩家的网格 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO Radar|Visualization")
    class UStaticMesh* PlayerMesh;

    /** 玩家网格的材质 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO Radar|Visualization")
    class UMaterialInterface* PlayerMaterial;

    /** 方向指示箭头的网格 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO Radar|Visualization")
    class UStaticMesh* DirectionArrowMesh;

    /** 方向箭头的材质 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO Radar|Visualization")
    class UMaterialInterface* DirectionArrowMaterial;

    /** 玩家名称文字颜色 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO Radar|Visualization")
    FLinearColor NameTextColor = FLinearColor::White;

    /** 玩家名称文字大小 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO Radar|Visualization")
    float NameTextSize = 50.0f;

    /** 可视化缩放比例 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO Radar|Visualization", meta = (ClampMin = "0.1", ClampMax = "5.0"))
    float VisualizationScale = 1.0f;

    /** 位置插值速度 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO Radar|Animation", meta = (ClampMin = "1.0", ClampMax = "50.0"))
    float PositionInterpSpeed = 10.0f;

    /** 旋转插值速度 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO Radar|Animation", meta = (ClampMin = "1.0", ClampMax = "50.0"))
    float RotationInterpSpeed = 15.0f;

    /** 是否启用位置插值 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO Radar|Animation")
    bool bEnablePositionInterpolation = true;

    /** 是否启用旋转插值 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO Radar|Animation")
    bool bEnableRotationInterpolation = true;

    /** 最大可视距离（超出此距离将隐藏） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO Radar|LOD", meta = (ClampMin = "100.0"))
    float MaxVisibilityDistance = 5000.0f;

    /** 是否基于距离调整透明度 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO Radar|LOD")
    bool bEnableDistanceFading = true;

    /** 开始淡出的距离 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO Radar|LOD", meta = (ClampMin = "100.0"))
    float FadeStartDistance = 2000.0f;

    /** 是否始终面向摄像机（Billboard效果） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO Radar|Display")
    bool bAlwaysFaceCamera = false;

    /** 是否显示玩家名称 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO Radar|Display")
    bool bShowPlayerName = true;

    /** 是否显示方向指示 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICO Radar|Display")
    bool bShowDirectionArrow = true;

private:
    /** 当前玩家数据 */
    FPICORadarPlayerData CurrentPlayerData;

    /** 目标位置（用于插值） */
    FVector TargetPosition = FVector::ZeroVector;

    /** 目标旋转（用于插值） */
    FRotator TargetRotation = FRotator::ZeroRotator;

    /** 可视化是否启用 */
    bool bIsVisualizationEnabled = true;

    /** 动态材质实例（用于透明度控制） */
    UPROPERTY()
    class UMaterialInstanceDynamic* DynamicPlayerMaterial;

    UPROPERTY()
    class UMaterialInstanceDynamic* DynamicArrowMaterial;

    // 内部方法
private:
    /** 初始化组件 */
    void InitializeComponents();

    /** 更新可视化状态 */
    void UpdateVisualization(float DeltaTime);

    /** 根据距离计算透明度 */
    float CalculateDistanceBasedOpacity() const;

    /** 获取到本地玩家的距离 */
    float GetDistanceToLocalPlayer() const;

    /** 更新材质透明度 */
    void UpdateMaterialOpacity(float Opacity);

    /** 更新面向摄像机逻辑 */
    void UpdateBillboardRotation();

protected:
    // 蓝图事件
    UFUNCTION(BlueprintImplementableEvent, Category = "PICO Radar|Events")
    void OnPlayerDataUpdated(const FPICORadarPlayerData& NewPlayerData);

    UFUNCTION(BlueprintImplementableEvent, Category = "PICO Radar|Events")
    void OnVisualizationEnabled();

    UFUNCTION(BlueprintImplementableEvent, Category = "PICO Radar|Events")
    void OnVisualizationDisabled();
};
