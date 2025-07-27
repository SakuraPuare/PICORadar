#!/usr/bin/env python3
"""
PICORadar 性能基准测试报告生成器

这个脚本读取 Google Benchmark 输出的 JSON 结果文件，
并生成一个清晰的 HTML 性能报告。
"""

import argparse
import json
import statistics
import sys
from datetime import datetime
from pathlib import Path
from typing import Any, Dict, List


class BenchmarkReportGenerator:
    def __init__(self, input_file: str, output_file: str):
        self.input_file = Path(input_file)
        self.output_file = Path(output_file)
        self.data = None
        
    def load_data(self) -> bool:
        """加载基准测试数据"""
        try:
            with open(self.input_file, 'r', encoding='utf-8') as f:
                self.data = json.load(f)
            return True
        except Exception as e:
            print(f"Error loading benchmark data: {e}")
            return False
    
    def format_time(self, time_ns: float) -> str:
        """格式化时间单位"""
        if time_ns < 1000:
            return f"{time_ns:.2f} ns"
        elif time_ns < 1000000:
            return f"{time_ns/1000:.2f} μs"
        elif time_ns < 1000000000:
            return f"{time_ns/1000000:.2f} ms"
        else:
            return f"{time_ns/1000000000:.2f} s"
    
    def format_bytes(self, bytes_val: float) -> str:
        """格式化字节大小"""
        if bytes_val < 1024:
            return f"{bytes_val:.2f} B"
        elif bytes_val < 1024**2:
            return f"{bytes_val/1024:.2f} KB"
        elif bytes_val < 1024**3:
            return f"{bytes_val/1024**2:.2f} MB"
        else:
            return f"{bytes_val/1024**3:.2f} GB"
    
    def categorize_benchmarks(self) -> Dict[str, List[Dict]]:
        """按模块分类基准测试"""
        categories = {
            'Core': [],
            'Network': [],
            'Config': [],
            'Memory': [],
            'Other': []
        }
        
        for benchmark in self.data.get('benchmarks', []):
            name = benchmark.get('name', '')
            
            if 'PlayerRegistry' in name:
                categories['Core'].append(benchmark)
            elif 'Protobuf' in name or 'Batch' in name or 'Serialization' in name:
                categories['Network'].append(benchmark)
            elif 'Config' in name:
                categories['Config'].append(benchmark)
            elif 'Memory' in name:
                categories['Memory'].append(benchmark)
            else:
                categories['Other'].append(benchmark)
        
        return categories
    
    def generate_summary_stats(self, benchmarks: List[Dict]) -> Dict[str, Any]:
        """生成总结统计信息"""
        if not benchmarks:
            return {}
        
        times = [b.get('real_time', 0) for b in benchmarks]
        
        return {
            'count': len(benchmarks),
            'avg_time': statistics.mean(times),
            'median_time': statistics.median(times),
            'min_time': min(times),
            'max_time': max(times),
            'total_tests': len(benchmarks)
        }
    
    def generate_html_report(self) -> str:
        """生成 HTML 报告"""
        if not self.data:
            return "<html><body><h1>No data available</h1></body></html>"
        
        categories = self.categorize_benchmarks()
        context_info = self.data.get('context', {})
        
        html = f"""
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>PICORadar 性能基准测试报告</title>
    <style>
        body {{
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            margin: 0;
            padding: 20px;
            background-color: #f5f5f5;
        }}
        .container {{
            max-width: 1200px;
            margin: 0 auto;
            background: white;
            padding: 30px;
            border-radius: 10px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
        }}
        h1 {{
            color: #2c3e50;
            text-align: center;
            margin-bottom: 30px;
            border-bottom: 3px solid #3498db;
            padding-bottom: 10px;
        }}
        h2 {{
            color: #34495e;
            border-left: 4px solid #3498db;
            padding-left: 15px;
            margin-top: 30px;
        }}
        .summary {{
            background: #ecf0f1;
            padding: 20px;
            border-radius: 8px;
            margin-bottom: 30px;
        }}
        .summary-grid {{
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 15px;
            margin-top: 15px;
        }}
        .summary-item {{
            background: white;
            padding: 15px;
            border-radius: 5px;
            text-align: center;
            box-shadow: 0 1px 3px rgba(0,0,0,0.1);
        }}
        .summary-item strong {{
            display: block;
            color: #2c3e50;
            font-size: 1.2em;
        }}
        table {{
            width: 100%;
            border-collapse: collapse;
            margin: 20px 0;
            background: white;
        }}
        th, td {{
            padding: 12px;
            text-align: left;
            border-bottom: 1px solid #ddd;
        }}
        th {{
            background-color: #3498db;
            color: white;
            font-weight: bold;
        }}
        tr:hover {{
            background-color: #f8f9fa;
        }}
        .time-cell {{
            font-family: 'Courier New', monospace;
            font-weight: bold;
        }}
        .good {{ color: #27ae60; }}
        .warning {{ color: #f39c12; }}
        .danger {{ color: #e74c3c; }}
        .info-grid {{
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
            gap: 20px;
            margin: 20px 0;
        }}
        .info-box {{
            background: #f8f9fa;
            padding: 15px;
            border-radius: 5px;
            border-left: 4px solid #3498db;
        }}
        .category-section {{
            margin: 30px 0;
            padding: 20px;
            border: 1px solid #e0e0e0;
            border-radius: 8px;
        }}
        .benchmark-name {{
            font-family: 'Courier New', monospace;
            font-size: 0.9em;
        }}
        .footer {{
            text-align: center;
            margin-top: 40px;
            padding: 20px;
            background: #f8f9fa;
            border-radius: 5px;
            color: #666;
        }}
    </style>
</head>
<body>
    <div class="container">
        <h1>🚀 PICORadar 性能基准测试报告</h1>
        
        <div class="summary">
            <h2>📊 测试概览</h2>
            <div class="info-grid">
                <div class="info-box">
                    <strong>生成时间</strong><br>
                    {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}
                </div>
                <div class="info-box">
                    <strong>测试主机</strong><br>
                    {context_info.get('host_name', 'Unknown')}
                </div>
                <div class="info-box">
                    <strong>CPU</strong><br>
                    {context_info.get('cpu_info', {}).get('name', 'Unknown')} 
                    ({context_info.get('cpu_info', {}).get('num_cpus', 'N/A')} cores)
                </div>
                <div class="info-box">
                    <strong>测试总数</strong><br>
                    {len(self.data.get('benchmarks', []))} 个基准测试
                </div>
            </div>
        </div>
"""
        
        # 为每个类别生成报告
        for category_name, benchmarks in categories.items():
            if not benchmarks:
                continue
                
            stats = self.generate_summary_stats(benchmarks)
            
            html += f"""
        <div class="category-section">
            <h2>📁 {category_name} 模块测试</h2>
            
            <div class="summary-grid">
                <div class="summary-item">
                    <strong>{stats['count']}</strong>
                    测试数量
                </div>
                <div class="summary-item">
                    <strong>{self.format_time(stats['avg_time'])}</strong>
                    平均执行时间
                </div>
                <div class="summary-item">
                    <strong>{self.format_time(stats['min_time'])}</strong>
                    最快时间
                </div>
                <div class="summary-item">
                    <strong>{self.format_time(stats['max_time'])}</strong>
                    最慢时间
                </div>
            </div>
            
            <table>
                <thead>
                    <tr>
                        <th>测试名称</th>
                        <th>执行时间</th>
                        <th>CPU 时间</th>
                        <th>迭代次数</th>
                        <th>吞吐量</th>
                    </tr>
                </thead>
                <tbody>
"""
            
            for benchmark in sorted(benchmarks, key=lambda x: x.get('real_time', 0)):
                name = benchmark.get('name', 'Unknown')
                real_time = benchmark.get('real_time', 0)
                cpu_time = benchmark.get('cpu_time', 0)
                iterations = benchmark.get('iterations', 0)
                
                # 计算吞吐量
                items_per_sec = benchmark.get('items_per_second', 0)
                throughput = f"{items_per_sec:.0f} items/sec" if items_per_sec > 0 else "N/A"
                
                # 根据性能设置颜色
                time_class = "good" if real_time < 1000000 else "warning" if real_time < 10000000 else "danger"
                
                html += f"""
                    <tr>
                        <td class="benchmark-name">{name}</td>
                        <td class="time-cell {time_class}">{self.format_time(real_time)}</td>
                        <td class="time-cell">{self.format_time(cpu_time)}</td>
                        <td>{iterations:,}</td>
                        <td>{throughput}</td>
                    </tr>
"""
            
            html += """
                </tbody>
            </table>
        </div>
"""
        
        html += f"""
        <div class="footer">
            <p>📝 该报告由 PICORadar 基准测试系统自动生成</p>
            <p>🔧 数据源: {self.input_file.name}</p>
        </div>
    </div>
</body>
</html>
"""
        
        return html
    
    def generate_report(self) -> bool:
        """生成完整报告"""
        if not self.load_data():
            return False
        
        try:
            html_content = self.generate_html_report()
            
            with open(self.output_file, 'w', encoding='utf-8') as f:
                f.write(html_content)
            
            print(f"✅ 性能报告已生成: {self.output_file}")
            return True
            
        except Exception as e:
            print(f"❌ 生成报告失败: {e}")
            return False

def main():
    parser = argparse.ArgumentParser(description='生成 PICORadar 性能基准测试报告')
    parser.add_argument('--input', '-i', 
                       default='benchmark_results.json',
                       help='基准测试结果 JSON 文件路径')
    parser.add_argument('--output', '-o',
                       default='performance_report.html', 
                       help='输出 HTML 报告文件路径')
    
    args = parser.parse_args()
    
    # 检查输入文件是否存在
    if not Path(args.input).exists():
        print(f"❌ 输入文件不存在: {args.input}")
        print("💡 请先运行基准测试生成结果文件")
        return 1
    
    # 生成报告
    generator = BenchmarkReportGenerator(args.input, args.output)
    if generator.generate_report():
        print(f"🌐 你可以在浏览器中打开报告: file://{Path(args.output).absolute()}")
        return 0
    else:
        return 1

if __name__ == "__main__":
    sys.exit(main())
