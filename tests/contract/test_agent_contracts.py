# AgentOS Agent 契约测试
# Version: 1.0.0.6
# Last updated: 2026-03-23

"""
AgentOS Agent 契约测试模块。

验证 Agent 契约的格式、结构和语义正确性，确保符合 AgentOS 规范。
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
# 数据类定义
# ============================================================

@dataclass
class Capability:
    """能力定义"""
    name: str
    description: str
    input_schema: Dict[str, Any]
    output_schema: Dict[str, Any]
    estimated_tokens: Optional[int] = None
    avg_duration_ms: Optional[int] = None
    success_rate: Optional[float] = None


@dataclass
class ModelConfig:
    """模型配置"""
    system1: str
    system2: str


@dataclass
class CostProfile:
    """成本概览"""
    token_per_task_avg: int
    api_cost_per_task: float
    maintenance_level: str


@dataclass
class TrustMetrics:
    """信任指标"""
    install_count: int
    rating: float
    verified_provider: bool
    last_audit: str


@dataclass
class AgentContract:
    """Agent 契约数据类"""
    schema_version: str
    agent_id: str
    agent_name: str
    version: str
    role: str
    description: str
    capabilities: List[Capability]
    models: ModelConfig
    required_permissions: List[str]
    cost_profile: CostProfile
    trust_metrics: TrustMetrics
    extensions: Dict[str, Any] = field(default_factory=dict)


# ============================================================
# 契约验证器
# ============================================================

class AgentContractValidator:
    """Agent 契约验证器"""
    
    def __init__(self):
        self.errors: List[str] = []
        self.warnings: List[str] = []
    
    def validate(self, contract: Dict[str, Any]) -> bool:
        """
        验证契约完整性和正确性。
        
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
        """验证必需字段是否存在"""存在性"""

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
        """验证语义规则"""
        if "schema_version" in contract:
            if contract["schema_version"] != SCHEMA_VERSION:
                self.warnings.append(
                    f"Schema 版本不匹配: 当前版本 {SCHEMA_VERSION}, "
                    f"契约版本 {contract['schema_version']}"
                )
        
        if "version" in contract:
            if not self._is_valid_semantic_version(contract["version"]):
                self.errors.append(
                    f"版本号不符合语义化版本规范: {contract['version']}"
                )
        
        if "role" in contract:
            if contract["role"] not in VALID_ROLES:
                self.warnings.append(
                    f"非标准角色类型: {contract['role']}"
                )
        
        if "capabilities" in contract:
            self._validate_capabilities(contract["capabilities"])
        
        if "models" in contract:
            self._validate_models(contract["models"])
        
        if "cost_profile" in contract:
            self._validate_cost_profile(contract["cost_profile"])
        
        if "trust_metrics" in contract:
            self._validate_trust_metrics(contract["trust_metrics"])
    
    def _is_valid_semantic_version(self, version: str) -> bool:
        """检查是否为有效的语义化版本"""
        pattern = r'^(\d+)\.(\d+)\.(\d+)(?:-([a-zA-Z0-9.-]+))?(?:\+([a-zA-Z0-9.-]+))?$'
        return bool(re.match(pattern, version))
    
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


# ============================================================
# 测试用例
# ============================================================

class TestAgentContractStructure:
    """Agent 契约结构测试"""
    
    @pytest.fixture
    def valid_contract(self) -> Dict[str, Any]:
        """
        提供有效的 Agent 契约示例。
        
        Returns:
            Dict: 有效的契约数据
        """
        return {
            "schema_version": "1.0.0",
            "agent_id": "com.agentos.product_manager.v1",
            "agent_name": "Product Manager Agent",
            "version": "1.2.0",
            "role": "product_manager",
            "description": "理解用户需求，撰写产品需求文档 (PRD),拆解用户故事。",
            "capabilities": [
                {
                    "name": "requirement_analysis",
                    "description": "将用户模糊需求转化为结构化需求文档",
                    "input_schema": {
                        "type": "object",
                        "properties": {
                            "goal": {"type": "string", "description": "用户目标描述"},
                            "context": {"type": "object", "description": "业务上下文信息"}
                        },
                        "required": ["goal"]
                    },
                    "output_schema": {
                        "type": "object",
                        "properties": {
                            "prd": {"type": "string", "description": "产品需求文档"},
                            "user_stories": {
                                "type": "array",
                                "items": {"type": "string"},
                                "description": "用户故事列表"
                            }
                        }
                    },
                    "estimated_tokens": 5000,
                    "avg_duration_ms": 3000,
                    "success_rate": 0.92
                }
            ],
            "models": {
                "system1": "gpt-3.5-turbo",
                "system2": "gpt-4"
            },
            "required_permissions": [
                "read_project_context",
                "write_project_artifacts"
            ],
            "cost_profile": {
                "token_per_task_avg": 8000,
                "api_cost_per_task": 0.02,
                "maintenance_level": "verified"
            },
            "trust_metrics": {
                "install_count": 1240,
                "rating": 4.7,
                "verified_provider": True,
                "last_audit": "2026-03-01"
            },
            "extensions": {
                "preferred_llm_provider": "openai"
            }
        }
    
    @pytest.fixture
    def validator(self) -> AgentContractValidator:
        """
        提供契约验证器实例。
        
        Returns:
            AgentContractValidator: 验证器实例
        """
        return AgentContractValidator()
    
    def test_valid_contract_passes_validation(self, valid_contract, validator):
        """
        测试有效契约通过验证。
        
        验证:
            - 完整有效的契约应通过所有验证
        """
        is_valid = validator.validate(valid_contract)
        
        assert is_valid is True
        assert len(validator.errors) == 0
    
    def test_missing_required_field_fails(self, valid_contract, validator):
        """
        测试缺失必需字段导致验证失败。
        
        验证:
            - 缺失必需字段应产生错误
        """
        del valid_contract["agent_id"]
        
        is_valid = validator.validate(valid_contract)
        
        assert is_valid is False
        assert any("agent_id" in e for e in validator.errors)
    
    def test_invalid_field_type_fails(self, valid_contract, validator):
        """
        测试字段类型错误导致验证失败。
        
        验证:
            - 字段类型不匹配应产生错误
        """
        valid_contract["capabilities"] = "not_a_list"
        
        is_valid = validator.validate(valid_contract)
        
        assert is_valid is False
        assert any("capabilities" in e for e in validator.errors)
    
    def test_invalid_version_format_fails(self, valid_contract, validator):
        """
        测试无效版本格式导致验证失败。
        
        验证:
            - 非语义化版本应产生错误
        """
        valid_contract["version"] = "v1.2"
        
        is_valid = validator.validate(valid_contract)
        
        assert is_valid is False
        assert any("版本号" in e for e in validator.errors)


class TestCapabilityValidation:
    """能力验证测试"""
    
    @pytest.fixture
    def validator(self) -> AgentContractValidator:
        """提供验证器实例"""
        return AgentContractValidator()
    
    def test_empty_capabilities_fails(self, validator):
        """
        测试空能力列表导致验证失败。
        
        验证:
            - 能力列表不能为空
        """
        contract = {
            "schema_version": "1.0.0",
            "agent_id": "test.agent",
            "version": "1.0.0",
            "capabilities": []
        }
        
        validator._validate_capabilities(contract["capabilities"])
        
        assert any("不能为空" in e for e in validator.errors)
    
    def test_capability_missing_required_field(self, validator):
        """
        测试能力缺失必需字段。
        
        验证:
            - 能力必须包含 name, description, input_schema, output_schema
        """
        capabilities = [
            {
                "name": "test_capability",
                "description": "测试能力"
            }
        ]
        
        validator._validate_capabilities(capabilities)
        
        assert any("input_schema" in e for e in validator.errors)
        assert any("output_schema" in e for e in validator.errors)
    
    def test_invalid_success_rate(self, validator):
        """
        测试无效的成功率。
        
        验证:
            - 成功率应在 0-1 之间
        """
        capabilities = [
            {
                "name": "test_cap",
                "description": "测试",
                "input_schema": {"type": "object"},
                "output_schema": {"type": "object"},
                "success_rate": 1.5
            }
        ]
        
        validator._validate_capabilities(capabilities)
        
        assert any("success_rate" in e for e in validator.errors)


class TestModelConfigValidation:
    """模型配置验证测试"""
    
    @pytest.fixture
    def validator(self) -> AgentContractValidator:
        """提供验证器实例"""
        return AgentContractValidator()
    
    def test_missing_system1_model(self, validator):
        """
        测试缺失 System1 模型配置。
        
        验证:
            - 必须配置 system1 模型
        """
        models = {"system2": "gpt-4"}
        
        validator._validate_models(models)
        
        assert any("system1" in e for e in validator.errors)
    
    def test_missing_system2_model(self, validator):
        """
        测试缺失 System2 模型配置。
        
        验证:
            - 必须配置 system2 模型
        """
        models = {"system1": "gpt-3.5-turbo"}
        
        validator._validate_models(models)
        
        assert any("system2" in e for e in validator.errors)
    
    def test_valid_model_config(self, validator):
        """
        测试有效的模型配置。
        
        验证:
            - 完整的双模型配置应通过验证
        """
        models = {
            "system1": "gpt-3.5-turbo",
            "system2": "gpt-4"
        }
        
        validator._validate_models(models)
        
        assert not any("models" in e for e in validator.errors)


class TestCostProfileValidation:
    """成本概览验证测试"""
    
    @pytest.fixture
    def validator(self) -> AgentContractValidator:
        """提供验证器实例"""
        return AgentContractValidator()
    
    def test_invalid_maintenance_level(self, validator):
        """
        测试无效的维护级别。
        
        验证:
            - 维护级别必须是 community, verified, official 之一
        """
        cost_profile = {
            "token_per_task_avg": 8000,
            "api_cost_per_task": 0.02,
            "maintenance_level": "premium"
        }
        
        validator._validate_cost_profile(cost_profile)
        
        assert any("maintenance_level" in e for e in validator.errors)
    
    def test_negative_api_cost(self, validator):
        """
        测试负的 API 成本。
        
        验证:
            - API 成本应为非负数
        """
        cost_profile = {
            "token_per_task_avg": 8000,
            "api_cost_per_task": -0.01,
            "maintenance_level": "verified"
        }
        
        validator._validate_cost_profile(cost_profile)
        
        assert any("api_cost_per_task" in e for e in validator.errors)
    
    def test_valid_cost_profile(self, validator):
        """
        测试有效的成本概览。
        
        验证:
            - 完整有效的成本概览应通过验证
        """
        cost_profile = {
            "token_per_task_avg": 8000,
            "api_cost_per_task": 0.02,
            "maintenance_level": "verified"
        }
        
        validator._validate_cost_profile(cost_profile)
        
        assert not any("cost_profile" in e for e in validator.errors)


class TestTrustMetricsValidation:
    """信任指标验证测试"""
    
    @pytest.fixture
    def validator(self) -> AgentContractValidator:
        """提供验证器实例"""
        return AgentContractValidator()
    
    def test_invalid_rating_range(self, validator):
        """
        测试无效的评分范围。
        
        验证:
            - 评分应在 1-5 之间
        """
        trust_metrics = {
            "install_count": 100,
            "rating": 6.0,
            "verified_provider": True,
            "last_audit": "2026-03-01"
        }
        
        validator._validate_trust_metrics(trust_metrics)
        
        assert any("rating" in e for e in validator.errors)
    
    def test_invalid_audit_date_format(self, validator):
        """
        测试无效的审计日期格式。
        
        验证:
            - 审计日期应为 ISO 8601 格式 (YYYY-MM-DD)
        """
        trust_metrics = {
            "install_count": 100,
            "rating": 4.5,
            "verified_provider": True,
            "last_audit": "2026/03/01"
        }
        
        validator._validate_trust_metrics(trust_metrics)
        
        assert any("last_audit" in e for e in validator.errors)
    
    def test_valid_trust_metrics(self, validator):
        """
        测试有效的信任指标。
        
        验证:
            - 完整有效的信任指标应通过验证
        """
        trust_metrics = {
            "install_count": 1240,
            "rating": 4.7,
            "verified_provider": True,
            "last_audit": "2026-03-01"
        }
        
        validator._validate_trust_metrics(trust_metrics)
        
        assert not any("trust_metrics" in e for e in validator.errors)


class TestSemanticVersionValidation:
    """语义化版本验证测试"""
    
    @pytest.fixture
    def validator(self) -> AgentContractValidator:
        """提供验证器实例"""
        return AgentContractValidator()
    
    @pytest.mark.parametrize("version,expected", [
        ("1.0.0", True),
        ("1.2.3", True),
        ("2.0.0-beta.1", True),
        ("1.2.3-rc.2", True),
        ("1.2.3+build.123", True),
        ("v1.2", False),
        ("1.2.beta", False),
        ("latest", False),
        ("1.2", False),
    ])
    def test_semantic_version_validation(self, validator, version, expected):
        """
        测试语义化版本验证。
        
        验证:
            - 正确识别有效和无效的语义化版本
        """
        result = validator._is_valid_semantic_version(version)
        
        assert result == expected, f"版本 {version} 验证结果应为 {expected}"


class TestContractSerialization:
    """契约序列化测试"""
    
    def test_contract_to_json(self):
        """
        测试契约序列化为 JSON。
        
        验证:
            - 契约对象能正确序列化为 JSON 字符串
        """
        contract = {
            "schema_version": "1.0.0",
            "agent_id": "com.test.agent",
            "agent_name": "Test Agent",
            "version": "1.0.0",
            "role": "software_engineer",
            "description": "测试 Agent",
            "capabilities": [],
            "models": {"system1": "gpt-3.5", "system2": "gpt-4"},
            "required_permissions": [],
            "cost_profile": {
                "token_per_task_avg": 1000,
                "api_cost_per_task": 0.01,
                "maintenance_level": "community"
            },
            "trust_metrics": {
                "install_count": 0,
                "rating": 3.0,
                "verified_provider": False,
                "last_audit": "2026-03-01"
            }
        }
        
        json_str = json.dumps(contract)
        parsed = json.loads(json_str)
        
        assert parsed["agent_id"] == contract["agent_id"]
        assert parsed["version"] == contract["version"]
    
    def test_contract_from_json(self):
        """
        测试从 JSON 解析契约。
        
        验证:
            - JSON 字符串能正确解析为契约对象
        """
        json_str = '''
        {
            "schema_version": "1.0.0",
            "agent_id": "com.test.agent",
            "agent_name": "Test Agent",
            "version": "1.0.0",
            "role": "software_engineer",
            "description": "测试 Agent",
            "capabilities": [],
            "models": {"system1": "gpt-3.5", "system2": "gpt-4"},
            "required_permissions": [],
            "cost_profile": {
                "token_per_task_avg": 1000,
                "api_cost_per_task": 0.01,
                "maintenance_level": "community"
            },
            "trust_metrics": {
                "install_count": 0,
                "rating": 3.0,
                "verified_provider": false,
                "last_audit": "2026-03-01"
            }
        }
        '''
        
        contract = json.loads(json_str)
        
        assert contract["agent_id"] == "com.test.agent"
        assert contract["trust_metrics"]["verified_provider"] is False


class TestContractComparison:
    """契约比较测试"""
    
    def test_contract_equality(self):
        """
        测试契约相等性比较。
        
        验证:
            - 相同内容的契约应被视为相等
        """
        contract1 = {
            "agent_id": "com.test.agent",
            "version": "1.0.0"
        }
        
        contract2 = {
            "agent_id": "com.test.agent",
            "version": "1.0.0"
        }
        
        assert contract1 == contract2
    
    def test_contract_inequality(self):
        """
        测试契约不等性比较。
        
        验证:
            - 不同内容的契约应被视为不相等
        """
        contract1 = {
            "agent_id": "com.test.agent",
            "version": "1.0.0"
        }
        
        contract2 = {
            "agent_id": "com.test.agent",
            "version": "1.1.0"
        }
        
        assert contract1 != contract2


class TestPermissionValidation:
    """权限验证测试"""
    
    def test_permission_format_validation(self):
        """
        测试权限格式验证。
        
        验证:
            - 权限字符串应符合命名规范
        """
        valid_permissions = [
            "read_project_context",
            "write_project_artifacts",
            "network:outbound:api.github.com",
            "filesystem:read:/workspace",
            "execute:internal:task"
        ]
        
        invalid_permissions = [
            "",
            "   ",
            "invalid permission with spaces",
            "INVALID_PERMISSION"
        ]
        
        def is_valid_permission(perm: str) -> bool:
            pattern = r'^[a-z][a-z0-9_:]*[a-z0-9]$|^[a-z][a-z0-9_]*$'
            return bool(re.match(pattern, perm))
        
        for perm in valid_permissions:
            assert is_valid_permission(perm), f"有效权限验证失败: {perm}"
        
        for perm in invalid_permissions:
            if perm:
                assert not is_valid_permission(perm), f"无效权限验证失败: {perm}"


# ============================================================
# 运行测试
# ============================================================

if __name__ == "__main__":
    pytest.main([__file__, "-v", "--tb=short", "-m", "contract"])
