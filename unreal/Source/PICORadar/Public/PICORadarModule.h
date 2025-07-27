#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Engine/Engine.h"

DECLARE_LOG_CATEGORY_EXTERN(LogPICORadar, Log, All);

/**
 * PICORadar模块的主要接口
 * 
 * 该模块负责初始化PICORadar客户端库，管理全局状态，
 * 并提供与底层C++库的集成接口。
 */
class PICORADAR_API FPICORadarModule : public IModuleInterface
{
public:
    /** 模块启动 */
    virtual void StartupModule() override;
    
    /** 模块关闭 */
    virtual void ShutdownModule() override;
    
    /** 获取模块实例 */
    static FPICORadarModule& Get()
    {
        return FModuleManager::LoadModuleChecked<FPICORadarModule>("PICORadar");
    }
    
    /** 检查模块是否可用 */
    static bool IsAvailable()
    {
        return FModuleManager::Get().IsModuleLoaded("PICORadar");
    }
    
    /** 获取客户端库版本信息 */
    FString GetClientLibraryVersion() const;
    
    /** 检查客户端库是否初始化成功 */
    bool IsClientLibraryInitialized() const { return bClientLibraryInitialized; }

private:
    /** 客户端库是否初始化成功 */
    bool bClientLibraryInitialized = false;
    
    /** 初始化客户端库 */
    void InitializeClientLibrary();
    
    /** 清理客户端库 */
    void CleanupClientLibrary();
};
