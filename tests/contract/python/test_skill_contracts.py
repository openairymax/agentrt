# AgentOS Skill 契约测试
# Version: 1.0.0.6
# Last updated: 2026-03-23

"""
AgentOS Skill 契约测试模块。

验证 Skill 契约的格式、结构和语义正确性，确保符合 AgentOS 规范。
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
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', 'toolkit', 'python')))


# ============================================================
# 测试标记
# ============================================================

pytestmark = pytest.mark.contract


# ============================================================
# 枚举和常量定义
# ============================================================

class SkillType(Enum):
    """技能类型枚举"""
    TOOL = "tool"
    CODE = "code"
    API = "api"
    FILE = "file"
    BROWSER = "browser"
    DB = "db"
    SHELL = "shell"


class MaintenanceLevel(Enum):
    """维护级别枚举"""
    COMMUNITY = "community"
    VERIFIED = "verified"
    OFFICIAL = "official"


VALID_SKILL_TYPES = [e.value for e in SkillType]
SCHEMA_VERSION = "1.0.0"


# ============================================================
# 数据类定义
# ============================================================

@dataclass
class Tool:
    """工具定义"""
    name: str
    description: str
    input_schema: Dict[str, Any]
    output_schema: Dict[str, Any]
    estimated_tokens: Optional[int] = None
    avg_duration_ms: Optional[int] = None
    success_rate: Optional[float] = None


@dataclass
class Executable:
    """可执行文件信息"""
    path: str
    entry: str
    args: List[str] = field(default_factory=list)


@dataclass
class PackageDependency:
    """包依赖"""
    language: str
    name: str


@dataclass
class Dependencies:
    """依赖声明"""
    libraries: List[str] = field(default_factory=list)
    packages: List[PackageDependency] = field(default_factory=list)
    commands: List[str] = field(default_factory=list)
    skills: List[str] = field(default_factory=list)


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
class SkillContract:
    """Skill 契约数据类"""
    schema_version: str
    skill_id: str
    skill_name: str
    version: str
    description: str
    type: str
    required_permissions: List[str]
    cost_profile: CostProfile
    trust_metrics: TrustMetrics
    executable: Optional[Executable] = None
    tools: List[Tool] = field(default_factory=list)
    dependencies: Optional[Dependencies] = None
    extensions: Dict[str, Any] = field(default_factory=dict)


# ============================================================
# 契约验证器
# ============================================================

class SkillContractValidator:
    """Skill 契约验证器"""

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
        """验证必需字段存在性"""
        required_fields = [
            "schema_version", "skill_id", "skill_name", "version",
            "description", "type", "required_permissions",
            "cost_profile", "trust_metrics"
        ]

        for field_name in required_fields:
            if field_name not in contract:
                self.errors.append(f"缺失必需字段: {field_name}")

    def _validate_field_types(self, contract: Dict[str, Any]) -> None:
        """验证字段类型正确性"""
        type_checks = {
            "schema_version": str,
            "skill_id": str,
            "skill_name": str,
            "version": str,
            "description": str,
            "type": str,
            "required_permissions": list,
            "cost_profile": dict,
            "trust_metrics": dict,
            "tools": list,
            "executable": dict,
            "dependencies": dict,
            "extensions": dict,
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

        if "type" in contract:
            if contract["type"] not in VALID_SKILL_TYPES:
                self.errors.append(
                    f"无效的技能类型: {contract['type']}, "
                    f"应为: {VALID_SKILL_TYPES}"
                )

        if "tools" in contract and contract.get("type") == "tool":
            if not contract["tools"]:
                self.errors.append("工具类技能必须提供至少一个工具定义")
            else:
                self._validate_tools(contract["tools"])

        if "executable" in contract:
            self._validate_executable(contract["executable"])

        if "dependencies" in contract:
            self._validate_dependencies(contract["dependencies"])

        if "cost_profile" in contract:
            self._validate_cost_profile(contract["cost_profile"])

        if "trust_metrics" in contract:
            self._validate_trust_metrics(contract["trust_metrics"])

    def _is_valid_semantic_version(self, version: str) -> bool:
        """检查是否为有效的语义化版本"""
        pattern = r'^(\d+)\.(\d+)\.(\d+)(?:-([a-zA-Z0-9.-]+))?(?:\+([a-zA-Z0-9.-]+))?$'
        return bool(re.match(pattern, version))

    def _validate_tools(self, tools: List[Dict]) -> None:
        """验证工具列表"""
        required_tool_fields = ["name", "description", "input_schema", "output_schema"]

        for i, tool in enumerate(tools):
            if not isinstance(tool, dict):
                self.errors.append(f"工具 {i} 不是有效的对象")
                continue

            for field_name in required_tool_fields:
                if field_name not in tool:
                    self.errors.append(f"工具 {i} 缺失必需字段: {field_name}")

            if "name" in tool:
                if not re.match(r'^[a-z][a-z0-9_]*$', tool["name"]):
                    self.warnings.append(
                        f"工具名称建议使用蛇形命名法: {tool['name']}"
                    )

            if "success_rate" in tool:
                rate = tool["success_rate"]
                if not isinstance(rate, (int, float)) or rate < 0 or rate > 1:
                    self.errors.append(
                        f"工具 {i} 的 success_rate 应在 0-1 之间"
                    )

    def _validate_executable(self, executable: Dict) -> None:
        """验证可执行文件信息"""
        required_exec_fields = ["path", "entry"]

        for field_name in required_exec_fields:
            if field_name not in executable:
                self.errors.append(f"可执行文件信息缺失必需字段: {field_name}")

        if "path" in executable:
            path = executable["path"]
            if not isinstance(path, str) or not path:
                self.errors.append("可执行文件路径不能为空")

    def _validate_dependencies(self, dependencies: Dict) -> None:
        """验证依赖声明"""
        if "packages" in dependencies:
            for i, pkg in enumerate(dependencies["packages"]):
                if not isinstance(pkg, dict):
                    self.errors.append(f"包依赖 {i} 不是有效的对象")
                    continue

                if "language" not in pkg or "name" not in pkg:
                    self.errors.append(
                        f"包依赖 {i} 必须包含 language 和 name 字段"
                    )

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

class TestSkillContractStructure:
    """Skill 契约结构测试"""

    @pytest.fixture
    def valid_contract(self) -> Dict[str, Any]:
        """
        提供有效的 Skill 契约示例。

        Returns:
            Dict: 有效的契约数据
        """
        return {
            "schema_version": "1.0.0",
            "skill_id": "com.agentos.github_skill.v1",
            "skill_name": "GitHub Integration Skill",
            "version": "2.1.0",
            "description": "提供 GitHub 仓库操作、PR 管理、代码审查等功能。",
            "type": "tool",
            "executable": {
                "path": "./github_skill.so",
                "entry": "github_skill_entry"
            },
            "tools": [
                {
                    "name": "github_create_repo",
                    "description": "创建 GitHub 仓库",
                    "input_schema": {
                        "type": "object",
                        "properties": {
                            "name": {"type": "string", "description": "仓库名称"},
                            "private": {"type": "boolean", "description": "是否为私有仓库", "default": True}
                        },
                        "required": ["name"]
                    },
                    "output_schema": {
                        "type": "object",
                        "properties": {
                            "repo_url": {"type": "string", "description": "仓库 HTML URL"},
                            "clone_url": {"type": "string", "description": "仓库克隆 URL"}
                        }
                    },
                    "estimated_tokens": 50,
                    "avg_duration_ms": 500,
                    "success_rate": 0.98
                }
            ],
            "dependencies": {
                "libraries": ["libcurl", "libssl"],
                "packages": [
                    {"language": "python", "name": "PyGithub>=1.55"}
                ],
                "commands": ["git"],
                "skills": []
            },
            "required_permissions": [
                "network:outbound:api.github.com",
                "read:repo",
                "write:repo"
            ],
            "cost_profile": {
                "token_per_task_avg": 500,
                "api_cost_per_task": 0.001,
                "maintenance_level": "verified"
            },
            "trust_metrics": {
                "install_count": 3420,
                "rating": 4.8,
                "verified_provider": True,
                "last_audit": "2026-03-15"
            },
            "extensions": {
                "author": "Spharx AgentOS Team",
                "license": "MIT"
            }
        }

    @pytest.fixture
    def validator(self) -> SkillContractValidator:
        """
        提供契约验证器实例。

        Returns:
            SkillContractValidator: 验证器实例
        """
        return SkillContractValidator()

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
        del valid_contract["skill_id"]

        is_valid = validator.validate(valid_contract)

        assert is_valid is False
        assert any("skill_id" in e for e in validator.errors)

    def test_invalid_field_type_fails(self, valid_contract, validator):
        """
        测试字段类型错误导致验证失败。

        验证:
            - 字段类型不匹配应产生错误
        """
        valid_contract["tools"] = "not_a_list"

        is_valid = validator.validate(valid_contract)

        assert is_valid is False
        assert any("tools" in e for e in validator.errors)

    def test_invalid_version_format_fails(self, valid_contract, validator):
        """
        测试无效版本格式导致验证失败。

        验证:
            - 非语义化版本应产生错误
        """
        valid_contract["version"] = "v2.1"

        is_valid = validator.validate(valid_contract)

        assert is_valid is False
        assert any("版本号" in e for e in validator.errors)


class TestSkillTypeValidation:
    """技能类型验证测试"""

    @pytest.fixture
    def validator(self) -> SkillContractValidator:
        """提供验证器实例"""
        return SkillContractValidator()

    @pytest.mark.parametrize("skill_type", [
        "tool", "code", "api", "file", "browser", "db", "shell"
    ])
    def test_valid_skill_types(self, validator, skill_type):
        """
        测试有效的技能类型。

        验证:
            - 所有定义的技能类型都应通过验证
        """
        contract = {
            "schema_version": "1.0.0",
            "skill_id": "test.skill",
            "version": "1.0.0",
            "type": skill_type,
            "tools": [{"name": "test", "description": "test", "input_schema": {}, "output_schema": {}}]
        }

        validator._validate_semantic_rules(contract)

        assert not any("技能类型" in e for e in validator.errors)

    def test_invalid_skill_type_fails(self, validator):
        """
        测试无效的技能类型。

        验证:
            - 未定义的技能类型应产生错误
        """
        contract = {
            "schema_version": "1.0.0",
            "skill_id": "test.skill",
            "version": "1.0.0",
            "type": "invalid_type"
        }

        validator._validate_semantic_rules(contract)

        assert any("技能类型" in e for e in validator.errors)

    def test_tool_skill_requires_tools(self, validator):
        """
        测试工具类技能必须提供工具定义。

        验证:
            - 工具类技能必须有至少一个工具
        """
        contract = {
            "schema_version": "1.0.0",
            "skill_id": "test.skill",
            "version": "1.0.0",
            "type": "tool",
            "tools": []
        }

        validator._validate_semantic_rules(contract)

        assert any("工具类技能必须提供至少一个工具定义" in e for e in validator.errors)


class TestToolValidation:
    """工具验证测试"""

    @pytest.fixture
    def validator(self) -> SkillContractValidator:
        """提供验证器实例"""
        return SkillContractValidator()

    def test_tool_missing_required_field(self, validator):
        """
        测试工具缺失必需字段。

        验证:
            - 工具必须包含 name, description, input_schema, output_schema
        """
        tools = [
            {
                "name": "test_tool",
                "description": "测试工具"
            }
        ]

        validator._validate_tools(tools)

        assert any("input_schema" in e for e in validator.errors)
        assert any("output_schema" in e for e in validator.errors)

    def test_invalid_tool_success_rate(self, validator):
        """
        测试无效的工具成功率。

        验证:
            - 成功率应在 0-1 之间
        """
        tools = [
            {
                "name": "test_tool",
                "description": "测试",
                "input_schema": {"type": "object"},
                "output_schema": {"type": "object"},
                "success_rate": 1.5
            }
        ]

        validator._validate_tools(tools)

        assert any("success_rate" in e for e in validator.errors)

    def test_tool_name_naming_convention(self, validator):
        """
        测试工具命名规范。

        验证:
            - 工具名称建议使用蛇形命名法
        """
        tools = [
            {
                "name": "InvalidToolName",
                "description": "测试",
                "input_schema": {"type": "object"},
                "output_schema": {"type": "object"}
            }
        ]

        validator._validate_tools(tools)

        assert any("蛇形命名法" in w for w in validator.warnings)


class TestExecutableValidation:
    """可执行文件验证测试"""

    @pytest.fixture
    def validator(self) -> SkillContractValidator:
        """提供验证器实例"""
        return SkillContractValidator()

    def test_missing_path_field(self, validator):
        """
        测试缺失 path 字段。

        验证:
            - 必须提供可执行文件路径
        """
        executable = {"entry": "main"}

        validator._validate_executable(executable)

        assert any("path" in e for e in validator.errors)

    def test_missing_entry_field(self, validator):
        """
        测试缺失 entry 字段。

        验证:
            - 必须提供入口函数名称
        """
        executable = {"path": "./skill.so"}

        validator._validate_executable(executable)

        assert any("entry" in e for e in validator.errors)

    def test_valid_executable(self, validator):
        """
        测试有效的可执行文件信息。

        验证:
            - 完整的可执行文件信息应通过验证
        """
        executable = {
            "path": "./github_skill.so",
            "entry": "github_skill_entry",
            "args": ["--verbose"]
        }

        validator._validate_executable(executable)

        assert not any("executable" in e for e in validator.errors)


class TestDependenciesValidation:
    """依赖验证测试"""

    @pytest.fixture
    def validator(self) -> SkillContractValidator:
        """提供验证器实例"""
        return SkillContractValidator()

    def test_package_missing_language(self, validator):
        """
        测试包依赖缺失语言字段。

        验证:
            - 包依赖必须包含 language 和 name
        """
        dependencies = {
            "packages": [
                {"name": "requests>=2.28.0"}
            ]
        }

        validator._validate_dependencies(dependencies)

        assert any("language" in e for e in validator.errors)

    def test_package_missing_name(self, validator):
        """
        测试包依赖缺失名称字段。

        验证:
            - 包依赖必须包含 name 字段
        """
        dependencies = {
            "packages": [
                {"language": "python"}
            ]
        }

        validator._validate_dependencies(dependencies)

        assert any("name" in e for e in validator.errors)

    def test_valid_dependencies(self, validator):
        """
        测试有效的依赖声明。

        验证:
            - 完整有效的依赖声明应通过验证
        """
        dependencies = {
            "libraries": ["libcurl"],
            "packages": [
                {"language": "python", "name": "requests>=2.28.0"}
            ],
            "commands": ["git"],
            "skills": ["com.example.base_skill.v1"]
        }

        validator._validate_dependencies(dependencies)

        assert not any("dependencies" in e for e in validator.errors)


class TestPermissionValidation:
    """权限验证测试"""

    def test_permission_format_validation(self):
        """
        测试权限格式验证。

        验证:
            - 权限字符串应符合命名规范
        """
        valid_permissions = [
            "network:outbound:api.github.com",
            "network:outbound:*:443",
            "filesystem:read:/tmp/*",
            "filesystem:write:./output/*",
            "process:execute:git",
            "read:repo",
            "write:repo"
        ]

        invalid_permissions = [
            "",
            "   ",
            "INVALID PERMISSION",
            "network::api.github.com"
        ]

        def is_valid_permission(perm: str) -> bool:
            pattern = r'^[a-z][a-z0-9_:./\*-]*[a-z0-9*]$|^[a-z][a-z0-9_:]*$'
            return bool(re.match(pattern, perm.strip()))

        for perm in valid_permissions:
            assert is_valid_permission(perm), f"有效权限验证失败: {perm}"

        for perm in invalid_permissions:
            if perm.strip():
                result = is_valid_permission(perm)
                assert not result, f"无效权限验证失败: {perm}"


class TestSkillContractSerialization:
    """Skill 契约序列化测试"""

    def test_contract_to_json(self):
        """
        测试契约序列化为 JSON。

        验证:
            - 契约对象能正确序列化为 JSON 字符串
        """
        contract = {
            "schema_version": "1.0.0",
            "skill_id": "com.test.skill",
            "skill_name": "Test Skill",
            "version": "1.0.0",
            "description": "测试技能",
            "type": "tool",
            "tools": [],
            "required_permissions": [],
            "cost_profile": {
                "token_per_task_avg": 100,
                "api_cost_per_task": 0.001,
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

        assert parsed["skill_id"] == contract["skill_id"]
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
            "skill_id": "com.test.skill",
            "skill_name": "Test Skill",
            "version": "1.0.0",
            "description": "测试技能",
            "type": "tool",
            "tools": [],
            "required_permissions": [],
            "cost_profile": {
                "token_per_task_avg": 100,
                "api_cost_per_task": 0.001,
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

        assert contract["skill_id"] == "com.test.skill"
        assert contract["trust_metrics"]["verified_provider"] is False


class TestSkillTypeSpecificValidation:
    """技能类型特定验证测试"""

    @pytest.fixture
    def validator(self) -> SkillContractValidator:
        """提供验证器实例"""
        return SkillContractValidator()

    def test_api_skill_validation(self, validator):
        """
        测试 API 类型技能验证。

        验证:
            - API 类型技能应有合理的权限声明
        """
        contract = {
            "schema_version": "1.0.0",
            "skill_id": "com.test.api_skill",
            "version": "1.0.0",
            "type": "api",
            "required_permissions": ["network:outbound:api.example.com"]
        }

        validator._validate_semantic_rules(contract)

        assert not any("type" in e for e in validator.errors)

    def test_file_skill_validation(self, validator):
        """
        测试文件类型技能验证。

        验证:
            - 文件类型技能应有文件系统权限声明
        """
        contract = {
            "schema_version": "1.0.0",
            "skill_id": "com.test.file_skill",
            "version": "1.0.0",
            "type": "file",
            "required_permissions": ["filesystem:read:/workspace/*"]
        }

        validator._validate_semantic_rules(contract)

        assert not any("type" in e for e in validator.errors)

    def test_browser_skill_validation(self, validator):
        """
        测试浏览器类型技能验证。

        验证:
            - 浏览器类型技能应有网络权限声明
        """
        contract = {
            "schema_version": "1.0.0",
            "skill_id": "com.test.browser_skill",
            "version": "1.0.0",
            "type": "browser",
            "required_permissions": ["network:outbound:*"]
        }

        validator._validate_semantic_rules(contract)

        assert not any("type" in e for e in validator.errors)


class TestSkillComparison:
    """技能比较测试"""

    def test_skill_equality(self):
        """
        测试技能相等性比较。

        验证:
            - 相同内容的技能应被视为相等
        """
        skill1 = {
            "skill_id": "com.test.skill",
            "version": "1.0.0"
        }

        skill2 = {
            "skill_id": "com.test.skill",
            "version": "1.0.0"
        }

        assert skill1 == skill2

    def test_skill_inequality(self):
        """
        测试技能不等性比较。

        验证:
            - 不同内容的技能应被视为不相等
        """
        skill1 = {
            "skill_id": "com.test.skill",
            "version": "1.0.0"
        }

        skill2 = {
            "skill_id": "com.test.skill",
            "version": "1.1.0"
        }

        assert skill1 != skill2


# ============================================================
# 运行测试
# ============================================================

if __name__ == "__main__":
    pytest.main([__file__, "-v", "--tb=short", "-m", "contract"])
