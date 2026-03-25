#!/usr/bin/env python3
"""
AgentOS 配置验证脚本
验证配置文件是否符合 JSON Schema 规范
"""

import sys
import os
import json
import yaml
from pathlib import Path
from typing import Dict, List, Tuple
import argparse

try:
    from jsonschema import validate, Draft7Validator
except ImportError:
    print("错误: 需要安装 jsonschema 库")
    print("安装命令: pip install jsonschema")
    sys.exit(1)


class ConfigValidator:
    """配置验证器"""
    
    def __init__(self, config_dir: str, schema_dir: str):
        self.config_dir = Path(config_dir)
        self.schema_dir = Path(schema_dir)
        self.errors: List[str] = []
        self.warnings: List[str] = []
    
    def load_schema(self, schema_file: str) -> dict:
        """加载 JSON Schema"""
        schema_path = self.schema_dir / schema_file
        if not schema_path.exists():
            self.errors.append(f"Schema 文件不存在: {schema_path}")
            return None
        
        with open(schema_path, 'r', encoding='utf-8') as f:
            try:
                return json.load(f)
            except json.JSONDecodeError as e:
                self.errors.append(f"Schema JSON 解析失败 {schema_file}: {e}")
                return None
    
    def load_config(self, config_file: str) -> dict:
        """加载配置文件（YAML 或 JSON）"""
        config_path = self.config_dir / config_file
        if not config_path.exists():
            self.errors.append(f"配置文件不存在: {config_path}")
            return None
        
        with open(config_path, 'r', encoding='utf-8') as f:
            try:
                if config_file.endswith('.json'):
                    return json.load(f)
                elif config_file.endswith('.yaml') or config_file.endswith('.yml'):
                    return yaml.safe_load(f)
                else:
                    self.errors.append(f"不支持的配置文件格式: {config_file}")
                    return None
            except (json.JSONDecodeError, yaml.YAMLError) as e:
                self.errors.append(f"配置文件解析失败 {config_file}: {e}")
                return None
    
    def validate_config(self, config_file: str, schema_file: str) -> bool:
        """验证配置文件是否符合 Schema"""
        config_data = self.load_config(config_file)
        schema_data = self.load_schema(schema_file)
        
        if not config_data or not schema_data:
            return False
        
        try:
            validate(instance=config_data, schema=schema_data, cls=Draft7Validator)
            return True
        except Exception as e:
            self.errors.append(f"验证失败 {config_file}: {e}")
            return False
    
    def validate_all(self) -> bool:
        """验证所有配置文件"""
        schema_mapping = {
            'kernel/settings.yaml': 'kernel-settings.schema.json',
            'model/model.yaml': 'model.schema.json',
            'agent/registry.yaml': 'agent-registry.schema.json',
            'skill/registry.yaml': 'skill-registry.schema.json',
            'security/policy.yaml': 'security-policy.schema.json',
            'sanitizer/sanitizer_rules.json': 'sanitizer-rules.schema.json',
            'logging/config.yaml': 'logging.schema.json',
            'config_management.yaml': 'config-management.schema.json',
        }
        
        all_valid = True
        for config_file, schema_file in schema_mapping.items():
            if self.validate_config(config_file, schema_file):
                print(f"✓ {config_file} 验证通过")
            else:
                print(f"✗ {config_file} 验证失败")
                all_valid = False
        
        return all_valid
    
    def check_config_version(self) -> bool:
        """检查配置文件版本"""
        version_compatible = True
        
        config_files = [
            'kernel/settings.yaml',
            'model/model.yaml',
            'agent/registry.yaml',
            'skill/registry.yaml',
            'security/policy.yaml',
            'logging/config.yaml',
            'config_management.yaml',
        ]
        
        for config_file in config_files:
            config_data = self.load_config(config_file)
            if config_data and '_config_version' in config_data:
                version = config_data['_config_version']
                print(f"  {config_file}: 版本 {version}")
            else:
                self.warnings.append(f"{config_file}: 缺少 _config_version 字段")
                version_compatible = False
        
        return version_compatible
    
    def check_environment_variables(self) -> bool:
        """检查环境变量引用"""
        env_vars_found = set()
        
        config_files = [
            'kernel/settings.yaml',
            'model/model.yaml',
            'security/policy.yaml',
            'logging/config.yaml',
            'config_management.yaml',
        ]
        
        for config_file in config_files:
            config_data = self.load_config(config_file)
            if not config_data:
                continue
            
            config_str = str(config_data)
            import re
            
            # 查找环境变量引用 ${VAR_NAME}
            env_refs = re.findall(r'\$\{([^}]+)\}', config_str)
            for ref in env_refs:
                env_vars_found.add(ref)
        
        if env_vars_found:
            print(f"\n发现的环境变量引用:")
            for var in sorted(env_vars_found):
                print(f"  - {var}")
        
        return True
    
    def generate_report(self) -> str:
        """生成验证报告"""
        report = []
        report.append("=" * 60)
        report.append("AgentOS 配置验证报告")
        report.append("=" * 60)
        report.append("")
        
        if self.errors:
            report.append("错误:")
            for error in self.errors:
                report.append(f"  ✗ {error}")
        else:
            report.append("✓ 所有配置文件验证通过")
        
        if self.warnings:
            report.append("\n警告:")
            for warning in self.warnings:
                report.append(f"  ⚠ {warning}")
        
        report.append("")
        report.append("=" * 60)
        return "\n".join(report)


def main():
    parser = argparse.ArgumentParser(description='AgentOS 配置验证工具')
    parser.add_argument('--config', '-c', 
                       default='config',
                       help='配置目录路径')
    parser.add_argument('--schema', '-s',
                       default='config/schema',
                       help='Schema 目录路径')
    parser.add_argument('--all', '-a',
                       action='store_true',
                       help='验证所有配置文件')
    parser.add_argument('--file', '-f',
                       help='验证单个配置文件')
    parser.add_argument('--check-version', '-v',
                       action='store_true',
                       help='检查配置版本')
    parser.add_argument('--check-env', '-e',
                       action='store_true',
                       help='检查环境变量引用')
    
    args = parser.parse_args()
    
    validator = ConfigValidator(args.config, args.schema)
    
    if args.check_version:
        validator.check_config_version()
    
    if args.check_env:
        validator.check_environment_variables()
    
    if args.all:
        success = validator.validate_all()
        print(validator.generate_report())
        sys.exit(0 if success else 1)
    elif args.file:
        # 确定对应的 schema
        schema_mapping = {
            'kernel/settings.yaml': 'kernel-settings.schema.json',
            'model/model.yaml': 'model.schema.json',
            'agent/registry.yaml': 'agent-registry.schema.json',
            'skill/registry.yaml': 'skill-registry.schema.json',
            'security/policy.yaml': 'security-policy.schema.json',
            'sanitizer/sanitizer_rules.json': 'sanitizer-rules.schema.json',
            'logging/config.yaml': 'logging.schema.json',
            'config_management.yaml': 'config-management.schema.json',
        }
        
        schema_file = schema_mapping.get(args.file)
        if not schema_file:
            print(f"错误: 无法找到 {args.file} 对应的 Schema 文件")
            sys.exit(1)
        
        success = validator.validate_config(args.file, schema_file)
        if success:
            print(f"✓ {args.file} 验证通过")
        else:
            print(f"✗ {args.file} 验证失败")
            print(validator.generate_report())
            sys.exit(1)
    else:
        parser.print_help()
        sys.exit(1)


if __name__ == '__main__':
    main()
