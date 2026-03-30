# AgentOS Python SDK 单元测试（重构版）
# Version: 1.0.0.6
# Last updated: 2026-03-22

"""
AgentOS Python SDK 单元测试模块（重构版）。

使用基类减少重复代码，提高可维护性。
"""

import pytest
import time
import asyncio
from unittest.mock import Mock, MagicMock, patch, AsyncMock
from typing import Dict, Any, List

# 导入测试基类
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent.parent.parent.parent.parent / "base"))

from base_test import SDKTestCase


class TestAgentOSClient(SDKTestCase):
    """AgentOS客户端测试"""

    def test_client_initialization(self):
        """测试客户端初始化"""
        # 测试默认初始化
        client = self.create_mock_client()
        assert client is not None
        assert client.endpoint == "http://localhost:18789"
        assert client.timeout == 30

        # 测试自定义端点
        custom_client = self.create_mock_client("http://custom:8080")
        assert custom_client.endpoint == "http://custom:8080"

    def test_client_initialization_with_auth(self):
        """测试带认证的客户端初始化"""
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
        """测试任务提交成功"""
        # 准备测试数据
        task_data = self.get_test_data("tasks", "0")
        if not task_data:
            pytest.skip("测试数据不可用")

        # 创建模拟客户端
        client = self.create_mock_client()

        # 设置模拟响应
        mock_response = self.create_mock_response(200, {
            "task_id": task_data["task_id"],
            "status": "pending",
            "description": task_data["description"]
        })
        client._session.post.return_value = mock_response

        # 执行测试
        task = client.submit_task(task_data["description"])

        # 验证结果
        assert task is not None
        assert task.task_id == task_data["task_id"]

        # 验证请求参数
        client._session.post.assert_called_once()
        call_args = client._session.post.call_args
        assert "json" in call_args[1]
        assert call_args[1]["json"]["description"] == task_data["description"]

    def test_submit_task_network_error(self):
        """测试任务提交网络错误"""
        client = self.create_mock_client()

        # 设置网络错误
        client._session.post.side_effect = Exception("网络连接失败")

        # 执行测试并验证异常
        with pytest.raises((self.NetworkError, self.AgentOSError)):
            client.submit_task("测试任务")

    def test_submit_task_timeout(self):
        """测试任务提交超时"""
        client = self.create_mock_client()

        # 设置超时错误
        client._session.post.side_effect = TimeoutError("请求超时")

        # 执行测试并验证异常
        with pytest.raises((self.NetworkError, self.AgentOSTimeoutError)):
            client.submit_task("测试任务")

    def test_get_task_status(self):
        """测试获取任务状态"""
        # 准备测试数据
        task_data = self.get_test_data("tasks", "0")
        if not task_data:
            pytest.skip("测试数据不可用")

        client = self.create_mock_client()

        # 设置模拟响应
        mock_response = self.create_mock_response(200, {
            "task_id": task_data["task_id"],
            "status": "completed",
            "output": "任务完成"
        })
        client._session.get.return_value = mock_response

        # 执行测试
        status = client.get_task_status(task_data["task_id"])

        # 验证结果
        assert status is not None
        assert status["status"] == "completed"
        assert status["output"] == "任务完成"

    def test_write_memory_success(self):
        """测试写入记忆成功"""
        # 准备测试数据
        memory_data = self.get_test_data("memories", "0")
        if not memory_data:
            pytest.skip("测试数据不可用")

        client = self.create_mock_client()

        # 设置模拟响应
        mock_response = self.create_mock_response(200, {
            "memory_id": memory_data["memory_id"],
            "content": memory_data["content"],
            "layer": memory_data["layer"]
        })
        client._session.post.return_value = mock_response

        # 执行测试
        memory = client.write_memory(
            content=memory_data["content"],
            layer=memory_data["layer"]
        )

        # 验证结果
        assert memory is not None
        assert memory.memory_id == memory_data["memory_id"]
        assert memory.content == memory_data["content"]

    def test_write_memory_error(self):
        """测试写入记忆错误"""
        client = self.create_mock_client()

        # 设置错误响应
        mock_response = self.create_mock_response(400, {"error": "无效的记忆内容"})
        mock_response.ok = False
        client._session.post.return_value = mock_response

        # 执行测试并验证异常
        with pytest.raises((self.AgentOSMemoryError, self.AgentOSError)):
            client.write_memory("", "L1")

    def test_search_memory(self):
        """测试搜索记忆"""
        client = self.create_mock_client()

        # 设置模拟响应
        mock_response = self.create_mock_response(200, {
            "results": [
                {
                    "memory_id": "mem_001",
                    "content": "测试记忆1",
                    "score": 0.9
                },
                {
                    "memory_id": "mem_002",
                    "content": "测试记忆2",
                    "score": 0.8
                }
            ],
            "total": 2
        })
        client._session.get.return_value = mock_response

        # 执行测试
        results = client.search_memory("测试", top_k=5)

        # 验证结果
        assert results is not None
        assert len(results["results"]) == 2
        assert results["total"] == 2

        # 验证请求参数
        client._session.get.assert_called_once()
        call_args = client._session.get.call_args
        assert "params" in call_args[1]
        assert call_args[1]["params"]["query"] == "测试"
        assert call_args[1]["params"]["top_k"] == 5

    def test_create_session(self):
        """测试创建会话"""
        # 准备测试数据
        session_data = self.get_test_data("sessions", "0")
        if not session_data:
            pytest.skip("测试数据不可用")

        client = self.create_mock_client()

        # 设置模拟响应
        mock_response = self.create_mock_response(200, {
            "session_id": session_data["session_id"],
            "user_id": session_data["user_id"],
            "status": "active"
        })
        client._session.post.return_value = mock_response

        # 执行测试
        session = client.create_session(user_id=session_data["user_id"])

        # 验证结果
        assert session is not None
        assert session.session_id == session_data["session_id"]
        assert session.user_id == session_data["user_id"]

    def test_load_skill(self):
        """测试加载技能"""
        # 准备测试数据
        skill_data = self.get_test_data("skills", "0")
        if not skill_data:
            pytest.skip("测试数据不可用")

        client = self.create_mock_client()

        # 设置模拟响应
        mock_response = self.create_mock_response(200, {
            "skill_id": skill_data["skill_id"],
            "name": skill_data["name"],
            "version": skill_data["version"],
            "status": "loaded"
        })
        client._session.post.return_value = mock_response

        # 执行测试
        skill = client.load_skill(skill_data["skill_id"])

        # 验证结果
        assert skill is not None
        assert skill.skill_id == skill_data["skill_id"]
        assert skill.name == skill_data["name"]

    def test_performance_submit_task(self):
        """测试任务提交性能"""
        client = self.create_mock_client()

        # 设置模拟响应
        mock_response = self.create_mock_response(200, {"task_id": "test_123"})
        client._session.post.return_value = mock_response

        # 性能测试 - 应该在0.1秒内完成
        self.assert_performance(
            client.submit_task,
            0.1,
            "性能测试任务"
        )

    def test_error_handling_invalid_response(self):
        """测试无效响应的错误处理"""
        client = self.create_mock_client()

        # 设置无效响应
        client._session.get.return_value = None

        # 执行测试并验证异常
        with pytest.raises((self.AgentOSError, Exception)):
            client.get_task_status("invalid_task")


class TestAsyncAgentOSClient(SDKTestCase):
    """异步AgentOS客户端测试"""

    def test_async_client_initialization(self):
        """测试异步客户端初始化"""
        client = self.create_mock_async_client()
        assert client is not None
        assert client.endpoint == "http://localhost:18789"

    @pytest.mark.asyncio
    async def test_async_submit_task(self):
        """测试异步任务提交"""
        # 准备测试数据
        task_data = self.get_test_data("tasks", "0")
        if not task_data:
            pytest.skip("测试数据不可用")

        client = self.create_mock_async_client()

        # 设置模拟响应
        mock_response = AsyncMock()
        mock_response.status = 200
        mock_response.json.return_value = {
            "task_id": task_data["task_id"],
            "status": "pending"
        }
        client._session.post.return_value.__aenter__.return_value = mock_response

        # 执行测试
        task = await client.submit_task(task_data["description"])

        # 验证结果
        assert task is not None
        assert task.task_id == task_data["task_id"]

    @pytest.mark.asyncio
    async def test_async_performance(self):
        """测试异步性能"""
        client = self.create_mock_async_client()

        # 设置模拟响应
        mock_response = AsyncMock()
        mock_response.status = 200
        mock_response.json.return_value = {"task_id": "test_123"}
        client._session.post.return_value.__aenter__.return_value = mock_response

        # 异步性能测试
        self.assert_async_performance(
            client.submit_task,
            0.1,
            "异步性能测试"
        )


class TestSDKIntegration(SDKTestCase):
    """SDK集成测试"""

    def test_full_workflow(self):
        """测试完整工作流程"""
        client = self.create_mock_client()

        # 1. 创建会话
        session_response = self.create_mock_response(200, {
            "session_id": "sess_001",
            "user_id": "user_001",
            "status": "active"
        })
        client._session.post.return_value = session_response

        session = client.create_session("user_001")
        assert session.session_id == "sess_001"

        # 2. 提交任务
        task_response = self.create_mock_response(200, {
            "task_id": "task_001",
            "status": "pending"
        })
        client._session.post.return_value = task_response

        task = client.submit_task("集成测试任务")
        assert task.task_id == "task_001"

        # 3. 写入记忆
        memory_response = self.create_mock_response(200, {
            "memory_id": "mem_001",
            "content": "集成测试记忆",
            "layer": "L1"
        })
        client._session.post.return_value = memory_response

        memory = client.write_memory("集成测试记忆", "L1")
        assert memory.memory_id == "mem_001"

        # 4. 搜索记忆
        search_response = self.create_mock_response(200, {
            "results": [{"memory_id": "mem_001", "content": "集成测试记忆"}],
            "total": 1
        })
        client._session.get.return_value = search_response

        results = client.search_memory("集成测试")
        assert len(results["results"]) == 1

    def test_error_recovery(self):
        """测试错误恢复"""
        client = self.create_mock_client()

        # 第一次调用失败
        client._session.post.side_effect = [
            Exception("网络错误"),
            self.create_mock_response(200, {"task_id": "recovered_task"})
        ]

        # 第一次调用应该失败
        with pytest.raises((self.NetworkError, self.AgentOSError)):
            client.submit_task("失败任务")

        # 第二次调用应该成功
        task = client.submit_task("恢复任务")
        assert task.task_id == "recovered_task"


# 运行测试
if __name__ == "__main__":
    pytest.main([__file__, "-v", "--tb=short"])
