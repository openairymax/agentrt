"""
AgentOS 测试报告可视化仪表板
Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
Version: 1.0.0

生成HTML格式的测试报告仪表板
支持多种图表和可视化组件
"""

import json
import os
from datetime import datetime
from typing import Any, Dict, List, Optional
from dataclasses import dataclass, field
from pathlib import Path
from enum import Enum


class ChartType(Enum):
    """图表类型"""
    LINE = "line"
    BAR = "bar"
    PIE = "pie"
    DOUGHNUT = "doughnut"
    RADAR = "radar"


@dataclass
class ChartData:
    """图表数据"""
    labels: List[str]
    datasets: List[Dict[str, Any]]
    title: str = ""
    chart_type: ChartType = ChartType.BAR


@dataclass
class TestResult:
    """测试结果数据"""
    name: str
    passed: int
    failed: int
    skipped: int
    duration: float
    coverage: Optional[float] = None


@dataclass
class DashboardConfig:
    """仪表板配置"""
    title: str = "AgentOS 测试报告"
    version: str = "1.0.0"
    theme: str = "light"
    show_coverage: bool = True
    show_trends: bool = True
    show_security: bool = True


class DashboardGenerator:
    """
    仪表板生成器
    
    生成HTML格式的测试报告仪表板。
    """
    
    def __init__(self, config: Optional[DashboardConfig] = None):
        """
        初始化仪表板生成器
        
        Args:
            config: 仪表板配置
        """
        self.config = config or DashboardConfig()
        self.test_results: List[TestResult] = []
        self.charts: List[ChartData] = []
        self.metrics: Dict[str, Any] = {}
    
    def add_test_result(self, result: TestResult):
        """
        添加测试结果
        
        Args:
            result: 测试结果
        """
        self.test_results.append(result)
    
    def add_chart(self, chart: ChartData):
        """
        添加图表
        
        Args:
            chart: 图表数据
        """
        self.charts.append(chart)
    
    def add_metric(self, name: str, value: Any):
        """
        添加指标
        
        Args:
            name: 指标名称
            value: 指标值
        """
        self.metrics[name] = value
    
    def _generate_css(self) -> str:
        """生成CSS样式"""
        return """
        <style>
            :root {
                --primary-color: #667eea;
                --success-color: #48bb78;
                --warning-color: #ed8936;
                --danger-color: #f56565;
                --info-color: #4299e1;
                --bg-color: #f7fafc;
                --card-bg: #ffffff;
                --text-color: #2d3748;
                --text-muted: #718096;
                --border-color: #e2e8f0;
            }
            
            [data-theme="dark"] {
                --bg-color: #1a202c;
                --card-bg: #2d3748;
                --text-color: #f7fafc;
                --text-muted: #a0aec0;
                --border-color: #4a5568;
            }
            
            * {
                margin: 0;
                padding: 0;
                box-sizing: border-box;
            }
            
            body {
                font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, 'Helvetica Neue', Arial, sans-serif;
                background-color: var(--bg-color);
                color: var(--text-color);
                line-height: 1.6;
            }
            
            .container {
                max-width: 1400px;
                margin: 0 auto;
                padding: 20px;
            }
            
            .header {
                background: linear-gradient(135deg, var(--primary-color), #764ba2);
                color: white;
                padding: 30px;
                border-radius: 12px;
                margin-bottom: 30px;
            }
            
            .header h1 {
                font-size: 2rem;
                margin-bottom: 10px;
            }
            
            .header .meta {
                opacity: 0.9;
                font-size: 0.9rem;
            }
            
            .grid {
                display: grid;
                grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
                gap: 20px;
                margin-bottom: 30px;
            }
            
            .card {
                background: var(--card-bg);
                border-radius: 12px;
                padding: 20px;
                box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);
                border: 1px solid var(--border-color);
            }
            
            .card-header {
                display: flex;
                justify-content: space-between;
                align-items: center;
                margin-bottom: 15px;
            }
            
            .card-title {
                font-size: 1.1rem;
                font-weight: 600;
            }
            
            .metric-value {
                font-size: 2.5rem;
                font-weight: 700;
                color: var(--primary-color);
            }
            
            .metric-label {
                color: var(--text-muted);
                font-size: 0.9rem;
            }
            
            .status-badge {
                padding: 4px 12px;
                border-radius: 20px;
                font-size: 0.8rem;
                font-weight: 600;
            }
            
            .status-passed { background: #c6f6d5; color: #276749; }
            .status-failed { background: #fed7d7; color: #c53030; }
            .status-warning { background: #feebc8; color: #c05621; }
            
            .progress-bar {
                height: 8px;
                background: var(--border-color);
                border-radius: 4px;
                overflow: hidden;
                margin: 10px 0;
            }
            
            .progress-fill {
                height: 100%;
                border-radius: 4px;
                transition: width 0.3s ease;
            }
            
            .progress-success { background: var(--success-color); }
            .progress-warning { background: var(--warning-color); }
            .progress-danger { background: var(--danger-color); }
            
            table {
                width: 100%;
                border-collapse: collapse;
            }
            
            th, td {
                padding: 12px;
                text-align: left;
                border-bottom: 1px solid var(--border-color);
            }
            
            th {
                background: var(--bg-color);
                font-weight: 600;
            }
            
            tr:hover {
                background: var(--bg-color);
            }
            
            .chart-container {
                position: relative;
                height: 300px;
                margin: 20px 0;
            }
            
            .footer {
                text-align: center;
                padding: 20px;
                color: var(--text-muted);
                font-size: 0.9rem;
            }
            
            .theme-toggle {
                position: fixed;
                top: 20px;
                right: 20px;
                background: var(--card-bg);
                border: 1px solid var(--border-color);
                border-radius: 8px;
                padding: 8px 16px;
                cursor: pointer;
                font-size: 0.9rem;
            }
            
            @media (max-width: 768px) {
                .grid {
                    grid-template-columns: 1fr;
                }
                
                .header h1 {
                    font-size: 1.5rem;
                }
                
                .metric-value {
                    font-size: 2rem;
                }
            }
        </style>
        """
    
    def _generate_js(self) -> str:
        """生成JavaScript脚本"""
        return """
        <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
        <script>
            // 主题切换
            function toggleTheme() {
                const body = document.body;
                const currentTheme = body.getAttribute('data-theme');
                body.setAttribute('data-theme', currentTheme === 'dark' ? 'light' : 'dark');
                localStorage.setItem('theme', body.getAttribute('data-theme'));
            }
            
            // 加载保存的主题
            document.addEventListener('DOMContentLoaded', function() {
                const savedTheme = localStorage.getItem('theme') || 'light';
                document.body.setAttribute('data-theme', savedTheme);
            });
            
            // 初始化图表
            function initCharts() {
                // 图表初始化代码将由Python动态生成
            }
        </script>
        """
    
    def _generate_summary_cards(self) -> str:
        """生成摘要卡片"""
        total_passed = sum(r.passed for r in self.test_results)
        total_failed = sum(r.failed for r in self.test_results)
        total_skipped = sum(r.skipped for r in self.test_results)
        total_tests = total_passed + total_failed + total_skipped
        
        pass_rate = (total_passed / total_tests * 100) if total_tests > 0 else 0
        
        avg_coverage = None
        coverages = [r.coverage for r in self.test_results if r.coverage is not None]
        if coverages:
            avg_coverage = sum(coverages) / len(coverages)
        
        cards = [
            {
                "title": "总测试数",
                "value": total_tests,
                "icon": "🧪",
                "color": "var(--primary-color)"
            },
            {
                "title": "通过率",
                "value": f"{pass_rate:.1f}%",
                "icon": "✅",
                "color": "var(--success-color)" if pass_rate >= 80 else "var(--warning-color)"
            },
            {
                "title": "失败数",
                "value": total_failed,
                "icon": "❌",
                "color": "var(--danger-color)" if total_failed > 0 else "var(--success-color)"
            },
            {
                "title": "覆盖率",
                "value": f"{avg_coverage:.1f}%" if avg_coverage else "N/A",
                "icon": "📊",
                "color": "var(--success-color)" if avg_coverage and avg_coverage >= 80 else "var(--warning-color)"
            },
        ]
        
        html = ['<div class="grid">']
        
        for card in cards:
            html.append(f'''
            <div class="card">
                <div class="card-header">
                    <span class="card-title">{card["icon"]} {card["title"]}</span>
                </div>
                <div class="metric-value" style="color: {card["color"]}">{card["value"]}</div>
            </div>
            ''')
        
        html.append('</div>')
        return '\n'.join(html)
    
    def _generate_test_table(self) -> str:
        """生成测试结果表格"""
        html = ['''
        <div class="card">
            <div class="card-header">
                <span class="card-title">📋 测试结果详情</span>
            </div>
            <table>
                <thead>
                    <tr>
                        <th>测试名称</th>
                        <th>通过</th>
                        <th>失败</th>
                        <th>跳过</th>
                        <th>耗时</th>
                        <th>覆盖率</th>
                        <th>状态</th>
                    </tr>
                </thead>
                <tbody>
        ''')
        
        for result in self.test_results:
            total = result.passed + result.failed + result.skipped
            pass_rate = (result.passed / total * 100) if total > 0 else 0
            
            status_class = "status-passed" if result.failed == 0 else "status-failed"
            status_text = "通过" if result.failed == 0 else "失败"
            
            coverage_str = f"{result.coverage:.1f}%" if result.coverage else "N/A"
            
            html.append(f'''
                <tr>
                    <td>{result.name}</td>
                    <td style="color: var(--success-color)">{result.passed}</td>
                    <td style="color: var(--danger-color)">{result.failed}</td>
                    <td style="color: var(--text-muted)">{result.skipped}</td>
                    <td>{result.duration:.2f}s</td>
                    <td>{coverage_str}</td>
                    <td><span class="status-badge {status_class}">{status_text}</span></td>
                </tr>
            ''')
        
        html.append('''
                </tbody>
            </table>
        </div>
        ''')
        
        return '\n'.join(html)
    
    def _generate_chart_html(self) -> str:
        """生成图表HTML"""
        if not self.charts:
            return ""
        
        html = ['<div class="grid">']
        
        for i, chart in enumerate(self.charts):
            chart_id = f"chart-{i}"
            
            html.append(f'''
            <div class="card">
                <div class="card-header">
                    <span class="card-title">{chart.title}</span>
                </div>
                <div class="chart-container">
                    <canvas id="{chart_id}"></canvas>
                </div>
            </div>
            ''')
        
        html.append('</div>')
        
        html.append('<script>')
        for i, chart in enumerate(self.charts):
            chart_id = f"chart-{i}"
            
            datasets_json = json.dumps(chart.datasets)
            labels_json = json.dumps(chart.labels)
            
            html.append(f'''
            new Chart(document.getElementById('{chart_id}'), {{
                type: '{chart.chart_type.value}',
                data: {{
                    labels: {labels_json},
                    datasets: {datasets_json}
                }},
                options: {{
                    responsive: true,
                    maintainAspectRatio: false,
                    plugins: {{
                        legend: {{
                            position: 'bottom'
                        }}
                    }}
                }}
            }});
            ''')
        
        html.append('</script>')
        
        return '\n'.join(html)
    
    def generate_html(self) -> str:
        """
        生成完整HTML仪表板
        
        Returns:
            HTML字符串
        """
        html = f'''
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>{self.config.title}</title>
    {self._generate_css()}
</head>
<body data-theme="light">
    <button class="theme-toggle" onclick="toggleTheme()">🌓 切换主题</button>
    
    <div class="container">
        <div class="header">
            <h1>🧪 {self.config.title}</h1>
            <div class="meta">
                <span>版本: {self.config.version}</span> | 
                <span>生成时间: {datetime.now().isoformat()}</span>
            </div>
        </div>
        
        {self._generate_summary_cards()}
        
        {self._generate_test_table()}
        
        {self._generate_chart_html()}
        
        <div class="footer">
            <p>© 2026 SPHARX Ltd. All Rights Reserved.</p>
            <p>From data intelligence emerges.</p>
        </div>
    </div>
    
    {self._generate_js()}
</body>
</html>
        '''
        
        return html
    
    def save_to_file(self, output_path: str) -> Path:
        """
        保存到文件
        
        Args:
            output_path: 输出路径
            
        Returns:
            文件路径
        """
        path = Path(output_path)
        path.parent.mkdir(parents=True, exist_ok=True)
        
        html = self.generate_html()
        
        with open(path, 'w', encoding='utf-8') as f:
            f.write(html)
        
        return path


def create_sample_dashboard() -> str:
    """
    创建示例仪表板
    
    Returns:
        HTML内容
    """
    generator = DashboardGenerator()
    
    generator.add_test_result(TestResult(
        name="单元测试",
        passed=150,
        failed=5,
        skipped=10,
        duration=45.2,
        coverage=85.5
    ))
    
    generator.add_test_result(TestResult(
        name="集成测试",
        passed=80,
        failed=2,
        skipped=5,
        duration=120.5,
        coverage=72.3
    ))
    
    generator.add_test_result(TestResult(
        name="契约测试",
        passed=25,
        failed=0,
        skipped=0,
        duration=15.8,
        coverage=95.0
    ))
    
    generator.add_test_result(TestResult(
        name="安全测试",
        passed=40,
        failed=1,
        skipped=2,
        duration=30.0,
        coverage=88.2
    ))
    
    generator.add_chart(ChartData(
        title="测试结果分布",
        labels=["单元测试", "集成测试", "契约测试", "安全测试"],
        datasets=[{
            "label": "通过",
            "data": [150, 80, 25, 40],
            "backgroundColor": "#48bb78"
        }, {
            "label": "失败",
            "data": [5, 2, 0, 1],
            "backgroundColor": "#f56565"
        }],
        chart_type=ChartType.BAR
    ))
    
    generator.add_chart(ChartData(
        title="覆盖率对比",
        labels=["单元测试", "集成测试", "契约测试", "安全测试"],
        datasets=[{
            "label": "覆盖率 (%)",
            "data": [85.5, 72.3, 95.0, 88.2],
            "backgroundColor": "#667eea"
        }],
        chart_type=ChartType.LINE
    ))
    
    return generator.generate_html()


if __name__ == "__main__":
    print("=" * 60)
    print("AgentOS 测试报告可视化仪表板")
    print("Copyright (c) 2026 SPHARX Ltd.")
    print("=" * 60)
    
    html = create_sample_dashboard()
    
    output_path = Path("tests/reports/dashboard/test_dashboard.html")
    output_path.parent.mkdir(parents=True, exist_ok=True)
    
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(html)
    
    print(f"\n✅ 仪表板已生成: {output_path}")
