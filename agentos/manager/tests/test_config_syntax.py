#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Manager 模块配置文件语法验证测试

本测试脚本验证 AgentOS/manager 模块中所有 YAML/JSON 配置文件的语法正确性。
遵循 ARCHITECTURAL_PRINCIPLES.md 的 E-8（可测试性原则）和 E-6（错误可追溯原则）。

使用方法:
    python test_config_syntax.py [--verbose] [--config-dir <path>]

依赖:
    - PyYAML (>= 5.0)
    - jsonschema (>= 4.0) [可选，用于Schema验证]

作者: CTO (SOLO Coder)
版本: v1.0.0
日期: 2026-04-01
"""

import os
import sys
import json
import yaml
import argparse
from pathlib import Path
from typing import List, Dict, Tuple, Optional
from dataclasses import dataclass, field


@dataclass
class TestResult:
    """测试结果数据类"""
    file_path: str
    passed: bool
    error_message: str = ""
    error_line: int = 0
    error_column: int = 0
    
    def __str__(self):
        status = "✅ PASS" if self.passed else "❌ FAIL"
        result = f"{status}: {self.file_path}"
        if not self.passed:
            result += f"\n  错误: {self.error_message}"
            if self.error_line > 0:
                result += f" (行 {self.error_line})"
        return result


class ConfigSyntaxValidator:
    """配置文件语法验证器"""
    
    # 支持的配置文件扩展名
    SUPPORTED_EXTENSIONS = {'.yaml', '.yml', '.json'}
    
    # 必须存在的核心配置文件
    REQUIRED_CONFIGS = [
        'kernel/settings.yaml',
        'model/model.yaml',
        'agent/registry.yaml',
        'security/policy.yaml',
        'logging/manager.yaml',
        'manager_management.yaml',
    ]
    
    # 可选的配置文件
    OPTIONAL_CONFIGS = [
        'skill/registry.yaml',
        'sanitizer/sanitizer_rules.json',
        'service/tool_d/tool.yaml',
        'security/permission_rules.yaml',
        'environment/development.yaml',
        'environment/staging.yaml',
        'environment/production.yaml',
        'example.yaml',
        '.env.template',
    ]
    
    def __init__(self, config_dir: str, verbose: bool = False):
        """
        初始化验证器
        
        Args:
            config_dir: 配置根目录路径
            verbose: 是否输出详细信息
        """
        self.config_dir = Path(config_dir)
        self.verbose = verbose
        self.results: List[TestResult] = []
        
        if not self.config_dir.exists():
            raise ValueError(f"配置目录不存在: {config_dir}")
    
    def validate_yaml_file(self, file_path: Path) -> TestResult:
        """
        验证 YAML 文件语法
        
        Args:
            file_path: YAML 文件路径
            
        Returns:
            TestResult: 验证结果
        """
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                yaml.safe_load(f)
            
            return TestResult(
                file_path=str(file_path),
                passed=True,
                error_message=""
            )
        
        except yaml.YAMLError as e:
            error_msg = str(e)
            line_num = 0
            
            if hasattr(e, 'problem_mark') and e.problem_mark is not None:
                line_num = e.problem_mark.line + 1  # 转换为1-based索引
                col_num = e.problem_mark.column + 1
                error_msg = f"YAML解析错误: {e.problem} (位置: 行{line_num}, 列{col_num})"
            
            return TestResult(
                file_path=str(file_path),
                passed=False,
                error_message=error_msg,
                error_line=line_num
            )
        
        except UnicodeDecodeError as e:
            return TestResult(
                file_path=str(file_path),
                passed=False,
                error_message=f"编码错误: 不支持的字符编码 ({e})",
                error_line=e.lineno or 0
            )
        
        except IOError as e:
            return TestResult(
                file_path=str(file_path),
                passed=False,
                error_message=f"IO错误: 无法读取文件 ({e})",
                error_line=0
            )
    
    def validate_json_file(self, file_path: Path) -> TestResult:
        """
        验证 JSON 文件语法
        
        Args:
            file_path: JSON 文件路径
            
        Returns:
            TestResult: 验证结果
        """
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                json.load(f)
            
            return TestResult(
                file_path=str(file_path),
                passed=True,
                error_message=""
            )
        
        except json.JSONDecodeError as e:
            return TestResult(
                file_path=str(file_path),
                passed=False,
                error_message=f"JSON解析错误: {e.msg} (位置: 行{e.lineno}, 列{e.colno})",
                error_line=e.lineno or 0,
                error_column=e.colno or 0
            )
        
        except Exception as e:
            return TestResult(
                file_path=str(file_path),
                passed=False,
                error_message=f"未知错误: {e}",
                error_line=0
            )
    
    def validate_file(self, file_path: Path) -> TestResult:
        """
        根据文件类型选择合适的验证方法
        
        Args:
            file_path: 文件路径
            
        Returns:
            TestResult: 验证结果
        """
        suffix = file_path.suffix.lower()
        
        if suffix in ('.yaml', '.yml'):
            return self.validate_yaml_file(file_path)
        elif suffix == '.json':
            return self.validate_json_file(file_path)
        else:
            return TestResult(
                file_path=str(file_path),
                passed=False,
                error_message=f"不支持的文件类型: {suffix}"
            )
    
    def discover_config_files(self) -> List[Path]:
        """
        发现所有配置文件
        
        Returns:
            List[Path]: 配置文件列表
        """
        config_files = []
        
        for root, dirs, files in os.walk(self.config_dir):
            # 排除 tests 目录和 schema 目录（单独处理）
            if 'tests' in dirs:
                dirs.remove('tests')
            if '__pycache__' in dirs:
                dirs.remove('__pycache__')
            
            for filename in files:
                file_path = Path(root) / filename
                
                if file_path.suffix.lower() in self.SUPPORTED_EXTENSIONS:
                    config_files.append(file_path)
        
        return sorted(config_files)
    
    def check_required_files(self) -> List[TestResult]:
        """
        检查必需的配置文件是否存在
        
        Returns:
            List[TestResult]: 检查结果列表
        """
        results = []
        
        for required_file in self.REQUIRED_CONFIGS:
            file_path = self.config_dir / required_file
            
            if file_path.exists():
                results.append(TestResult(
                    file_path=str(required_file),
                    passed=True,
                    error_message="文件存在"
                ))
            else:
                results.append(TestResult(
                    file_path=str(required_file),
                    passed=False,
                    error_message=f"必需的配置文件不存在"
                ))
        
        return results
    
    def validate_environment_variable_references(self, file_path: Path) -> TestResult:
        """
        验证环境变量引用格式是否符合标准（${VARIABLE}）
        
        Args:
            file_path: 配置文件路径
            
        Returns:
            TestResult: 验证结果
        """
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                content = f.read()
            
            import re
            
            # 查找不符合标准的环境变量引用格式
            # 匹配 $VAR 或 ${VAR} 格式（排除文档中的示例代码）
            invalid_patterns = re.findall(r'(?<!\{)\$([A-Z_][A-Z0-9_]*)\b(?!\})', content)
            
            # 排除 .md 文件中的 shell 命令示例
            if file_path.suffix == '.md':
                return TestResult(
                    file_path=str(file_path),
                    passed=True,
                    error_message="Markdown 文件跳过环境变量格式检查"
                )
            
            if invalid_patterns:
                return TestResult(
                    file_path=str(file_path),
                    passed=False,
                    error_message=f"发现非标准环境变量引用格式: {', '.join(invalid_patterns)} (应使用 ${{VARIABLE}} 格式)"
                )
            
            return TestResult(
                file_path=str(file_path),
                passed=True,
                error_message="环境变量引用格式正确"
            )
        
        except Exception as e:
            return TestResult(
                file_path=str(file_path),
                passed=False,
                error_message=f"检查失败: {e}"
            )
    
    def validate_utf8_encoding(self, file_path: Path) -> TestResult:
        """
        验证文件是否为 UTF-8 编码且无 BOM
        
        Args:
            file_path: 文件路径
            
        Returns:
            TestResult: 验证结果
        """
        try:
            with open(file_path, 'rb') as f:
                raw_bytes = f.read(3)
                
                # 检查 BOM
                if raw_bytes.startswith(b'\xef\xbb\xbf'):
                    return TestResult(
                        file_path=str(file_path),
                        passed=False,
                        error_message="文件包含 UTF-8 BOM 标记，应移除"
                    )
                
                # 尝试解码整个文件
                f.seek(0)
                content = f.read()
                content.decode('utf-8')
            
            return TestResult(
                file_path=str(file_path),
                passed=True,
                error_message="UTF-8 编码正确，无 BOM"
            )
        
        except UnicodeDecodeError as e:
            return TestResult(
                file_path=str(file_path),
                passed=False,
                error_message=f"编码错误: 非 UTF-8 编码或包含非法字符 (位置: 字节 {e.start})"
            )
        except Exception as e:
            return TestResult(
                file_path=str(file_path),
                passed=False,
                error_message=f"编码检查失败: {e}"
            )
    
    def run_all_tests(self) -> Tuple[int, int, int]:
        """
        运行所有测试
        
        Returns:
            Tuple[int, int, int]: (通过数, 失败数, 总计)
        """
        print("=" * 80)
        print("AgentOS Manager 模块配置文件语法验证测试")
        print("=" * 80)
        print(f"配置目录: {self.config_dir}")
        print(f"测试时间: {__import__('datetime').datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        print()
        
        # 1. 检查必需文件
        print("-" * 80)
        print("1. 必需配置文件存在性检查")
        print("-" * 80)
        
        required_results = self.check_required_files()
        for result in required_results:
            print(result)
            self.results.append(result)
        
        print()
        
        # 2. 语法验证
        print("-" * 80)
        print("2. 配置文件语法验证")
        print("-" * 80)
        
        config_files = self.discover_config_files()
        syntax_pass = 0
        syntax_fail = 0
        
        for file_path in config_files:
            relative_path = file_path.relative_to(self.config_dir)
            
            if self.verbose:
                print(f"\n正在验证: {relative_path}")
            
            # 语法验证
            result = self.validate_file(file_path)
            self.results.append(result)
            print(result)
            
            if result.passed:
                syntax_pass += 1
            else:
                syntax_fail += 1
            
            # UTF-8 编码检查
            encoding_result = self.validate_utf8_encoding(file_path)
            self.results.append(encoding_result)
            if self.verbose or not encoding_result.passed:
                print(encoding_result)
            
            # 环境变量引用格式检查（仅对 YAML/JSON 文件）
            if file_path.suffix.lower() in ('.yaml', '.yml', '.json'):
                env_result = self.validate_environment_variable_references(file_path)
                self.results.append(env_result)
                if self.verbose or not env_result.passed:
                    print(env_result)
        
        print()
        
        # 3. 统计汇总
        total_passed = sum(1 for r in self.results if r.passed)
        total_failed = sum(1 for r in self.results if not r.passed)
        total_tests = len(self.results)
        
        print("=" * 80)
        print("测试结果统计")
        print("=" * 80)
        print(f"总测试数: {total_tests}")
        print(f"通过数量: {total_passed} ✅")
        print(f"失败数量: {total_failed} ❌")
        print(f"通过率:   {(total_passed / total_tests * 100):.1f}%" if total_tests > 0 else "N/A")
        print()
        
        if total_failed > 0:
            print("=" * 80)
            print("失败的测试详情:")
            print("=" * 80)
            for result in self.results:
                if not result.passed:
                    print(result)
                    print()
        
        return total_passed, total_failed, total_tests


def main():
    """主函数"""
    parser = argparse.ArgumentParser(
        description='AgentOS Manager 模块配置文件语法验证工具',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例用法:
  python test_config_syntax.py                          # 使用默认路径
  python test_config_syntax.py --verbose                 # 详细输出
  python test_config_syntax.py --config-dir ./my-configs # 指定配置目录
  
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
        help='输出详细验证信息'
    )
    
    args = parser.parse_args()
    
    try:
        validator = ConfigSyntaxValidator(args.config_dir, args.verbose)
        passed, failed, total = validator.run_all_tests()
        
        # 返回适当的退出码
        sys.exit(0 if failed == 0 else 1)
    
    except Exception as e:
        print(f"❌ 执行异常: {e}", file=sys.stderr)
        sys.exit(2)


if __name__ == '__main__':
    main()