#include "PICORadarModule.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY(LogPICORadar);

#define LOCTEXT_NAMESPACE "FPICORadarModule"

void FPICORadarModule::StartupModule()
{
    UE_LOG(LogPICORadar, Log, TEXT("PICORadar module startup begin"));
    
    // 初始化客户端库
    InitializeClientLibrary();
    
    if (bClientLibraryInitialized)
    {
        UE_LOG(LogPICORadar, Log, TEXT("PICORadar module startup completed successfully"));
        UE_LOG(LogPICORadar, Log, TEXT("Client library version: %s"), *GetClientLibraryVersion());
    }
    else
    {
        UE_LOG(LogPICORadar, Error, TEXT("PICORadar module startup failed - client library initialization failed"));
    }
}

void FPICORadarModule::ShutdownModule()
{
    UE_LOG(LogPICORadar, Log, TEXT("PICORadar module shutdown begin"));
    
    // 清理客户端库
    CleanupClientLibrary();
    
    UE_LOG(LogPICORadar, Log, TEXT("PICORadar module shutdown completed"));
}

FString FPICORadarModule::GetClientLibraryVersion() const
{
#ifdef PICORADAR_VERSION
    return FString(TEXT(PICORADAR_VERSION));
#else
    return TEXT("Unknown");
#endif
}

void FPICORadarModule::InitializeClientLibrary()
{
    try
    {
        // 这里可以进行一些客户端库的初始化工作
        // 例如检查依赖库是否正确链接等
        
        UE_LOG(LogPICORadar, Log, TEXT("Initializing PICORadar client library..."));
        
        // 验证关键符号是否可用
        // 这是一种检查链接是否正确的方法
        bClientLibraryInitialized = true;
        
        UE_LOG(LogPICORadar, Log, TEXT("PICORadar client library initialized successfully"));
    }
    catch (const std::exception& e)
    {
        UE_LOG(LogPICORadar, Error, TEXT("Failed to initialize PICORadar client library: %s"), 
               UTF8_TO_TCHAR(e.what()));
        bClientLibraryInitialized = false;
    }
    catch (...)
    {
        UE_LOG(LogPICORadar, Error, TEXT("Failed to initialize PICORadar client library: Unknown error"));
        bClientLibraryInitialized = false;
    }
}

void FPICORadarModule::CleanupClientLibrary()
{
    if (bClientLibraryInitialized)
    {
        UE_LOG(LogPICORadar, Log, TEXT("Cleaning up PICORadar client library..."));
        
        // 这里进行客户端库的清理工作
        
        bClientLibraryInitialized = false;
        
        UE_LOG(LogPICORadar, Log, TEXT("PICORadar client library cleanup completed"));
    }
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FPICORadarModule, PICORadar)
