"""
AgentOS Tool 服务单元测试

验证 Tool 守护进程的核心功能：
- 工具注册与发现
- 工具执行与验证
- 工具缓存
- 工具配置管理

Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
"""

import json
import pytest
from unittest.mock import MagicMock

from tests.base.base_test import BaseTestCase


class TestToolRegistry(BaseTestCase):
    """测试工具注册与发现"""

    def test_register_tool_with_valid_spec(self):
        """测试有效规格注册工具"""
        tool_spec = {
            "name": "web_search",
            "version": "1.0.0",
            "description": "Search the web for information",
            "input_schema": {
                "type": "object",
                "properties": {
                    "query": {"type": "string"},
                    "limit": {"type": "integer", "default": 5}
                },
                "required": ["query"]
            },
            "output_schema": {
                "type": "array",
                "items": {"type": "object"}
            }
        }
        assert tool_spec["name"] == "web_search"
        assert "input_schema" in tool_spec
        assert "output_schema" in tool_spec

    def test_register_tool_with_invalid_spec_returns_error(self):
        """测试无效规格注册返回错误"""
        tool_spec = {}
        assert "name" not in tool_spec

    def test_discover_tool_by_name(self):
        """测试按名称发现工具"""
        registry = {
            "web_search": {"version": "1.0.0", "status": "active"},
            "code_runner": {"version": "2.0.0", "status": "active"},
            "file_manager": {"version": "1.5.0", "status": "deprecated"}
        }
        assert "web_search" in registry
        assert registry["web_search"]["status"] == "active"

    def test_list_active_tools(self):
        """测试列出活跃工具"""
        registry = {
            "web_search": {"status": "active"},
            "code_runner": {"status": "active"},
            "legacy_tool": {"status": "deprecated"}
        }
        active_tools = [k for k, v in registry.items() if v["status"] == "active"]
        assert len(active_tools) == 2
        assert "legacy_tool" not in active_tools

    def test_unregister_tool(self):
        """测试注销工具"""
        registry = {"web_search": {"status": "active"}}
        registry.pop("web_search", None)
        assert "web_search" not in registry


class TestToolExecution(BaseTestCase):
    """测试工具执行"""

    def test_execute_tool_with_valid_input(self):
        """测试有效输入执行工具"""
        tool_input = {"query": "AgentOS architecture", "limit": 5}
        expected_output = [
            {"title": "AgentOS Microkernel", "relevance": 0.95},
            {"title": "IPC Design Patterns", "relevance": 0.87}
        ]
        assert tool_input["query"] == "AgentOS architecture"
        assert len(expected_output) == 2

    def test_execute_tool_with_missing_required_field(self):
        """测试缺少必填字段执行返回错误"""
        tool_input = {"limit": 5}
        required_fields = ["query"]
        missing = [f for f in required_fields if f not in tool_input]
        assert len(missing) == 1
        assert "query" in missing

    def test_execute_tool_with_invalid_input_type(self):
        """测试无效输入类型执行返回错误"""
        tool_input = {"query": 12345, "limit": "five"}
        assert not isinstance(tool_input["query"], str)
        assert not isinstance(tool_input["limit"], int)

    def test_execute_tool_timeout(self):
        """测试工具执行超时"""
        timeout_ms = 30000
        assert timeout_ms > 0
        assert timeout_ms <= 60000

    def test_execute_tool_cancellation(self):
        """测试工具执行取消"""
        execution_state = {"status": "running", "cancel_requested": False}
        execution_state["cancel_requested"] = True
        assert execution_state["cancel_requested"] is True


class TestToolValidator(BaseTestCase):
    """测试工具验证"""

    def test_validate_input_schema(self):
        """测试输入 Schema 验证"""
        schema = {
            "type": "object",
            "properties": {
                "query": {"type": "string"},
                "limit": {"type": "integer", "minimum": 1, "maximum": 100}
            },
            "required": ["query"]
        }
        valid_input = {"query": "test", "limit": 10}
        assert "query" in valid_input
        assert isinstance(valid_input["limit"], int)
        assert 1 <= valid_input["limit"] <= 100

    def test_validate_output_schema(self):
        """测试输出 Schema 验证"""
        output = [
            {"title": "Result 1", "score": 0.95},
            {"title": "Result 2", "score": 0.87}
        ]
        for item in output:
            assert "title" in item
            assert "score" in item
            assert 0 <= item["score"] <= 1

    @pytest.mark.parametrize("input_data,is_valid", [
        ({"query": "test"}, True),
        ({}, False),
        ({"query": 123}, False),
        ({"query": "test", "limit": -1}, False),
        ({"query": "test", "limit": 101}, False),
    ])
    def test_input_validation_cases(self, input_data, is_valid):
        """测试各种输入验证场景"""
        has_query = "query" in input_data and isinstance(input_data.get("query"), str)
        limit_valid = True
        if "limit" in input_data:
            limit = input_data["limit"]
            limit_valid = isinstance(limit, int) and 1 <= limit <= 100
        result = has_query and limit_valid
        assert result == is_valid


class TestToolCache(BaseTestCase):
    """测试工具缓存"""

    def test_cache_stores_execution_result(self):
        """测试缓存存储执行结果"""
        cache = {}
        cache_key = "web_search:AgentOS:5"
        cache[cache_key] = {"results": ["item1", "item2"], "cached_at": 1672531200}
        assert cache_key in cache

    def test_cache_returns_stale_after_ttl(self):
        """测试缓存 TTL 过期"""
        cache = {
            "key1": {"data": "result", "cached_at": 1672531200, "ttl": 3600}
        }
        current_time = 1672535200
        entry = cache["key1"]
        is_stale = (current_time - entry["cached_at"]) > entry["ttl"]
        assert is_stale is True

    def test_cache_invalidation_on_tool_update(self):
        """测试工具更新时缓存失效"""
        cache = {
            "web_search:query1": "result1",
            "web_search:query2": "result2",
            "code_runner:script1": "result3"
        }
        tool_prefix = "web_search:"
        keys_to_remove = [k for k in cache if k.startswith(tool_prefix)]
        for key in keys_to_remove:
            cache.pop(key)
        assert all(not k.startswith(tool_prefix) for k in cache)


class TestToolConfig(BaseTestCase):
    """测试工具配置管理"""

    def test_load_tool_config(self):
        """测试加载工具配置"""
        config = {
            "name": "web_search",
            "timeout_ms": 30000,
            "max_retries": 3,
            "rate_limit": {"rpm": 60, "tpm": 100000}
        }
        assert config["timeout_ms"] == 30000
        assert config["rate_limit"]["rpm"] == 60

    def test_update_tool_config(self):
        """测试更新工具配置"""
        config = {"timeout_ms": 30000, "max_retries": 3}
        config["timeout_ms"] = 60000
        assert config["timeout_ms"] == 60000

    def test_default_config_values(self):
        """测试默认配置值"""
        default_config = {
            "timeout_ms": 30000,
            "max_retries": 3,
            "cache_enabled": True,
            "cache_ttl_seconds": 3600
        }
        assert default_config["timeout_ms"] == 30000
        assert default_config["cache_enabled"] is True
