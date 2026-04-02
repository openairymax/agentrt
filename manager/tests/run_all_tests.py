#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Manager 模块测试套件运行器

一键运行所有 Manager 模块的测试用例，包括：
1. 配置文件语法验证 (test_config_syntax.py)
2. JSON Schema 验证 (test_schema_validation.py)
3. 配置集成测试 (test_config_integration.py)

遵循 ARCHITECTURAL_PRINCIPLES.md 的 E-8（可测试性原则）。

使用方法:
    python run_all_tests.py [--verbose] [--config-dir <path>] [test_name...]

作者: CTO (SOLO Coder)
版本: v1.0.0
日期: 2026-04-01
"""

import os
import sys
import subprocess
import argparse
from pathlib import Path
from typing import List, Tuple
from datetime import datetime


class TestSuiteRunner:
    """测试套件运行器"""
    
    # 测试定义
    TESTS = [
        {
            'name': '配置语法验证',
            'script': 'test_config_syntax.py',
            'description': '验证 YAML/JSON 文件语法、UTF-8 编码、环境变量格式',
            'required': True,
        },
        {
            'name': 'Schema 验证',
            'script': 'test_schema_validation.py',
            'description': '验证配置文件是否符合 JSON Schema 定义',
            'required': True,
        },
        {
            'name': '配置集成测试',
            'script': 'test_config_integration.py',
            'description': '验证配置完整性、一致性、跨模块依赖关系',
            'required': True,
        },
    ]
    
    def __init__(self, test_dir: str, config_dir: str, verbose: bool = False):
        """
        初始化测试运行器
        
        Args:
            test_dir: 测试脚本目录
            config_dir: 配置根目录
            verbose: 是否输出详细信息
        """
        self.test_dir = Path(test_dir)
        self.config_dir = config_dir
        self.verbose = verbose
        self.results: List[dict] = []
    
    def run_single_test(self, test_def: dict) -> dict:
        """
        运行单个测试
        
        Args:
            test_def: 测试定义字典
            
        Returns:
            dict: 测试结果
        """
        script_path = self.test_dir / test_def['script']
        
        if not script_path.exists():
            return {
                'name': test_def['name'],
                'passed': False,
                'exit_code': -1,
                'output': f"测试脚本不存在: {script_path}",
                'duration': 0.0
            }
        
        # 构建命令
        cmd = [
            sys.executable,
            str(script_path),
            '--config-dir', self.config_dir
        ]
        
        if self.verbose:
            cmd.append('--verbose')
        
        print(f"\n{'='*80}")
        print(f"▶ 运行测试: {test_def['name']}")
        print(f"  脚本: {test_def['script']}")
        print(f"  描述: {test_def['description']}")
        print(f"{'='*80}\n")
        
        start_time = datetime.now()
        
        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                cwd=str(self.test_dir),
                timeout=120  # 2分钟超时
            )
            
            duration = (datetime.now() - start_time).total_seconds()
            
            # 输出标准输出
            if result.stdout:
                print(result.stdout)
            
            # 输出错误（如果有）
            if result.stderr and self.verbose:
                print("⚠️ 标准错误输出:")
                print(result.stderr)
            
            passed = result.returncode == 0
            
            return {
                'name': test_def['name'],
                'passed': passed,
                'exit_code': result.returncode,
                'output': result.stdout[-500:] if len(result.stdout) > 500 else result.stdout,
                'duration': duration
            }
        
        except subprocess.TimeoutExpired:
            duration = (datetime.now() - start_time).total_seconds()
            return {
                'name': test_def['name'],
                'passed': False,
                'exit_code': -1,
                'output': "测试超时 (120秒)",
                'duration': duration
            }
        
        except Exception as e:
            duration = (datetime.now() - start_time).total_seconds()
            return {
                'name': test_def['name'],
                'passed': False,
                'exit_code': -1,
                'output': f"执行异常: {e}",
                'duration': duration
            }
    
    def run_selected_tests(self, selected_tests: List[str]) -> Tuple[int, int]:
        """
        运行选定的测试
        
        Args:
            selected_tests: 要运行的测试名称列表（空则运行全部）
            
        Returns:
            Tuple[int, int]: (通过数, 失败数)
        """
        tests_to_run = []
        
        if not selected_tests:
            tests_to_run = self.TESTS
        else:
            for test in self.TESTS:
                if any(keyword.lower() in test['name'].lower() or 
                       keyword.lower() in test['script'].lower() 
                       for keyword in selected_tests):
                    tests_to_run.append(test)
            
            if not tests_to_run:
                print(f"❌ 未找到匹配的测试: {', '.join(selected_tests)}")
                print(f"可用的测试:")
                for test in self.TESTS:
                    print(f"  - {test['name']} ({test['script']})")
                return 0, 0
        
        total_passed = 0
        total_failed = 0
        
        for test_def in tests_to_run:
            result = self.run_single_test(test_def)
            self.results.append(result)
            
            if result['passed']:
                total_passed += 1
            else:
                total_failed += 1
        
        return total_passed, total_failed
    
    def generate_report(self) -> None:
        """生成测试报告"""
        print("\n")
        print("=" * 80)
        print("📊 AgentOS Manager 模块测试报告")
        print("=" * 80)
        print(f"测试时间: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        print(f"配置目录: {self.config_dir}")
        print()
        
        # 表头
        print(f"{'测试名称':<25} {'状态':<10} {'退出码':<8} {'耗时(秒)':<10}")
        print("-" * 80)
        
        for result in self.results:
            status = "✅ PASS" if result['passed'] else "❌ FAIL"
            print(f"{result['name']:<25} {status:<10} {result['exit_code']:<8} {result['duration']:<10.2f}")
        
        print("-" * 80)
        
        total_passed = sum(1 for r in self.results if r['passed'])
        total_failed = sum(1 for r in self.results if not r['passed'])
        total = len(self.results)
        
        print(f"\n总计: {total} 个测试")
        print(f"通过: {total_passed} ✅ ({100*total_passed/total:.1f}%)" if total > 0 else "")
        print(f"失败: {total_failed} ❌ ({100*total_failed/total:.1f}%)" if total > 0 else "")
        
        if total_failed > 0:
            print("\n" + "=" * 80)
            print("⚠️ 失败的测试详情:")
            print("=" * 80)
            for result in self.results:
                if not result['passed']:
                    print(f"\n❌ {result['name']}")
                    print(f"   退出码: {result['exit_code']}")
                    print(f"   输出摘要:\n{result['output'][:300]}...")
        
        print("\n" + "=" * 80)


def main():
    """主函数"""
    parser = argparse.ArgumentParser(
        description='AgentOS Manager 模块测试套件运行器',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例用法:
  python run_all_tests.py                          # 运行所有测试
  python run_all_tests.py --verbose                 # 详细模式
  python run_all_tests.py syntax schema             # 只运行指定测试
  python run_all_tests.py --config-dir ./configs     # 指定配置目录
  
可用测试名称关键词:
  syntax      - 配置语法验证
  schema      - Schema 验证
  integration - 集成测试
  
退出码:
  0 - 所有测试通过
  1 - 存在失败的测试
  2 - 参数错误或执行异常
        """
    )
    
    parser.add_argument(
        '--config-dir', '-c',
        type=str,
        default=os.path.join(os.path.dirname(__file__), '..'),
        help='Manager 配置根目录路径 (默认: ../)'
    )
    
    parser.add_argument(
        '--verbose', '-v',
        action='store_true',
        default=False,
        help='输出详细测试信息'
    )
    
    parser.add_argument(
        'tests',
        nargs='*',
        help='要运行的测试名称（可选，默认运行全部）'
    )
    
    args = parser.parse_args()
    
    try:
        test_dir = os.path.dirname(os.path.abspath(__file__))
        
        runner = TestSuiteRunner(test_dir, args.config_dir, args.verbose)
        passed, failed = runner.run_selected_tests(args.tests)
        
        # 生成报告
        runner.generate_report()
        
        sys.exit(0 if failed == 0 else 1)
    
    except Exception as e:
        print(f"❌ 执行异常: {e}", file=sys.stderr)
        sys.exit(2)


if __name__ == '__main__':
    main()