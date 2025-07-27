#!/bin/bash

# ==============================================================================
# PICORadar 性能测试和报告生成脚本
# ==============================================================================

set -e

# 配置变量
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"
BENCHMARK_DIR="${BUILD_DIR}/benchmark"
PERFORMANCE_REPORTS_DIR="${PROJECT_ROOT}/performance_reports"
TIMESTAMP=$(date "+%Y%m%d_%H%M%S")
REPORT_DIR="${PERFORMANCE_REPORTS_DIR}/${TIMESTAMP}"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 打印函数
print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_header() {
    echo
    echo "========================================"
    echo "$1"
    echo "========================================"
}

# 检查依赖
check_dependencies() {
    print_info "检查依赖..."
    
    if [ ! -d "$BUILD_DIR" ]; then
        print_error "构建目录不存在: $BUILD_DIR"
        exit 1
    fi
    
    if [ ! -d "$BENCHMARK_DIR" ]; then
        print_error "基准测试目录不存在: $BENCHMARK_DIR"
        exit 1
    fi
    
    # 检查基准测试可执行文件
    local missing_benchmarks=()
    local benchmarks=(
        "simple_benchmark"
        "benchmark_player_registry"
        "benchmark_config_manager"
        "benchmark_protobuf_serialization"
        "benchmark_json_serialization"
        "benchmark_network_simple"
    )
    
    for benchmark in "${benchmarks[@]}"; do
        if [ ! -x "$BENCHMARK_DIR/$benchmark" ]; then
            missing_benchmarks+=("$benchmark")
        fi
    done
    
    if [ ${#missing_benchmarks[@]} -gt 0 ]; then
        print_warning "以下基准测试未找到或不可执行:"
        for benchmark in "${missing_benchmarks[@]}"; do
            echo "  - $benchmark"
        done
        print_info "请先编译基准测试: cmake --build build --target performance_tests"
    fi
}

# 创建报告目录
create_report_directory() {
    print_info "创建报告目录: $REPORT_DIR"
    
    mkdir -p "$REPORT_DIR"
    mkdir -p "$REPORT_DIR/raw_data"
    mkdir -p "$REPORT_DIR/processed"
    mkdir -p "$REPORT_DIR/charts"
    
    print_success "报告目录创建完成"
}

# 收集系统信息
collect_system_info() {
    print_info "收集系统信息..."
    
    local system_info_file="$REPORT_DIR/system_info.txt"
    
    cat > "$system_info_file" << EOF
# PICORadar 性能测试系统信息
# 生成时间: $(date)

## 硬件信息
CPU 信息: $(lscpu | grep "Model name" | sed 's/Model name:[[:space:]]*//')
CPU 核心数: $(nproc)
CPU 频率: $(lscpu | grep "CPU MHz" | sed 's/CPU MHz:[[:space:]]*//')
内存信息: $(free -h | grep "Mem:" | awk '{print $2 " 总内存, " $3 " 已使用, " $7 " 可用"}')

## 系统信息
操作系统: $(lsb_release -d | sed 's/Description:[[:space:]]*//')
内核版本: $(uname -r)
架构: $(uname -m)

## 编译器信息
GCC 版本: $(gcc --version | head -n1)
CMake 版本: $(cmake --version | head -n1)

## 构建信息
项目根目录: $PROJECT_ROOT
构建目录: $BUILD_DIR
构建模式: $(if [ -f "$BUILD_DIR/CMakeCache.txt" ]; then grep CMAKE_BUILD_TYPE "$BUILD_DIR/CMakeCache.txt" | cut -d= -f2; else echo "未知"; fi)

## 依赖库版本
$(find "$BUILD_DIR/vcpkg_installed" -name "*benchmark*" -type d 2>/dev/null | head -5 || echo "Google Benchmark: 未检测到版本信息")

## 测试时间
开始时间: $(date)
EOF

    print_success "系统信息收集完成"
}

# 运行基准测试
run_benchmark() {
    local benchmark_name="$1"
    local benchmark_path="$BENCHMARK_DIR/$benchmark_name"
    
    if [ ! -x "$benchmark_path" ]; then
        print_warning "跳过 $benchmark_name (文件不存在或不可执行)"
        return
    fi
    
    print_info "运行基准测试: $benchmark_name"
    
    local output_json="$REPORT_DIR/raw_data/${benchmark_name}.json"
    local output_txt="$REPORT_DIR/raw_data/${benchmark_name}.txt"
    
    # 运行基准测试，同时输出 JSON 和控制台格式
    cd "$BENCHMARK_DIR"
    
    # JSON 格式输出
    "./$benchmark_name" --benchmark_format=json --benchmark_out="$output_json" 2>/dev/null || {
        print_warning "$benchmark_name JSON 输出失败"
    }
    
    # 控制台格式输出
    "./$benchmark_name" > "$output_txt" 2>&1 || {
        print_warning "$benchmark_name 控制台输出失败"
    }
    
    if [ -f "$output_json" ]; then
        print_success "$benchmark_name 完成"
    else
        print_error "$benchmark_name 失败"
    fi
}

# 运行所有基准测试
run_all_benchmarks() {
    print_header "运行性能基准测试"
    
    local benchmarks=(
        "simple_benchmark"
        "benchmark_player_registry" 
        "benchmark_config_manager"
        "benchmark_protobuf_serialization"
        "benchmark_json_serialization"
        "benchmark_network_simple"
    )
    
    for benchmark in "${benchmarks[@]}"; do
        run_benchmark "$benchmark"
    done
    
    print_success "所有基准测试完成"
}

# 生成性能摘要报告
generate_summary_report() {
    print_info "生成性能摘要报告..."
    
    local summary_file="$REPORT_DIR/processed/performance_summary.md"
    
    cat > "$summary_file" << EOF
# PICORadar 性能测试报告
*生成时间: $(date)*

## 系统环境
- **CPU**: $(lscpu | grep "Model name" | sed 's/Model name:[[:space:]]*//')
- **核心数**: $(nproc)
- **内存**: $(free -h | grep "Mem:" | awk '{print $2}')
- **操作系统**: $(lsb_release -d | sed 's/Description:[[:space:]]*//')
- **构建模式**: $(if [ -f "$BUILD_DIR/CMakeCache.txt" ]; then grep CMAKE_BUILD_TYPE "$BUILD_DIR/CMakeCache.txt" | cut -d= -f2; else echo "未知"; fi)

## 测试概览

### 已执行的基准测试
EOF

    # 统计已执行的测试
    local test_count=0
    for json_file in "$REPORT_DIR/raw_data"/*.json; do
        if [ -f "$json_file" ]; then
            local benchmark_name=$(basename "$json_file" .json)
            echo "- ✅ $benchmark_name" >> "$summary_file"
            ((test_count++))
        fi
    done
    
    cat >> "$summary_file" << EOF

**总计**: $test_count 个性能测试套件

## 关键性能指标

### 1. 核心组件性能
EOF

    # 尝试从 JSON 结果中提取关键指标
    if [ -f "$REPORT_DIR/raw_data/simple_benchmark.json" ]; then
        echo "- **简化基准测试**: 已完成" >> "$summary_file"
    fi
    
    if [ -f "$REPORT_DIR/raw_data/benchmark_player_registry.json" ]; then
        echo "- **PlayerRegistry**: 已完成" >> "$summary_file"
    fi
    
    if [ -f "$REPORT_DIR/raw_data/benchmark_protobuf_serialization.json" ]; then
        echo "- **Protobuf 序列化**: 已完成" >> "$summary_file"
    fi
    
    cat >> "$summary_file" << EOF

### 2. 序列化性能
- **Protobuf**: $([ -f "$REPORT_DIR/raw_data/benchmark_protobuf_serialization.json" ] && echo "已测试" || echo "未测试")
- **JSON**: $([ -f "$REPORT_DIR/raw_data/benchmark_json_serialization.json" ] && echo "已测试" || echo "未测试")

### 3. 网络性能
- **网络层**: $([ -f "$REPORT_DIR/raw_data/benchmark_network_simple.json" ] && echo "已测试" || echo "未测试")

## 详细结果

详细的性能数据请查看以下文件：
- **原始数据**: \`raw_data/\` 目录下的 JSON 和 TXT 文件
- **系统信息**: \`system_info.txt\`

## 性能建议

基于测试结果，我们建议：
1. 定期运行性能测试以监控性能回归
2. 在生产环境中使用 Release 构建模式
3. 根据实际负载调整配置参数

---
*报告生成时间: $(date)*
*PICORadar 性能测试框架 v1.0*
EOF

    print_success "性能摘要报告生成完成"
}

# 生成 HTML 仪表板
generate_html_dashboard() {
    print_info "生成 HTML 性能仪表板..."
    
    local dashboard_file="$REPORT_DIR/dashboard.html"
    
    cat > "$dashboard_file" << 'EOF'
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>PICORadar 性能测试仪表板</title>
    <style>
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            margin: 0;
            padding: 20px;
            background-color: #f5f5f5;
        }
        
        .header {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 30px;
            border-radius: 10px;
            text-align: center;
            margin-bottom: 30px;
        }
        
        .header h1 {
            margin: 0;
            font-size: 2.5em;
        }
        
        .header p {
            margin: 10px 0 0 0;
            opacity: 0.9;
        }
        
        .dashboard-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
            gap: 20px;
            margin-bottom: 30px;
        }
        
        .card {
            background: white;
            padding: 20px;
            border-radius: 10px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
        }
        
        .card h3 {
            margin: 0 0 15px 0;
            color: #333;
            border-bottom: 2px solid #667eea;
            padding-bottom: 10px;
        }
        
        .metric {
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 10px 0;
            border-bottom: 1px solid #eee;
        }
        
        .metric:last-child {
            border-bottom: none;
        }
        
        .metric-name {
            font-weight: 500;
            color: #555;
        }
        
        .metric-value {
            font-weight: bold;
            color: #667eea;
        }
        
        .status-badge {
            padding: 4px 12px;
            border-radius: 20px;
            font-size: 0.8em;
            font-weight: bold;
        }
        
        .status-success {
            background-color: #d4edda;
            color: #155724;
        }
        
        .status-warning {
            background-color: #fff3cd;
            color: #856404;
        }
        
        .files-section {
            background: white;
            padding: 20px;
            border-radius: 10px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
        }
        
        .file-list {
            list-style: none;
            padding: 0;
        }
        
        .file-list li {
            padding: 10px;
            margin: 5px 0;
            background: #f8f9fa;
            border-radius: 5px;
            border-left: 4px solid #667eea;
        }
        
        .file-list li a {
            text-decoration: none;
            color: #333;
            font-weight: 500;
        }
        
        .file-list li a:hover {
            color: #667eea;
        }
        
        .footer {
            text-align: center;
            margin-top: 40px;
            color: #666;
            font-size: 0.9em;
        }
    </style>
</head>
<body>
    <div class="header">
        <h1>🚀 PICORadar 性能测试仪表板</h1>
        <p>生成时间: NEW_TIMESTAMP_PLACEHOLDER</p>
    </div>

    <div class="dashboard-grid">
        <div class="card">
            <h3>📊 测试概览</h3>
            <div class="metric">
                <span class="metric-name">执行的测试套件</span>
                <span class="metric-value">TEST_COUNT_PLACEHOLDER</span>
            </div>
            <div class="metric">
                <span class="metric-name">系统核心数</span>
                <span class="metric-value">CPU_CORES_PLACEHOLDER</span>
            </div>
            <div class="metric">
                <span class="metric-name">测试状态</span>
                <span class="status-badge status-success">完成</span>
            </div>
        </div>

        <div class="card">
            <h3>🔧 系统信息</h3>
            <div class="metric">
                <span class="metric-name">操作系统</span>
                <span class="metric-value">OS_INFO_PLACEHOLDER</span>
            </div>
            <div class="metric">
                <span class="metric-name">CPU</span>
                <span class="metric-value">CPU_INFO_PLACEHOLDER</span>
            </div>
            <div class="metric">
                <span class="metric-name">内存</span>
                <span class="metric-value">MEMORY_INFO_PLACEHOLDER</span>
            </div>
        </div>

        <div class="card">
            <h3>⚡ 核心性能</h3>
            <div class="metric">
                <span class="metric-name">PlayerRegistry</span>
                <span class="status-badge PLAYER_REGISTRY_STATUS_PLACEHOLDER">PLAYER_REGISTRY_TEXT_PLACEHOLDER</span>
            </div>
            <div class="metric">
                <span class="metric-name">Protobuf 序列化</span>
                <span class="status-badge PROTOBUF_STATUS_PLACEHOLDER">PROTOBUF_TEXT_PLACEHOLDER</span>
            </div>
            <div class="metric">
                <span class="metric-name">网络层</span>
                <span class="status-badge NETWORK_STATUS_PLACEHOLDER">NETWORK_TEXT_PLACEHOLDER</span>
            </div>
        </div>

        <div class="card">
            <h3>📈 性能指标</h3>
            <div class="metric">
                <span class="metric-name">测试持续时间</span>
                <span class="metric-value">TEST_DURATION_PLACEHOLDER</span>
            </div>
            <div class="metric">
                <span class="metric-name">总数据量</span>
                <span class="metric-value">估算中...</span>
            </div>
            <div class="metric">
                <span class="metric-name">构建模式</span>
                <span class="metric-value">BUILD_MODE_PLACEHOLDER</span>
            </div>
        </div>
    </div>

    <div class="files-section">
        <h3>📁 测试结果文件</h3>
        <ul class="file-list">
            <li><a href="processed/performance_summary.md">📋 性能摘要报告</a></li>
            <li><a href="system_info.txt">🖥️ 系统信息</a></li>
            <li><a href="raw_data/">📊 原始数据 (JSON/TXT)</a></li>
        </ul>
    </div>

    <div class="footer">
        <p>PICORadar 性能测试框架 | 自动生成于 NEW_TIMESTAMP_PLACEHOLDER</p>
    </div>

    <script>
        // 自动刷新时间显示
        function updateTimestamp() {
            const now = new Date();
            const timestamp = now.toLocaleString('zh-CN');
            document.querySelector('.header p').textContent = '当前时间: ' + timestamp;
        }
        
        // 每分钟更新一次时间
        setInterval(updateTimestamp, 60000);
    </script>
</body>
</html>
EOF

    # 替换占位符
    sed -i "s/NEW_TIMESTAMP_PLACEHOLDER/$(date)/g" "$dashboard_file"
    sed -i "s/CPU_CORES_PLACEHOLDER/$(nproc)/g" "$dashboard_file"
    sed -i "s/OS_INFO_PLACEHOLDER/$(lsb_release -d | sed 's/Description:[[:space:]]*//' | cut -c1-30)/g" "$dashboard_file"
    sed -i "s/CPU_INFO_PLACEHOLDER/$(lscpu | grep "Model name" | sed 's/Model name:[[:space:]]*//' | cut -c1-30)/g" "$dashboard_file"
    sed -i "s/MEMORY_INFO_PLACEHOLDER/$(free -h | grep "Mem:" | awk '{print $2}')/g" "$dashboard_file"
    
    # 计算测试文件数量
    local test_count=$(find "$REPORT_DIR/raw_data" -name "*.json" 2>/dev/null | wc -l)
    sed -i "s/TEST_COUNT_PLACEHOLDER/$test_count/g" "$dashboard_file"
    
    # 设置状态标记
    local player_registry_status="status-warning"
    local player_registry_text="未测试"
    if [ -f "$REPORT_DIR/raw_data/benchmark_player_registry.json" ]; then
        player_registry_status="status-success"
        player_registry_text="已完成"
    fi
    
    local protobuf_status="status-warning"
    local protobuf_text="未测试"
    if [ -f "$REPORT_DIR/raw_data/benchmark_protobuf_serialization.json" ]; then
        protobuf_status="status-success"
        protobuf_text="已完成"
    fi
    
    local network_status="status-warning"
    local network_text="未测试"
    if [ -f "$REPORT_DIR/raw_data/benchmark_network_simple.json" ]; then
        network_status="status-success"
        network_text="已完成"
    fi
    
    sed -i "s/PLAYER_REGISTRY_STATUS_PLACEHOLDER/$player_registry_status/g" "$dashboard_file"
    sed -i "s/PLAYER_REGISTRY_TEXT_PLACEHOLDER/$player_registry_text/g" "$dashboard_file"
    sed -i "s/PROTOBUF_STATUS_PLACEHOLDER/$protobuf_status/g" "$dashboard_file"
    sed -i "s/PROTOBUF_TEXT_PLACEHOLDER/$protobuf_text/g" "$dashboard_file"
    sed -i "s/NETWORK_STATUS_PLACEHOLDER/$network_status/g" "$dashboard_file"
    sed -i "s/NETWORK_TEXT_PLACEHOLDER/$network_text/g" "$dashboard_file"
    
    # 构建模式
    local build_mode="未知"
    if [ -f "$BUILD_DIR/CMakeCache.txt" ]; then
        build_mode=$(grep CMAKE_BUILD_TYPE "$BUILD_DIR/CMakeCache.txt" | cut -d= -f2)
    fi
    sed -i "s/BUILD_MODE_PLACEHOLDER/$build_mode/g" "$dashboard_file"
    
    # 测试持续时间 (估算)
    local test_duration="估算中..."
    sed -i "s/TEST_DURATION_PLACEHOLDER/$test_duration/g" "$dashboard_file"
    
    print_success "HTML 仪表板生成完成"
}

# 显示报告信息
show_report_info() {
    print_header "性能测试完成"
    
    print_success "报告已生成到: $REPORT_DIR"
    echo
    echo "📁 报告内容："
    echo "  ├── 📊 dashboard.html              (交互式性能仪表板)"
    echo "  ├── 📋 processed/"
    echo "  │   └── performance_summary.md    (性能摘要报告)"
    echo "  ├── 📈 raw_data/"
    echo "  │   ├── *.json                    (机器可读数据)"
    echo "  │   └── *.txt                     (人类可读输出)"
    echo "  └── 🖥️ system_info.txt            (系统配置信息)"
    echo
    echo "🌐 查看报告："
    echo "  浏览器打开: file://$REPORT_DIR/dashboard.html"
    echo "  查看摘要: cat $REPORT_DIR/processed/performance_summary.md"
    echo
    print_info "提示: 使用 'python3 -m http.server' 在报告目录中启动本地服务器以获得更好的浏览体验"
}

# 清理旧报告（可选）
cleanup_old_reports() {
    if [ "$1" = "--cleanup" ]; then
        print_info "清理 7 天前的旧报告..."
        find "$PERFORMANCE_REPORTS_DIR" -maxdepth 1 -type d -name "20*" -mtime +7 -exec rm -rf {} \; 2>/dev/null || true
        print_success "旧报告清理完成"
    fi
}

# 主函数
main() {
    print_header "PICORadar 性能测试和报告生成"
    
    # 解析命令行参数
    while [[ $# -gt 0 ]]; do
        case $1 in
            --cleanup)
                cleanup_old_reports "$1"
                shift
                ;;
            --help|-h)
                echo "用法: $0 [选项]"
                echo "选项:"
                echo "  --cleanup    清理 7 天前的旧报告"
                echo "  --help/-h    显示此帮助信息"
                exit 0
                ;;
            *)
                print_warning "未知选项: $1"
                shift
                ;;
        esac
    done
    
    # 记录开始时间
    local start_time=$(date +%s)
    
    # 执行测试流程
    check_dependencies
    create_report_directory
    collect_system_info
    run_all_benchmarks
    generate_summary_report
    generate_html_dashboard
    
    # 计算执行时间
    local end_time=$(date +%s)
    local duration=$((end_time - start_time))
    
    # 更新测试持续时间
    if [ -f "$REPORT_DIR/dashboard.html" ]; then
        sed -i "s/估算中.../${duration}秒/g" "$REPORT_DIR/dashboard.html"
    fi
    
    show_report_info
    
    print_success "性能测试完成！总耗时: ${duration}秒"
}

# 运行主函数
main "$@"
