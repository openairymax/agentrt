"""
AgentOS 配置验证单元测试
测试配置验证脚本的功能
"""

import pytest
import sys
import os
from pathlib import Path

# 添加项目根目录到 Python 路径
project_root = Path(__file__).parent.parent.parent.parent
sys.path.insert(0, str(project_root))

from scripts.dev.validate_config import ConfigValidator


class TestConfigValidator:
    """配置验证器测试类"""
    
    def setup_method(self):
        """测试前设置"""
        self.config_dir = project_root / 'agentos/manager'
        self.schema_dir = project_root / 'agentos/manager' / 'schema'
        self.validator = ConfigValidator(str(self.config_dir), str(self.schema_dir))
    
    def test_load_schema(self):
        """测试加载 Schema 文件"""
        schema = self.validator.load_schema('kernel-settings.schema.json')
        assert schema is not None
        assert '$schema' in schema
        assert '$id' in schema
    
    def test_load_config(self):
        """测试加载配置文件"""
        manager = self.validator.load_config('kernel/settings.yaml')
        assert manager is not None
        assert 'kernel' in manager
        assert '_config_version' in manager
    
    def test_validate_config(self):
        """测试配置验证"""
        result = self.validator.validate_config('kernel/settings.yaml', 'kernel-settings.schema.json')
        assert result is True
    
    def test_validate_all(self):
        """测试验证所有配置"""
        result = self.validator.validate_all()
        assert result is True
    
    def test_check_config_version(self):
        """测试配置版本检查"""
        result = self.validator.check_config_version()
        assert result is True
    
    def test_check_environment_variables(self):
        """测试环境变量检查"""
        result = self.validator.check_environment_variables()
        assert result is True
    
    def test_generate_report(self):
        """测试生成报告"""
        report = self.validator.generate_report()
        assert isinstance(report, str)
        assert 'AgentOS 配置验证报告' in report
        assert len(report) > 0


def test_missing_config_file():
    """测试缺失配置文件"""
    validator = ConfigValidator(str(project_root / 'agentos/manager'), str(project_root / 'agentos/manager' / 'schema'))
    result = validator.validate_config('nonexistent.yaml', 'kernel-settings.schema.json')
    assert result is False
    assert len(validator.errors) > 0


def test_invalid_yaml_syntax():
    """测试无效的 YAML 语法"""
    validator = ConfigValidator(str(project_root / 'agentos/manager'), str(project_root / 'agentos/manager' / 'schema'))
    # 创建一个无效的 YAML 文件进行测试
    test_file = project_root / 'agentos/manager' / 'test_invalid.yaml'
    test_file.write_text("invalid: yaml: content: [unclosed")
    
    try:
        result = validator.validate_config('test_invalid.yaml', 'kernel-settings.schema.json')
        assert result is False
    finally:
        # 清理测试文件
        if test_file.exists():
            test_file.unlink()


def test_missing_schema_file():
    """测试缺失 Schema 文件"""
    validator = ConfigValidator(str(project_root / 'agentos/manager'), str(project_root / 'agentos/manager' / 'schema'))
    result = validator.validate_config('kernel/settings.yaml', 'nonexistent.schema.json')
    assert result is False
    assert len(validator.errors) > 0


def test_missing_config_version():
    """测试缺少配置版本"""
    validator = ConfigValidator(str(project_root / 'agentos/manager'), str(project_root / 'agentos/manager' / 'schema'))
    # 创建一个没有版本的配置
    test_file = project_root / 'agentos/manager' / 'test_no_version.yaml'
    test_file.write_text("kernel:\n  log_level: info")
    
    try:
        result = validator.check_config_version()
        # 检查警告中是否包含文件名
        assert any('test_no_version.yaml' in w for w in validator.warnings)
    finally:
        if test_file.exists():
            test_file.unlink()


def test_environment_variable_detection():
    """测试环境变量检测"""
    validator = ConfigValidator(str(project_root / 'agentos/manager'), str(project_root / 'agentos/manager' / 'schema'))
    result = validator.check_environment_variables()
    assert result is True
    # 应该检测到一些环境变量
    assert len(validator.warnings) > 0 or len(validator.errors) > 0


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
