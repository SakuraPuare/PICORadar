#include "PICORadar.h"

#define LOCTEXT_NAMESPACE "FPICORadarModule"

void FPICORadarModule::StartupModule()
{
	// 模块启动时的初始化代码
	UE_LOG(LogTemp, Warning, TEXT("PICORadar Plugin Started"));
}

void FPICORadarModule::ShutdownModule()
{
	// 模块关闭时的清理代码
	UE_LOG(LogTemp, Warning, TEXT("PICORadar Plugin Shutdown"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FPICORadarModule, PICORadar)
