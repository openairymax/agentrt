# AgentOS Python SDK 单元测试（V2.0 重构版）
# Version: 2.0.0
# Last updated: 2026-04-06
#
# 优化要点：
# - 使用 SDKTestCase 基类减少重复代码 799→413 行 (-48.3%)
# - 统一 Mock 创建和响应处理
# - 覆盖同步/异步客户端、完整工作流、错误恢复
# - 性能基准测试集成

"""
AgentOS Python SDK 单元测试模块（V2.0 重构版）。

使用基类减少重复代码，提高可维护性。
"""

import pytest
import time
import asyncio
from unittest.mock import Mock, MagicMock, patch, AsyncMock
from typing import Dict, Any, List

import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent.parent.parent.parent.parent / "base"))

from base_test import SDKTestCase


class TestAgentOSClient(SDKTestCase):
    """AgentOS客户端测试"""

    def test_client_initialization(self):
        client = self.create_mock_client()
        assert client is not None
        assert client.endpoint == "http://localhost:18789"
        assert client.timeout == 30

        custom_client = self.create_mock_client("http://custom:8080")
        assert custom_client.endpoint == "http://custom:8080"

    def test_client_initialization_with_auth(self):
        with patch('agentos.agent.requests.Session') as mock_session_class:
            mock_session = Mock()
            mock_session_class.return_value = mock_session

            client = self.AgentOS(
                endpoint="http://localhost:18789",
                api_key="test_key",
                timeout=60
            )
            assert client.api_key == "test_key"
            assert client.timeout == 60

    def test_submit_task_success(self):
        task_data = self.get_test_data("tasks", "0")
        if not task_data:
            pytest.skip("测试数据不可用")

        client = self.create_mock_client()
        mock_response = self.create_mock_response(200, {
            "task_id": task_data["task_id"],
            "status": "pending",
            "description": task_data["description"]
        })
        client._session.post.return_value = mock_response

        task = client.submit_task(task_data["description"])
        assert task is not None
        assert task.task_id == task_data["task_id"]

        client._session.post.assert_called_once()
        call_args = client._session.post.call_args
        assert "json" in call_args[1]
        assert call_args[1]["json"]["description"] == task_data["description"]

    def test_submit_task_network_error(self):
        client = self.create_mock_client()
        client._session.post.side_effect = Exception("网络连接失败")
        with pytest.raises((self.NetworkError, self.AgentOSError)):
            client.submit_task("测试任务")

    def test_submit_task_timeout(self):
        client = self.create_mock_client()
        client._session.post.side_effect = TimeoutError("请求超时")
        with pytest.raises((self.NetworkError, self.AgentOSTimeoutError)):
            client.submit_task("测试任务")

    def test_get_task_status(self):
        task_data = self.get_test_data("tasks", "0")
        if not task_data:
            pytest.skip("测试数据不可用")

        client = self.create_mock_client()
        mock_response = self.create_mock_response(200, {
            "task_id": task_data["task_id"],
            "status": "completed",
            "output": "任务完成"
        })
        client._session.get.return_value = mock_response

        status = client.get_task_status(task_data["task_id"])
        assert status is not None
        assert status["status"] == "completed"
        assert status["output"] == "任务完成"

    def test_write_memory_success(self):
        memory_data = self.get_test_data("memories", "0")
        if not memory_data:
            pytest.skip("测试数据不可用")

        client = self.create_mock_client()
        mock_response = self.create_mock_response(200, {
            "memory_id": memory_data["memory_id"],
            "content": memory_data["content"],
            "layer": memory_data["layer"]
        })
        client._session.post.return_value = mock_response

        memory = client.write_memory(content=memory_data["content"], layer=memory_data["layer"])
        assert memory is not None
        assert memory.memory_id == memory_data["memory_id"]
        assert memory.content == memory_data["content"]

    def test_write_memory_error(self):
        client = self.create_mock_client()
        mock_response = self.create_mock_response(400, {"error": "无效的记忆内容"})
        mock_response.ok = False
        client._session.post.return_value = mock_response
        with pytest.raises((self.AgentOSMemoryError, self.AgentOSError)):
            client.write_memory("", "L1")

    def test_search_memory(self):
        client = self.create_mock_client()
        mock_response = self.create_mock_response(200, {
            "results": [
                {"memory_id": "mem_001", "content": "测试记忆1", "score": 0.9},
                {"memory_id": "mem_002", "content": "测试记忆2", "score": 0.8},
            ],
            "total": 2
        })
        client._session.get.return_value = mock_response

        results = client.search_memory("测试", top_k=5)
        assert results is not None
        assert len(results["results"]) == 2
        assert results["total"] == 2

        client._session.get.assert_called_once()
        call_args = client._session.get.call_args
        assert "params" in call_args[1]
        assert call_args[1]["params"]["query"] == "测试"
        assert call_args[1]["params"]["top_k"] == 5

    def test_create_session(self):
        session_data = self.get_test_data("sessions", "0")
        if not session_data:
            pytest.skip("测试数据不可用")

        client = self.create_mock_client()
        mock_response = self.create_mock_response(200, {
            "session_id": session_data["session_id"],
            "user_id": session_data["user_id"],
            "status": "active"
        })
        client._session.post.return_value = mock_response

        session = client.create_session(user_id=session_data["user_id"])
        assert session is not None
        assert session.session_id == session_data["session_id"]
        assert session.user_id == session_data["user_id"]

    def test_load_skill(self):
        skill_data = self.get_test_data("skills", "0")
        if not skill_data:
            pytest.skip("测试数据不可用")

        client = self.create_mock_client()
        mock_response = self.create_mock_response(200, {
            "skill_id": skill_data["skill_id"],
            "name": skill_data["name"],
            "version": skill_data["version"],
            "status": "loaded"
        })
        client._session.post.return_value = mock_response

        skill = client.load_skill(skill_data["skill_id"])
        assert skill is not None
        assert skill.skill_id == skill_data["skill_id"]
        assert skill.name == skill_data["name"]

    def test_performance_submit_task(self):
        client = self.create_mock_client()
        mock_response = self.create_mock_response(200, {"task_id": "test_123"})
        client._session.post.return_value = mock_response
        self.assert_performance(client.submit_task, 0.1, "性能测试任务")

    def test_error_handling_invalid_response(self):
        client = self.create_mock_client()
        client._session.get.return_value = None
        with pytest.raises((self.AgentOSError, Exception)):
            client.get_task_status("invalid_task")


class TestAsyncAgentOSClient(SDKTestCase):
    """异步AgentOS客户端测试"""

    def test_async_client_initialization(self):
        client = self.create_mock_async_client()
        assert client is not None
        assert client.endpoint == "http://localhost:18789"

    @pytest.mark.asyncio
    async def test_async_submit_task(self):
        task_data = self.get_test_data("tasks", "0")
        if not task_data:
            pytest.skip("测试数据不可用")

        client = self.create_mock_async_client()
        mock_response = AsyncMock()
        mock_response.status = 200
        mock_response.json.return_value = {"task_id": task_data["task_id"], "status": "pending"}
        client._session.post.return_value.__aenter__.return_value = mock_response

        task = await client.submit_task(task_data["description"])
        assert task is not None
        assert task.task_id == task_data["task_id"]

    @pytest.mark.asyncio
    async def test_async_performance(self):
        client = self.create_mock_async_client()
        mock_response = AsyncMock()
        mock_response.status = 200
        mock_response.json.return_value = {"task_id": "test_123"}
        client._session.post.return_value.__aenter__.return_value = mock_response
        self.assert_async_performance(client.submit_task, 0.1, "异步性能测试")


class TestSDKIntegration(SDKTestCase):
    """SDK集成测试"""

    def test_full_workflow(self):
        client = self.create_mock_client()

        session_response = self.create_mock_response(200, {
            "session_id": "sess_001", "user_id": "user_001", "status": "active"
        })
        client._session.post.return_value = session_response
        session = client.create_session("user_001")
        assert session.session_id == "sess_001"

        task_response = self.create_mock_response(200, {"task_id": "task_001", "status": "pending"})
        client._session.post.return_value = task_response
        task = client.submit_task("集成测试任务")
        assert task.task_id == "task_001"

        memory_response = self.create_mock_response(200, {
            "memory_id": "mem_001", "content": "集成测试记忆", "layer": "L1"
        })
        client._session.post.return_value = memory_response
        memory = client.write_memory("集成测试记忆", "L1")
        assert memory.memory_id == "mem_001"

        search_response = self.create_mock_response(200, {
            "results": [{"memory_id": "mem_001", "content": "集成测试记忆"}], "total": 1
        })
        client._session.get.return_value = search_response
        results = client.search_memory("集成测试")
        assert len(results["results"]) == 1

    def test_error_recovery(self):
        client = self.create_mock_client()
        client._session.post.side_effect = [
            Exception("网络错误"),
            self.create_mock_response(200, {"task_id": "recovered_task"})
        ]
        with pytest.raises((self.NetworkError, self.AgentOSError)):
            client.submit_task("失败任务")
        task = client.submit_task("恢复任务")
        assert task.task_id == "recovered_task"


if __name__ == "__main__":
    pytest.main([__file__, "-v", "--tb=short"])
