#!/bin/bash

# PICORadar UE 集成构建脚本
# 专门为虚幻引擎集成优化的构建配置

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# UE 相关配置
UE_VERSIONS=("4.27" "5.0" "5.1" "5.2" "5.3" "5.4")
UE_PLATFORMS=("Win64" "Linux")

log_info() {
    echo -e "\033[0;34m[INFO]\033[0m $1"
}

log_success() {
    echo -e "\033[0;32m[SUCCESS]\033[0m $1"
}

log_error() {
    echo -e "\033[0;31m[ERROR]\033[0m $1"
}

# 为 UE 构建优化的库
build_for_ue() {
    local platform=$1
    local ue_version=$2
    
    log_info "为 UE $ue_version ($platform) 构建 PICORadar..."
    
    local output_dir="$PROJECT_ROOT/ue-integration/UE$ue_version/$platform"
    mkdir -p "$output_dir"/{lib,include,config,examples}
    
    # 根据平台选择配置
    local cmake_preset=""
    case "$platform" in
        "Win64")
            cmake_preset="windows-x64-ue"
            ;;
        "Linux")
            cmake_preset="linux-x64-ue"
            ;;
        *)
            log_error "不支持的平台: $platform"
            return 1
            ;;
    esac
    
    # 构建项目
    cd "$PROJECT_ROOT"
    
    # 清理
    rm -rf build
    
    # 配置 (如果预设不存在，使用基础预设)
    if cmake --list-presets | grep -q "$cmake_preset"; then
        cmake --preset "$cmake_preset"
    else
        case "$platform" in
            "Win64")
                cmake --preset "windows-x64"
                ;;
            "Linux")
                cmake --preset "linux-x64"
                ;;
        esac
    fi
    
    # 构建
    cmake --build build --config Release
    
    # 复制文件
    copy_ue_files "$platform" "$output_dir"
    
    # 生成 UE 集成文件
    generate_ue_files "$platform" "$ue_version" "$output_dir"
    
    log_success "UE $ue_version ($platform) 构建完成"
}

# 复制 UE 需要的文件
copy_ue_files() {
    local platform=$1
    local output_dir=$2
    
    # 复制库文件
    if [ "$platform" = "Win64" ]; then
        find build -name "*.lib" -exec cp {} "$output_dir/lib/" \;
        find build -name "*.dll" -exec cp {} "$output_dir/lib/" \;
        find build -name "*.exe" -exec cp {} "$output_dir/lib/" \;
    else
        find build -name "*.a" -exec cp {} "$output_dir/lib/" \;
        find build -name "*.so" -exec cp {} "$output_dir/lib/" \;
        find build -executable -type f -not -path "*/CMakeFiles/*" -exec cp {} "$output_dir/lib/" \;
    fi
    
    # 复制头文件
    find src -name "*.h" -exec cp --parents {} "$output_dir/include/" \;
    find src -name "*.hpp" -exec cp --parents {} "$output_dir/include/" \;
    
    # 复制生成的 protobuf 头文件
    find build -name "*.pb.h" -exec cp {} "$output_dir/include/" \;
    
    # 复制配置文件
    cp -r config/* "$output_dir/config/"
    
    # 复制示例
    cp examples/*.cpp "$output_dir/examples/"
}

# 生成 UE 集成文件
generate_ue_files() {
    local platform=$1
    local ue_version=$2
    local output_dir=$3
    
    # 生成 .Build.cs 文件
    cat > "$output_dir/PICORadar.Build.cs" << EOF
using UnrealBuildTool;
using System.IO;

public class PICORadar : ModuleRules
{
    public PICORadar(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;
        
        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "include"));
        
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "lib", "picoradar.lib"));
            PublicDelayLoadDLLs.Add("picoradar.dll");
            RuntimeDependencies.Add(Path.Combine(ModuleDirectory, "lib", "picoradar.dll"));
        }
        else if (Target.Platform == UnrealTargetPlatform.Linux)
        {
            PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "lib", "libpicoradar.a"));
        }
        
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine"
        });
        
        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Sockets",
            "Networking"
        });
    }
}
EOF

    # 生成集成示例
    cat > "$output_dir/examples/UE_Integration_Example.cpp" << EOF
// PICORadar UE Integration Example for UE $ue_version
#include "Engine/Engine.h"
#include "PICORadarClient.h" // 假设的客户端头文件

UCLASS()
class YOURGAME_API APICORadarActor : public AActor
{
    GENERATED_BODY()

public:
    APICORadarActor();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    virtual void Tick(float DeltaTime) override;

    UFUNCTION(BlueprintCallable)
    void ConnectToRadar(const FString& ServerAddress, int32 Port);

    UFUNCTION(BlueprintCallable)
    void DisconnectFromRadar();

private:
    // PICORadar 客户端实例
    // std::unique_ptr<PICORadarClient> RadarClient;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PICORadar", meta = (AllowPrivateAccess = "true"))
    bool bIsConnected = false;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICORadar", meta = (AllowPrivateAccess = "true"))
    FString DefaultServerAddress = TEXT("127.0.0.1");
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PICORadar", meta = (AllowPrivateAccess = "true"))
    int32 DefaultPort = 8080;
};

// 实现示例 (需要根据实际 API 调整)
APICORadarActor::APICORadarActor()
{
    PrimaryActorTick.bCanEverTick = true;
}

void APICORadarActor::BeginPlay()
{
    Super::BeginPlay();
    
    // 自动连接
    ConnectToRadar(DefaultServerAddress, DefaultPort);
}

void APICORadarActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    DisconnectFromRadar();
    Super::EndPlay(EndPlayReason);
}

void APICORadarActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    // 处理雷达数据更新
    if (bIsConnected)
    {
        // 更新雷达数据的逻辑
    }
}

void APICORadarActor::ConnectToRadar(const FString& ServerAddress, int32 Port)
{
    // 连接到 PICORadar 服务器的实现
    UE_LOG(LogTemp, Log, TEXT("Connecting to PICORadar at %s:%d"), *ServerAddress, Port);
    
    // 实际连接逻辑
    // RadarClient = std::make_unique<PICORadarClient>();
    // bool Success = RadarClient->Connect(TCHAR_TO_UTF8(*ServerAddress), Port);
    
    bIsConnected = true; // 临时设置，实际应该基于连接结果
    
    if (bIsConnected)
    {
        UE_LOG(LogTemp, Log, TEXT("Successfully connected to PICORadar"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to connect to PICORadar"));
    }
}

void APICORadarActor::DisconnectFromRadar()
{
    if (bIsConnected)
    {
        UE_LOG(LogTemp, Log, TEXT("Disconnecting from PICORadar"));
        
        // 断开连接的逻辑
        // if (RadarClient)
        // {
        //     RadarClient->Disconnect();
        //     RadarClient.reset();
        // }
        
        bIsConnected = false;
    }
}
EOF

    # 生成安装说明
    cat > "$output_dir/README.md" << EOF
# PICORadar UE $ue_version ($platform) 集成包

## 安装步骤

1. **复制文件到 UE 项目**
   \`\`\`
   YourProject/
   ├── Source/
   ├── ThirdParty/
   │   └── PICORadar/
   │       ├── lib/           # 库文件
   │       ├── include/       # 头文件
   │       ├── config/        # 配置文件
   │       └── PICORadar.Build.cs
   \`\`\`

2. **修改项目的 .Build.cs 文件**
   \`\`\`csharp
   PublicDependencyModuleNames.AddRange(new string[]
   {
       "Core",
       "CoreUObject", 
       "Engine",
       "PICORadar"  // 添加这一行
   });
   \`\`\`

3. **在代码中使用**
   参考 \`examples/UE_Integration_Example.cpp\` 中的示例代码

## 配置说明

- **服务器地址**: 默认 127.0.0.1
- **端口**: 默认 8080
- **配置文件**: 位于 config/ 目录

## 平台特定说明

### Windows (Win64)
- 包含 .lib 和 .dll 文件
- DLL 需要与游戏可执行文件在同一目录

### Linux
- 包含静态库 (.a) 文件
- 无需额外的运行时依赖

## 构建信息
- 构建时间: $(date)
- UE 版本: $ue_version
- 平台: $platform
- PICORadar 版本: $(git describe --tags --always 2>/dev/null || echo "unknown")

## 故障排除

1. **链接错误**: 确保 PICORadar.Build.cs 中的路径正确
2. **运行时错误**: 检查配置文件和网络连接
3. **性能问题**: 调整雷达更新频率和数据处理策略

## 支持

如有问题，请参考项目主页或提交 Issue。
EOF
}

# 主函数
main() {
    local target_ue_version="${1:-all}"
    local target_platform="${2:-all}"
    
    log_info "开始为 UE 构建 PICORadar..."
    
    # 创建输出目录
    mkdir -p "$PROJECT_ROOT/ue-integration"
    
    # 确定要构建的版本和平台
    local versions_to_build=()
    local platforms_to_build=()
    
    if [ "$target_ue_version" = "all" ]; then
        versions_to_build=("${UE_VERSIONS[@]}")
    else
        versions_to_build=("$target_ue_version")
    fi
    
    if [ "$target_platform" = "all" ]; then
        platforms_to_build=("${UE_PLATFORMS[@]}")
    else
        platforms_to_build=("$target_platform")
    fi
    
    # 构建所有组合
    for ue_version in "${versions_to_build[@]}"; do
        for platform in "${platforms_to_build[@]}"; do
            build_for_ue "$platform" "$ue_version"
        done
    done
    
    # 创建总体文档
    cat > "$PROJECT_ROOT/ue-integration/README.md" << EOF
# PICORadar UE 集成包

本目录包含针对不同 UE 版本和平台的 PICORadar 集成包。

## 可用版本

$(for version in "${UE_VERSIONS[@]}"; do
    echo "- UE $version"
    for platform in "${UE_PLATFORMS[@]}"; do
        if [ -d "$PROJECT_ROOT/ue-integration/UE$version/$platform" ]; then
            echo "  - $platform ✓"
        else
            echo "  - $platform ✗"
        fi
    done
done)

## 使用说明

1. 选择对应的 UE 版本和平台目录
2. 按照该目录中的 README.md 进行集成
3. 参考示例代码进行开发

## 构建时间
$(date)
EOF
    
    log_success "UE 集成包构建完成！"
    log_info "输出目录: $PROJECT_ROOT/ue-integration"
}

# 显示帮助
show_help() {
    cat << EOF
用法: $0 [UE_VERSION] [PLATFORM]

UE_VERSION:
    4.27, 5.0, 5.1, 5.2, 5.3, 5.4, all (默认)

PLATFORM:
    Win64, Linux, all (默认)

示例:
    $0                    # 构建所有版本和平台
    $0 5.3               # 为 UE 5.3 构建所有平台
    $0 5.3 Win64         # 仅为 UE 5.3 Win64 构建
    $0 all Linux         # 为所有 UE 版本构建 Linux 版本

输出:
    ue-integration/UE{版本}/{平台}/
EOF
}

case "${1:-}" in
    "-h"|"--help"|"help")
        show_help
        exit 0
        ;;
    *)
        main "$@"
        ;;
esac
