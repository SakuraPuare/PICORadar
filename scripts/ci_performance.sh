#!/bin/bash

# PICORadar CI Performance Integration Script
# This script integrates performance testing into CI/CD pipeline

set -e

# Configuration
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CI_OUTPUT_DIR="${PROJECT_ROOT}/ci_performance_reports"
BASELINE_DIR="${PROJECT_ROOT}/performance_baselines"
PERFORMANCE_THRESHOLD_FILE="${PROJECT_ROOT}/performance_thresholds.json"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() { echo -e "${BLUE}[CI-PERF]${NC} $1"; }
log_success() { echo -e "${GREEN}[CI-PERF]${NC} $1"; }
log_warning() { echo -e "${YELLOW}[CI-PERF]${NC} $1"; }
log_error() { echo -e "${RED}[CI-PERF]${NC} $1"; }

# Create default performance thresholds if not exists
create_default_thresholds() {
    if [ ! -f "$PERFORMANCE_THRESHOLD_FILE" ]; then
        log_info "Creating default performance thresholds..."
        cat > "$PERFORMANCE_THRESHOLD_FILE" << 'EOF'
{
  "version": "1.0",
  "thresholds": {
    "config_manager": {
      "CachePerformance": {
        "max_time_ms": 1.0,
        "min_cache_hit_rate": 90.0
      },
      "JsonParsing": {
        "max_time_ms": 5.0,
        "min_operations_per_second": 1000
      }
    },
    "player_registry": {
      "PlayerUpdate": {
        "max_time_ms": 0.5,
        "min_updates_per_second": 10000
      },
      "PlayerQuery": {
        "max_time_ms": 0.1,
        "min_queries_per_second": 50000
      }
    },
    "network": {
      "Serialization": {
        "max_time_ms": 0.2,
        "min_serializations_per_second": 5000
      },
      "Deserialization": {
        "max_time_ms": 0.3,
        "min_deserializations_per_second": 4000
      }
    },
    "memory": {
      "PlayerDataMassAllocation": {
        "max_time_ms": 10.0,
        "min_allocations_per_second": 1000
      }
    },
    "concurrent": {
      "ConcurrentPlayerRegistryAccess": {
        "max_time_ms": 1.0,
        "min_operations_per_second": 5000
      }
    }
  },
  "global_limits": {
    "max_regression_percent": 20.0,
    "max_memory_increase_mb": 50.0,
    "max_cpu_utilization_percent": 80.0
  }
}
EOF
        log_success "Default thresholds created at: $PERFORMANCE_THRESHOLD_FILE"
    fi
}

# Run performance tests in CI mode
run_ci_performance_tests() {
    log_info "Running performance tests in CI mode..."
    
    local timestamp=$(date +"%Y%m%d_%H%M%S")
    local ci_report_dir="${CI_OUTPUT_DIR}/${timestamp}"
    
    mkdir -p "$ci_report_dir"
    
    # Run the main performance testing script
    if [ -x "${PROJECT_ROOT}/scripts/generate_performance_report.sh" ]; then
        "${PROJECT_ROOT}/scripts/generate_performance_report.sh" "$ci_report_dir" "json"
    else
        log_error "Performance report script not found or not executable"
        return 1
    fi
    
    echo "$ci_report_dir" > /tmp/ci_perf_report_dir
    log_success "CI performance tests completed"
}

# Compare with baseline performance
compare_with_baseline() {
    local current_report_dir=$(cat /tmp/ci_perf_report_dir)
    
    log_info "Comparing with baseline performance..."
    
    # Create Python script for performance comparison
    cat > "${current_report_dir}/compare_performance.py" << 'EOF'
#!/usr/bin/env python3
import json
import sys
from pathlib import Path
import statistics

def load_benchmark_results(json_file):
    """Load benchmark results from JSON file"""
    try:
        with open(json_file, 'r') as f:
            data = json.load(f)
        return data.get('benchmarks', [])
    except Exception as e:
        print(f"Error loading {json_file}: {e}")
        return []

def load_thresholds(threshold_file):
    """Load performance thresholds"""
    try:
        with open(threshold_file, 'r') as f:
            return json.load(f)
    except Exception as e:
        print(f"Error loading thresholds: {e}")
        return {}

def compare_benchmark_results(current_dir, baseline_dir, thresholds):
    """Compare current results with baseline"""
    current_path = Path(current_dir)
    baseline_path = Path(baseline_dir)
    
    results = {
        'passed': [],
        'failed': [],
        'warnings': [],
        'new_tests': [],
        'missing_tests': []
    }
    
    # Get all benchmark files from current run
    current_files = list(current_path.glob('raw_data/*.json'))
    
    for current_file in current_files:
        benchmark_name = current_file.stem
        baseline_file = baseline_path / 'raw_data' / current_file.name
        
        current_benchmarks = load_benchmark_results(current_file)
        if not current_benchmarks:
            continue
        
        if baseline_file.exists():
            baseline_benchmarks = load_benchmark_results(baseline_file)
            baseline_dict = {b['name']: b for b in baseline_benchmarks}
            
            for current_bench in current_benchmarks:
                test_name = current_bench.get('name', '')
                baseline_bench = baseline_dict.get(test_name)
                
                if baseline_bench:
                    # Compare performance
                    comparison = compare_single_benchmark(
                        current_bench, baseline_bench, benchmark_name, thresholds
                    )
                    
                    if comparison['status'] == 'pass':
                        results['passed'].append(comparison)
                    elif comparison['status'] == 'fail':
                        results['failed'].append(comparison)
                    else:
                        results['warnings'].append(comparison)
                else:
                    results['new_tests'].append({
                        'name': test_name,
                        'benchmark_group': benchmark_name
                    })
        else:
            # No baseline file - all tests are new
            for bench in current_benchmarks:
                results['new_tests'].append({
                    'name': bench.get('name', ''),
                    'benchmark_group': benchmark_name
                })
    
    return results

def compare_single_benchmark(current, baseline, benchmark_group, thresholds):
    """Compare a single benchmark result"""
    test_name = current.get('name', '')
    current_time = current.get('real_time', 0) / 1_000_000  # Convert to ms
    baseline_time = baseline.get('real_time', 0) / 1_000_000
    
    # Calculate performance change
    if baseline_time > 0:
        time_change_percent = ((current_time - baseline_time) / baseline_time) * 100
    else:
        time_change_percent = 0
    
    # Check against thresholds
    threshold_config = thresholds.get('thresholds', {}).get(benchmark_group, {})
    global_limits = thresholds.get('global_limits', {})
    
    max_regression = global_limits.get('max_regression_percent', 20.0)
    
    status = 'pass'
    messages = []
    
    # Check time regression
    if time_change_percent > max_regression:
        status = 'fail'
        messages.append(f"Performance regression: {time_change_percent:.1f}% slower")
    elif time_change_percent > max_regression / 2:
        status = 'warning'
        messages.append(f"Performance warning: {time_change_percent:.1f}% slower")
    
    # Check absolute time thresholds
    for test_pattern, limits in threshold_config.items():
        if test_pattern in test_name:
            max_time = limits.get('max_time_ms', float('inf'))
            if current_time > max_time:
                status = 'fail'
                messages.append(f"Exceeds time limit: {current_time:.2f}ms > {max_time}ms")
    
    # Check rate-based metrics
    for key, value in current.items():
        if 'PerSecond' in key and isinstance(value, (int, float)):
            baseline_rate = baseline.get(key, 0)
            if baseline_rate > 0:
                rate_change_percent = ((value - baseline_rate) / baseline_rate) * 100
                if rate_change_percent < -max_regression:
                    status = 'fail'
                    messages.append(f"{key} regression: {rate_change_percent:.1f}%")
    
    return {
        'name': test_name,
        'benchmark_group': benchmark_group,
        'status': status,
        'current_time_ms': current_time,
        'baseline_time_ms': baseline_time,
        'time_change_percent': time_change_percent,
        'messages': messages
    }

def generate_ci_report(results, output_file):
    """Generate CI-friendly performance report"""
    total_tests = len(results['passed']) + len(results['failed']) + len(results['warnings'])
    failed_count = len(results['failed'])
    
    with open(output_file, 'w') as f:
        f.write("# PICORadar CI Performance Report\n\n")
        f.write(f"**Total Tests:** {total_tests}\n")
        f.write(f"**Passed:** {len(results['passed'])}\n")
        f.write(f"**Failed:** {failed_count}\n")
        f.write(f"**Warnings:** {len(results['warnings'])}\n")
        f.write(f"**New Tests:** {len(results['new_tests'])}\n\n")
        
        if failed_count > 0:
            f.write("## ❌ Failed Tests\n\n")
            for failure in results['failed']:
                f.write(f"- **{failure['name']}**\n")
                f.write(f"  - Current: {failure['current_time_ms']:.2f}ms\n")
                f.write(f"  - Baseline: {failure['baseline_time_ms']:.2f}ms\n")
                f.write(f"  - Change: {failure['time_change_percent']:+.1f}%\n")
                for msg in failure['messages']:
                    f.write(f"  - ⚠️ {msg}\n")
                f.write("\n")
        
        if results['warnings']:
            f.write("## ⚠️ Warnings\n\n")
            for warning in results['warnings']:
                f.write(f"- **{warning['name']}**: {warning['time_change_percent']:+.1f}% change\n")
        
        if results['new_tests']:
            f.write("## 🆕 New Tests\n\n")
            for new_test in results['new_tests']:
                f.write(f"- {new_test['name']} ({new_test['benchmark_group']})\n")
        
        f.write(f"\n## Summary\n\n")
        if failed_count == 0:
            f.write("✅ **All performance tests passed!**\n")
        else:
            f.write(f"❌ **{failed_count} performance test(s) failed.**\n")
    
    return failed_count == 0

def main():
    if len(sys.argv) != 4:
        print("Usage: python3 compare_performance.py <current_dir> <baseline_dir> <thresholds_file>")
        sys.exit(1)
    
    current_dir = sys.argv[1]
    baseline_dir = sys.argv[2]
    thresholds_file = sys.argv[3]
    
    thresholds = load_thresholds(thresholds_file)
    results = compare_benchmark_results(current_dir, baseline_dir, thresholds)
    
    report_file = Path(current_dir) / 'ci_performance_report.md'
    success = generate_ci_report(results, report_file)
    
    print(f"Performance comparison report: {report_file}")
    
    # Exit with error code if tests failed
    if not success:
        print(f"Performance regression detected: {len(results['failed'])} test(s) failed")
        sys.exit(1)
    else:
        print("All performance tests passed!")

if __name__ == '__main__':
    main()
EOF

    # Run comparison if Python is available and baseline exists
    if command -v python3 &> /dev/null && [ -d "$BASELINE_DIR" ]; then
        python3 "${current_report_dir}/compare_performance.py" \
                "$current_report_dir" \
                "$BASELINE_DIR" \
                "$PERFORMANCE_THRESHOLD_FILE"
        
        comparison_result=$?
        
        if [ $comparison_result -eq 0 ]; then
            log_success "Performance comparison passed"
        else
            log_error "Performance regression detected"
            return 1
        fi
    else
        log_warning "Skipping performance comparison (no Python3 or baseline)"
    fi
}

# Update baseline if this is a successful main branch build
update_baseline() {
    local current_report_dir=$(cat /tmp/ci_perf_report_dir)
    
    # Only update baseline on main branch and if tests passed
    if [ "${CI_BRANCH:-}" = "main" ] || [ "${GITHUB_REF:-}" = "refs/heads/main" ]; then
        log_info "Updating performance baseline..."
        
        mkdir -p "$BASELINE_DIR"
        
        # Copy current results as new baseline
        cp -r "${current_report_dir}/raw_data" "$BASELINE_DIR/"
        
        # Keep baseline metadata
        echo "Baseline updated: $(date)" > "${BASELINE_DIR}/baseline_info.txt"
        echo "Commit: ${CI_COMMIT_SHA:-$(git rev-parse HEAD 2>/dev/null || echo 'unknown')}" >> "${BASELINE_DIR}/baseline_info.txt"
        echo "Branch: ${CI_BRANCH:-main}" >> "${BASELINE_DIR}/baseline_info.txt"
        
        log_success "Performance baseline updated"
    else
        log_info "Skipping baseline update (not main branch)"
    fi
}

# Generate CI artifacts
generate_ci_artifacts() {
    local current_report_dir=$(cat /tmp/ci_perf_report_dir)
    
    log_info "Generating CI artifacts..."
    
    # Create performance badge data
    cat > "${current_report_dir}/performance_badge.json" << EOF
{
  "schemaVersion": 1,
  "label": "performance",
  "message": "monitored",
  "color": "green"
}
EOF

    # Create JUnit-style XML report for CI systems
    cat > "${current_report_dir}/performance_junit.xml" << 'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<testsuite name="PICORadar Performance Tests" tests="1" failures="0" time="0">
  <testcase name="performance_suite" classname="PICORadar" time="0">
    <system-out>Performance tests completed successfully</system-out>
  </testcase>
</testsuite>
EOF

    # Create GitHub Actions summary if running in GHA
    if [ -n "${GITHUB_STEP_SUMMARY:-}" ]; then
        cat >> "$GITHUB_STEP_SUMMARY" << EOF
## 🚀 Performance Test Results

### Summary
- ✅ Performance testing completed
- 📊 Results available in artifacts
- 📈 Dashboard: \`${current_report_dir}/dashboard.html\`

### Quick Stats
\`\`\`
Report Directory: ${current_report_dir}
Timestamp: $(date)
Commit: ${GITHUB_SHA:-unknown}
\`\`\`

### Files Generated
- \`dashboard.html\` - Interactive performance dashboard
- \`ci_performance_report.md\` - Performance comparison report
- \`raw_data/\` - Raw benchmark data (JSON/CSV)
- \`system_info.txt\` - System configuration

EOF
    fi
    
    log_success "CI artifacts generated"
}

# Main CI function
main() {
    log_info "Starting CI performance integration..."
    
    # Setup
    create_default_thresholds
    
    # Run performance tests
    run_ci_performance_tests || exit 1
    
    # Compare with baseline
    compare_with_baseline || exit 1
    
    # Update baseline if appropriate
    update_baseline
    
    # Generate CI artifacts
    generate_ci_artifacts
    
    local current_report_dir=$(cat /tmp/ci_perf_report_dir)
    
    log_success "✅ CI performance integration completed successfully!"
    log_info "📊 Report available at: ${current_report_dir}"
    
    # Cleanup
    rm -f /tmp/ci_perf_report_dir
    
    exit 0
}

# Handle script arguments
case "${1:-}" in
    "baseline")
        log_info "Creating initial performance baseline..."
        create_default_thresholds
        run_ci_performance_tests
        current_report_dir=$(cat /tmp/ci_perf_report_dir)
        mkdir -p "$BASELINE_DIR"
        cp -r "${current_report_dir}/raw_data" "$BASELINE_DIR/"
        echo "Initial baseline created: $(date)" > "${BASELINE_DIR}/baseline_info.txt"
        log_success "Initial baseline created at: $BASELINE_DIR"
        ;;
    "check")
        main
        ;;
    *)
        echo "Usage: $0 {baseline|check}"
        echo "  baseline - Create initial performance baseline"
        echo "  check    - Run performance tests and compare with baseline"
        exit 1
        ;;
esac
