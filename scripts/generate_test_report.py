#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
AgentOS Test Report Generator Script
Version: 1.0.0

本脚本生成HTML格式的测试报告
"""

import argparse
import json
import os
import sys
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Optional
from xml.etree import ElementTree as ET


class TestReportGenerator:
    """测试报告生成器"""
    
    def __init__(self, input_dir: str, output_file: str):
        self.input_dir = Path(input_dir)
        self.output_file = Path(output_file)
        self.test_results: List[Dict] = []
        self.summary = {
            'total_tests': 0,
            'passed': 0,
            'failed': 0,
            'skipped': 0,
            'duration': 0.0
        }
    
    def parse_junit_xml(self, xml_file: Path) -> Optional[Dict]:
        """解析JUnit XML格式测试结果"""
        try:
            tree = ET.parse(xml_file)
            root = tree.getroot()
            
            # JUnit XML格式
            testsuite = root.find('testsuite') if root.tag == 'testsuites' else root
            
            result = {
                'name': testsuite.get('name', 'Unknown'),
                'tests': int(testsuite.get('tests', 0)),
                'failures': int(testsuite.get('failures', 0)),
                'errors': int(testsuite.get('errors', 0)),
                'skipped': int(testsuite.get('skipped', 0)),
                'time': float(testsuite.get('time', 0)),
                'testcases': []
            }
            
            for testcase in testsuite.findall('testcase'):
                case = {
                    'name': testcase.get('name', 'Unknown'),
                    'classname': testcase.get('classname', ''),
                    'time': float(testcase.get('time', 0)),
                    'status': 'passed'
                }
                
                # 检查失败和跳过状态
                if testcase.find('failure') is not None:
                    case['status'] = 'failed'
                    case['message'] = testcase.find('failure').get('message', '')
                elif testcase.find('error') is not None:
                    case['status'] = 'error'
                    case['message'] = testcase.find('error').get('message', '')
                elif testcase.find('skipped') is not None:
                    case['status'] = 'skipped'
                    case['message'] = testcase.find('skipped').get('message', '')
                
                result['testcases'].append(case)
            
            return result
            
        except Exception as e:
            print(f"[!] 解析测试结果失败 {xml_file}: {e}")
            return None
    
    def load_test_results(self) -> None:
        """加载测试结果"""
        if not self.input_dir.exists():
            print(f"[!] 测试结果目录不存在: {self.input_dir}")
            return
        
        # 查找所有XML文件
        xml_files = list(self.input_dir.rglob('*.xml'))
        
        for xml_file in xml_files:
            result = self.parse_junit_xml(xml_file)
            if result:
                self.test_results.append(result)
                self.summary['total_tests'] += result['tests']
                self.summary['failed'] += result['failures'] + result['errors']
                self.summary['skipped'] += result['skipped']
                self.summary['duration'] += result['time']
        
        self.summary['passed'] = self.summary['total_tests'] - self.summary['failed'] - self.summary['skipped']
    
    def generate_html_report(self) -> str:
        """生成HTML报告"""
        pass_rate = (self.summary['passed'] / self.summary['total_tests'] * 100) if self.summary['total_tests'] > 0 else 0
        
        html = f"""<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>AgentOS Test Report</title>
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
            background-color: white;
            padding: 30px;
            border-radius: 8px;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
        }}
        h1 {{
            color: #333;
            border-bottom: 3px solid #4CAF50;
            padding-bottom: 10px;
        }}
        .summary {{
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 20px;
            margin: 30px 0;
        }}
        .summary-card {{
            padding: 20px;
            border-radius: 8px;
            text-align: center;
        }}
        .summary-card.total {{ background-color: #2196F3; color: white; }}
        .summary-card.passed {{ background-color: #4CAF50; color: white; }}
        .summary-card.failed {{ background-color: #f44336; color: white; }}
        .summary-card.skipped {{ background-color: #FF9800; color: white; }}
        .summary-card h2 {{
            margin: 0;
            font-size: 36px;
        }}
        .summary-card p {{
            margin: 10px 0 0 0;
            font-size: 14px;
        }}
        .progress-bar {{
            width: 100%;
            height: 30px;
            background-color: #e0e0e0;
            border-radius: 15px;
            overflow: hidden;
            margin: 20px 0;
        }}
        .progress-fill {{
            height: 100%;
            background-color: #4CAF50;
            display: flex;
            align-items: center;
            justify-content: center;
            color: white;
            font-weight: bold;
        }}
        table {{
            width: 100%;
            border-collapse: collapse;
            margin-top: 30px;
        }}
        th, td {{
            padding: 12px;
            text-align: left;
            border-bottom: 1px solid #ddd;
        }}
        th {{
            background-color: #4CAF50;
            color: white;
        }}
        tr:hover {{
            background-color: #f5f5f5;
        }}
        .status-passed {{ color: #4CAF50; font-weight: bold; }}
        .status-failed {{ color: #f44336; font-weight: bold; }}
        .status-skipped {{ color: #FF9800; font-weight: bold; }}
        .timestamp {{
            color: #666;
            font-size: 14px;
            margin-top: 20px;
        }}
    </style>
</head>
<body>
    <div class="container">
        <h1>🧪 AgentOS Test Report</h1>
        
        <div class="summary">
            <div class="summary-card total">
                <h2>{self.summary['total_tests']}</h2>
                <p>Total Tests</p>
            </div>
            <div class="summary-card passed">
                <h2>{self.summary['passed']}</h2>
                <p>Passed</p>
            </div>
            <div class="summary-card failed">
                <h2>{self.summary['failed']}</h2>
                <p>Failed</p>
            </div>
            <div class="summary-card skipped">
                <h2>{self.summary['skipped']}</h2>
                <p>Skipped</p>
            </div>
        </div>
        
        <div class="progress-bar">
            <div class="progress-fill" style="width: {pass_rate:.1f}%;">
                {pass_rate:.1f}% Pass Rate
            </div>
        </div>
        
        <h2>📊 Test Suites</h2>
        <table>
            <thead>
                <tr>
                    <th>Test Suite</th>
                    <th>Tests</th>
                    <th>Passed</th>
                    <th>Failed</th>
                    <th>Skipped</th>
                    <th>Duration (s)</th>
                </tr>
            </thead>
            <tbody>
"""
        
        for result in self.test_results:
            passed = result['tests'] - result['failures'] - result['errors'] - result['skipped']
            html += f"""                <tr>
                    <td>{result['name']}</td>
                    <td>{result['tests']}</td>
                    <td class="status-passed">{passed}</td>
                    <td class="status-failed">{result['failures'] + result['errors']}</td>
                    <td class="status-skipped">{result['skipped']}</td>
                    <td>{result['time']:.2f}</td>
                </tr>
"""
        
        html += f"""            </tbody>
        </table>
        
        <div class="timestamp">
            Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')} | 
            Total Duration: {self.summary['duration']:.2f}s
        </div>
    </div>
</body>
</html>
"""
        
        return html
    
    def save_report(self, html: str) -> None:
        """保存HTML报告"""
        self.output_file.parent.mkdir(parents=True, exist_ok=True)
        with open(self.output_file, 'w', encoding='utf-8') as f:
            f.write(html)
    
    def run(self) -> int:
        """生成测试报告"""
        print(f"[*] 加载测试结果: {self.input_dir}")
        
        self.load_test_results()
        
        print(f"[+] 测试结果统计:")
        print(f"    - 总测试数: {self.summary['total_tests']}")
        print(f"    - 通过: {self.summary['passed']}")
        print(f"    - 失败: {self.summary['failed']}")
        print(f"    - 跳过: {self.summary['skipped']}")
        print(f"    - 耗时: {self.summary['duration']:.2f}s")
        
        html = self.generate_html_report()
        self.save_report(html)
        
        print(f"[+] 报告已生成: {self.output_file}")
        
        return 0 if self.summary['failed'] == 0 else 1


def main():
    parser = argparse.ArgumentParser(description='AgentOS Test Report Generator')
    parser.add_argument('--input', required=True, help='测试结果目录')
    parser.add_argument('--output', required=True, help='输出HTML报告文件')
    
    args = parser.parse_args()
    
    generator = TestReportGenerator(args.input, args.output)
    sys.exit(generator.run())


if __name__ == '__main__':
    main()
