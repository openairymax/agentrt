#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
AgentOS 测试模块综合报告生成器

生成包含所有测试结果的综合性 HTML 报告，支持：
- 单元测试结果
- 集成测试结果
- 安全测试结果
- 契约测试结果
- 性能基准测试结果
- 覆盖率报告

Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
"""

import json
import os
import sys
from datetime import datetime
from pathlib import Path
from typing import Any, Dict, List, Optional


class ReportGenerator:
    """
    综合报告生成器。

    负责收集各测试阶段的报告并生成综合性 HTML 报告。
    """

    def __init__(self, report_dir: str = "reports"):
        """
        初始化报告生成器。

        Args:
            report_dir: 报告目录路径
        """
        self.report_dir = Path(report_dir)
        self.artifacts_dir = Path("artifacts")
        self.output_dir = self.report_dir / "final"

        self.output_dir.mkdir(parents=True, exist_ok=True)

        self.test_results: Dict[str, Any] = {}
        self.coverage_data: Dict[str, Any] = {}
        self.security_data: Dict[str, Any] = {}
        self.benchmark_data: Dict[str, Any] = {}

    def collect_junit_results(self) -> Dict[str, Any]:
        """
        收集 JUnit XML 测试结果。

        Returns:
            Dict: 测试结果汇总
        """
        results = {}

        junit_files = [
            ("unit", "junit_unit.xml"),
            ("integration", "junit_integration.xml"),
            ("contract", "junit_contract.xml"),
            ("security", "junit_security.xml"),
            ("benchmark", "junit_benchmark.xml"),
        ]

        for test_type, filename in junit_files:
            filepath = self._find_file(filename)
            if filepath and filepath.exists():
                results[test_type] = {
                    "status": "found",
                    "path": str(filepath),
                }
            else:
                results[test_type] = {
                    "status": "not_found",
                    "path": None,
                }

        return results

    def collect_coverage_results(self) -> Dict[str, Any]:
        """
        收集覆盖率报告。

        Returns:
            Dict: 覆盖率数据
        """
        coverage = {}

        coverage_files = [
            ("xml", "coverage.xml"),
            ("json", "coverage.json"),
            ("html", "coverage/html/index.html"),
        ]

        for format_type, filepath in coverage_files:
            full_path = self._find_file(filepath)
            if full_path and full_path.exists():
                coverage[format_type] = {
                    "status": "found",
                    "path": str(full_path),
                }
            else:
                coverage[format_type] = {
                    "status": "not_found",
                    "path": None,
                }

        return coverage

    def collect_security_results(self) -> Dict[str, Any]:
        """
        收集安全扫描结果。

        Returns:
            Dict: 安全扫描数据
        """
        security = {}

        security_files = [
            ("bandit", "bandit_report.json"),
            ("safety", "safety_report.json"),
            ("pip_audit", "pip_audit_report.json"),
        ]

        for scan_type, filename in security_files:
            filepath = self._find_file(filename)
            if filepath and filepath.exists():
                try:
                    with open(filepath, 'r', encoding='utf-8') as f:
                        data = json.load(f)
                    security[scan_type] = {
                        "status": "found",
                        "path": str(filepath),
                        "data": data,
                    }
                except Exception:
                    security[scan_type] = {
                        "status": "error",
                        "path": str(filepath),
                    }
            else:
                security[scan_type] = {
                    "status": "not_found",
                    "path": None,
                }

        return security

    def collect_benchmark_results(self) -> Dict[str, Any]:
        """
        收集性能基准测试结果。

        Returns:
            Dict: 性能基准数据
        """
        benchmark = {}

        filepath = self._find_file("benchmark.json")
        if filepath and filepath.exists():
            try:
                with open(filepath, 'r', encoding='utf-8') as f:
                    data = json.load(f)
                benchmark = {
                    "status": "found",
                    "path": str(filepath),
                    "data": data,
                }
            except Exception:
                benchmark = {
                    "status": "error",
                    "path": str(filepath),
                }
        else:
            benchmark = {
                "status": "not_found",
                "path": None,
            }

        return benchmark

    def _find_file(self, filename: str) -> Optional[Path]:
        """
        在报告目录和 artifacts 目录中查找文件。

        Args:
            filename: 文件名

        Returns:
            Optional[Path]: 文件路径
        """
        search_paths = [
            self.report_dir / filename,
            self.artifacts_dir / filename,
        ]

        for path in search_paths:
            if path.exists():
                return path

        for artifact_dir in self.artifacts_dir.iterdir():
            if artifact_dir.is_dir():
                potential_path = artifact_dir / filename
                if potential_path.exists():
                    return potential_path

        return None

    def generate_html_report(self) -> str:
        """
        生成综合性 HTML 报告。

        Returns:
            str: HTML 报告内容
        """
        self.test_results = self.collect_junit_results()
        self.coverage_data = self.collect_coverage_results()
        self.security_data = self.collect_security_results()
        self.benchmark_data = self.collect_benchmark_results()

        html = self._generate_html()

        output_path = self.output_dir / "comprehensive_report.html"
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write(html)

        return str(output_path)

    def _generate_html(self) -> str:
        """
        生成 HTML 内容。

        Returns:
            str: HTML 内容
        """
        timestamp = datetime.utcnow().strftime("%Y-%m-%d %H:%M:%S UTC")

        return f'''<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>AgentOS Tests - 综合测试报告</title>
    <style>
        * {{
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }}

        body {{
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, 'Helvetica Neue', Arial, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            padding: 20px;
        }}

        .container {{
            max-width: 1200px;
            margin: 0 auto;
        }}

        .header {{
            background: white;
            border-radius: 16px;
            padding: 30px;
            margin-bottom: 20px;
            box-shadow: 0 10px 40px rgba(0,0,0,0.1);
        }}

        .header h1 {{
            color: #333;
            font-size: 2.5em;
            margin-bottom: 10px;
        }}

        .header .subtitle {{
            color: #666;
            font-size: 1.1em;
        }}

        .meta-info {{
            display: flex;
            flex-wrap: wrap;
            gap: 20px;
            margin-top: 20px;
        }}

        .meta-item {{
            background: #f8f9fa;
            padding: 15px 20px;
            border-radius: 8px;
            flex: 1;
            min-width: 200px;
        }}

        .meta-item label {{
            display: block;
            color: #666;
            font-size: 0.9em;
            margin-bottom: 5px;
        }}

        .meta-item value {{
            display: block;
            color: #333;
            font-size: 1.1em;
            font-weight: 600;
        }}

        .section {{
            background: white;
            border-radius: 16px;
            padding: 25px;
            margin-bottom: 20px;
            box-shadow: 0 10px 40px rgba(0,0,0,0.1);
        }}

        .section h2 {{
            color: #333;
            font-size: 1.5em;
            margin-bottom: 20px;
            padding-bottom: 10px;
            border-bottom: 2px solid #667eea;
        }}

        .status-grid {{
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
            gap: 15px;
        }}

        .status-card {{
            background: #f8f9fa;
            border-radius: 12px;
            padding: 20px;
            border-left: 4px solid #ddd;
        }}

        .status-card.success {{
            border-left-color: #28a745;
            background: #d4edda;
        }}

        .status-card.warning {{
            border-left-color: #ffc107;
            background: #fff3cd;
        }}

        .status-card.error {{
            border-left-color: #dc3545;
            background: #f8d7da;
        }}

        .status-card h3 {{
            color: #333;
            font-size: 1.1em;
            margin-bottom: 10px;
        }}

        .status-card .status {{
            font-size: 0.9em;
            color: #666;
        }}

        .status-icon {{
            font-size: 1.5em;
            margin-right: 10px;
        }}

        table {{
            width: 100%;
            border-collapse: collapse;
            margin-top: 15px;
        }}

        th, td {{
            padding: 12px 15px;
            text-align: left;
            border-bottom: 1px solid #ddd;
        }}

        th {{
            background: #667eea;
            color: white;
            font-weight: 600;
        }}

        tr:hover {{
            background: #f5f5f5;
        }}

        .badge {{
            display: inline-block;
            padding: 4px 12px;
            border-radius: 20px;
            font-size: 0.85em;
            font-weight: 600;
        }}

        .badge-success {{
            background: #28a745;
            color: white;
        }}

        .badge-warning {{
            background: #ffc107;
            color: #333;
        }}

        .badge-error {{
            background: #dc3545;
            color: white;
        }}

        .badge-info {{
            background: #17a2b8;
            color: white;
        }}

        .footer {{
            text-align: center;
            padding: 20px;
            color: white;
            font-size: 0.9em;
        }}

        .summary-stats {{
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
            gap: 15px;
            margin-bottom: 20px;
        }}

        .stat-box {{
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 20px;
            border-radius: 12px;
            text-align: center;
        }}

        .stat-box .number {{
            font-size: 2em;
            font-weight: 700;
        }}

        .stat-box .label {{
            font-size: 0.9em;
            opacity: 0.9;
        }}
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>🧪 AgentOS Tests 综合报告</h1>
            <p class="subtitle">AgentOS 智能体操作系统测试模块执行报告</p>

            <div class="meta-info">
                <div class="meta-item">
                    <label>生成时间</label>
                    <value>{timestamp}</value>
                </div>
                <div class="meta-item">
                    <label>报告版本</label>
                    <value>1.0.0.6</value>
                </div>
                <div class="meta-item">
                    <label>Python 版本</label>
                    <value>{sys.version.split()[0]}</value>
                </div>
                <div class="meta-item">
                    <label>操作系统</label>
                    <value>{sys.platform}</value>
                </div>
            </div>
        </div>

        <div class="section">
            <h2>📊 测试概览</h2>
            <div class="summary-stats">
                <div class="stat-box">
                    <div class="number">{self._count_found(self.test_results)}</div>
                    <div class="label">测试类型</div>
                </div>
                <div class="stat-box">
                    <div class="number">{self._count_found(self.coverage_data)}</div>
                    <div class="label">覆盖率报告</div>
                </div>
                <div class="stat-box">
                    <div class="number">{self._count_found(self.security_data)}</div>
                    <div class="label">安全扫描</div>
                </div>
                <div class="stat-box">
                    <div class="number">{'1' if self.benchmark_data.get('status') == 'found' else '0'}</div>
                    <div class="label">性能报告</div>
                </div>
            </div>
        </div>

        <div class="section">
            <h2>🧪 测试结果</h2>
            <div class="status-grid">
                {self._generate_test_cards()}
            </div>
        </div>

        <div class="section">
            <h2>📈 覆盖率报告</h2>
            <div class="status-grid">
                {self._generate_coverage_cards()}
            </div>
        </div>

        <div class="section">
            <h2>🔒 安全扫描</h2>
            <div class="status-grid">
                {self._generate_security_cards()}
            </div>
        </div>

        <div class="section">
            <h2>⚡ 性能基准</h2>
            {self._generate_benchmark_section()}
        </div>

        <div class="footer">
            <p>© 2026 SPHARX Ltd. All Rights Reserved.</p>
            <p>Generated by AgentOS Tests Report Generator</p>
        </div>
    </div>
</body>
</html>'''

    def _count_found(self, data: Dict[str, Any]) -> str:
        """
        计算找到的项目数量。

        Args:
            data: 数据字典

        Returns:
            str: 数量字符串
        """
        count = sum(1 for v in data.values() if v.get("status") == "found")
        return str(count)

    def _generate_test_cards(self) -> str:
        """
        生成测试结果卡片。

        Returns:
            str: HTML 内容
        """
        cards = []

        test_names = {
            "unit": "单元测试",
            "integration": "集成测试",
            "contract": "契约测试",
            "security": "安全测试",
            "benchmark": "性能测试",
        }

        for test_type, name in test_names.items():
            result = self.test_results.get(test_type, {})
            status = result.get("status", "not_found")

            if status == "found":
                card_class = "success"
                icon = "✅"
                status_text = "已找到报告"
            else:
                card_class = "warning"
                icon = "⚠️"
                status_text = "未找到报告"

            cards.append(f'''
                <div class="status-card {card_class}">
                    <h3><span class="status-icon">{icon}</span>{name}</h3>
                    <p class="status">{status_text}</p>
                </div>
            ''')

        return "\n".join(cards)

    def _generate_coverage_cards(self) -> str:
        """
        生成覆盖率报告卡片。

        Returns:
            str: HTML 内容
        """
        cards = []

        format_names = {
            "xml": "XML 报告",
            "json": "JSON 报告",
            "html": "HTML 报告",
        }

        for format_type, name in format_names.items():
            result = self.coverage_data.get(format_type, {})
            status = result.get("status", "not_found")

            if status == "found":
                card_class = "success"
                icon = "✅"
                status_text = "已生成"
            else:
                card_class = "warning"
                icon = "⚠️"
                status_text = "未生成"

            cards.append(f'''
                <div class="status-card {card_class}">
                    <h3><span class="status-icon">{icon}</span>{name}</h3>
                    <p class="status">{status_text}</p>
                </div>
            ''')

        return "\n".join(cards)

    def _generate_security_cards(self) -> str:
        """
        生成安全扫描卡片。

        Returns:
            str: HTML 内容
        """
        cards = []

        scan_names = {
            "bandit": "Bandit 扫描",
            "safety": "Safety 检查",
            "pip_audit": "Pip-Audit 检查",
        }

        for scan_type, name in scan_names.items():
            result = self.security_data.get(scan_type, {})
            status = result.get("status", "not_found")

            if status == "found":
                card_class = "success"
                icon = "✅"
                status_text = "扫描完成"
            else:
                card_class = "warning"
                icon = "⚠️"
                status_text = "未找到报告"

            cards.append(f'''
                <div class="status-card {card_class}">
                    <h3><span class="status-icon">{icon}</span>{name}</h3>
                    <p class="status">{status_text}</p>
                </div>
            ''')

        return "\n".join(cards)

    def _generate_benchmark_section(self) -> str:
        """
        生成性能基准测试部分。

        Returns:
            str: HTML 内容
        """
        status = self.benchmark_data.get("status", "not_found")

        if status == "found":
            return '''
                <div class="status-card success">
                    <h3><span class="status-icon">✅</span>性能基准测试</h3>
                    <p class="status">基准测试已完成，报告已生成</p>
                </div>
            '''
        else:
            return '''
                <div class="status-card warning">
                    <h3><span class="status-icon">⚠️</span>性能基准测试</h3>
                    <p class="status">未找到性能基准测试报告</p>
                </div>
            '''


def main():
    """
    主函数。

    执行报告生成流程。
    """
    print("=" * 50)
    print("AgentOS Tests 综合报告生成器")
    print("=" * 50)

    report_dir = os.environ.get("REPORT_OUTPUT_DIR", "reports")

    generator = ReportGenerator(report_dir)

    print("\n📂 收集测试结果...")
    generator.test_results = generator.collect_junit_results()
    print(f"   找到 {generator._count_found(generator.test_results)} 个测试报告")

    print("\n📈 收集覆盖率报告...")
    generator.coverage_data = generator.collect_coverage_results()
    print(f"   找到 {generator._count_found(generator.coverage_data)} 个覆盖率报告")

    print("\n🔒 收集安全扫描结果...")
    generator.security_data = generator.collect_security_results()
    print(f"   找到 {generator._count_found(generator.security_data)} 个安全扫描报告")

    print("\n⚡ 收集性能基准测试结果...")
    generator.benchmark_data = generator.collect_benchmark_results()

    print("\n📝 生成综合报告...")
    output_path = generator.generate_html_report()

    print(f"\n✅ 报告已生成: {output_path}")
    print("=" * 50)


if __name__ == "__main__":
    main()
