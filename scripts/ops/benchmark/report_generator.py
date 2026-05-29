#!/usr/bin/env python3
"""
AgentOS 性能基准测试报告生成器

生成专业、美观的性能测试报告，支持多种格式：
1. HTML 报告 - 交互式、可视化
2. Markdown 报告 - 轻量级、版本控制友好
3. PDF 报告 - 正式文档、打印友好
4. JSON 报告 - 机器可读、自动化处理
5. Console 报告 - 命令行即时查看

设计原则：
1. 可读性 - 报告清晰易懂，突出重点
2. 可视化 - 丰富的图表和数据可视化
3. 标准化 - 一致的报告格式和结构
4. 可定制 - 支持自定义模板和样式
5. 多平台 - 支持跨平台生成和查看

@version v1.0.0
@date 2026-04-11
@copyright (c) 2026 SPHARX. All Rights Reserved.
"""

import base64
import json
import os
import sys
import textwrap
from dataclasses import asdict, dataclass, field
from datetime import datetime
from enum import Enum
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple, Union

import matplotlib.pyplot as plt
import numpy as np
from jinja2 import Environment, FileSystemLoader, select_autoescape

# 尝试导入可选的依赖
try:
    import pandas as pd
    PANDAS_AVAILABLE = True
except ImportError:
    PANDAS_AVAILABLE = False

try:
    from weasyprint import HTML
    WEASYPRINT_AVAILABLE = True
except ImportError:
    WEASYPRINT_AVAILABLE = False


class ReportFormat(Enum):
    """报告格式"""
    HTML = "html"
    MARKDOWN = "markdown"
    PDF = "pdf"
    JSON = "json"
    CONSOLE = "console"


class ReportTheme(Enum):
    """报告主题"""
    LIGHT = "light"
    DARK = "dark"
    AGENTOS = "agentos"  # AgentOS品牌主题


@dataclass
class ReportSection:
    """报告部分"""
    title: str
    content: str
    level: int = 2  # 标题级别
    metadata: Dict[str, Any] = field(default_factory=dict)


@dataclass
class ReportChart:
    """报告图表"""
    title: str
    chart_type: str  # line, bar, scatter, histogram, boxplot, heatmap
    data: Dict[str, Any]
    width: int = 800
    height: int = 400
    description: str = ""
    metadata: Dict[str, Any] = field(default_factory=dict)


@dataclass
class ReportTable:
    """报告表格"""
    title: str
    headers: List[str]
    rows: List[List[Any]]
    description: str = ""
    metadata: Dict[str, Any] = field(default_factory=dict)


@dataclass
class ReportMetadata:
    """报告元数据"""
    project_name: str = "AgentOS"
    project_version: str = "1.0.0"
    report_title: str = "性能基准测试报告"
    report_subtitle: str = ""
    author: str = "AgentOS Benchmark Framework"
    generation_date: str = ""
    benchmark_date: str = ""
    environment: Dict[str, str] = field(default_factory=dict)
    tags: List[str] = field(default_factory=list)
    
    def __post_init__(self):
        if not self.generation_date:
            self.generation_date = datetime.now().isoformat()


class ReportGenerator:
    """报告生成器"""
    
    def __init__(self, 
                 metadata: Optional[ReportMetadata] = None,
                 theme: ReportTheme = ReportTheme.AGENTOS,
                 output_dir: Optional[Path] = None):
        
        self.metadata = metadata or ReportMetadata()
        self.theme = theme
        self.output_dir = output_dir or Path.cwd() / "reports"
        self.output_dir.mkdir(parents=True, exist_ok=True)
        
        self.sections: List[ReportSection] = []
        self.charts: List[ReportChart] = []
        self.tables: List[ReportTable] = []
        
        # 设置Jinja2模板环境
        template_dir = Path(__file__).parent / "templates"
        if not template_dir.exists():
            template_dir.mkdir(parents=True, exist_ok=True)
            self._create_default_templates(template_dir)
        
        self.jinja_env = Environment(
            loader=FileSystemLoader(template_dir),
            autoescape=select_autoescape(['html', 'xml'])
        )
        
        # 设置Matplotlib样式
        self._setup_matplotlib_style()
    
    def _setup_matplotlib_style(self):
        """设置Matplotlib样式"""
        plt.style.use('seaborn-v0_8-darkgrid')
        
        # AgentOS品牌颜色
        agentos_colors = {
            'primary': '#4A90E2',    # 主蓝色
            'secondary': '#50E3C2',   # 青色
            'accent': '#B8E986',      # 浅绿色
            'warning': '#F5A623',     # 橙色
            'danger': '#D0021B',      # 红色
            'dark': '#333333',        # 深灰色
            'light': '#F8F8F8'        # 浅灰色
        }
        
        if self.theme == ReportTheme.DARK:
            plt.rcParams.update({
                'axes.facecolor': '#1E1E1E',
                'figure.facecolor': '#1E1E1E',
                'axes.edgecolor': '#FFFFFF',
                'axes.labelcolor': '#FFFFFF',
                'text.color': '#FFFFFF',
                'xtick.color': '#FFFFFF',
                'ytick.color': '#FFFFFF'
            })
    
    def _create_default_templates(self, template_dir: Path):
        """创建默认模板"""
        # HTML模板
        html_template = """<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>{{ metadata.report_title }} - {{ metadata.project_name }}</title>
    <style>
        :root {
            --primary-color: #4A90E2;
            --secondary-color: #50E3C2;
            --accent-color: #B8E986;
            --warning-color: #F5A623;
            --danger-color: #D0021B;
            --dark-color: #333333;
            --light-color: #F8F8F8;
            --text-color: #333333;
            --bg-color: #FFFFFF;
        }
        
        body.dark-theme {
            --text-color: #F8F8F8;
            --bg-color: #1E1E1E;
            --dark-color: #F8F8F8;
            --light-color: #333333;
        }
        
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, 'Helvetica Neue', Arial, sans-serif;
            line-height: 1.6;
            color: var(--text-color);
            background-color: var(--bg-color);
            padding: 20px;
            transition: all 0.3s ease;
        }
        
        .container {
            max-width: 1200px;
            margin: 0 auto;
        }
        
        header {
            text-align: center;
            margin-bottom: 40px;
            padding-bottom: 20px;
            border-bottom: 2px solid var(--primary-color);
        }
        
        h1 {
            color: var(--primary-color);
            font-size: 2.5rem;
            margin-bottom: 10px;
        }
        
        .subtitle {
            color: var(--secondary-color);
            font-size: 1.2rem;
            font-weight: 300;
        }
        
        .metadata {
            display: flex;
            justify-content: space-between;
            flex-wrap: wrap;
            margin-top: 20px;
            padding: 15px;
            background-color: rgba(74, 144, 226, 0.1);
            border-radius: 8px;
        }
        
        .metadata-item {
            margin: 5px 10px;
        }
        
        .metadata-label {
            font-weight: bold;
            color: var(--primary-color);
        }
        
        section {
            margin-bottom: 40px;
        }
        
        h2 {
            color: var(--primary-color);
            font-size: 1.8rem;
            margin-bottom: 15px;
            padding-bottom: 8px;
            border-bottom: 1px solid rgba(74, 144, 226, 0.3);
        }
        
        h3 {
            color: var(--secondary-color);
            font-size: 1.4rem;
            margin: 20px 0 10px 0;
        }
        
        .content {
            line-height: 1.8;
            margin-bottom: 20px;
        }
        
        .chart-container {
            margin: 20px 0;
            padding: 15px;
            background-color: var(--light-color);
            border-radius: 8px;
            box-shadow: 0 2px 10px rgba(0, 0, 0, 0.1);
        }
        
        .chart-title {
            font-size: 1.2rem;
            font-weight: bold;
            margin-bottom: 10px;
            color: var(--dark-color);
        }
        
        .chart-description {
            font-size: 0.9rem;
            color: #666;
            margin-bottom: 15px;
        }
        
        .table-container {
            margin: 20px 0;
            overflow-x: auto;
        }
        
        table {
            width: 100%;
            border-collapse: collapse;
            margin-bottom: 20px;
        }
        
        th {
            background-color: var(--primary-color);
            color: white;
            padding: 12px 15px;
            text-align: left;
            font-weight: 600;
        }
        
        td {
            padding: 10px 15px;
            border-bottom: 1px solid #ddd;
        }
        
        tr:nth-child(even) {
            background-color: rgba(0, 0, 0, 0.02);
        }
        
        tr:hover {
            background-color: rgba(74, 144, 226, 0.1);
        }
        
        .status-badge {
            display: inline-block;
            padding: 4px 8px;
            border-radius: 12px;
            font-size: 0.8rem;
            font-weight: bold;
        }
        
        .status-success {
            background-color: #B8E986;
            color: #333;
        }
        
        .status-warning {
            background-color: #F5A623;
            color: white;
        }
        
        .status-error {
            background-color: #D0021B;
            color: white;
        }
        
        .summary-cards {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
            gap: 20px;
            margin: 30px 0;
        }
        
        .summary-card {
            padding: 20px;
            background: linear-gradient(135deg, var(--primary-color), var(--secondary-color));
            color: white;
            border-radius: 10px;
            box-shadow: 0 4px 15px rgba(0, 0, 0, 0.2);
        }
        
        .card-value {
            font-size: 2.5rem;
            font-weight: bold;
            margin-bottom: 5px;
        }
        
        .card-label {
            font-size: 0.9rem;
            opacity: 0.9;
        }
        
        footer {
            text-align: center;
            margin-top: 40px;
            padding-top: 20px;
            border-top: 1px solid #ddd;
            color: #666;
            font-size: 0.9rem;
        }
        
        .theme-toggle {
            position: fixed;
            top: 20px;
            right: 20px;
            padding: 10px 15px;
            background-color: var(--primary-color);
            color: white;
            border: none;
            border-radius: 5px;
            cursor: pointer;
            z-index: 1000;
        }
        
        @media (max-width: 768px) {
            h1 {
                font-size: 2rem;
            }
            
            .metadata {
                flex-direction: column;
            }
            
            .summary-cards {
                grid-template-columns: 1fr;
            }
        }
    </style>
</head>
<body>
    <button class="theme-toggle" onclick="toggleTheme()">切换主题</button>
    
    <div class="container">
        <header>
            <h1>{{ metadata.report_title }}</h1>
            {% if metadata.report_subtitle %}
            <div class="subtitle">{{ metadata.report_subtitle }}</div>
            {% endif %}
            
            <div class="metadata">
                <div class="metadata-item">
                    <span class="metadata-label">项目:</span> {{ metadata.project_name }} {{ metadata.project_version }}
                </div>
                <div class="metadata-item">
                    <span class="metadata-label">生成时间:</span> {{ metadata.generation_date }}
                </div>
                <div class="metadata-item">
                    <span class="metadata-label">测试时间:</span> {{ metadata.benchmark_date }}
                </div>
                <div class="metadata-item">
                    <span class="metadata-label">作者:</span> {{ metadata.author }}
                </div>
            </div>
        </header>
        
        <main>
            {% for section in sections %}
            <section>
                <h{{ section.level }}>{{ section.title }}</h{{ section.level }}>
                <div class="content">
                    {{ section.content|safe }}
                </div>
            </section>
            {% endfor %}
            
            {% if charts %}
            <section>
                <h2>图表分析</h2>
                {% for chart in charts %}
                <div class="chart-container">
                    <div class="chart-title">{{ chart.title }}</div>
                    {% if chart.description %}
                    <div class="chart-description">{{ chart.description }}</div>
                    {% endif %}
                    <div class="chart-image">
                        <img src="data:image/png;base64,{{ chart.data.image }}" alt="{{ chart.title }}" style="max-width: 100%;">
                    </div>
                </div>
                {% endfor %}
            </section>
            {% endif %}
            
            {% if tables %}
            <section>
                <h2>数据表格</h2>
                {% for table in tables %}
                <div class="table-container">
                    <h3>{{ table.title }}</h3>
                    {% if table.description %}
                    <p>{{ table.description }}</p>
                    {% endif %}
                    <table>
                        <thead>
                            <tr>
                                {% for header in table.headers %}
                                <th>{{ header }}</th>
                                {% endfor %}
                            </tr>
                        </thead>
                        <tbody>
                            {% for row in table.rows %}
                            <tr>
                                {% for cell in row %}
                                <td>{{ cell }}</td>
                                {% endfor %}
                            </tr>
                            {% endfor %}
                        </tbody>
                    </table>
                </div>
                {% endfor %}
            </section>
            {% endif %}
        </main>
        
        <footer>
            <p>© {{ now.year }} {{ metadata.project_name }}. 所有权利保留。</p>
            <p>报告由 AgentOS 性能基准测试框架生成 | 始于数据，终于智能</p>
        </footer>
    </div>
    
    <script>
        function toggleTheme() {
            document.body.classList.toggle('dark-theme');
            const button = document.querySelector('.theme-toggle');
            button.textContent = document.body.classList.contains('dark-theme') ? '切换浅色主题' : '切换深色主题';
        }
        
        // 保存主题偏好
        if (localStorage.getItem('prefers-dark-theme') === 'true') {
            document.body.classList.add('dark-theme');
            document.querySelector('.theme-toggle').textContent = '切换浅色主题';
        }
        
        document.querySelector('.theme-toggle').addEventListener('click', function() {
            const isDark = document.body.classList.contains('dark-theme');
            localStorage.setItem('prefers-dark-theme', isDark ? 'false' : 'true');
        });
    </script>
</body>
</html>"""
        
        (template_dir / "report.html").write_text(html_template, encoding='utf-8')
        
        # Markdown模板
        markdown_template = """# {{ metadata.report_title }}

{% if metadata.report_subtitle %}
> {{ metadata.report_subtitle }}
{% endif %}

**项目:** {{ metadata.project_name }} {{ metadata.project_version }}  
**生成时间:** {{ metadata.generation_date }}  
**测试时间:** {{ metadata.benchmark_date }}  
**作者:** {{ metadata.author }}

{% for section in sections %}
{{ '#' * section.level }} {{ section.title }}

{{ section.content }}

{% endfor %}

{% if charts %}
## 图表分析

{% for chart in charts %}
### {{ chart.title }}

{% if chart.description %}
{{ chart.description }}
{% endif %}

![{{ chart.title }}](data:image/png;base64,{{ chart.data.image }})

{% endfor %}
{% endif %}

{% if tables %}
## 数据表格

{% for table in tables %}
### {{ table.title }}

{% if table.description %}
{{ table.description }}
{% endif %}

| {{ table.headers|join(' | ') }} |
|{% for header in table.headers %}---|{% endfor %}
{% for row in table.rows %}| {{ row|join(' | ') }} |
{% endfor %}

{% endfor %}
{% endif %}

---

*报告由 AgentOS 性能基准测试框架生成*  
*© {{ now.year }} {{ metadata.project_name }}. 所有权利保留.*
"""
        
        (template_dir / "report.md").write_text(markdown_template, encoding='utf-8')
    
    def add_section(self, section: ReportSection):
        """添加报告部分"""
        self.sections.append(section)
    
    def add_chart(self, chart: ReportChart):
        """添加图表"""
        self.charts.append(chart)
    
    def add_table(self, table: ReportTable):
        """添加表格"""
        self.tables.append(table)
    
    def add_summary_section(self, benchmark_results: List[Dict[str, Any]]):
        """添加摘要部分"""
        if not benchmark_results:
            return
        
        # 创建摘要卡片
        summary_content = '<div class="summary-cards">\n'
        
        # 计算关键指标
        total_tests = len(benchmark_results)
        successful_tests = sum(1 for r in benchmark_results if r.get('status') == 'completed')
        failed_tests = total_tests - successful_tests
        
        # 吞吐量统计
        throughput_values = []
        for result in benchmark_results:
            for metric in result.get('metrics', []):
                if metric.get('name') == 'throughput':
                    throughput_values.append(metric.get('value', 0))
        
        avg_throughput = np.mean(throughput_values) if throughput_values else 0
        
        # 延迟统计
        latency_values = []
        for result in benchmark_results:
            for metric in result.get('metrics', []):
                if metric.get('name') == 'latency':
                    latency_values.append(metric.get('value', 0))
        
        avg_latency = np.mean(latency_values) if latency_values else 0
        
        # 添加卡片
        cards = [
            ("总测试数", total_tests, "个"),
            ("成功测试", successful_tests, "个"),
            ("失败测试", failed_tests, "个"),
            ("平均吞吐量", f"{avg_throughput:.2f}", "ops/s"),
            ("平均延迟", f"{avg_latency:.2f}", "ms"),
            ("成功率", f"{(successful_tests/total_tests*100):.1f}", "%")
        ]
        
        for label, value, unit in cards:
            summary_content += f'''
            <div class="summary-card">
                <div class="card-value">{value}</div>
                <div class="card-label">{label} ({unit})</div>
            </div>
            '''
        
        summary_content += '</div>\n'
        
        # 添加状态概览
        summary_content += '<h3>测试状态概览</h3>\n<ul>\n'
        for result in benchmark_results:
            test_name = result.get('test_name', '未知测试')
            status = result.get('status', 'unknown')
            
            status_class = {
                'completed': 'status-success',
                'failed': 'status-error',
                'running': 'status-warning',
                'pending': 'status-warning'
            }.get(status, '')
            
            status_text = {
                'completed': '成功',
                'failed': '失败',
                'running': '运行中',
                'pending': '待开始'
            }.get(status, status)
            
            summary_content += f'<li>{test_name}: <span class="status-badge {status_class}">{status_text}</span></li>\n'
        
        summary_content += '</ul>\n'
        
        self.add_section(ReportSection(
            title="测试摘要",
            content=summary_content,
            level=2
        ))
    
    def generate_chart_image(self, chart: ReportChart) -> str:
        """生成图表图像并返回base64编码"""
        plt.figure(figsize=(chart.width/100, chart.height/100), dpi=100)
        
        try:
            if chart.chart_type == 'line':
                self._create_line_chart(chart)
            elif chart.chart_type == 'bar':
                self._create_bar_chart(chart)
            elif chart.chart_type == 'scatter':
                self._create_scatter_chart(chart)
            elif chart.chart_type == 'histogram':
                self._create_histogram_chart(chart)
            elif chart.chart_type == 'boxplot':
                self._create_boxplot_chart(chart)
            elif chart.chart_type == 'heatmap':
                self._create_heatmap_chart(chart)
            else:
                raise ValueError(f"不支持的图表类型: {chart.chart_type}")
            
            # 保存图表到内存
            from io import BytesIO
            buffer = BytesIO()
            plt.savefig(buffer, format='png', dpi=100, bbox_inches='tight')
            plt.close()
            
            # 转换为base64
            buffer.seek(0)
            image_base64 = base64.b64encode(buffer.read()).decode('utf-8')
            
            return image_base64
            
        finally:
            plt.close('all')
    
    def _create_line_chart(self, chart: ReportChart):
        """创建折线图"""
        data = chart.data
        
        x = data.get('x', [])
        y = data.get('y', [])
        labels = data.get('labels', [])
        
        if len(x) != len(y):
            raise ValueError("x和y数据长度必须相同")
        
        plt.plot(x, y, marker='o', linewidth=2, markersize=6)
        
        if 'x_label' in data:
            plt.xlabel(data['x_label'])
        if 'y_label' in data:
            plt.ylabel(data['y_label'])
        
        plt.title(chart.title)
        plt.grid(True, alpha=0.3)
        
        if labels and len(labels) == len(x):
            # 添加标签（避免重叠）
            for i, (xi, yi, label) in enumerate(zip(x, y, labels)):
                if i % max(1, len(x) // 10) == 0:  # 每10个点显示一个标签
                    plt.annotate(label, (xi, yi), textcoords="offset points", 
                                xytext=(0,10), ha='center', fontsize=8)
    
    def _create_bar_chart(self, chart: ReportChart):
        """创建柱状图"""
        data = chart.data
        
        categories = data.get('categories', [])
        values = data.get('values', [])
        colors = data.get('colors', None)
        
        if len(categories) != len(values):
            raise ValueError("分类和值数据长度必须相同")
        
        bars = plt.bar(categories, values, color=colors)
        
        if 'x_label' in data:
            plt.xlabel(data['x_label'])
        if 'y_label' in data:
            plt.ylabel(data['y_label'])
        
        plt.title(chart.title)
        plt.xticks(rotation=45, ha='right')
        plt.grid(True, alpha=0.3, axis='y')
        
        # 在柱子上显示数值
        for bar in bars:
            height = bar.get_height()
            plt.text(bar.get_x() + bar.get_width()/2., height + max(values)*0.01,
                    f'{height:.2f}', ha='center', va='bottom', fontsize=9)
    
    def _create_scatter_chart(self, chart: ReportChart):
        """创建散点图"""
        data = chart.data
        
        x = data.get('x', [])
        y = data.get('y', [])
        sizes = data.get('sizes', 20)
        colors = data.get('colors', None)
        
        if len(x) != len(y):
            raise ValueError("x和y数据长度必须相同")
        
        scatter = plt.scatter(x, y, s=sizes, c=colors, alpha=0.6, edgecolors='w', linewidth=0.5)
        
        if 'x_label' in data:
            plt.xlabel(data['x_label'])
        if 'y_label' in data:
            plt.ylabel(data['y_label'])
        
        plt.title(chart.title)
        plt.grid(True, alpha=0.3)
        
        # 添加颜色条（如果有颜色数据）
        if colors is not None and isinstance(colors, (list, np.ndarray)):
            plt.colorbar(scatter)
    
    def _create_histogram_chart(self, chart: ReportChart):
        """创建直方图"""
        data = chart.data
        
        values = data.get('values', [])
        bins = data.get('bins', 20)
        density = data.get('density', False)
        
        plt.hist(values, bins=bins, density=density, alpha=0.7, edgecolor='black')
        
        if 'x_label' in data:
            plt.xlabel(data['x_label'])
        if 'y_label' in data:
            plt.ylabel('频数' if not density else '概率密度')
        
        plt.title(chart.title)
        plt.grid(True, alpha=0.3)
    
    def _create_boxplot_chart(self, chart: ReportChart):
        """创建箱线图"""
        data = chart.data
        
        # 支持多组数据
        if 'data_groups' in data:
            data_groups = data['data_groups']
            labels = data.get('labels', [f'组{i+1}' for i in range(len(data_groups))])
            
            boxes = plt.boxplot(data_groups, labels=labels, patch_artist=True)
            
            # 设置颜色
            colors = data.get('colors', ['#4A90E2', '#50E3C2', '#B8E986', '#F5A623', '#D0021B'])
            for patch, color in zip(boxes['boxes'], colors * (len(data_groups) // len(colors) + 1)):
                patch.set_facecolor(color)
        else:
            values = data.get('values', [])
            plt.boxplot(values)
        
        if 'y_label' in data:
            plt.ylabel(data['y_label'])
        
        plt.title(chart.title)
        plt.grid(True, alpha=0.3, axis='y')
    
    def _create_heatmap_chart(self, chart: ReportChart):
        """创建热力图"""
        data = chart.data
        
        matrix = data.get('matrix', [])
        x_labels = data.get('x_labels', [])
        y_labels = data.get('y_labels', [])
        
        if not matrix:
            raise ValueError("矩阵数据不能为空")
        
        # 转换为numpy数组
        matrix_np = np.array(matrix)
        
        # 创建热力图
        im = plt.imshow(matrix_np, cmap='viridis', aspect='auto')
        
        # 设置坐标轴标签
        if x_labels:
            plt.xticks(range(len(x_labels)), x_labels, rotation=45, ha='right')
        if y_labels:
            plt.yticks(range(len(y_labels)), y_labels)
        
        # 添加颜色条
        plt.colorbar(im)
        
        # 在每个单元格中添加数值
        for i in range(matrix_np.shape[0]):
            for j in range(matrix_np.shape[1]):
                text = plt.text(j, i, f'{matrix_np[i, j]:.2f}',
                              ha="center", va="center", 
                              color="w" if matrix_np[i, j] > matrix_np.max()/2 else "k",
                              fontsize=8)
        
        plt.title(chart.title)
    
    def generate_report(self, 
                       format: ReportFormat = ReportFormat.HTML,
                       filename: Optional[str] = None) -> Path:
        """生成报告"""
        if filename is None:
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            filename = f"benchmark_report_{timestamp}.{format.value}"
        
        output_path = self.output_dir / filename
        
        # 准备模板数据
        template_data = {
            'metadata': asdict(self.metadata),
            'sections': [asdict(s) for s in self.sections],
            'charts': [],
            'tables': [asdict(t) for t in self.tables],
            'now': datetime.now()
        }
        
        # 生成图表图像
        for chart in self.charts:
            chart_dict = asdict(chart)
            try:
                image_base64 = self.generate_chart_image(chart)
                chart_dict['data'] = {'image': image_base64}
            except Exception as e:
                print(f"图表生成失败: {e}", file=sys.stderr)
                chart_dict['data'] = {'image': '', 'error': str(e)}
            
            template_data['charts'].append(chart_dict)
        
        # 根据格式生成报告
        if format == ReportFormat.HTML:
            self._generate_html_report(template_data, output_path)
        elif format == ReportFormat.MARKDOWN:
            self._generate_markdown_report(template_data, output_path)
        elif format == ReportFormat.PDF:
            self._generate_pdf_report(template_data, output_path)
        elif format == ReportFormat.JSON:
            self._generate_json_report(template_data, output_path)
        elif format == ReportFormat.CONSOLE:
            self._generate_console_report(template_data)
            return output_path  # 控制台报告不保存文件
        
        print(f"报告已生成: {output_path}")
        return output_path
    
    def _generate_html_report(self, template_data: Dict, output_path: Path):
        """生成HTML报告"""
        template = self.jinja_env.get_template('report.html')
        html_content = template.render(**template_data)
        
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write(html_content)
    
    def _generate_markdown_report(self, template_data: Dict, output_path: Path):
        """生成Markdown报告"""
        template = self.jinja_env.get_template('report.md')
        markdown_content = template.render(**template_data)
        
        # 处理图表（Markdown中嵌入base64图片）
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write(markdown_content)
    
    def _generate_pdf_report(self, template_data: Dict, output_path: Path):
        """生成PDF报告"""
        if not WEASYPRINT_AVAILABLE:
            raise ImportError("生成PDF报告需要安装 weasyprint 库: pip install weasyprint")
        
        # 先生成HTML
        html_template = self.jinja_env.get_template('report.html')
        html_content = html_template.render(**template_data)
        
        # 转换为PDF
        HTML(string=html_content).write_pdf(output_path)
    
    def _generate_json_report(self, template_data: Dict, output_path: Path):
        """生成JSON报告"""
        with open(output_path, 'w', encoding='utf-8') as f:
            json.dump(template_data, f, indent=2, ensure_ascii=False)
    
    def _generate_console_report(self, template_data: Dict):
        """生成控制台报告"""
        print("\n" + "="*80)
        print(f"性能基准测试报告")
        print("="*80)
        
        metadata = template_data['metadata']
        print(f"\n项目: {metadata['project_name']} {metadata['project_version']}")
        print(f"测试时间: {metadata['benchmark_date']}")
        print(f"生成时间: {metadata['generation_date']}")
        print(f"作者: {metadata['author']}")
        
        if metadata['report_subtitle']:
            print(f"\n{metadata['report_subtitle']}")
        
        print("\n" + "="*80)
        print("报告内容")
        print("="*80)
        
        for section in template_data['sections']:
            print(f"\n{section['title']}")
            print("-" * len(section['title']))
            
            # 简单处理HTML内容
            content = section['content']
            # 移除HTML标签（简单方法）
            import re
            content_text = re.sub(r'<[^>]+>', '', content)
            content_text = re.sub(r'\s+', ' ', content_text).strip()
            
            if content_text:
                # 自动换行
                for line in textwrap.wrap(content_text, width=78):
                    print(f"  {line}")
        
        if template_data['tables']:
            print("\n" + "="*80)
            print("数据表格")
            print("="*80)
            
            for table in template_data['tables']:
                print(f"\n{table['title']}")
                if table['description']:
                    print(f"  {table['description']}")
                
                # 打印表格
                headers = table['headers']
                rows = table['rows']
                
                # 计算列宽
                col_widths = []
                for i, header in enumerate(headers):
                    max_width = len(str(header))
                    for row in rows:
                        if i < len(row):
                            max_width = max(max_width, len(str(row[i])))
                    col_widths.append(max_width)
                
                # 打印表头
                header_line = " | ".join(str(h).ljust(w) for h, w in zip(headers, col_widths))
                print("  " + header_line)
                print("  " + "-" * len(header_line))
                
                # 打印数据行
                for row in rows:
                    row_line = " | ".join(str(cell).ljust(w) for cell, w in zip(row, col_widths))
                    print("  " + row_line)
        
        print("\n" + "="*80)
        print("报告结束")
        print("="*80)


# 示例使用
def example_usage():
    """示例使用"""
    # 创建报告元数据
    metadata = ReportMetadata(
        project_name="AgentOS",
        project_version="1.0.0",
        report_title="AgentOS 性能基准测试报告",
        report_subtitle="网关服务吞吐量测试",
        author="AgentOS 性能测试团队",
        benchmark_date="2026-04-11T10:30:00",
        environment={
            "操作系统": "Ubuntu 22.04",
            "CPU": "AMD Ryzen 9 5900X",
            "内存": "64GB",
            "Python版本": "3.10.12"
        },
        tags=["性能测试", "网关", "吞吐量"]
    )
    
    # 创建报告生成器
    generator = ReportGenerator(metadata=metadata, theme=ReportTheme.AGENTOS)
    
    # 添加摘要部分（模拟数据）
    benchmark_results = [
        {
            "test_name": "网关HTTP吞吐量测试",
            "status": "completed",
            "metrics": [
                {"name": "throughput", "value": 1250.5},
                {"name": "latency", "value": 12.3}
            ]
        },
        {
            "test_name": "网关WebSocket连接测试",
            "status": "completed",
            "metrics": [
                {"name": "throughput", "value": 850.2},
                {"name": "latency", "value": 25.7}
            ]
        },
        {
            "test_name": "网关并发连接测试",
            "status": "failed",
            "metrics": []
        }
    ]
    
    generator.add_summary_section(benchmark_results)
    
    # 添加详细分析部分
    generator.add_section(ReportSection(
        title="测试环境",
        content="""
        <p>本次测试在标准开发环境中进行，模拟真实生产环境配置。</p>
        <ul>
            <li><strong>测试工具</strong>: AgentOS 性能基准测试框架 v1.0.0</li>
            <li><strong>测试时长</strong>: 每个测试运行30分钟，包含5分钟预热和5分钟冷却</li>
            <li><strong>数据收集</strong>: 每秒收集一次性能指标</li>
            <li><strong>测试目标</strong>: 验证网关服务在高并发场景下的稳定性和性能</li>
        </ul>
        """,
        level=2
    ))
    
    # 添加图表
    generator.add_chart(ReportChart(
        title="网关服务吞吐量随时间变化",
        chart_type="line",
        data={
            "x": list(range(0, 1800, 30)),  # 30分钟，每30秒一个点
            "y": [1000 + np.random.normal(0, 100) for _ in range(60)],
            "x_label": "时间 (秒)",
            "y_label": "吞吐量 (请求/秒)"
        },
        description="网关HTTP服务的吞吐量在测试期间保持稳定，平均吞吐量为1250.5请求/秒。"
    ))
    
    # 添加表格
    generator.add_table(ReportTable(
        title="性能指标汇总",
        headers=["指标", "平均值", "最小值", "最大值", "P95", "单位"],
        rows=[
            ["吞吐量", "1250.5", "980.2", "1450.8", "1380.3", "请求/秒"],
            ["延迟", "12.3", "5.1", "45.6", "28.4", "毫秒"],
            ["CPU使用率", "65.2", "45.8", "89.3", "78.6", "%"],
            ["内存使用", "1.2", "0.8", "1.8", "1.6", "GB"]
        ],
        description="各项性能指标在测试期间的统计汇总。"
    ))
    
    # 生成报告
    report_path = generator.generate_report(format=ReportFormat.HTML)
    print(f"HTML报告已生成: {report_path}")
    
    # 也可以生成其他格式
    # generator.generate_report(format=ReportFormat.MARKDOWN)
    # generator.generate_report(format=ReportFormat.PDF)
    # generator.generate_report(format=ReportFormat.CONSOLE)


# 命令行接口
def main():
    """命令行主函数"""
    import argparse
    
    parser = argparse.ArgumentParser(
        description="AgentOS 性能基准测试报告生成器",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
使用示例:
  # 从JSON文件生成HTML报告
  python report_generator.py --input results.json --format html
  
  # 生成多格式报告
  python report_generator.py --input results.json --format all
  
  # 自定义输出目录
  python report_generator.py --input results.json --format html --output ./reports
        """
    )
    
    parser.add_argument(
        "--input", "-i",
        type=Path,
        help="基准测试结果文件（JSON格式）"
    )
    
    parser.add_argument(
        "--format", "-f",
        type=str,
        choices=["html", "markdown", "pdf", "json", "console", "all"],
        default="html",
        help="报告格式"
    )
    
    parser.add_argument(
        "--output", "-o",
        type=Path,
        help="输出目录"
    )
    
    parser.add_argument(
        "--theme", "-t",
        type=str,
        choices=["light", "dark", "agentos"],
        default="agentos",
        help="报告主题"
    )
    
    parser.add_argument(
        "--title", "-T",
        type=str,
        default="性能基准测试报告",
        help="报告标题"
    )
    
    parser.add_argument(
        "--example", "-e",
        action="store_true",
        help="生成示例报告"
    )
    
    args = parser.parse_args()
    
    if args.example:
        print("生成示例报告...")
        example_usage()
        return 0
    
    if not args.input:
        print("错误: 需要指定输入文件")
        return 1
    
    if not args.input.exists():
        print(f"错误: 文件不存在: {args.input}")
        return 1
    
    # 加载数据
    with open(args.input, 'r', encoding='utf-8') as f:
        data = json.load(f)
    
    # 创建报告生成器
    theme = ReportTheme(args.theme)
    generator = ReportGenerator(
        metadata=ReportMetadata(
            report_title=args.title,
            benchmark_date=data.get('timestamp', datetime.now().isoformat())
        ),
        theme=theme,
        output_dir=args.output
    )
    
    # 根据数据添加内容
    if 'benchmark_results' in data:
        generator.add_summary_section(data['benchmark_results'])
    
    # 生成报告
    formats = [ReportFormat(args.format)] if args.format != 'all' else [
        ReportFormat.HTML, ReportFormat.MARKDOWN, ReportFormat.PDF, ReportFormat.JSON
    ]
    
    for fmt in formats:
        try:
            report_path = generator.generate_report(format=fmt)
            print(f"{fmt.value.upper()}报告已生成: {report_path}")
        except Exception as e:
            print(f"生成{fmt.value.upper()}报告失败: {e}")
    
    return 0


if __name__ == "__main__":
    main()