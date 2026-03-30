"""
AgentOS Agent 契约测试模块（优化版）
Version: 1.1.0
Last updated: 2026-03-26

使用参数化测试减少重复代码，提高可维护性
"""

import pytest
import json
import re
from typing import Dict, Any, List, Optional
from pathlib import Path
from unittest.mock import Mock, MagicMock, patch
from dataclasses import dataclass, field
from enum import Enum

import sys
import os
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', 'tools', 'python')))

from tests.utils.test_helpers import (
    ContractTestHelper,
    TestDataBuilder,
    assert_error_contains
)


# ============================================================
# 测试标记
# ============================================================

pytestmark = pytest.mark.contract


# ============================================================
# 枚举和常量定义
# ============================================================

class MaintenanceLevel(Enum):
    """维护级别枚举"""
    COMMUNITY = "community"
    VERIFIED = "verified"
    OFFICIAL = "official"


VALID_ROLES = [
    "product_manager",
    "software_engineer",
    "data_analyst",
    "ui_designer",
    "qa_engineer",
    "devops_engineer",
    "security_analyst",
    "architect",
    "technical_writer",
    "project_manager",
]

SCHEMA_VERSION = "1.0.0"


# ============================================================
# 契约验证器（优化版）
# ============================================================

class AgentContractValidator:
    """Agent 契约验证器（优化版 - 使用策略模式降低复杂度）"""

    def __init__(self):
        self.errors: List[str] = []
        self.warnings: List[str] = []
        self._validators = {
            "schema_version": self._validate_schema_version,
            "version": self._validate_version,
            "role": self._validate_role,
            "capabilities": self._validate_capabilities,
            "models": self._validate_models,
            "cost_profile": self._validate_cost_profile,
            "trust_metrics": self._validate_trust_metrics,
        }

    def validate(self, contract: Dict[str, Any]) -> bool:
        """
        验证契约完整性和正确性

        Args:
            contract: 契约字典

        Returns:
            bool: 验证是否通过
        """
        self.errors = []
        self.warnings = []

        self._validate_required_fields(contract)
        self._validate_field_types(contract)
        self._validate_semantic_rules(contract)

        return len(self.errors) == 0

    def _validate_required_fields(self, contract: Dict[str, Any]) -> None:
        """验证必需字段是否存在"""
        required_fields = [
            "schema_version", "agent_id", "agent_name", "version",
            "role", "description", "capabilities", "models",
            "required_permissions", "cost_profile", "trust_metrics"
        ]

        for field_name in required_fields:
            if field_name not in contract:
                self.errors.append(f"缺失必需字段: {field_name}")

    def _validate_field_types(self, contract: Dict[str, Any]) -> None:
        """验证字段类型正确性"""
        type_checks = {
            "schema_version": str,
            "agent_id": str,
            "agent_name": str,
            "version": str,
            "role": str,
            "description": str,
            "capabilities": list,
            "models": dict,
            "required_permissions": list,
            "cost_profile": dict,
            "trust_metrics": dict,
        }

        for field_name, expected_type in type_checks.items():
            if field_name in contract:
                if not isinstance(contract[field_name], expected_type):
                    self.errors.append(
                        f"字段类型错误: {field_name} 应为 {expected_type.__name__}, "
                        f"实际为 {type(contract[field_name]).__name__}"
                    )

    def _validate_semantic_rules(self, contract: Dict[str, Any]) -> None:
        """验证语义规则（优化版 - 使用策略模式）"""
        for field_name, validator in self._validators.items():
            if field_name in contract:
                validator(contract[field_name])

    def _validate_schema_version(self, version: str) -> None:
        """验证 schema 版本"""
        if version != SCHEMA_VERSION:
            self.warnings.append(
                f"Schema 版本不匹配: 当前版本 {SCHEMA_VERSION}, "
                f"契约版本 {version}"
            )

    def _validate_version(self, version: str) -> None:
        """验证版本号"""
        if not self._is_valid_semantic_version(version):
            self.errors.append(f"版本号不符合语义化版本规范: {version}")

    def _validate_role(self, role: str) -> None:
        """验证角色"""
        if role not in VALID_ROLES:
            self.warnings.append(f"非标准角色类型: {role}")

    def _validate_capabilities(self, capabilities: List[Dict]) -> None:
        """验证能力列表"""
        if not capabilities:
            self.errors.append("能力列表不能为空")
            return

        required_cap_fields = ["name", "description", "input_schema", "output_schema"]

        for i, cap in enumerate(capabilities):
            if not isinstance(cap, dict):
                self.errors.append(f"能力 {i} 不是有效的对象")
                continue

            for field_name in required_cap_fields:
                if field_name not in cap:
                    self.errors.append(f"能力 {i} 缺失必需字段: {field_name}")

            if "name" in cap:
                if not re.match(r'^[a-z][a-z0-9_]*$', cap["name"]):
                    self.warnings.append(
                        f"能力名称建议使用蛇形命名法: {cap['name']}"
                    )

            if "success_rate" in cap:
                rate = cap["success_rate"]
                if not isinstance(rate, (int, float)) or rate < 0 or rate > 1:
                    self.errors.append(
                        f"能力 {i} 的 success_rate 应在 0-1 之间"
                    )

    def _validate_models(self, models: Dict) -> None:
        """验证模型配置"""
        required_model_fields = ["system1", "system2"]

        for field_name in required_model_fields:
            if field_name not in models:
                self.errors.append(f"模型配置缺失必需字段: {field_name}")

    def _validate_cost_profile(self, cost_profile: Dict) -> None:
        """验证成本概览"""
        required_cost_fields = [
            "token_per_task_avg", "api_cost_per_task", "maintenance_level"
        ]

        for field_name in required_cost_fields:
            if field_name not in cost_profile:
                self.errors.append(f"成本概览缺失必需字段: {field_name}")

        if "maintenance_level" in cost_profile:
            valid_levels = [e.value for e in MaintenanceLevel]
            if cost_profile["maintenance_level"] not in valid_levels:
                self.errors.append(
                    f"无效的维护级别: {cost_profile['maintenance_level']}, "
                    f"应为: {valid_levels}"
                )

        if "api_cost_per_task" in cost_profile:
            cost = cost_profile["api_cost_per_task"]
            if not isinstance(cost, (int, float)) or cost < 0:
                self.errors.append("api_cost_per_task 应为非负数")

    def _validate_trust_metrics(self, trust_metrics: Dict) -> None:
        """验证信任指标"""
        required_trust_fields = [
            "install_count", "rating", "verified_provider", "last_audit"
        ]

        for field_name in required_trust_fields:
            if field_name not in trust_metrics:
                self.errors.append(f"信任指标缺失必需字段: {field_name}")

        if "rating" in trust_metrics:
            rating = trust_metrics["rating"]
            if not isinstance(rating, (int, float)) or rating < 1 or rating > 5:
                self.errors.append("rating 应在 1-5 之间")

        if "last_audit" in trust_metrics:
            audit_date = trust_metrics["last_audit"]
            if not re.match(r'^\d{4}-\d{2}-\d{2}$', str(audit_date)):
                self.errors.append(
                    f"last_audit 应为 ISO 8601 日期格式 (YYYY-MM-DD): {audit_date}"
                )

    def _is_valid_semantic_version(self, version: str) -> bool:
        """检查是否为有效的语义化版本"""
        pattern = r'^(\d+)\.(\d+)\.(\d+)(?:-([a-zA-Z0-9.-]+))?(?:\+([a-zA-Z0-9.-]+))?$'
        return bool(re.match(pattern, version))


# ============================================================
# 参数化测试用例
# ============================================================

# 必需字段缺失测试用例
REQUIRED_FIELD_TEST_CASES = [
    {"name": "schema_version", "expected_error": "schema_version"},
    {"name": "agent_id", "expected_error": "agent_id"},
    {"name": "agent_name", "expected_error": "agent_name"},
    {"name": "version", "expected_error": "version"},
    {"name": "role", "expected_error": "role"},
    {"name": "description", "expected_error": "description"},
    {"name": "capabilities", "expected_error": "capabilities"},
    {"name": "models", "expected_error": "models"},
    {"name": "required_permissions", "expected_error": "required_permissions"},
    {"name": "cost_profile", "expected_error": "cost_profile"},
    {"name": "trust_metrics", "expected_error": "trust_metrics"},
]

# 字段类型错误测试用例
FIELD_TYPE_TEST_CASES = [
    {"name": "capabilities_as_string", "field": "capabilities", "invalid_value": "not_a_list"},
    {"name": "models_as_string", "field": "models", "invalid_value": "not_a_dict"},
    {"name": "required_permissions_as_string", "field": "required_permissions", "invalid_value": "not_a_list"},
]

# 版本格式测试用例
VERSION_FORMAT_TEST_CASES = [
    {"name": "invalid_v_prefix", "version": "v1.2", "expected_valid": False},
    {"name": "invalid_missing_patch", "version": "1.2", "expected_valid": False},
    {"name": "valid_semantic", "version": "1.2.3", "expected_valid": True},
    {"name": "valid_with_prerelease", "version": "1.2.3-alpha", "expected_valid": True},
    {"name": "valid_with_build", "version": "1.2.3+build.123", "expected_valid": True},
]


# ============================================================
# 测试类
# ============================================================

class TestAgentContractValidation:
    """Agent 契约验证测试（优化版 - 使用参数化测试）"""

    @pytest.fixture
    def valid_contract(self) -> Dict[str, Any]:
        """提供有效的 Agent 契约示例"""
        return ContractTestHelper.create_valid_contract()

    @pytest.fixture
    def validator(self) -> AgentContractValidator:
        """提供契约验证器实例"""
        return AgentContractValidator()

    def test_valid_contract_passes_validation(self, valid_contract, validator):
        """测试有效契约通过验证"""
        is_valid = validator.validate(valid_contract)

        assert is_valid is True
        assert len(validator.errors) == 0

    @pytest.mark.parametrize("test_case", REQUIRED_FIELD_TEST_CASES, ids=lambda tc: tc["name"])
    def test_missing_required_field_fails(self, valid_contract, validator, test_case):
        """测试缺失必需字段导致验证失败（参数化）"""
        invalid_contract = ContractTestHelper.create_invalid_contract(
            missing_field=test_case["name"]
        )

        is_valid = validator.validate(invalid_contract)

        assert is_valid is False
        assert_error_contains(validator.errors, test_case["expected_error"])

    @pytest.mark.parametrize("test_case", FIELD_TYPE_TEST_CASES, ids=lambda tc: tc["name"])
    def test_invalid_field_type_fails(self, valid_contract, validator, test_case):
        """测试字段类型错误导致验证失败（参数化）"""
        valid_contract[test_case["field"]] = test_case["invalid_value"]

        is_valid = validator.validate(valid_contract)

        assert is_valid is False
        assert_error_contains(validator.errors, test_case["field"])

    @pytest.mark.parametrize("test_case", VERSION_FORMAT_TEST_CASES, ids=lambda tc: tc["name"])
    def test_version_format_validation(self, validator, test_case):
        """测试版本格式验证（参数化）"""
        is_valid = validator._is_valid_semantic_version(test_case["version"])

        assert is_valid == test_case["expected_valid"]


class TestCapabilityValidation:
    """能力验证测试（优化版）"""

    @pytest.fixture
    def validator(self) -> AgentContractValidator:
        """提供验证器实例"""
        return AgentContractValidator()

    def test_empty_capabilities_fails(self, validator):
        """测试空能力列表导致验证失败"""
        validator._validate_capabilities([])

        assert any("不能为空" in e for e in validator.errors)

    @pytest.mark.parametrize("missing_field", ["input_schema", "output_schema"])
    def test_capability_missing_required_field(self, validator, missing_field):
        """测试能力缺失必需字段（参数化）"""
        capabilities = [
            {
                "name": "test_capability",
                "description": "测试能力",
                "input_schema": {"type": "object"},
                "output_schema": {"type": "object"}
            }
        ]
        del capabilities[0][missing_field]

        validator._validate_capabilities(capabilities)

        assert any(missing_field in e for e in validator.errors)

    @pytest.mark.parametrize("success_rate,expected_valid", [
        (-0.1, False),
        (0.0, True),
        (0.5, True),
        (1.0, True),
        (1.5, False),
    ])
    def test_success_rate_validation(self, validator, success_rate, expected_valid):
        """测试成功率验证（参数化）"""
        capabilities = [
            {
                "name": "test_cap",
                "description": "测试",
                "input_schema": {"type": "object"},
                "output_schema": {"type": "object"},
                "success_rate": success_rate
            }
        ]

        validator._validate_capabilities(capabilities)

        has_error = any("success_rate" in e for e in validator.errors)
        assert has_error == (not expected_valid)


class TestModelValidation:
    """模型配置验证测试"""

    @pytest.fixture
    def validator(self) -> AgentContractValidator:
        """提供验证器实例"""
        return AgentContractValidator()

    @pytest.mark.parametrize("missing_field", ["system1", "system2"])
    def test_missing_model_field(self, validator, missing_field):
        """测试模型配置缺失必需字段（参数化）"""
        models = {
            "system1": "gpt-3.5-turbo",
            "system2": "gpt-4"
        }
        del models[missing_field]

        validator._validate_models(models)

        assert any(missing_field in e for e in validator.errors)


class TestCostProfileValidation:
    """成本概览验证测试"""

    @pytest.fixture
    def validator(self) -> AgentContractValidator:
        """提供验证器实例"""
        return AgentContractValidator()

    @pytest.mark.parametrize("maintenance_level,expected_valid", [
        ("community", True),
        ("verified", True),
        ("official", True),
        ("invalid", False),
    ])
    def test_maintenance_level_validation(self, validator, maintenance_level, expected_valid):
        """测试维护级别验证（参数化）"""
        cost_profile = {
            "token_per_task_avg": 1000,
            "api_cost_per_task": 0.01,
            "maintenance_level": maintenance_level
        }

        validator._validate_cost_profile(cost_profile)

        has_error = any("维护级别" in e for e in validator.errors)
        assert has_error == (not expected_valid)


class TestTrustMetricsValidation:
    """信任指标验证测试"""

    @pytest.fixture
    def validator(self) -> AgentContractValidator:
        """提供验证器实例"""
        return AgentContractValidator()

    @pytest.mark.parametrize("rating,expected_valid", [
        (0.5, False),
        (1.0, True),
        (3.5, True),
        (5.0, True),
        (5.5, False),
    ])
    def test_rating_validation(self, validator, rating, expected_valid):
        """测试评分验证（参数化）"""
        trust_metrics = {
            "install_count": 100,
            "rating": rating,
            "verified_provider": True,
            "last_audit": "2026-03-01"
        }

        validator._validate_trust_metrics(trust_metrics)

        has_error = any("rating" in e for e in validator.errors)
        assert has_error == (not expected_valid)

    @pytest.mark.parametrize("audit_date,expected_valid", [
        ("2026-03-01", True),
        ("2026-1-1", False),
        ("2026/03/01", False),
        ("03-01-2026", False),
    ])
    def test_audit_date_format_validation(self, validator, audit_date, expected_valid):
        """测试审计日期格式验证（参数化）"""
        trust_metrics = {
            "install_count": 100,
            "rating": 4.5,
            "verified_provider": True,
            "last_audit": audit_date
        }

        validator._validate_trust_metrics(trust_metrics)

        has_error = any("last_audit" in e for e in validator.errors)
        assert has_error == (not expected_valid)
