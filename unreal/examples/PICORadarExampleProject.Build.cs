// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PICORadarExampleProject : ModuleRules
{
    public PICORadarExampleProject(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] 
        { 
            "Core", 
            "CoreUObject", 
            "Engine", 
            "InputCore",
            "PICORadar"  // 添加PICORadar插件依赖
        });

        PrivateDependencyModuleNames.AddRange(new string[] 
        { 
            "Slate", 
            "SlateCore",
            "EnhancedInput",
            "HeadMountedDisplay",  // VR支持
            "XRBase"               // XR基础功能
        });

        // 启用C++20标准（与PICORadar保持一致）
        CppStandard = CppStandardVersion.Cpp20;
        
        // 优化设置
        OptimizeCode = CodeOptimization.InShippingBuildsOnly;
        
        // 如果是开发构建，启用调试信息
        if (Target.Configuration == UnrealTargetConfiguration.Development || 
            Target.Configuration == UnrealTargetConfiguration.Debug)
        {
            PublicDefinitions.Add("PICORADAR_DEBUG=1");
        }
    }
}
