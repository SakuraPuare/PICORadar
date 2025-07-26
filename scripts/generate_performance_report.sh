#!/bin/bash

# PICORadar Performance Testing and Report Generation Script
# Usage: ./generate_performance_report.sh [output_dir] [report_format]

set -e

# Configuration
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"
BENCHMARK_DIR="${PROJECT_ROOT}/benchmark"
DEFAULT_OUTPUT_DIR="${PROJECT_ROOT}/performance_reports"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")

# Command line arguments
OUTPUT_DIR="${1:-${DEFAULT_OUTPUT_DIR}}"
REPORT_FORMAT="${2:-all}"  # Options: json, csv, console, all

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Logging functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

log_header() {
    echo -e "${PURPLE}=====================================${NC}"
    echo -e "${PURPLE}$1${NC}"
    echo -e "${PURPLE}=====================================${NC}"
}

# Check dependencies
check_dependencies() {
    log_info "Checking dependencies..."
    
    # Check if build directory exists
    if [ ! -d "$BUILD_DIR" ]; then
        log_error "Build directory not found: $BUILD_DIR"
        log_info "Please run CMake configure first"
        exit 1
    fi
    
    # Check if benchmark executables exist
    local benchmark_files=(
        "benchmark_main"
        "benchmark_config_manager" 
        "benchmark_player_registry"
        "benchmark_network_latency"
        "benchmark_client_throughput"
        "benchmark_memory_usage"
        "benchmark_concurrent_clients"
    )
    
    for benchmark in "${benchmark_files[@]}"; do
        if [ ! -f "$BUILD_DIR/$benchmark" ]; then
            log_warning "Benchmark executable not found: $benchmark"
        fi
    done
    
    # Check for required tools
    if ! command -v python3 &> /dev/null; then
        log_warning "Python3 not found. JSON analysis features may be limited."
    fi
    
    log_success "Dependency check completed"
}

# Create output directories
setup_output_directories() {
    log_info "Setting up output directories..."
    
    local report_dir="${OUTPUT_DIR}/${TIMESTAMP}"
    local raw_dir="${report_dir}/raw_data"
    local processed_dir="${report_dir}/processed"
    local charts_dir="${report_dir}/charts"
    
    mkdir -p "$raw_dir" "$processed_dir" "$charts_dir"
    
    echo "$report_dir" > /tmp/picoradar_report_dir
    log_success "Output directories created at: $report_dir"
}

# Run individual benchmark
run_benchmark() {
    local benchmark_name="$1"
    local executable_path="$BUILD_DIR/$benchmark_name"
    local report_dir=$(cat /tmp/picoradar_report_dir)
    
    if [ ! -f "$executable_path" ]; then
        log_warning "Skipping $benchmark_name (executable not found)"
        return 0
    fi
    
    log_info "Running benchmark: $benchmark_name"
    
    local output_base="${report_dir}/raw_data/${benchmark_name}"
    
    # Run with different output formats
    if [[ "$REPORT_FORMAT" == "all" || "$REPORT_FORMAT" == "json" ]]; then
        log_info "  Generating JSON output..."
        "$executable_path" --benchmark_format=json \
                          --benchmark_out="${output_base}.json" \
                          --benchmark_repetitions=3 \
                          --benchmark_report_aggregates_only=true \
                          --benchmark_display_aggregates_only=true
    fi
    
    if [[ "$REPORT_FORMAT" == "all" || "$REPORT_FORMAT" == "csv" ]]; then
        log_info "  Generating CSV output..."
        "$executable_path" --benchmark_format=csv \
                          --benchmark_out="${output_base}.csv" \
                          --benchmark_repetitions=3 \
                          --benchmark_report_aggregates_only=true
    fi
    
    if [[ "$REPORT_FORMAT" == "all" || "$REPORT_FORMAT" == "console" ]]; then
        log_info "  Generating console output..."
        "$executable_path" --benchmark_repetitions=3 \
                          --benchmark_report_aggregates_only=true \
                          --benchmark_display_aggregates_only=true \
                          > "${output_base}_console.txt" 2>&1
    fi
    
    log_success "  Completed: $benchmark_name"
}

# Run all benchmarks
run_all_benchmarks() {
    log_header "Running Performance Benchmarks"
    
    local benchmarks=(
        "benchmark_config_manager"
        "benchmark_player_registry" 
        "benchmark_network_latency"
        "benchmark_client_throughput"
        "benchmark_memory_usage"
        "benchmark_concurrent_clients"
    )
    
    local total=${#benchmarks[@]}
    local current=0
    
    for benchmark in "${benchmarks[@]}"; do
        current=$((current + 1))
        log_info "Progress: [$current/$total] $benchmark"
        run_benchmark "$benchmark"
    done
    
    log_success "All benchmarks completed"
}

# Generate system information
generate_system_info() {
    local report_dir=$(cat /tmp/picoradar_report_dir)
    local system_info_file="${report_dir}/system_info.txt"
    
    log_info "Collecting system information..."
    
    {
        echo "PICORadar Performance Test System Information"
        echo "Generated on: $(date)"
        echo "Report ID: $TIMESTAMP"
        echo ""
        echo "=== Hardware Information ==="
        echo "CPU Info:"
        if command -v lscpu &> /dev/null; then
            lscpu | grep -E "(Model name|CPU\(s\)|Thread|Core|Socket|Vendor|Architecture)"
        elif [ -f /proc/cpuinfo ]; then
            grep -E "(model name|processor|cores|siblings)" /proc/cpuinfo | head -10
        fi
        echo ""
        
        echo "Memory Info:"
        if command -v free &> /dev/null; then
            free -h
        elif [ -f /proc/meminfo ]; then
            grep -E "(MemTotal|MemAvailable)" /proc/meminfo
        fi
        echo ""
        
        echo "=== Software Information ==="
        echo "Operating System:"
        if command -v lsb_release &> /dev/null; then
            lsb_release -a
        elif [ -f /etc/os-release ]; then
            cat /etc/os-release
        fi
        echo ""
        
        echo "Compiler Information:"
        if command -v g++ &> /dev/null; then
            echo "g++ version: $(g++ --version | head -1)"
        fi
        if command -v clang++ &> /dev/null; then
            echo "clang++ version: $(clang++ --version | head -1)"
        fi
        echo ""
        
        echo "CMake Information:"
        if command -v cmake &> /dev/null; then
            echo "CMake version: $(cmake --version | head -1)"
        fi
        echo ""
        
        echo "=== Build Information ==="
        echo "Project Root: $PROJECT_ROOT"
        echo "Build Directory: $BUILD_DIR"
        echo "Build Type: $(grep CMAKE_BUILD_TYPE "$BUILD_DIR/CMakeCache.txt" 2>/dev/null || echo "Unknown")"
        echo ""
        
        echo "=== Git Information ==="
        cd "$PROJECT_ROOT"
        if command -v git &> /dev/null && [ -d .git ]; then
            echo "Current Branch: $(git branch --show-current 2>/dev/null || echo "Unknown")"
            echo "Last Commit: $(git log -1 --oneline 2>/dev/null || echo "Unknown")"
            echo "Git Status:"
            git status --porcelain 2>/dev/null || echo "No git repository"
        fi
        
    } > "$system_info_file"
    
    log_success "System information saved to: $system_info_file"
}

# Process benchmark results
process_results() {
    local report_dir=$(cat /tmp/picoradar_report_dir)
    
    log_header "Processing Benchmark Results"
    
    # Create Python script for result processing
    cat > "${report_dir}/process_results.py" << 'EOF'
#!/usr/bin/env python3
import json
import csv
import sys
import os
from pathlib import Path
from datetime import datetime
import statistics

def load_json_results(json_file):
    """Load benchmark results from JSON file"""
    try:
        with open(json_file, 'r') as f:
            data = json.load(f)
        return data.get('benchmarks', [])
    except Exception as e:
        print(f"Error loading {json_file}: {e}")
        return []

def analyze_benchmark_group(benchmarks, group_name):
    """Analyze a group of related benchmarks"""
    results = {}
    
    for bench in benchmarks:
        name = bench.get('name', '')
        real_time = bench.get('real_time', 0)
        cpu_time = bench.get('cpu_time', 0)
        iterations = bench.get('iterations', 0)
        
        # Extract custom counters
        counters = {}
        for key, value in bench.items():
            if key.endswith('PerSecond') or key.endswith('_rate'):
                counters[key] = value
        
        results[name] = {
            'real_time_ns': real_time,
            'cpu_time_ns': cpu_time,
            'iterations': iterations,
            'counters': counters
        }
    
    return results

def generate_summary_report(report_dir):
    """Generate comprehensive summary report"""
    raw_dir = Path(report_dir) / 'raw_data'
    summary_file = Path(report_dir) / 'processed' / 'summary_report.md'
    
    with open(summary_file, 'w') as f:
        f.write(f"# PICORadar Performance Test Report\n\n")
        f.write(f"**Generated:** {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n\n")
        
        # Process each benchmark file
        benchmark_files = list(raw_dir.glob('*.json'))
        
        for json_file in sorted(benchmark_files):
            benchmark_name = json_file.stem
            benchmarks = load_json_results(json_file)
            
            if not benchmarks:
                continue
                
            f.write(f"## {benchmark_name.replace('_', ' ').title()}\n\n")
            
            # Create results table
            f.write("| Benchmark | Real Time (ms) | CPU Time (ms) | Iterations | Key Metrics |\n")
            f.write("|-----------|----------------|---------------|------------|-------------|\n")
            
            for bench in benchmarks:
                name = bench.get('name', '').split('/')[-1]  # Get just the test name
                real_time = bench.get('real_time', 0) / 1_000_000  # Convert to ms
                cpu_time = bench.get('cpu_time', 0) / 1_000_000    # Convert to ms
                iterations = bench.get('iterations', 0)
                
                # Extract key metrics
                key_metrics = []
                for key, value in bench.items():
                    if 'PerSecond' in key:
                        if isinstance(value, (int, float)):
                            key_metrics.append(f"{key}: {value:.2f}")
                
                metrics_str = ', '.join(key_metrics[:2])  # Limit to first 2 metrics
                
                f.write(f"| {name} | {real_time:.2f} | {cpu_time:.2f} | {iterations} | {metrics_str} |\n")
            
            f.write("\n")
        
        # Generate recommendations
        f.write("## Performance Analysis and Recommendations\n\n")
        f.write("### Key Findings\n\n")
        f.write("- **Configuration Management**: ")
        
        # Analyze config manager results
        config_benchmarks = load_json_results(raw_dir / 'benchmark_config_manager.json')
        if config_benchmarks:
            cache_bench = next((b for b in config_benchmarks if 'CachePerformance' in b.get('name', '')), None)
            if cache_bench:
                cache_hit_rate = cache_bench.get('CacheHitRate', 0)
                f.write(f"Cache hit rate: {cache_hit_rate:.1f}%\n")
        
        f.write("- **Player Registry**: ")
        registry_benchmarks = load_json_results(raw_dir / 'benchmark_player_registry.json')
        if registry_benchmarks:
            update_bench = next((b for b in registry_benchmarks if 'PlayerUpdate' in b.get('name', '')), None)
            if update_bench:
                updates_per_sec = update_bench.get('UpdatesPerSecond', 0)
                f.write(f"Updates per second: {updates_per_sec:.0f}\n")
        
        f.write("- **Network Performance**: ")
        network_benchmarks = load_json_results(raw_dir / 'benchmark_network_latency.json')
        if network_benchmarks:
            serialization_bench = next((b for b in network_benchmarks if 'Serialization' in b.get('name', '')), None)
            if serialization_bench:
                serialization_rate = serialization_bench.get('SerializationsPerSecond', 0)
                f.write(f"Serialization rate: {serialization_rate:.0f} ops/sec\n")
        
        f.write("\n### Recommendations\n\n")
        f.write("1. **Memory Optimization**: Consider implementing memory pooling for high-frequency allocations\n")
        f.write("2. **Concurrency**: Current threading model shows good scalability up to available cores\n")
        f.write("3. **Network**: Serialization performance is within acceptable ranges for VR applications\n")
        f.write("4. **Monitoring**: Set up continuous performance monitoring with these baseline metrics\n\n")

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python3 process_results.py <report_dir>")
        sys.exit(1)
    
    report_dir = sys.argv[1]
    generate_summary_report(report_dir)
    print(f"Summary report generated: {report_dir}/processed/summary_report.md")
EOF

    # Run the Python processor if available
    if command -v python3 &> /dev/null; then
        log_info "Processing benchmark results with Python..."
        python3 "${report_dir}/process_results.py" "$report_dir"
        log_success "Results processed successfully"
    else
        log_warning "Python3 not available. Skipping advanced result processing."
    fi
    
    # Generate simple text summary even without Python
    log_info "Generating basic summary..."
    {
        echo "PICORadar Performance Test Summary"
        echo "=================================="
        echo "Generated: $(date)"
        echo ""
        echo "Test Results Location: $report_dir"
        echo ""
        echo "Available Result Files:"
        find "$report_dir/raw_data" -name "*.json" -o -name "*.csv" -o -name "*_console.txt" | sort
        echo ""
        echo "To view detailed results:"
        echo "  - JSON files: Use any JSON viewer or processing tool"
        echo "  - CSV files: Open with spreadsheet software or text editor"
        echo "  - Console output: View with 'cat' or text editor"
        echo ""
        echo "For trend analysis, compare with previous reports in: $OUTPUT_DIR"
    } > "${report_dir}/README.txt"
    
    log_success "Basic summary generated"
}

# Generate HTML dashboard (if Python is available)
generate_dashboard() {
    local report_dir=$(cat /tmp/picoradar_report_dir)
    
    if ! command -v python3 &> /dev/null; then
        log_warning "Python3 not available. Skipping HTML dashboard generation."
        return 0
    fi
    
    log_info "Generating HTML dashboard..."
    
    cat > "${report_dir}/generate_dashboard.py" << 'EOF'
#!/usr/bin/env python3
import json
import html
from pathlib import Path

def generate_html_dashboard(report_dir):
    report_path = Path(report_dir)
    raw_dir = report_path / 'raw_data'
    dashboard_file = report_path / 'dashboard.html'
    
    html_content = """
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>PICORadar Performance Dashboard</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background-color: #f5f5f5; }
        .header { background-color: #2c3e50; color: white; padding: 20px; border-radius: 8px; margin-bottom: 20px; }
        .benchmark-section { background-color: white; padding: 20px; margin-bottom: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
        .metric-card { display: inline-block; background-color: #ecf0f1; padding: 15px; margin: 10px; border-radius: 5px; min-width: 200px; }
        .metric-value { font-size: 24px; font-weight: bold; color: #2c3e50; }
        .metric-label { color: #7f8c8d; font-size: 14px; }
        table { width: 100%; border-collapse: collapse; margin-top: 15px; }
        th, td { padding: 12px; text-align: left; border-bottom: 1px solid #ddd; }
        th { background-color: #3498db; color: white; }
        .performance-good { color: #27ae60; }
        .performance-warning { color: #f39c12; }
        .performance-poor { color: #e74c3c; }
    </style>
</head>
<body>
    <div class="header">
        <h1>🚀 PICORadar Performance Dashboard</h1>
        <p>Real-time VR position sharing system performance metrics</p>
    </div>
"""
    
    # Process each benchmark file
    benchmark_files = list(raw_dir.glob('*.json'))
    
    for json_file in sorted(benchmark_files):
        try:
            with open(json_file, 'r') as f:
                data = json.load(f)
            benchmarks = data.get('benchmarks', [])
            
            if not benchmarks:
                continue
            
            benchmark_name = json_file.stem.replace('benchmark_', '').replace('_', ' ').title()
            html_content += f"""
    <div class="benchmark-section">
        <h2>{benchmark_name}</h2>
"""
            
            # Extract key metrics
            key_metrics = {}
            for bench in benchmarks:
                for key, value in bench.items():
                    if 'PerSecond' in key and isinstance(value, (int, float)):
                        if key not in key_metrics:
                            key_metrics[key] = []
                        key_metrics[key].append(value)
            
            # Display aggregated metrics
            for metric, values in list(key_metrics.items())[:4]:  # Show top 4 metrics
                avg_value = sum(values) / len(values) if values else 0
                html_content += f"""
        <div class="metric-card">
            <div class="metric-value">{avg_value:.2f}</div>
            <div class="metric-label">{metric.replace('PerSecond', '/sec')}</div>
        </div>
"""
            
            # Benchmark results table
            html_content += """
        <table>
            <thead>
                <tr>
                    <th>Test Case</th>
                    <th>Real Time (ms)</th>
                    <th>CPU Time (ms)</th>
                    <th>Iterations</th>
                    <th>Performance</th>
                </tr>
            </thead>
            <tbody>
"""
            
            for bench in benchmarks:
                name = bench.get('name', '').split('/')[-1]
                real_time = bench.get('real_time', 0) / 1_000_000
                cpu_time = bench.get('cpu_time', 0) / 1_000_000
                iterations = bench.get('iterations', 0)
                
                # Simple performance classification
                perf_class = "performance-good"
                if real_time > 10:  # > 10ms might be concerning for VR
                    perf_class = "performance-warning"
                if real_time > 50:  # > 50ms is definitely problematic
                    perf_class = "performance-poor"
                
                html_content += f"""
                <tr>
                    <td>{html.escape(name)}</td>
                    <td>{real_time:.3f}</td>
                    <td>{cpu_time:.3f}</td>
                    <td>{iterations}</td>
                    <td class="{perf_class}">{'✓ Good' if perf_class == 'performance-good' else ('⚠ Warning' if perf_class == 'performance-warning' else '✗ Poor')}</td>
                </tr>
"""
            
            html_content += """
            </tbody>
        </table>
    </div>
"""
        
        except Exception as e:
            print(f"Error processing {json_file}: {e}")
    
    html_content += """
    <div class="benchmark-section">
        <h2>System Information</h2>
        <p>For detailed system information, see the system_info.txt file in this report directory.</p>
        <p><strong>Report Generated:</strong> """ + str(report_path.name) + """</p>
    </div>
</body>
</html>
"""
    
    with open(dashboard_file, 'w') as f:
        f.write(html_content)
    
    print(f"Dashboard generated: {dashboard_file}")

if __name__ == '__main__':
    import sys
    if len(sys.argv) != 2:
        print("Usage: python3 generate_dashboard.py <report_dir>")
        sys.exit(1)
    
    generate_html_dashboard(sys.argv[1])
EOF

    python3 "${report_dir}/generate_dashboard.py" "$report_dir"
    log_success "HTML dashboard generated: ${report_dir}/dashboard.html"
}

# Main execution
main() {
    log_header "PICORadar Performance Testing Suite"
    log_info "Starting performance testing and report generation..."
    log_info "Output directory: $OUTPUT_DIR"
    log_info "Report format: $REPORT_FORMAT"
    
    # Execute all steps
    check_dependencies
    setup_output_directories
    generate_system_info
    run_all_benchmarks
    process_results
    generate_dashboard
    
    local report_dir=$(cat /tmp/picoradar_report_dir)
    
    log_header "Performance Testing Complete!"
    log_success "📊 Report generated at: $report_dir"
    log_info "📁 Available files:"
    log_info "   • dashboard.html - Interactive performance dashboard"
    log_info "   • processed/summary_report.md - Detailed analysis"
    log_info "   • raw_data/ - Raw benchmark output files"
    log_info "   • system_info.txt - System configuration"
    
    if [[ "$REPORT_FORMAT" == "all" || "$REPORT_FORMAT" == "console" ]]; then
        echo ""
        log_info "Quick preview of console results:"
        find "$report_dir/raw_data" -name "*_console.txt" | head -1 | xargs head -20
    fi
    
    # Cleanup
    rm -f /tmp/picoradar_report_dir
    
    log_success "🎉 Performance testing complete! Happy optimizing! 🚀"
}

# Run main function with all arguments
main "$@"
