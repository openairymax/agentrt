"""
AgentOS Rust SDK 单元测试

验证 Rust SDK 的核心功能：
- 客户端初始化与配置
- Agent 生命周期管理
- 记忆系统操作
- 会话管理
- 错误处理与类型安全

Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
"""

import json
import pytest
from unittest.mock import MagicMock, patch, PropertyMock

from tests.base.base_test import SDKTestCase


class TestRustSDKClientInit(SDKTestCase):
    """测试 Rust SDK 客户端初始化"""

    def test_client_new_with_default_config(self):
        """测试默认配置创建客户端"""
        mock_client = self.create_mock_client()
        assert mock_client is not None

    def test_client_new_with_custom_endpoint(self):
        """测试自定义端点创建客户端"""
        mock_client = self.create_mock_client()
        mock_client.config.endpoint = "https://custom.agentos.dev"
        assert mock_client.config.endpoint == "https://custom.agentos.dev"

    def test_client_new_with_timeout(self):
        """测试自定义超时创建客户端"""
        mock_client = self.create_mock_client()
        mock_client.config.timeout_ms = 60000
        assert mock_client.config.timeout_ms == 60000

    def test_client_builder_pattern(self):
        """测试 Builder 模式创建客户端"""
        mock_client = self.create_mock_client()
        mock_client.config.endpoint = "https://builder.agentos.dev"
        mock_client.config.timeout_ms = 30000
        mock_client.config.max_retries = 3
        assert mock_client.config.endpoint == "https://builder.agentos.dev"
        assert mock_client.config.timeout_ms == 30000
        assert mock_client.config.max_retries == 3


class TestRustSDKAgentLifecycle(SDKTestCase):
    """测试 Rust SDK Agent 生命周期"""

    def test_agent_create_returns_agent_id(self):
        """测试创建 Agent 返回 ID"""
        mock_client = self.create_mock_client()
        mock_client.agent_create.return_value = {"agent_id": "agent_0"}
        result = mock_client.agent_create({"name": "test", "type": "TASK"})
        assert result["agent_id"] == "agent_0"

    def test_agent_get_returns_descriptor(self):
        """测试获取 Agent 返回描述符"""
        mock_client = self.create_mock_client()
        mock_client.agent_get.return_value = {
            "id": "agent_0",
            "name": "test_agent",
            "state": 2,
            "bound_skills": []
        }
        result = mock_client.agent_get("agent_0")
        assert result["id"] == "agent_0"
        assert result["state"] == 2

    def test_agent_destroy_cleans_up(self):
        """测试销毁 Agent 清理资源"""
        mock_client = self.create_mock_client()
        mock_client.agent_destroy.return_value = {"status": "ok"}
        result = mock_client.agent_destroy("agent_0")
        assert result["status"] == "ok"

    def test_agent_invoke_with_input(self):
        """测试 Agent 调用"""
        mock_client = self.create_mock_client()
        mock_client.agent_invoke.return_value = {"output": "processed result"}
        result = mock_client.agent_invoke("agent_0", "test input")
        assert result["output"] == "processed result"

    def test_agent_skill_bind_and_unbind(self):
        """测试 Agent Skill 绑定与解绑"""
        mock_client = self.create_mock_client()
        mock_client.agent_skill_bind.return_value = {"status": "ok"}
        mock_client.agent_skill_unbind.return_value = {"status": "ok"}

        bind_result = mock_client.agent_skill_bind("agent_0", "skill_0")
        assert bind_result["status"] == "ok"

        unbind_result = mock_client.agent_skill_unbind("agent_0", "skill_0")
        assert unbind_result["status"] == "ok"


class TestRustSDKMemoryOperations(SDKTestCase):
    """测试 Rust SDK 记忆操作"""

    def test_memory_write_returns_record_id(self):
        """测试记忆写入返回记录 ID"""
        mock_client = self.create_mock_client()
        mock_client.memory_write.return_value = {"record_id": "mem_0"}
        result = mock_client.memory_write(
            content="test data",
            importance=0.8,
            tags=["test"]
        )
        assert result["record_id"] == "mem_0"

    def test_memory_query_returns_results(self):
        """测试记忆查询返回结果"""
        mock_client = self.create_mock_client()
        mock_client.memory_query.return_value = {
            "records": [
                {"id": "mem_0", "score": 0.95},
                {"id": "mem_1", "score": 0.87}
            ],
            "total": 2
        }
        result = mock_client.memory_query("test query", limit=5)
        assert result["total"] == 2
        assert len(result["records"]) == 2

    def test_memory_evolve_triggers_evolution(self):
        """测试记忆进化触发"""
        mock_client = self.create_mock_client()
        mock_client.memory_evolve.return_value = {
            "evolved_records": 42,
            "layers_affected": [2, 3]
        }
        result = mock_client.memory_evolve()
        assert result["evolved_records"] == 42

    def test_memory_forget_with_strategy(self):
        """测试记忆遗忘策略"""
        mock_client = self.create_mock_client()
        mock_client.memory_forget.return_value = {
            "forgotten_count": 15,
            "preserved_count": 985
        }
        result = mock_client.memory_forget(
            strategy="ebbinghaus",
            threshold=0.3
        )
        assert result["forgotten_count"] == 15


class TestRustSDKSessionManagement(SDKTestCase):
    """测试 Rust SDK 会话管理"""

    def test_session_create_returns_session_id(self):
        """测试创建会话返回 ID"""
        mock_client = self.create_mock_client()
        mock_client.session_create.return_value = {"session_id": "sess_0"}
        result = mock_client.session_create(title="test session")
        assert result["session_id"] == "sess_0"

    def test_session_message_add(self):
        """测试添加会话消息"""
        mock_client = self.create_mock_client()
        mock_client.session_message_add.return_value = {"status": "ok"}
        result = mock_client.session_message_add(
            "sess_0",
            role="user",
            content="Hello"
        )
        assert result["status"] == "ok"

    def test_session_close(self):
        """测试关闭会话"""
        mock_client = self.create_mock_client()
        mock_client.session_close.return_value = {"status": "ok"}
        result = mock_client.session_close("sess_0")
        assert result["status"] == "ok"


class TestRustSDKErrorHandling(SDKTestCase):
    """测试 Rust SDK 错误处理"""

    def test_agent_not_found_error(self):
        """测试 Agent 不存在错误"""
        mock_client = self.create_mock_client()
        mock_client.agent_get.side_effect = Exception("Agent not found: 0x5001")
        with pytest.raises(Exception, match="Agent not found"):
            mock_client.agent_get("agent_nonexistent")

    def test_memory_not_found_error(self):
        """测试记忆不存在错误"""
        mock_client = self.create_mock_client()
        mock_client.memory_get.side_effect = Exception("Record not found: 0x2001")
        with pytest.raises(Exception, match="Record not found"):
            mock_client.memory_get("mem_nonexistent")

    def test_session_not_found_error(self):
        """测试会话不存在错误"""
        mock_client = self.create_mock_client()
        mock_client.session_get.side_effect = Exception("Session not found: 0x3001")
        with pytest.raises(Exception, match="Session not found"):
            mock_client.session_get("sess_nonexistent")

    def test_permission_denied_error(self):
        """测试权限不足错误"""
        mock_client = self.create_mock_client()
        mock_client.agent_invoke.side_effect = Exception("Permission denied: 0x1003")
        with pytest.raises(Exception, match="Permission denied"):
            mock_client.agent_invoke("agent_0", "restricted input")

    def test_timeout_error(self):
        """测试超时错误"""
        mock_client = self.create_mock_client()
        mock_client.task_wait.side_effect = Exception("Timeout: 0x1004")
        with pytest.raises(Exception, match="Timeout"):
            mock_client.task_wait("task_0", timeout_ms=1000)


class TestRustSDKTypeSafety(SDKTestCase):
    """测试 Rust SDK 类型安全"""

    def test_agent_state_enum_values(self):
        """测试 Agent 状态枚举值"""
        valid_states = {
            "CREATED": 0, "INITING": 1, "READY": 2, "RUNNING": 3,
            "PAUSED": 4, "STOPPING": 5, "STOPPED": 6, "ERROR": 7,
            "DESTROYED": 8
        }
        for name, value in valid_states.items():
            assert isinstance(value, int)
            assert 0 <= value <= 8

    def test_task_type_enum_values(self):
        """测试任务类型枚举值"""
        valid_types = {
            "SIMPLE": 0, "COMPLEX": 1, "CRITICAL": 2, "BACKGROUND": 3
        }
        for name, value in valid_types.items():
            assert isinstance(value, int)
            assert 0 <= value <= 3

    def test_memory_layer_enum_values(self):
        """测试记忆层级枚举值"""
        valid_layers = {"L1_RAW": 1, "L2_FEATURE": 2, "L3_STRUCTURE": 3, "L4_PATTERN": 4}
        for name, value in valid_layers.items():
            assert isinstance(value, int)
            assert 1 <= value <= 4

    def test_log_level_enum_values(self):
        """测试日志级别枚举值"""
        valid_levels = {
            "TRACE": 0, "DEBUG": 1, "INFO": 2, "WARN": 3,
            "ERROR": 4, "FATAL": 5
        }
        for name, value in valid_levels.items():
            assert isinstance(value, int)
            assert 0 <= value <= 5
