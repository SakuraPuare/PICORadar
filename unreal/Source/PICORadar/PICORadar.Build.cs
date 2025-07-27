using UnrealBuildTool;
using System.IO;

public class PICORadar : ModuleRules
{
	public PICORadar(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Slate",
				"SlateCore",
				"ToolMenus",
				"UnrealEd",
				"Networking",
				"Sockets",
				"Json",
				"JsonUtilities",
				"HTTP"
			}
		);

		// 添加第三方库路径（当我们有PICORadar C++库时）
		// 暂时注释掉，直到我们有实际的库文件
		/*
		string ThirdPartyPath = Path.Combine(ModuleDirectory, "../../ThirdParty");
		if (Directory.Exists(ThirdPartyPath))
		{
			PublicIncludePaths.Add(Path.Combine(ThirdPartyPath, "include"));
			
			// 根据平台添加库文件
			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				PublicAdditionalLibraries.Add(Path.Combine(ThirdPartyPath, "lib", "Win64", "PICORadarClient.lib"));
			}
			else if (Target.Platform == UnrealTargetPlatform.Linux)
			{
				PublicAdditionalLibraries.Add(Path.Combine(ThirdPartyPath, "lib", "Linux", "libPICORadarClient.a"));
			}
			else if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				PublicAdditionalLibraries.Add(Path.Combine(ThirdPartyPath, "lib", "Mac", "libPICORadarClient.a"));
			}
		}
		*/

		// 启用C++20支持（匹配UE5.6）
		CppStandard = CppStandardVersion.Cpp20;
	}
}
