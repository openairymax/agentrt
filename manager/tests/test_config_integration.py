#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Manager 模块配置加载集成测试

本测试脚本验证 AgentOS/manager 模块配置的完整性和一致性，包括：
1. 配置文件之间的引用关系
2. 环境变量定义的完整性
3. 配置值的合理性和约束
4. 跨模块配置的一致性

遵循 ARCHITECTURAL_PRINCIPLES.md 的 E-8（可测试性原则）和 S-3（总体设计部原则）。

使用方法:
    python test_config_integration.py [--verbose] [--config-dir <path>]

作者: CTO (SOLO Coder)
版本: v1.0.0
日期: 2026-04-01
"""

import os
import sys
import re
import yaml
import json
import argparse
from pathlib import Path
from typing import List, Dict, Tuple, Set, Optional, Any
from dataclasses import dataclass, field


@dataclass
class IntegrationTestResult:
    """集成测试结果"""
    test_name: str
    passed: bool
    details: str = ""
    severity: str = "info"  # info, warning, error
    
    def __str__(self):
        status = {
            True: "✅ PASS",
            False: "❌ FAIL"
        }[self.passed]
        
        result = f"{status}: {self.test_name}"
        if self.details:
            result += f"\n  {self.details}"
        return result


class ConfigIntegrationTester:
    """配置集成测试器"""
    
    # 环境变量定义（从 .env.template 提取）
    EXPECTED_ENV_VARS = {
        'AGENTOS_ROOT': {'required': True, 'description': 'AgentOS 根目录'},
        'AGENTOS_DATA_DIR': {'required': True, 'description': '数据存储目录'},
        'AGENTOS_LOG_DIR': {'required': True, 'description': '日志存储目录'},
        'AGENTOS_TEMP_DIR': {'required': True, 'description': '临时文件目录'},
        'AGENTOS_CACHE_DIR': {'required': True, 'description': '缓存目录'},
        'AGENTOS_CONFIG_DIR': {'required': True, 'description': '配置管理目录'},
        'OPENAI_API_KEY': {'required': False, 'description': 'OpenAI API 密钥'},
        'ANTHROPIC_API_KEY': {'required': False, 'description': 'Anthropic API 密钥'},
        'SECURITY_WEBHOOK_URL': {'required': False, 'description': '安全告警 Webhook URL'},
    }
    
    def __init__(self, config_dir: str, verbose: bool = False):
        """
        初始化集成测试器
        
        Args:
            config_dir: 配置根目录路径
            verbose: 是否输出详细信息
        """
        self.config_dir = Path(config_dir)
        self.verbose = verbose
        self.results: List[IntegrationTestResult] = []
        self.configs_cache: Dict[str, Any] = {}
    
    def load_all_configs(self) -> bool:
        """
        加载所有配置文件到缓存
        
        Returns:
            bool: 是否全部成功加载
        """
        config_files = [
            ('kernel/settings.yaml', 'kernel'),
            ('model/model.yaml', 'model'),
            ('agent/registry.yaml', 'agents'),
            ('security/policy.yaml', 'security'),
            ('logging/manager.yaml', 'logging'),
            ('manager_management.yaml', 'management'),
        ]
        
        all_success = True
        
        for file_rel, cache_key in config_files:
            file_path = self.config_dir / file_rel
            
            if not file_path.exists():
                if self.verbose:
                    print(f"  ⚠️ 配置文件不存在: {file_rel}")
                all_success = False
                continue
            
            try:
                with open(file_path, 'r', encoding='utf-8') as f:
                    if file_path.suffix in ('.yaml', '.yml'):
                        self.configs_cache[cache_key] = yaml.safe_load(f)
                    elif file_path.suffix == '.json':
                        self.configs_cache[cache_key] = json.load(f)
                
                if self.verbose:
                    print(f"  ✅ 已加载: {file_rel}")
            
            except Exception as e:
                print(f"  ❌ 加载失败: {file_rel} - {e}")
                all_success = False
        
        return all_success
    
    def test_environment_variable_completeness(self) -> IntegrationTestResult:
        """
        测试环境变量定义完整性
        
        检查 .env.template 是否定义了所有必需的环境变量
        """
        env_template_path = self.config_dir / '.env.template'
        
        if not env_template_path.exists():
            return IntegrationTestResult(
                test_name="环境变量模板完整性",
                passed=False,
                details=".env.template 文件不存在",
                severity="error"
            )
        
        try:
            with open(env_template_path, 'r', encoding='utf-8') as f:
                content = f.read()
            
            defined_vars = set()
            for line in content.split('\n'):
                line = line.strip()
                if '=' in line and not line.startswith('#'):
                    var_name = line.split('=')[0].strip()
                    defined_vars.add(var_name)
            
            missing_vars = []
            extra_vars = []
            
            for var_name, info in self.EXPECTED_ENV_VARS.items():
                if var_name not in defined_vars and info['required']:
                    missing_vars.append(var_name)
            
            for var_name in defined_vars:
                if var_name not in self.EXPECTED_ENV_VARS:
                    extra_vars.append(var_name)
            
            details_parts = []
            if missing_vars:
                details_parts.append(f"缺少必需变量: {', '.join(missing_vars)}")
            if extra_vars:
                details_parts.append(f"额外定义的变量: {', '.join(extra_vars[:5])}...")
            
            is_pass = len(missing_vars) == 0
            
            return IntegrationTestResult(
                test_name="环境变量模板完整性",
                passed=is_pass,
                details='; '.join(details_parts) if details_parts else f"已定义 {len(defined_vars)} 个环境变量",
                severity="error" if not is_pass else "info"
            )
        
        except Exception as e:
            return IntegrationTestResult(
                test_name="环境变量模板完整性",
                passed=False,
                details=f"解析失败: {e}",
                severity="error"
            )
    
    def test_owner_metadata_present(self) -> IntegrationTestResult:
        """
        测试配置归属元数据完整性
        
        验证所有核心配置文件都包含 _owner 字段
        """
        required_owners = ['kernel', 'model', 'security']
        missing_owners = []
        
        for key in required_owners:
            config_data = self.configs_cache.get(key)
            
            if config_data is None:
                missing_owners.append(f"{key} (未加载)")
                continue
            
            if '_owner' not in config_data:
                owner_info = config_data.get('_owner', {})
                missing_owners.append(key)
            elif not isinstance(config_data['_owner'], dict):
                missing_owners.append(f"{key} (_owner 格式错误)")
            else:
                owner = config_data['_owner']
                required_fields = ['module', 'contact', 'path']
                missing_fields = [f for f in required_fields if f not in owner]
                if missing_fields:
                    missing_owners.append(f"{key} (缺少字段: {', '.join(missing_fields)})")
        
        is_pass = len(missing_owners) == 0
        
        return IntegrationTestResult(
            test_name="配置归属元数据 (_owner)",
            passed=is_pass,
            details=f"检查 {len(required_owners)} 个配置; 缺失: {', '.join(missing_owners) if missing_owners else '无'}",
            severity="warning" if not is_pass else "info"
        )
    
    def test_version_metadata_consistency(self) -> IntegrationTestResult:
        """
        测试版本元数据一致性
        
        验证所有配置文件的版本号格式一致
        """
        version_pattern = re.compile(r'^\d+\.\d+\.\d+$')
        inconsistent_versions = []
        
        for key, config_data in self.configs_cache.items():
            if config_data is None:
                continue
            
            version = config_data.get('_config_version')
            
            if version is None:
                inconsistent_versions.append(f"{key}: 缺少 _config_version")
            elif not isinstance(version, str):
                inconsistent_versions.append(f"{key}: 版本格式非字符串")
            elif not version_pattern.match(version):
                inconsistent_versions.append(f"{key}: 版本格式无效 '{version}'")
        
        is_pass = len(inconsistent_versions) == 0
        
        return IntegrationTestResult(
            test_name="版本元数据一致性 (_config_version)",
            passed=is_pass,
            details=f"检查 {len(self.configs_cache)} 个配置; 问题: {len(inconsistent_versions)} 个" + 
                   (f"\n  - {'\n  - '.join(inconsistent_versions[:5])}" if inconsistent_versions else ""),
            severity="warning" if not is_pass else "info"
        )
    
    def test_schema_reference_validity(self) -> IntegrationTestResult:
        """
        测试 Schema 引用有效性
        
        验证配置文件中声明的 Schema 文件是否存在
        """
        schema_refs = {}
        invalid_refs = []
        
        for key, config_data in self.configs_cache.items():
            if config_data is None:
                continue
            
            schema_ref = config_data.get('_schema')
            if schema_ref:
                schema_refs[key] = schema_ref
                
                schema_path = self.config_dir / schema_ref
                if not schema_path.exists():
                    invalid_refs.append(f"{key} → {schema_ref}")
        
        is_pass = len(invalid_refs) == 0
        
        return IntegrationTestResult(
            test_name="Schema 引用有效性 (_schema)",
            passed=is_pass,
            details=f"找到 {len(schema_refs)} 个 Schema 引用; 无效: {len(invalid_refs)} 个" +
                   (f"\n  - {'\n  - '.join(invalid_refs)}" if invalid_refs else ""),
            severity="error" if not is_pass else "info"
        )
    
    def test_agent_registry_completeness(self) -> IntegrationTestResult:
        """
        测试 Agent 注册表完整性
        
        验证 agent/registry.yaml 包含所有预期的 Agent 定义
        """
        agents_config = self.configs_cache.get('agents')
        
        if agents_config is None:
            return IntegrationTestResult(
                test_name="Agent 注册表完整性",
                passed=False,
                details="无法加载 agent/registry.yaml",
                severity="error"
            )
        
        agents_list = agents_config.get('agents', [])
        
        expected_agents = [
            'planner',
            'architect',
            'code_reviewer',
            'frontend_engineer',
            'backend_engineer',
            'data_engineer',
            'test_engineer',
            'deployment_engineer',
            'security_analyst',
            'documentation_specialist'
        ]
        
        defined_agents = [a.get('name') for a in agents_list if isinstance(a, dict)]
        missing_agents = [a for a in expected_agents if a not in defined_agents]
        
        is_pass = len(missing_agents) == 0
        
        return IntegrationTestResult(
            test_name="Agent 注册表完整性",
            passed=is_pass,
            details=f"已定义 {len(defined_agents)} 个 Agent; 缺失: {', '.join(missing_agents) if missing_agents else '无'}",
            severity="error" if not is_pass else "info"
        )
    
    def test_scheduler_dual_cognitive_system(self) -> IntegrationTestResult:
        """
        测试调度器双系统认知配置
        
        验证 kernel/settings.yaml 中包含 System 1 和 System 2 的完整配置
        """
        kernel_config = self.configs_cache.get('kernel')
        
        if kernel_config is None:
            return IntegrationTestResult(
                test_name="调度器双系统认知配置",
                passed=False,
                details="无法加载 kernel/settings.yaml",
                severity="error"
            )
        
        scheduler = kernel_config.get('scheduler', {})
        cognitive_systems = scheduler.get('cognitive_systems', {})
        
        system1 = cognitive_systems.get('system1', {})
        system2 = cognitive_systems.get('system2', {})
        
        issues = []
        
        # 检查 System 1
        if not system1:
            issues.append("缺少 system1 (快思考系统) 配置")
        else:
            required_s1_fields = ['enabled', 'name', 'task_types', 'max_latency_ms']
            for field in required_s1_fields:
                if field not in system1:
                    issues.append(f"system1 缺少字段: {field}")
        
        # 检查 System 2
        if not system2:
            issues.append("缺少 system2 (慢思考系统) 配置")
        else:
            required_s2_fields = ['enabled', 'name', 'task_types', 'max_latency_ms']
            for field in required_s2_fields:
                if field not in system2:
                    issues.append(f"system2 缺少字段: {field}")
        
        is_pass = len(issues) == 0
        
        return IntegrationTestResult(
            test_name="调度器双系统认知配置 (C-1 原则)",
            passed=is_pass,
            details=f"System 1: {'✅' if system1 else '❌'}, System 2: {'✅' if system2 else '❌'}" +
                   (f"\n问题: {'; '.join(issues)}" if issues else ""),
            severity="error" if not is_pass else "info"
        )
    
    def test_security_default_policy_deny(self) -> IntegrationTestResult:
        """
        测试安全默认策略为拒绝
        
        验证 security/policy.yaml 的 default_policy 为 "deny"
        """
        security_config = self.configs_cache.get('security')
        
        if security_config is None:
            return IntegrationTestResult(
                test_name="安全默认拒绝策略 (E-1 原则)",
                passed=False,
                details="无法加载 security/policy.yaml",
                severity="error"
            )
        
        default_policy = security_config.get('security', {}).get('default_policy', '')
        is_pass = default_policy.lower() == 'deny'
        
        return IntegrationTestResult(
            test_name="安全默认拒绝策略 (E-1 原则)",
            passed=is_pass,
            details=f"default_policy = '{default_policy}'" +
                   (" ✅ 符合最小权限原则" if is_pass else " ❌ 应设置为 'deny'"),
            severity="error" if not is_pass else "info"
        )
    
    def test_logging_domain_id_format(self) -> IntegrationTestResult:
        """
        测试日志 Domain ID 格式
        
        验证 logging/manager.yaml 中的 Domain ID 符合 Log_guide.md 规范
        """
        logging_config = self.configs_cache.get('logging')
        
        if logging_config is None:
            return IntegrationTestResult(
                test_name="日志 Domain ID 格式",
                passed=False,
                details="无法加载 logging/manager.yaml",
                severity="error"
            )
        
        domain_config = logging_config.get('domain', {})
        domain_id = domain_config.get('id', '')
        
        # Domain ID 应符合格式: 0xD0xxxyy (16进制，Manager 模块范围)
        pattern = re.compile(r'^0xD0\d{6}$')
        is_valid = bool(pattern.match(domain_id))
        
        return IntegrationTestResult(
            test_name="日志 Domain ID 格式 (Log_guide 规范)",
            passed=is_valid,
            details=f"Domain ID: '{domain_id}'" +
                   (" ✅ 符合规范" if is_valid else " ❌ 应符合 0xD0xxxxx 格式"),
            severity="error" if not is_valid else "info"
        )
    
    def test_model_fallback_configuration(self) -> IntegrationTestResult:
        """
        测试模型降级配置完整性
        
        验证 model/model.yaml 包含完整的降级和熔断配置
        """
        model_config = self.configs_cache.get('model')
        
        if model_config is None:
            return IntegrationTestResult(
                test_name="模型降级配置完整性",
                passed=False,
                details="无法加载 model/model.yaml",
                severity="error"
            )
        
        fallback = model_config.get('fallback', {})
        circuit_breaker = model_config.get('circuit_breaker', {})
        
        issues = []
        
        # 检查降级配置
        fallback_required = ['enabled', 'strategy', 'timeout_ms']
        for field in fallback_required:
            if field not in fallback:
                issues.append(f"fallback 缺少字段: {field}")
        
        # 检查熔断器配置
        cb_required = ['enabled', 'failure_threshold', 'recovery_timeout_ms']
        for field in cb_required:
            if field not in circuit_breaker:
                issues.append(f"circuit_breaker 缺少字段: {field}")
        
        is_pass = len(issues) == 0
        
        return IntegrationTestResult(
            test_name="模型降级与熔断配置",
            passed=is_pass,
            details=f"fallback: {'✅' if fallback else '❌'}, circuit_breaker: {'✅' if circuit_breaker else '❌'}" +
                   (f"\n问题: {'; '.join(issues)}" if issues else ""),
            severity="warning" if not is_pass else "info"
        )
    
    def test_audit_log_configuration(self) -> IntegrationTestResult:
        """
        测试审计日志配置完整性
        
        验证 manager_management.yaml 中审计配置是否完整
        """
        management_config = self.configs_cache.get('management')
        
        if management_config is None:
            return IntegrationTestResult(
                test_name="审计日志配置完整性 (E-2 原则)",
                passed=False,
                details="无法加载 manager_management.yaml",
                severity="error"
            )
        
        audit_config = management_config.get('audit', {})
        
        issues = []
        
        if not audit_config.get('enabled'):
            issues.append("审计未启用 (audit.enabled)")
        else:
            if not audit_config.get('log_path'):
                issues.append("缺少审计日志路径 (audit.log_path)")
            
            events = audit_config.get('events', [])
            expected_events = ['config.load', 'config.reload', 'config.change', 'config.rollback']
            missing_events = [e for e in expected_events if e not in events]
            if missing_events:
                issues.append(f"缺少审计事件类型: {', '.join(missing_events)}")
            
            if not audit_config.get('retention_days'):
                issues.append("缺少审计保留天数 (audit.retention_days)")
        
        is_pass = len(issues) == 0
        
        return IntegrationTestResult(
            test_name="审计日志配置完整性 (E-2 原则)",
            passed=is_pass,
            details=f"审计启用: {'✅' if audit_config.get('enabled') else '❌'}" +
                   (f"\n问题: {'; '.join(issues)}" if issues else ""),
            severity="error" if not is_pass else "info"
        )
    
    def test_hot_reload_configuration(self) -> IntegrationTestResult:
        """
        测试热更新配置完整性
        
        验证 manager_management.yaml 中热更新配置是否完整
        """
        management_config = self.configs_cache.get('management')
        
        if management_config is None:
            return IntegrationTestResult(
                test_name="热更新配置完整性 (S-1 原则)",
                passed=False,
                details="无法加载 manager_management.yaml",
                severity="error"
            )
        
        hot_reload = management_config.get('hot_reload', {})
        
        issues = []
        
        if not hot_reload.get('enabled'):
            issues.append("热更新未启用 (hot_reload.enabled)")
        else:
            if not hot_reload.get('check_interval_sec'):
                issues.append("缺少检查间隔 (hot_reload.check_interval_sec)")
            
            watch_files = hot_reload.get('watch_files', [])
            if not watch_files:
                issues.append("缺少监控文件列表 (hot_reload.watch_files)")
            
            supported_paths = hot_reload.get('supported_paths', [])
            if not supported_paths:
                issues.append("缺少支持热更新的配置路径 (hot_reload.supported_paths)")
        
        is_pass = len(issues) == 0
        
        return IntegrationTestResult(
            test_name="热更新配置完整性 (S-1 原则)",
            passed=is_pass,
            details=f"热更新启用: {'✅' if hot_reload.get('enabled') else '❌'}" +
                   (f"\n问题: {'; '.join(issues)}" if issues else ""),
            severity="warning" if not is_pass else "info"
        )
    
    def test_encryption_configuration(self) -> IntegrationTestResult:
        """
        测试加密配置完整性
        
        验证 manager_management.yaml 中加密配置是否完整
        """
        management_config = self.configs_cache.get('management')
        
        if management_config is None:
            return IntegrationTestResult(
                test_name="加密配置完整性 (E-1 原则)",
                passed=False,
                details="无法加载 manager_management.yaml",
                severity="error"
            )
        
        encryption = management_config.get('encryption', {})
        
        issues = []
        
        if not encryption.get('enabled'):
            issues.append("加密未启用 (encryption.enabled)")
        else:
            if not encryption.get('algorithm'):
                issues.append("缺少加密算法 (encryption.algorithm)")
            elif encryption.get('algorithm') != 'aes-256-gcm':
                issues.append(f"加密算法不推荐: {encryption.get('algorithm')} (推荐: aes-256-gcm)")
            
            if not encryption.get('key_source'):
                issues.append("缺少密钥来源 (encryption.key_source)")
            
            if not encryption.get('key_env_var'):
                issues.append("缺少密钥环境变量名 (encryption.key_env_var)")
            
            encrypted_fields = encryption.get('encrypted_fields', [])
            if not encrypted_fields:
                issues.append("缺少加密字段列表 (encryption.encrypted_fields)")
        
        is_pass = len(issues) == 0
        
        return IntegrationTestResult(
            test_name="加密配置完整性 (E-1 原则)",
            passed=is_pass,
            details=f"加密启用: {'✅' if encryption.get('enabled') else '❌'}" +
                   (f"\n问题: {'; '.join(issues)}" if issues else ""),
            severity="warning" if not is_pass else "info"
        )
    
    def test_rollback_configuration(self) -> IntegrationTestResult:
        """
        测试回滚配置完整性
        
        验证 manager_management.yaml 中回滚配置是否完整
        """
        management_config = self.configs_cache.get('management')
        
        if management_config is None:
            return IntegrationTestResult(
                test_name="回滚配置完整性 (S-1 原则)",
                passed=False,
                details="无法加载 manager_management.yaml",
                severity="error"
            )
        
        rollback = management_config.get('rollback', {})
        
        issues = []
        
        if not rollback.get('enabled'):
            issues.append("回滚未启用 (rollback.enabled)")
        else:
            if not rollback.get('max_steps'):
                issues.append("缺少最大回滚步数 (rollback.max_steps)")
            
            auto_rollback = rollback.get('auto_rollback', {})
            if auto_rollback:
                if auto_rollback.get('on_load_failure') is None:
                    issues.append("缺少加载失败自动回滚配置 (rollback.auto_rollback.on_load_failure)")
                if auto_rollback.get('on_validation_failure') is None:
                    issues.append("缺少验证失败自动回滚配置 (rollback.auto_rollback.on_validation_failure)")
        
        is_pass = len(issues) == 0
        
        return IntegrationTestResult(
            test_name="回滚配置完整性 (S-1 原则)",
            passed=is_pass,
            details=f"回滚启用: {'✅' if rollback.get('enabled') else '❌'}" +
                   (f"\n问题: {'; '.join(issues)}" if issues else ""),
            severity="warning" if not is_pass else "info"
        )
    
    def run_all_tests(self) -> Tuple[int, int, int]:
        """
        运行所有集成测试
        
        Returns:
            Tuple[int, int, int]: (通过数, 失败数, 总计)
        """
        print("=" * 80)
        print("AgentOS Manager 模块配置集成测试")
        print("=" * 80)
        print(f"配置目录: {self.config_dir}")
        print(f"测试时间: {__import__('datetime').datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        print()
        
        # 预先加载所有配置
        print("-" * 80)
        print("0. 预加载所有配置文件")
        print("-" * 80)
        
        load_success = self.load_all_configs()
        status = "✅ 成功" if load_success else "⚠️ 部分失败"
        print(f"配置加载状态: {status}\n")
        
        # 定义测试列表
        tests = [
            self.test_environment_variable_completeness,
            self.test_owner_metadata_present,
            self.test_version_metadata_consistency,
            self.test_schema_reference_validity,
            self.test_agent_registry_completeness,
            self.test_scheduler_dual_cognitive_system,
            self.test_security_default_policy_deny,
            self.test_logging_domain_id_format,
            self.test_model_fallback_configuration,
            self.test_audit_log_configuration,
            self.test_hot_reload_configuration,
            self.test_encryption_configuration,
            self.test_rollback_configuration,
        ]
        
        # 执行所有测试
        print("=" * 80)
        print("执行集成测试")
        print("=" * 80)
        
        for i, test_func in enumerate(tests, 1):
            print(f"\n[{i}/{len(tests)}] ", end="")
            result = test_func()
            self.results.append(result)
            print(result)
        
        print("\n")
        
        # 统计汇总
        total_passed = sum(1 for r in self.results if r.passed)
        total_failed = sum(1 for r in self.results if not r.passed)
        total_tests = len(self.results)
        
        error_count = sum(1 for r in self.results if not r.passed and r.severity == 'error')
        warning_count = sum(1 for r in self.results if not r.passed and r.severity == 'warning')
        
        print("=" * 80)
        print("集成测试结果统计")
        print("=" * 80)
        print(f"总测试数: {total_tests}")
        print(f"通过数量: {total_passed} ✅")
        print(f"失败数量: {total_failed} ❌")
        print(f"  - 错误级: {error_count} 🔴")
        print(f"  - 警告级: {warning_count} 🟡")
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
        description='AgentOS Manager 模块配置集成测试工具',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例用法:
  python test_config_integration.py                          # 使用默认路径
  python test_config_integration.py --verbose                 # 详细输出
  python test_config_integration.py --config-dir ./my-configs # 指定配置目录
  
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
    
    args = parser.parse_args()
    
    try:
        tester = ConfigIntegrationTester(args.config_dir, args.verbose)
        passed, failed, total = tester.run_all_tests()
        
        sys.exit(0 if failed == 0 else 1)
    
    except Exception as e:
        print(f"❌ 执行异常: {e}", file=sys.stderr)
        sys.exit(2)


if __name__ == '__main__':
    main()