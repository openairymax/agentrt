#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
AgentOS Security Gate Check Script
Version: 1.0.0

本脚本检查安全扫描结果，执行安全门禁策略
"""

import argparse
import json
import sys
import xml.etree.ElementTree as ET
from pathlib import Path
from typing import Dict, Optional


class SecurityGateChecker:
    """安全门禁检查器"""
    
    def __init__(self, cppcheck_report: str, security_report: str, 
                 max_errors: int = 0, max_warnings: int = 10):
        self.cppcheck_report = Path(cppcheck_report) if cppcheck_report else None
        self.security_report = Path(security_report) if security_report else None
        self.max_errors = max_errors
        self.max_warnings = max_warnings
        self.results = {
            'cppcheck': {'errors': 0, 'warnings': 0},
            'security': {'high': 0, 'medium': 0, 'low': 0}
        }
    
    def parse_cppcheck_report(self) -> None:
        """解析Cppcheck XML报告"""
        if not self.cppcheck_report or not self.cppcheck_report.exists():
            print(f"[!] Cppcheck报告不存在: {self.cppcheck_report}")
            return
        
        try:
            tree = ET.parse(self.cppcheck_report)
            root = tree.getroot()
            
            # Cppcheck XML格式: <results><errors>...</errors></results>
            errors_elem = root.find('.//errors')
            if errors_elem is not None:
                for error in errors_elem.findall('error'):
                    severity = error.get('severity', 'warning')
                    if severity == 'error':
                        self.results['cppcheck']['errors'] += 1
                    else:
                        self.results['cppcheck']['warnings'] += 1
            
            print(f"[+] Cppcheck分析结果:")
            print(f"    - 错误: {self.results['cppcheck']['errors']}")
            print(f"    - 警告: {self.results['cppcheck']['warnings']}")
            
        except Exception as e:
            print(f"[!] 解析Cppcheck报告失败: {e}")
    
    def parse_security_report(self) -> None:
        """解析安全扫描JSON报告"""
        if not self.security_report or not self.security_report.exists():
            print(f"[!] 安全报告不存在: {self.security_report}")
            return
        
        try:
            with open(self.security_report, 'r', encoding='utf-8') as f:
                report = json.load(f)
            
            summary = report.get('summary', {})
            severity_counts = summary.get('severity_counts', {})
            
            self.results['security']['high'] = severity_counts.get('high', 0)
            self.results['security']['medium'] = severity_counts.get('medium', 0)
            self.results['security']['low'] = severity_counts.get('low', 0)
            
            print(f"[+] 安全扫描结果:")
            print(f"    - 高危: {self.results['security']['high']}")
            print(f"    - 中危: {self.results['security']['medium']}")
            print(f"    - 低危: {self.results['security']['low']}")
            
        except Exception as e:
            print(f"[!] 解析安全报告失败: {e}")
    
    def check_gate(self) -> bool:
        """检查是否通过安全门禁"""
        print("\n" + "="*60)
        print("安全门禁检查")
        print("="*60)
        
        passed = True
        
        # 检查Cppcheck错误
        if self.results['cppcheck']['errors'] > self.max_errors:
            print(f"[✗] Cppcheck错误数量超标: {self.results['cppcheck']['errors']} > {self.max_errors}")
            passed = False
        else:
            print(f"[✓] Cppcheck错误数量: {self.results['cppcheck']['errors']} <= {self.max_errors}")
        
        # 检查Cppcheck警告
        if self.results['cppcheck']['warnings'] > self.max_warnings:
            print(f"[✗] Cppcheck警告数量超标: {self.results['cppcheck']['warnings']} > {self.max_warnings}")
            passed = False
        else:
            print(f"[✓] Cppcheck警告数量: {self.results['cppcheck']['warnings']} <= {self.max_warnings}")
        
        # 检查安全高危问题
        if self.results['security']['high'] > 0:
            print(f"[✗] 发现高危安全问题: {self.results['security']['high']}")
            passed = False
        else:
            print(f"[✓] 无高危安全问题")
        
        print("="*60)
        
        if passed:
            print("[✓] 安全门禁检查通过")
        else:
            print("[✗] 安全门禁检查失败")
        
        print("="*60)
        
        return passed
    
    def run(self) -> int:
        """执行安全门禁检查"""
        self.parse_cppcheck_report()
        self.parse_security_report()
        
        passed = self.check_gate()
        
        return 0 if passed else 1


def main():
    parser = argparse.ArgumentParser(description='AgentOS Security Gate Check Script')
    parser.add_argument('--cppcheck', default='', help='Cppcheck XML报告文件')
    parser.add_argument('--security', default='', help='安全扫描JSON报告文件')
    parser.add_argument('--max-errors', type=int, default=0, help='最大允许错误数')
    parser.add_argument('--max-warnings', type=int, default=10, help='最大允许警告数')
    
    args = parser.parse_args()
    
    checker = SecurityGateChecker(
        args.cppcheck, 
        args.security,
        args.max_errors,
        args.max_warnings
    )
    
    sys.exit(checker.run())


if __name__ == '__main__':
    main()
