using UnrealBuildTool;
using System.IO;
using System;

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
				"InputCore",
				"HeadMountedDisplay" // VR支持
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Slate",
				"SlateCore",
				"ToolMenus",
				"Networking",
				"Sockets",
				"Json",
				"JsonUtilities",
				"HTTP",
				"Projects" // 用于获取项目路径
			}
		);

		// 仅在编辑器中添加编辑器相关依赖
		if (Target.Type == TargetType.Editor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"UnrealEd",
					"EditorStyle",
					"EditorWidgets"
				}
			);
		}

		// 动态获取PICORadar项目路径
		string PluginRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "../../.."));
		string PICORadarProjectRoot = GetPICORadarProjectRoot(PluginRoot);
		string BuildDir = Path.Combine(PICORadarProjectRoot, "build");
		string SrcDir = Path.Combine(PICORadarProjectRoot, "src");
		
		// 添加包含路径
		PublicIncludePaths.AddRange(new string[] {
			Path.Combine(SrcDir, "client", "include"),
			Path.Combine(SrcDir, "common", "include"),
			BuildDir // 包含生成的 protobuf 头文件
		});

		// 跨平台库文件配置
		AddPlatformSpecificLibraries(Target, BuildDir);
		
		// 启用C++20支持（匹配UE5.6）
		CppStandard = CppStandardVersion.Cpp20;
		
		// 定义宏
		PublicDefinitions.AddRange(new string[] {
			"WITH_PICORADAR_CLIENT=1",
			string.Format("PICORADAR_VERSION=\"{0}\"", "1.0.0")
		});
		
		// 优化设置
		OptimizeCode = CodeOptimization.InShippingBuildsOnly;
	}

	// 动态获取PICORadar项目根目录
	private string GetPICORadarProjectRoot(string PluginRoot)
	{
		// 方法1：通过环境变量
		string envPath = Environment.GetEnvironmentVariable("PICORADAR_ROOT");
		if (!string.IsNullOrEmpty(envPath) && Directory.Exists(envPath))
		{
			return envPath;
		}

		// 方法2：相对于插件目录查找
		string candidatePath = Path.GetFullPath(Path.Combine(PluginRoot, ".."));
		if (Directory.Exists(Path.Combine(candidatePath, "src")) && 
			File.Exists(Path.Combine(candidatePath, "CMakeLists.txt")))
		{
			return candidatePath;
		}

		// 方法3：默认路径
		string defaultPath = "/home/sakurapuare/Desktop/PICORadar";
		if (Directory.Exists(defaultPath))
		{
			return defaultPath;
		}

		throw new BuildException("Could not find PICORadar project root. Please set PICORADAR_ROOT environment variable.");
	}

	// 跨平台库配置
	private void AddPlatformSpecificLibraries(ReadOnlyTargetRules Target, string BuildDir)
	{
		if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			PublicAdditionalLibraries.AddRange(new string[] {
				Path.Combine(BuildDir, "src", "client", "libclient_lib.a"),
				Path.Combine(BuildDir, "libproto_gen.a")
			});
			
			PublicSystemLibraries.AddRange(new string[] {
				"pthread",
				"ssl",
				"crypto",
				"protobuf"
			});
		}
		else if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicAdditionalLibraries.AddRange(new string[] {
				Path.Combine(BuildDir, "src", "client", "Release", "client_lib.lib"),
				Path.Combine(BuildDir, "Release", "proto_gen.lib")
			});
			
			PublicSystemLibraries.AddRange(new string[] {
				"ws2_32.lib",
				"crypt32.lib"
			});
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicAdditionalLibraries.AddRange(new string[] {
				Path.Combine(BuildDir, "src", "client", "libclient_lib.a"),
				Path.Combine(BuildDir, "libproto_gen.a")
			});
			
			PublicFrameworks.AddRange(new string[] {
				"Security",
				"Foundation"
			});
		}
		else if (Target.Platform == UnrealTargetPlatform.Android)
		{
			// Android特定配置
			string AndroidArchPath = Path.Combine(BuildDir, "android", Target.Architecture);
			if (Directory.Exists(AndroidArchPath))
			{
				PublicAdditionalLibraries.AddRange(new string[] {
					Path.Combine(AndroidArchPath, "libclient_lib.a"),
					Path.Combine(AndroidArchPath, "libproto_gen.a")
				});
			}
		}
	}
}
