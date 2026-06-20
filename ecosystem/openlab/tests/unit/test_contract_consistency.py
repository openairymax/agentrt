# Copyright (c) 2026 SPHARX. All Rights Reserved.
"""
Integration tests for contract consistency: OpenLab Markets ↔ market_d.

Tests cover:
  - T1: AgentInfo dataclass ↔ C struct agent_info_t field alignment
  - T2: SkillInfo dataclass ↔ C struct skill_info_t field alignment
  - T3: InstallRequest/InstallResult ↔ C struct alignment
  - T4: Error code mapping completeness
  - T5: RPC method name consistency
  - T6: JSON serialization round-trip for all models
  - T7: Contract audit report runs without errors
"""

from __future__ import annotations

import json
import pytest

from markets.client.errors import (
    MARKET_ERR_INVALID_PARAM,
    MARKET_ERR_NOT_FOUND,
    MARKET_ERR_ALREADY_EXISTS,
    MARKET_ERR_INSTALL_FAIL,
    MARKET_ERR_OUT_OF_MEMORY,
    market_error_from_code,
    MarketError,
    MarketConnectionError,
    MarketNotFoundError,
    MarketAlreadyExistsError,
    MarketInstallError,
    MarketValidationError,
)
from markets.client.models import (
    AgentInfo,
    AgentType,
    AgentStatus,
    SkillInfo,
    SkillType,
    InstallRequest,
    InstallResult,
    SearchParams,
)
from markets.client.contract_audit import ContractAuditor


# ──────────────────────────────────────────────────────────────────────
# T1: AgentInfo ↔ C struct agent_info_t field alignment
# ──────────────────────────────────────────────────────────────────────

class TestAgentInfoContract:
    """Verify AgentInfo dataclass matches agent_info_t C struct."""

    C_STRUCT_FIELDS = {
        "agent_id", "name", "version", "description",
        "type", "status", "author", "repository",
        "dependencies", "rating", "download_count", "last_updated",
    }

    def test_all_c_fields_present(self):
        """Every C struct field must have a corresponding Python field."""
        py_fields = {f.name for f in AgentInfo.__dataclass_fields__.values()}
        missing = self.C_STRUCT_FIELDS - py_fields
        assert not missing, f"Missing fields in AgentInfo: {missing}"

    def test_agent_id_required_for_serialization(self):
        """agent_id should be present in to_json output."""
        agent = AgentInfo(agent_id="test-agent", name="Test")
        data = agent.to_json()
        assert data["agent_id"] == "test-agent"
        assert data["name"] == "Test"

    def test_from_json_preserves_all_fields(self):
        """Round-trip: from_json(to_json(x)) should be equivalent."""
        original = AgentInfo(
            agent_id="test-agent",
            name="Test Agent",
            version="1.2.3",
            description="A test agent",
            type=AgentType.EXPERT,
            status=AgentStatus.AVAILABLE,
            author="Test Author",
            repository="https://github.com/test/agent",
            dependencies="dep1,dep2",
            rating=4.5,
            download_count=100,
            last_updated=1700000000,
        )
        serialized = original.to_json()
        restored = AgentInfo.from_json(serialized)
        assert restored.agent_id == original.agent_id
        assert restored.name == original.name
        assert restored.version == original.version
        assert restored.description == original.description
        assert restored.type == original.type
        assert restored.author == original.author
        assert restored.rating == original.rating
        assert restored.download_count == original.download_count
        assert restored.last_updated == original.last_updated

    def test_to_json_uses_snake_case_keys(self):
        """JSON keys must match C struct field names (snake_case)."""
        agent = AgentInfo(agent_id="test", name="T", version="1.0")
        data = agent.to_json()
        # All keys should be snake_case to match C side
        for key in data:
            assert "_" in key or key.islower(), f"Key '{key}' should be snake_case"

    def test_enum_types_serialize_as_int(self):
        """C struct uses int for enums, so JSON should serialize as int."""
        agent = AgentInfo(
            agent_id="test",
            type=AgentType.SPECIALIZED,
            status=AgentStatus.INSTALLING,
        )
        data = agent.to_json()
        assert isinstance(data["type"], int)
        assert data["type"] == AgentType.SPECIALIZED.value
        assert isinstance(data["status"], int)
        assert data["status"] == AgentStatus.INSTALLING.value


# ──────────────────────────────────────────────────────────────────────
# T2: SkillInfo ↔ C struct skill_info_t field alignment
# ──────────────────────────────────────────────────────────────────────

class TestSkillInfoContract:
    """Verify SkillInfo dataclass matches skill_info_t C struct."""

    C_STRUCT_FIELDS = {
        "skill_id", "name", "version", "description",
        "type", "author", "repository", "dependencies",
        "rating", "download_count", "last_updated",
    }

    def test_all_c_fields_present(self):
        """Every C struct field must have a corresponding Python field."""
        py_fields = {f.name for f in SkillInfo.__dataclass_fields__.values()}
        missing = self.C_STRUCT_FIELDS - py_fields
        assert not missing, f"Missing fields in SkillInfo: {missing}"

    def test_skill_to_json_from_json_roundtrip(self):
        """Round-trip serialization for SkillInfo."""
        original = SkillInfo(
            skill_id="file-reader",
            name="File Reader",
            version="1.0.0",
            description="Reads files from disk",
            type=SkillType.TOOL,
            author="Test Author",
            repository="https://github.com/test/skill",
            dependencies="",
            rating=4.0,
            download_count=50,
            last_updated=1700000000,
        )
        data = original.to_json()
        restored = SkillInfo.from_json(data)
        assert restored.skill_id == original.skill_id
        assert restored.name == original.name
        assert restored.version == original.version
        assert restored.type == original.type


# ──────────────────────────────────────────────────────────────────────
# T3: InstallRequest/InstallResult ↔ C struct alignment
# ──────────────────────────────────────────────────────────────────────

class TestInstallContracts:
    """Verify InstallRequest/InstallResult match C structs."""

    def test_install_request_to_json(self):
        """InstallRequest should serialize with agent_id field (matching C)."""
        req = InstallRequest(
            id="test-agent",
            version="2.0.0",
            force_update=True,
            install_path="/tmp/agents",
        )
        data = req.to_json()
        # C handler expects "agent_id" key
        assert data["agent_id"] == "test-agent"
        assert data["version"] == "2.0.0"
        assert data["force_update"] is True
        assert data["install_path"] == "/tmp/agents"

    def test_install_request_defaults(self):
        """Default values should match C struct defaults."""
        req = InstallRequest(id="test")
        data = req.to_json()
        assert data["version"] == "latest"
        assert data["force_update"] is False

    def test_install_result_from_json(self):
        """Parsing market_d response should produce InstallResult."""
        response = {
            "status": "installed",
            "agent_id": "test-agent",
            "installed_version": "1.0.0",
            "install_path": "/tmp/agents/test-agent",
            "message": "OK",
            "error_code": 0,
        }
        result = InstallResult.from_json(response)
        assert result.success is True
        assert result.installed_version == "1.0.0"
        assert result.install_path == "/tmp/agents/test-agent"

    def test_install_result_failure_from_json(self):
        """Failed installation should produce success=False."""
        response = {
            "status": "error",
            "message": "Not found",
            "error_code": -1002,
        }
        result = InstallResult.from_json(response)
        assert result.success is False
        assert result.message == "Not found"


# ──────────────────────────────────────────────────────────────────────
# T4: Error code mapping completeness
# ──────────────────────────────────────────────────────────────────────

class TestErrorCodeMapping:
    """Verify error code → exception mapping is complete and correct."""

    def test_all_error_codes_defined(self):
        """All expected error codes must be defined in errors.py."""
        assert MARKET_ERR_INVALID_PARAM == -1000
        assert MARKET_ERR_OUT_OF_MEMORY == -1001
        assert MARKET_ERR_NOT_FOUND == -1002
        assert MARKET_ERR_ALREADY_EXISTS == -1003
        assert MARKET_ERR_INSTALL_FAIL == -1004

    def test_error_code_to_exception_mapping(self):
        """Each error code should map to the correct exception type."""
        test_cases = [
            (MARKET_ERR_INVALID_PARAM, MarketValidationError),
            (MARKET_ERR_NOT_FOUND, MarketNotFoundError),
            (MARKET_ERR_ALREADY_EXISTS, MarketAlreadyExistsError),
            (MARKET_ERR_INSTALL_FAIL, MarketInstallError),
            (MARKET_ERR_OUT_OF_MEMORY, MarketError),
            (-32700, MarketValidationError),  # JSONRPC_PARSE_ERROR
            (-32600, MarketValidationError),  # JSONRPC_INVALID_REQUEST
            (-32601, MarketError),            # JSONRPC_METHOD_NOT_FOUND
            (-32602, MarketValidationError),  # JSONRPC_INVALID_PARAMS
            (-32603, MarketError),            # JSONRPC_INTERNAL_ERROR
        ]
        for code, expected_cls in test_cases:
            exc = market_error_from_code(code, "test message")
            assert isinstance(exc, expected_cls), \
                f"Code {code} should map to {expected_cls.__name__}, got {type(exc).__name__}"

    def test_unknown_error_code_returns_base_error(self):
        """Unknown error codes should return MarketError."""
        exc = market_error_from_code(99999, "unknown")
        assert isinstance(exc, MarketError)
        assert not isinstance(exc, (MarketNotFoundError, MarketValidationError))

    def test_error_preserves_details(self):
        """Error details should be preserved in the exception."""
        exc = market_error_from_code(-1002, "Not found", "details here")
        assert exc.code == -1002
        assert exc.message == "Not found"
        assert exc.details == "details here"


# ──────────────────────────────────────────────────────────────────────
# T5: RPC method name consistency
# ──────────────────────────────────────────────────────────────────────

class TestRPCMethodConsistency:
    """Verify RPC method names are consistent between client and daemon."""

    EXPECTED_METHODS = {
        "register_agent",
        "search_agents",
        "install_agent",
        "register_skill",
        "search_skills",
        "health_check",
    }

    def test_client_has_all_rpc_methods(self):
        """MarketClient must expose all RPC methods defined in main.c."""
        from markets.client.market_client import MarketClient
        client_methods = {
            name for name in dir(MarketClient)
            if not name.startswith("_") and callable(getattr(MarketClient, name, None))
        }
        for method in self.EXPECTED_METHODS:
            assert method in client_methods, \
                f"MarketClient missing method '{method}'"


# ──────────────────────────────────────────────────────────────────────
# T6: JSON serialization round-trip
# ──────────────────────────────────────────────────────────────────────

class TestJSONRoundTrip:
    """Verify all models can be serialized to JSON and back."""

    def test_search_params_round_trip(self):
        """SearchParams should serialize to JSON-compatible dict."""
        sp = SearchParams(
            query="code review",
            limit=10,
            offset=0,
            sort_by_rating=True,
        )
        data = sp.to_json()
        assert data["keyword"] == "code review"
        assert data["limit"] == 10
        assert data["offset"] == 0
        assert data["sort_by_rating"] is True
        # Verify JSON serialization
        json_str = json.dumps(data)
        parsed = json.loads(json_str)
        assert parsed["keyword"] == "code review"

    def test_search_params_default_not_included(self):
        """Default/None values should not be serialized."""
        sp = SearchParams(query="test")
        data = sp.to_json()
        # optional fields with None should not appear
        assert "agent_type" not in data
        assert "skill_type" not in data
        assert "only_installed" not in data


# ──────────────────────────────────────────────────────────────────────
# T7: Contract audit tool
# ──────────────────────────────────────────────────────────────────────

class TestContractAuditTool:
    """Verify contract audit tool runs correctly."""

    def test_audit_models_passes(self):
        """Models audit should pass (no errors)."""
        auditor = ContractAuditor()
        report = auditor.audit_models()
        assert not report.has_errors(), \
            f"Model audit found errors:\n{report.format()}"

    def test_audit_error_codes_passes(self):
        """Error code audit should pass."""
        auditor = ContractAuditor()
        report = auditor.audit_error_codes()
        assert not report.has_errors(), \
            f"Error code audit found errors:\n{report.format()}"

    def test_audit_rpc_methods_passes(self):
        """RPC method audit should pass."""
        auditor = ContractAuditor()
        report = auditor.audit_rpc_methods()
        assert not report.has_errors(), \
            f"RPC method audit found errors:\n{report.format()}"

    def test_full_audit_no_errors(self):
        """Full audit should have no errors."""
        auditor = ContractAuditor()
        report = auditor.run_full_audit()
        assert not report.has_errors(), \
            f"Full audit found errors:\n{report.format()}"

    def test_audit_report_formatting(self):
        """Audit report should produce readable output."""
        auditor = ContractAuditor()
        report = auditor.run_full_audit()
        formatted = report.format()
        assert "Contract Consistency Audit Report" in formatted
        assert "RESULT:" in formatted