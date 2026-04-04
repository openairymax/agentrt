# AgentOS Python SDK 单元测试
# Version: 1.0.0.6
# Last updated: 2026-03-22

"""
AgentOS Python SDK 单元测试模块。

测试SDK核心功能，包括客户端初始化、任务管理、记忆操作、会话管理和技能加载。
"""

import pytest
import time
import asyncio
from unittest.mock import Mock, MagicMock, patch, AsyncMock
from typing import Dict, Any, List

import sys
import os
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..', 'toolkit', 'python')))

from agentos import AgentOS, AsyncAgentOS
from agentos.exceptions import (
    AgentOSError,
    NetworkError,
    TimeoutError as AgentOSTimeoutError,
    TaskError,
    MemoryError as AgentOSMemoryError,
    SessionError,
    SkillError,
    AuthenticationError,
)
from agentos.types import TaskStatus, Memory, TaskResult, SkillInfo
from agentos.task import Task
from agentos.session import Session
from agentos.skill import Skill


# ============================================================
# 测试标记
# ============================================================

pytestmark = pytest.mark.sdk


# ============================================================
# AgentOS 客户端初始化测试
# ============================================================

class TestAgentOSInitialization:
    """AgentOS 客户端初始化测试"""

    def test_init_with_default_endpoint(self):
        """
        测试使用默认端点初始化客户端。

        验证:
            - 客户端成功创建
            - 端点设置为默认值
            - 超时设置为默认值
        """
        client = AgentOS()

        assert client is not None
        assert client.endpoint == "http://localhost:18789"
        assert client.timeout == 30

    def test_init_with_custom_endpoint(self):
        """
        测试使用自定义端点初始化客户端。

        验证:
            - 客户端成功创建
            - 端点设置为自定义值
        """
        custom_endpoint = "http://custom.host:8080"
        client = AgentOS(endpoint=custom_endpoint)

        assert client.endpoint == custom_endpoint

    def test_init_with_custom_timeout(self):
        """
        测试使用自定义超时初始化客户端。

        验证:
            - 客户端成功创建
            - 超时设置为自定义值
        """
        custom_timeout = 60
        client = AgentOS(timeout=custom_timeout)

        assert client.timeout == custom_timeout

    def test_init_strips_trailing_slash(self):
        """
        测试端点URL尾部斜杠被移除。

        验证:
            - 端点URL尾部斜杠被正确移除
        """
        client = AgentOS(endpoint="http://localhost:18789/")

        assert client.endpoint == "http://localhost:18789"

    @patch('agentos.agent.requests.Session')
    def test_session_headers_set(self, mock_session):
        """
        测试会话头正确设置。

        验证:
            - Content-Type头设置为application/json
        """
        mock_session_instance = Mock()
        mock_session.return_value = mock_session_instance

        client = AgentOS()

        mock_session_instance.headers.update.assert_called_with(
            {"Content-Type": "application/json"}
        )


# ============================================================
# 任务管理测试
# ============================================================

class TestTaskManagement:
    """任务管理测试"""

    @patch('agentos.agent.requests.Session')
    def test_submit_task_success(self, mock_session):
        """
        测试成功提交任务。

        验证:
            - 任务成功提交
            - 返回正确的任务ID
            - 任务状态为pending
        """
        mock_response = Mock()
        mock_response.status_code = 200
        mock_response.json.return_value = {"task_id": "task_12345"}

        mock_session_instance = Mock()
        mock_session.return_value = mock_session_instance
        mock_session_instance.post.return_value = mock_response

        client = AgentOS()
        task = client.submit_task("分析销售数据")

        assert task is not None
        assert task.task_id == "task_12345"
        mock_session_instance.post.assert_called_once()

    @patch('agentos.agent.requests.Session')
    def test_submit_task_with_priority(self, mock_session):
        """
        测试带优先级提交任务。

        验证:
            - 任务成功提交
            - 优先级正确传递
        """
        mock_response = Mock()
        mock_response.status_code = 200
        mock_response.json.return_value = {"task_id": "task_12345"}

        mock_session_instance = Mock()
        mock_session.return_value = mock_session_instance
        mock_session_instance.post.return_value = mock_response

        client = AgentOS()
        task = client.submit_task("高优先级任务")

        assert task is not None
        assert task.task_id == "task_12345"

        # 验证请求参数
        call_args = mock_session_instance.post.call_args
        assert call_args is not None
        args, kwargs = call_args
        assert "json" in kwargs
        assert kwargs["json"]["description"] == "高优先级任务"

    @patch('agentos.agent.requests.Session')
    def test_submit_task_network_error(self, mock_session):
        """
        测试提交任务时网络错误。

        验证:
            - 抛出NetworkError异常
        """
        mock_session_instance = Mock()
        mock_session.return_value = mock_session_instance
        mock_session_instance.post.side_effect = Exception("Connection refused")

        client = AgentOS()

        with pytest.raises(NetworkError):
            client.submit_task("测试任务")

    @patch('agentos.agent.requests.Session')
    def test_submit_task_timeout_error(self, mock_session):
        """
        测试提交任务时超时错误。

        验证:
            - 抛出AgentOSTimeoutError异常
        """
        import requests
        mock_session_instance = Mock()
        mock_session.return_value = mock_session_instance
        mock_session_instance.post.side_effect = requests.Timeout("Request timeout")

        client = AgentOS()

        with pytest.raises((NetworkError, AgentOSTimeoutError)):
            client.submit_task("测试任务")


# ============================================================
# 记忆管理测试
# ============================================================

class TestMemoryManagement:
    """记忆管理测试"""

    @patch('agentos.agent.requests.Session')
    def test_write_memory_success(self, mock_session):
        """
        测试成功写入记忆。

        验证:
            - 记忆成功写入
            - 返回正确的记忆ID
        """
        mock_response = Mock()
        mock_response.status_code = 200
        mock_response.json.return_value = {"memory_id": "mem_12345"}

        mock_session_instance = Mock()
        mock_session.return_value = mock_session_instance
        mock_session_instance.post.return_value = mock_response

        client = AgentOS()
        memory_id = client.write_memory("用户偏好使用Python")

        assert memory_id == "mem_12345"
        mock_session_instance.post.assert_called_once()

    @patch('agentos.agent.requests.Session')
    def test_write_memory_with_metadata(self, mock_session):
        """
        测试带元数据写入记忆。

        验证:
            - 记忆成功写入
            - 元数据正确传递
        """
        mock_response = Mock()
        mock_response.status_code = 200
        mock_response.json.return_value = {"memory_id": "mem_12345"}

        mock_session_instance = Mock()
        mock_session.return_value = mock_session_instance
        mock_session_instance.post.return_value = mock_response

        client = AgentOS()
        metadata = {"category": "preference", "confidence": 0.95}
        memory_id = client.write_memory("测试记忆", metadata=metadata)

        assert memory_id == "mem_12345"

    @patch('agentos.agent.requests.Session')
    def test_search_memory_success(self, mock_session):
        """
        测试成功搜索记忆。

        验证:
            - 搜索成功
            - 返回正确的记忆列表
        """
        mock_response = Mock()
        mock_response.status_code = 200
        mock_response.json.return_value = {
            "memories": [
                {
                    "memory_id": "mem_001",
                    "content": "Python数据分析",
                    "created_at": "2026-03-22T10:00:00Z",
                    "metadata": {"category": "skill"},
                    "score": 0.95
                },
                {
                    "memory_id": "mem_002",
                    "content": "FastAPI开发",
                    "created_at": "2026-03-22T10:05:00Z",
                    "metadata": {"category": "skill"},
                    "score": 0.88
                }
            ]
        }

        mock_session_instance = Mock()
        mock_session.return_value = mock_session_instance
        mock_session_instance.get.return_value = mock_response

        client = AgentOS()
        memories = client.search_memory("Python", top_k=5)

        assert len(memories) == 2
        assert memories[0].memory_id == "mem_001"
        assert memories[0].content == "Python数据分析"
        mock_session_instance.get.assert_called_once()

    @patch('agentos.agent.requests.Session')
    def test_get_memory_success(self, mock_session):
        """
        测试成功获取单个记忆。

        验证:
            - 成功获取记忆
            - 返回正确的记忆内容
        """
        mock_response = Mock()
        mock_response.status_code = 200
        mock_response.json.return_value = {
            "memory_id": "mem_001",
            "content": "测试记忆内容",
            "created_at": "2026-03-22T10:00:00Z",
            "metadata": {"category": "test"}
        }

        mock_session_instance = Mock()
        mock_session.return_value = mock_session_instance
        mock_session_instance.get.return_value = mock_response

        client = AgentOS()
        memory = client.get_memory("mem_001")

        assert memory.memory_id == "mem_001"
        assert memory.content == "测试记忆内容"

    @patch('agentos.agent.requests.Session')
    def test_delete_memory_success(self, mock_session):
        """
        测试成功删除记忆。

        验证:
            - 成功删除记忆
            - 返回True
        """
        mock_response = Mock()
        mock_response.status_code = 200
        mock_response.json.return_value = {"success": True}

        mock_session_instance = Mock()
        mock_session.return_value = mock_session_instance
        mock_session_instance.delete.return_value = mock_response

        client = AgentOS()
        result = client.delete_memory("mem_001")

        assert result is True
        mock_session_instance.delete.assert_called_once()

    @patch('agentos.agent.requests.Session')
    def test_delete_memory_not_found(self, mock_session):
        """
        测试删除不存在的记忆。

        验证:
            - 返回False或抛出异常
        """
        mock_response = Mock()
        mock_response.status_code = 404
        mock_response.json.return_value = {"error": "Memory not found"}

        mock_session_instance = Mock()
        mock_session.return_value = mock_session_instance
        mock_session_instance.delete.return_value = mock_response

        client = AgentOS()

        with pytest.raises((AgentOSMemoryError, AgentOSError)):
            client.delete_memory("nonexistent_mem")


# ============================================================
# 会话管理测试
# ============================================================

class TestSessionManagement:
    """会话管理测试"""

    @patch('agentos.agent.requests.Session')
    def test_create_session_success(self, mock_session):
        """
        测试成功创建会话。

        验证:
            - 会话成功创建
            - 返回正确的会话ID
        """
        mock_response = Mock()
        mock_response.status_code = 200
        mock_response.json.return_value = {"session_id": "sess_12345"}

        mock_session_instance = Mock()
        mock_session.return_value = mock_session_instance
        mock_session_instance.post.return_value = mock_response

        client = AgentOS()
        session = client.create_session()

        assert session is not None
        assert session.session_id == "sess_12345"

    @patch('agentos.agent.requests.Session')
    def test_session_set_context(self, mock_session):
        """
        测试设置会话上下文。

        验证:
            - 上下文成功设置
        """
        mock_response = Mock()
        mock_response.status_code = 200
        mock_response.json.return_value = {"success": True}

        mock_session_instance = Mock()
        mock_session.return_value = mock_session_instance
        mock_session_instance.post.return_value = mock_response

        client = AgentOS()
        session = client.create_session()

        mock_response.json.return_value = {"success": True}
        result = session.set_context("user_id", "12345")

        assert result is True

    @patch('agentos.agent.requests.Session')
    def test_session_get_context(self, mock_session):
        """
        测试获取会话上下文。

        验证:
            - 成功获取上下文值
        """
        mock_response = Mock()
        mock_response.status_code = 200
        mock_response.json.return_value = {"session_id": "sess_12345"}

        mock_session_instance = Mock()
        mock_session.return_value = mock_session_instance
        mock_session_instance.post.return_value = mock_response

        client = AgentOS()
        session = client.create_session()

        mock_response.json.return_value = {"value": "12345"}
        mock_session_instance.get.return_value = mock_response

        value = session.get_context("user_id")

        assert value == "12345"

    @patch('agentos.agent.requests.Session')
    def test_session_close(self, mock_session):
        """
        测试关闭会话。

        验证:
            - 会话成功关闭
        """
        mock_response = Mock()
        mock_response.status_code = 200
        mock_response.json.return_value = {"session_id": "sess_12345"}

        mock_session_instance = Mock()
        mock_session.return_value = mock_session_instance
        mock_session_instance.post.return_value = mock_response

        client = AgentOS()
        session = client.create_session()

        mock_response.json.return_value = {"success": True}
        mock_session_instance.delete.return_value = mock_response

        result = session.close()

        assert result is True


# ============================================================
# 技能管理测试
# ============================================================

class TestSkillManagement:
    """技能管理测试"""

    def test_load_skill_success(self):
        """
        测试成功加载技能。

        验证:
            - 技能成功加载
            - 返回正确的技能对象
        """
        client = AgentOS()
        skill = client.load_skill("browser_skill")

        assert skill is not None
        assert skill.skill_name == "browser_skill"

    def test_skill_get_info(self):
        """
        测试获取技能信息。

        验证:
            - 成功获取技能信息
        """
        client = AgentOS()
        skill = client.load_skill("data_analysis_skill")

        info = skill.get_info()

        assert info is not None
        assert info.skill_name == "data_analysis_skill"

    @patch('agentos.agent.requests.Session')
    def test_skill_execute(self, mock_session):
        """
        测试执行技能。

        验证:
            - 技能成功执行
            - 返回执行结果
        """
        mock_response = Mock()
        mock_response.status_code = 200
        mock_response.json.return_value = {
            "success": True,
            "output": {"result": "执行成功"}
        }

        mock_session_instance = Mock()
        mock_session.return_value = mock_session_instance
        mock_session_instance.post.return_value = mock_response

        client = AgentOS()
        skill = client.load_skill("test_skill")

        result = skill.execute({"param1": "value1"})

        assert result is not None


# ============================================================
# 异步客户端测试
# ============================================================

class TestAsyncAgentOS:
    """异步客户端测试"""

    @pytest.mark.asyncio
    async def test_async_init(self):
        """
        测试异步客户端初始化。

        验证:
            - 客户端成功创建
            - 端点设置正确
        """
        client = AsyncAgentOS()

        assert client is not None
        assert client.endpoint == "http://localhost:18789"

    @pytest.mark.asyncio
    @patch('aiohttp.ClientSession')
    async def test_async_submit_task(self, mock_session):
        """
        测试异步提交任务。

        验证:
            - 任务成功提交
            - 返回正确的任务ID
        """
        mock_response = AsyncMock()
        mock_response.status = 200
        mock_response.json.return_value = {"task_id": "async_task_12345"}

        mock_session_instance = AsyncMock()
        mock_session.return_value = mock_session_instance
        mock_session_instance.post.return_value.__aenter__.return_value = mock_response

        client = AsyncAgentOS()
        task = await client.submit_task("异步任务测试")

        assert task is not None
        assert task.task_id == "async_task_12345"

    @pytest.mark.asyncio
    @patch('aiohttp.ClientSession')
    async def test_async_write_memory(self, mock_session):
        """
        测试异步写入记忆。

        验证:
            - 记忆成功写入
            - 返回正确的记忆ID
        """
        mock_response = AsyncMock()
        mock_response.status = 200
        mock_response.json.return_value = {"memory_id": "async_mem_12345"}

        mock_session_instance = AsyncMock()
        mock_session.return_value = mock_session_instance
        mock_session_instance.post.return_value.__aenter__.return_value = mock_response

        client = AsyncAgentOS()
        memory_id = await client.write_memory("异步记忆测试")

        assert memory_id == "async_mem_12345"

    @pytest.mark.asyncio
    @patch('aiohttp.ClientSession')
    async def test_async_search_memory(self, mock_session):
        """
        测试异步搜索记忆。

        验证:
            - 搜索成功
            - 返回正确的记忆列表
        """
        mock_response = AsyncMock()
        mock_response.status = 200
        mock_response.json.return_value = {
            "memories": [
                {
                    "memory_id": "mem_001",
                    "content": "异步记忆内容",
                    "created_at": "2026-03-22T10:00:00Z",
                    "metadata": {}
                }
            ]
        }

        mock_session_instance = AsyncMock()
        mock_session.return_value = mock_session_instance
        mock_session_instance.get.return_value.__aenter__.return_value = mock_response

        client = AsyncAgentOS()
        memories = await client.search_memory("测试")

        assert len(memories) == 1
        assert memories[0].memory_id == "mem_001"


# ============================================================
# 错误处理测试
# ============================================================

class TestErrorHandling:
    """错误处理测试"""

    @patch('agentos.agent.requests.Session')
    def test_network_error_handling(self, mock_session):
        """
        测试网络错误处理。

        验证:
            - 正确抛出NetworkError
        """
        mock_session_instance = Mock()
        mock_session.return_value = mock_session_instance
        mock_session_instance.post.side_effect = Exception("Network error")

        client = AgentOS()

        with pytest.raises(NetworkError):
            client.submit_task("测试任务")

    @patch('agentos.agent.requests.Session')
    def test_server_error_handling(self, mock_session):
        """
        测试服务器错误处理。

        验证:
            - 正确抛出AgentOSError
        """
        mock_response = Mock()
        mock_response.status_code = 500
        mock_response.json.return_value = {"error": "Internal server error"}

        mock_session_instance = Mock()
        mock_session.return_value = mock_session_instance
        mock_session_instance.post.return_value = mock_response

        client = AgentOS()

        with pytest.raises(AgentOSError):
            client.submit_task("测试任务")

    @patch('agentos.agent.requests.Session')
    def test_authentication_error_handling(self, mock_session):
        """
        测试认证错误处理。

        验证:
            - 正确抛出AuthenticationError
        """
        mock_response = Mock()
        mock_response.status_code = 401
        mock_response.json.return_value = {"error": "Unauthorized"}

        mock_session_instance = Mock()
        mock_session.return_value = mock_session_instance
        mock_session_instance.post.return_value = mock_response

        client = AgentOS()

        with pytest.raises((AuthenticationError, AgentOSError)):
            client.submit_task("测试任务")


# ============================================================
# 性能测试
# ============================================================

class TestPerformance:
    """性能测试"""

    @pytest.mark.benchmark
    @patch('agentos.agent.requests.Session')
    def test_submit_task_latency(self, mock_session, performance_timer):
        """
        测试任务提交延迟。

        验证:
            - 提交延迟小于100ms
        """
        mock_response = Mock()
        mock_response.status_code = 200
        mock_response.json.return_value = {"task_id": "task_12345"}

        mock_session_instance = Mock()
        mock_session.return_value = mock_session_instance
        mock_session_instance.post.return_value = mock_response

        client = AgentOS()

        with performance_timer as timer:
            client.submit_task("性能测试任务")

        timer.assert_faster_than(0.1)

    @pytest.mark.benchmark
    @patch('agentos.agent.requests.Session')
    def test_search_memory_latency(self, mock_session, performance_timer):
        """
        测试记忆搜索延迟。

        验证:
            - 搜索延迟小于200ms
        """
        mock_response = Mock()
        mock_response.status_code = 200
        mock_response.json.return_value = {
            "memories": [
                {
                    "memory_id": f"mem_{i}",
                    "content": f"内容{i}",
                    "created_at": "2026-03-22T10:00:00Z",
                    "metadata": {}
                }
                for i in range(10)
            ]
        }

        mock_session_instance = Mock()
        mock_session.return_value = mock_session_instance
        mock_session_instance.get.return_value = mock_response

        client = AgentOS()

        with performance_timer as timer:
            client.search_memory("测试", top_k=10)

        timer.assert_faster_than(0.2)


# ============================================================
# 运行测试
# ============================================================

if __name__ == "__main__":
    pytest.main([__file__, "-v", "--tb=short"])
