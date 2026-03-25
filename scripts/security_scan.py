#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
AgentOS Security Scan Script
Version: 1.0.0

本脚本执行代码安全扫描，检测潜在的安全漏洞和危险模式
"""

import argparse
import json
import os
import re
import sys
from pathlib import Path
from typing import Dict, List, Set, Tuple


class SecurityScanner:
    """安全扫描器"""
    
    def __init__(self, source_dir: str, output_file: str):
        self.source_dir = Path(source_dir)
        self.output_file = Path(output_file)
        self.forbidden_functions: Set[str] = set()
        self.dangerous_patterns: List[str] = []
        self.findings: List[Dict] = []
        
    def parse_forbidden_functions(self, functions_str: str) -> None:
        """解析禁止函数列表"""
        if functions_str:
            self.forbidden_functions = set(f.strip() for f in functions_str.split(';'))
    
    def parse_dangerous_patterns(self, patterns_str: str) -> None:
        """解析危险模式列表"""
        if patterns_str:
            self.dangerous_patterns = [p.strip() for p in patterns_str.split(';') if p.strip()]
    
    def scan_file(self, file_path: Path) -> List[Dict]:
        """扫描单个文件"""
        findings = []
        
        try:
            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
                lines = content.split('\n')
                
            # 检查禁止函数
            for func in self.forbidden_functions:
                pattern = rf'\b{re.escape(func)}\s*\('
                for i, line in enumerate(lines, 1):
                    if re.search(pattern, line):
                        findings.append({
                            'file': str(file_path.relative_to(self.source_dir)),
                            'line': i,
                            'type': 'forbidden_function',
                            'severity': 'high',
                            'message': f'使用禁止函数: {func}',
                            'code': line.strip()
                        })
            
            # 检查危险模式
            for pattern_str in self.dangerous_patterns:
                try:
                    pattern = re.compile(pattern_str, re.IGNORECASE)
                    for i, line in enumerate(lines, 1):
                        if pattern.search(line):
                            findings.append({
                                'file': str(file_path.relative_to(self.source_dir)),
                                'line': i,
                                'type': 'dangerous_pattern',
                                'severity': 'medium',
                                'message': f'检测到危险模式: {pattern_str}',
                                'code': line.strip()
                            })
                except re.error:
                    pass
                    
        except Exception as e:
            findings.append({
                'file': str(file_path),
                'line': 0,
                'type': 'scan_error',
                'severity': 'low',
                'message': f'扫描错误: {str(e)}',
                'code': ''
            })
        
        return findings
    
    def scan_directory(self) -> None:
        """扫描源代码目录"""
        extensions = {'.c', '.h', '.cpp', '.hpp', '.py', '.go', '.rs', '.ts', '.js'}
        
        for file_path in self.source_dir.rglob('*'):
            if file_path.is_file() and file_path.suffix in extensions:
                findings = self.scan_file(file_path)
                self.findings.extend(findings)
    
    def generate_report(self) -> Dict:
        """生成扫描报告"""
        severity_counts = {'high': 0, 'medium': 0, 'low': 0}
        type_counts = {}
        
        for finding in self.findings:
            severity = finding['severity']
            severity_counts[severity] = severity_counts.get(severity, 0) + 1
            
            finding_type = finding['type']
            type_counts[finding_type] = type_counts.get(finding_type, 0) + 1
        
        return {
            'summary': {
                'total_findings': len(self.findings),
                'severity_counts': severity_counts,
                'type_counts': type_counts,
                'scan_directory': str(self.source_dir)
            },
            'findings': self.findings
        }
    
    def save_report(self, report: Dict) -> None:
        """保存报告到文件"""
        self.output_file.parent.mkdir(parents=True, exist_ok=True)
        with open(self.output_file, 'w', encoding='utf-8') as f:
            json.dump(report, f, indent=2, ensure_ascii=False)
    
    def run(self) -> int:
        """执行扫描"""
        print(f"[*] 开始安全扫描: {self.source_dir}")
        print(f"[*] 禁止函数数量: {len(self.forbidden_functions)}")
        print(f"[*] 危险模式数量: {len(self.dangerous_patterns)}")
        
        self.scan_directory()
        report = self.generate_report()
        self.save_report(report)
        
        print(f"\n[+] 扫描完成")
        print(f"[+] 发现问题: {report['summary']['total_findings']}")
        print(f"    - 高危: {report['summary']['severity_counts']['high']}")
        print(f"    - 中危: {report['summary']['severity_counts']['medium']}")
        print(f"    - 低危: {report['summary']['severity_counts']['low']}")
        print(f"[+] 报告已保存: {self.output_file}")
        
        # 返回高危问题数量作为退出码
        return min(report['summary']['severity_counts']['high'], 127)


def main():
    parser = argparse.ArgumentParser(description='AgentOS Security Scan Script')
    parser.add_argument('--source', required=True, help='源代码目录')
    parser.add_argument('--output', required=True, help='输出报告文件')
    parser.add_argument('--forbidden-functions', default='', help='禁止函数列表（分号分隔）')
    parser.add_argument('--dangerous-patterns', default='', help='危险模式列表（分号分隔）')
    
    args = parser.parse_args()
    
    scanner = SecurityScanner(args.source, args.output)
    scanner.parse_forbidden_functions(args.forbidden_functions)
    scanner.parse_dangerous_patterns(args.dangerous_patterns)
    
    sys.exit(scanner.run())


if __name__ == '__main__':
    main()
