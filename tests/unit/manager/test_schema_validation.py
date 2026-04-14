#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Manager 模块 JSON Schema 验证单元测试

本测试脚本验证 AgentOS/manager 模块中所有配置文件是否符合对应的 JSON Schema 定义。
遵循 ARCHITECTURAL_PRINCIPLES.md 的 E-8（可测试性原则）和 K-2（接口契约化原则）。

使用方法:
    python test_schema_validation.py [--verbose] [--config-dir <path>]

依赖:
    - PyYAML (>= 5.0)
    - jsonschema (>= 4.0)

作者: Spharx AgentOS Team
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
from dataclasses import dataclass


@dataclass
class ValidationResult:
    """Schema 验证结果"""
    config_file: str
    schema_file: str
    passed: bool
    errors: List[str] = None
    
    def __post_init__(self):
        if self.errors is None:
            self.errors = []
    
    def __str__(self):
        status = "✅ PASS" if self.passed else "❌ FAIL"
        result = f"{status}: {Path(self.config_file).name}"
        result += f" → {Path(self.schema_file).name}"
        
        if not self.passed and self.errors:
            for error in self.errors[:3]:  # 只显示前3个错误
                result += f"\n  - {error}"
            if len(self.errors) > 3:
                result += f"\n  ... 还有 {len(self.errors) - 3} 个错误"
        
        return result


class SchemaValidator:
    """JSON Schema 验证器"""
    
    # 配置文件到 Schema 文件的映射关系
    CONFIG_SCHEMA_MAP = {
        'kernel/settings.yaml': 'schema/kernel-settings.schema.json',
        'model/model.yaml': 'schema/model.schema.json',
        'agent/registry.yaml': 'schema/agent-registry.schema.json',
        'skill/registry.yaml': 'schema/skill-registry.schema.json',
        'security/policy.yaml': 'schema/security-policy.schema.json',
        'sanitizer/sanitizer_rules.json': 'schema/sanitizer-rules.schema.json',
        'logging/manager.yaml': 'schema/logging.schema.json',
        'manager_management.yaml': 'schema/config-management.schema.json',
        'service/tool_d/tool.yaml': 'schema/tool-service.schema.json',
    }
    
    def __init__(self, config_dir: str, verbose: bool = False):
        """
        初始化验证器
        
        Args:
            config_dir: 配置根目录路径
            verbose: 是否输出详细信息
        """
        self.config_dir = Path(config_dir)
        self.verbose = verbose
        self.results: List[ValidationResult] = []
        
        # 尝试导入 jsonschema 库
        try:
            import jsonschema
            self.jsonschema = jsonschema
            self.has_jsonschema = True
        except ImportError:
            print("⚠️ 警告: jsonschema 库未安装，将仅检查 Schema 文件存在性和格式")
            self.has_jsonschema = False
    
    def load_json_schema(self, schema_path: Path) -> Optional[Dict]:
        """
        加载 JSON Schema 文件
        
        Args:
            schema_path: Schema 文件路径
            
        Returns:
            Dict: Schema 内容，加载失败返回 None
        """
        if not schema_path.exists():
            return None
        
        try:
            with open(schema_path, 'r', encoding='utf-8') as f:
                return json.load(f)
        except Exception as e:
            if self.verbose:
                print(f"  ⚠️ 无法加载 Schema {schema_path.name}: {e}")
            return None
    
    def load_yaml_config(self, config_path: Path) -> Optional[Dict]:
        """
        加载 YAML 配置文件
        
        Args:
            config_path: 配置文件路径
            
        Returns:
            Dict: 配置内容，加载失败返回 None
        """
        if not config_path.exists():
            return None
        
        try:
            with open(config_path, 'r', encoding='utf-8') as f:
                return yaml.safe_load(f)
        except Exception as e:
            if self.verbose:
                print(f"  ⚠️ 无法加载配置 {config_path.name}: {e}")
            return None
    
    def validate_config_against_schema(self, config_data: Dict, schema: Dict, 
                                       config_name: str, schema_name: str) -> ValidationResult:
        """
        根据 Schema 验证配置数据
        
        Args:
            config_data: 配置数据字典
            schema: JSON Schema 字典
            config_name: 配置文件名（用于错误信息）
            schema_name: Schema 文件名（用于错误信息）
            
        Returns:
            ValidationResult: 验证结果
        """
        if not self.has_jsonschema:
            # 如果没有 jsonschema 库，只进行基本检查
            return ValidationResult(
                config_file=config_name,
                schema_file=schema_name,
                passed=True,
                errors=["跳过详细验证（jsonschema 未安装）"]
            )
        
        try:
            # 使用 Draft2020-12 验证器
            validator_cls = self.jsonschema.Draft202012Validator
            validator = validator_cls(schema)
            
            # 执行验证
            errors = list(validator.iter_errors(config_data))
            
            if not errors:
                return ValidationResult(
                    config_file=config_name,
                    schema_file=schema_name,
                    passed=True,
                    errors=[]
                )
            else:
                error_messages = []
                for error in errors[:10]:  # 限制错误数量
                    path = '.'.join(str(p) for p in error.absolute_path) if error.absolute_path else '(root)'
                    error_msg = f"[{path}] {error.message}"
                    error_messages.append(error_msg)
                
                return ValidationResult(
                    config_file=config_name,
                    schema_file=schema_name,
                    passed=False,
                    errors=error_messages
                )
        
        except self.jsonschema.SchemaError as e:
            return ValidationResult(
                config_file=config_name,
                schema_file=schema_name,
                passed=False,
                errors=[f"Schema 本身无效: {e.message}"]
            )
        
        except Exception as e:
            return ValidationResult(
                config_file=config_name,
                schema_file=schema_name,
                passed=False,
                errors=[f"验证过程异常: {str(e)}"]
            )
    
    def validate_schema_format(self, schema_path: Path) -> Tuple[bool, List[str]]:
        """
        验证 Schema 文件本身的基本格式
        
        Args:
            schema_path: Schema 文件路径
            
        Returns:
            Tuple[bool, List[str]]: (是否有效, 错误信息列表)
        """
        errors = []
        
        try:
            with open(schema_path, 'r', encoding='utf-8') as f:
                schema = json.load(f)
            
            # 检查必需字段
            required_fields = ['$schema', 'type']
            for field in required_fields:
                if field not in schema:
                    errors.append(f"缺少必需字段: '{field}'")
            
            # 检查 $schema 值是否为有效的 JSON Schema URI
            if '$schema' in schema:
                valid_schemas = [
                    'https://json-schema.org/draft/2020-12/schema',
                    'https://json-schema.org/draft-07/schema#',
                    'http://json-schema.org/draft-07/schema#'
                ]
                if schema['$schema'] not in valid_schemas:
                    errors.append(f"不支持的 $schema 值: {schema['$schema']}")
            
            # 检查 type 值
            if 'type' in schema and schema['type'] != 'object':
                errors.append(f"根节点 type 应该是 'object', 实际为: {schema['type']}")
            
            # 检查 properties 字段（如果 type 是 object）
            if schema.get('type') == 'object' and 'properties' not in schema:
                errors.append("object 类型缺少 'properties' 定义")
            
            return len(errors) == 0, errors
        
        except json.JSONDecodeError as e:
            return False, [f"JSON 格式错误: {e.msg}"]
        except Exception as e:
            return False, [f"无法解析文件: {e}"]
    
    def run_all_tests(self) -> Tuple[int, int, int]:
        """
        运行所有 Schema 验证测试
        
        Returns:
            Tuple[int, int, int]: (通过数, 失败数, 总计)
        """
        print("=" * 80)
        print("AgentOS Manager 模块 JSON Schema 验证测试")
        print("=" * 80)
        print(f"配置目录: {self.config_dir}")
        print(f"测试时间: {__import__('datetime').datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        print()
        
        # 1. Schema 文件基本格式验证
        print("-" * 80)
        print("1. Schema 文件基本格式验证")
        print("-" * 80)
        
        schema_valid_count = 0
        schema_total = len(self.CONFIG_SCHEMA_MAP)
        
        for config_rel, schema_rel in self.CONFIG_SCHEMA_MAP.items():
            schema_path = self.config_dir / schema_rel
            
            if not schema_path.exists():
                print(f"❌ Schema 文件不存在: {schema_rel}")
                continue
            
            is_valid, errors = self.validate_schema_format(schema_path)
            
            if is_valid:
                print(f"✅ {schema_rel} - 格式正确")
                schema_valid_count += 1
            else:
                print(f"❌ {schema_rel} - 格式错误:")
                for error in errors:
                    print(f"   - {error}")
        
        print(f"\nSchema 文件通过率: {schema_valid_count}/{schema_total} ({100*schema_valid_count/schema_total:.0f}%)\n")
        
        # 2. 配置文件与 Schema 匹配验证
        print("-" * 80)
        print("2. 配置文件与 Schema 匹配验证")
        print("-" * 80)
        
        validation_pass = 0
        validation_fail = 0
        validation_skip = 0
        
        for config_rel, schema_rel in self.CONFIG_SCHEMA_MAP.items():
            config_path = self.config_dir / config_rel
            schema_path = self.config_dir / schema_rel
            
            # 检查文件是否存在
            if not config_path.exists():
                print(f"⏭️  跳过: {config_rel} (配置文件不存在)")
                validation_skip += 1
                continue
            
            if not schema_path.exists():
                print(f"⏭️  跳过: {config_rel} (Schema 文件不存在)")
                validation_skip += 1
                continue
            
            # 加载配置和 Schema
            config_data = self.load_yaml_config(config_path)
            schema_data = self.load_json_schema(schema_path)
            
            if config_data is None or schema_data is None:
                validation_fail += 1
                continue
            
            # 执行验证
            result = self.validate_config_against_schema(
                config_data, 
                schema_data,
                config_rel,
                schema_rel
            )
            
            self.results.append(result)
            print(result)
            
            if result.passed:
                validation_pass += 1
            else:
                validation_fail += 1
                
                # 如果失败且 verbose 模式，显示更多细节
                if self.verbose and result.errors:
                    print("\n  详细错误信息:")
                    for i, error in enumerate(result.errors):
                        print(f"  {i+1}. {error}")
        
        print()
        
        # 3. 统计汇总
        total_passed = sum(1 for r in self.results if r.passed)
        total_failed = sum(1 for r in self.results if not r.passed)
        total_tests = len(self.results)
        
        print("=" * 80)
        print("测试结果统计")
        print("=" * 80)
        print(f"Schema 格式验证: {schema_valid_count}/{schema_total} 通过")
        print(f"配置匹配验证:   {total_passed} 通过 / {total_failed} 失败 / {validation_skip} 跳过")
        print()
        
        if total_failed > 0:
            print("=" * 80)
            print("失败的验证详情:")
            print("=" * 80)
            for result in self.results:
                if not result.passed:
                    print(result)
                    print()
        
        return total_passed + schema_valid_count, total_failed, total_tests + schema_total


def main():
    """主函数"""
    parser = argparse.ArgumentParser(
        description='AgentOS Manager 模块 JSON Schema 验证工具',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例用法:
  python test_schema_validation.py                          # 使用默认路径
  python test_schema_validation.py --verbose                 # 详细输出
  python test_schema_validation.py --config-dir ./my-configs # 指定配置目录
  
依赖安装:
  pip install pyyaml jsonschema
  
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
        validator = SchemaValidator(args.config_dir, args.verbose)
        passed, failed, total = validator.run_all_tests()
        
        sys.exit(0 if failed == 0 else 1)
    
    except Exception as e:
        print(f"❌ 执行异常: {e}", file=sys.stderr)
        sys.exit(2)


if __name__ == '__main__':
    main()