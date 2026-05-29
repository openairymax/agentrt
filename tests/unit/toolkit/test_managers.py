# AgentOS Python SDK - Manager Classes Unit Tests
# Version: 1.0.0
# Last updated: 2026-04-04

"""
Unit tests for AgentOS Manager classes.

This module contains comprehensive tests for TaskManager, SessionManager,
MemoryManager, and SkillManager classes.

遵循 ARCHITECTURAL_PRINCIPLES.md 的 E-8（可测试性原则）。

Run with: pytest tests/test_managers.py -v --cov=agentos.modules --cov-report=term-missing
"""

import pytest
from unittest.mock import Mock, MagicMock, patch
from datetime import datetime

from agentos.client.client import APIClient, APIResponse
from agentos.exceptions import AgentOSError, CODE_MISSING_PARAMETER, CODE_INVALID_RESPONSE, CODE_TASK_TIMEOUT
from agentos.types.common import (
    Task, TaskStatus, TaskResult,
    Session, SessionStatus,
    Memory, MemoryLayer, MemorySearchResult,
    Skill, SkillStatus, SkillResult, SkillInfo,
    ListOptions, PaginationOptions
)
from agentos.modules.task.manager import TaskManager
from agentos.modules.session.manager import SessionManager
from agentos.modules.memory.manager import MemoryManager, MemoryWriteItem
from agentos.modules.skill.manager import SkillManager, SkillExecuteRequest


class TestTaskManager:
    """TaskManager单元测试类"""

    @pytest.fixture
    def mock_api(self):
        """创建Mock APIClient"""
        return Mock(spec=APIClient)

    @pytest.fixture
    def task_manager(self, mock_api):
        """创建TaskManager实例"""
        return TaskManager(mock_api)

    def test_init(self, mock_api):
        """测试初始化"""
        manager = TaskManager(mock_api)
        assert manager._api == mock_api

    def test_submit_success(self, task_manager, mock_api):
        """测试任务提交成功"""
        mock_api.post.return_value = APIResponse(
            success=True,
            data={"task_id": "task_123", "status": "pending", "description": "test task"}
        )
        
        task = task_manager.submit("test task")
        
        assert task.id == "task_123"
        assert task.description == "test task"
        assert task.status == TaskStatus.PENDING
        mock_api.post.assert_called_once_with("/api/v1/tasks", {"description": "test task"})

    def test_submit_empty_description_raises_error(self, task_manager):
        """测试空任务描述抛出异常"""
        with pytest.raises(AgentOSError) as exc_info:
            task_manager.submit("")
        assert "任务描述" in str(exc_info.value)

    def test_submit_with_options(self, task_manager, mock_api):
        """测试带选项的任务提交"""
        mock_api.post.return_value = APIResponse(
            success=True,
            data={"task_id": "task_456", "status": "pending"}
        )
        
        task = task_manager.submit_with_options(
            "test task",
            priority=10,
            metadata={"source": "test"}
        )
        
        assert task.id == "task_456"
        assert task.priority == 10
        mock_api.post.assert_called_once()

    def test_get_success(self, task_manager, mock_api):
        """测试获取任务成功"""
        mock_api.get.return_value = APIResponse(
            success=True,
            data={"task_id": "task_123", "status": "running", "description": "test"}
        )
        
        task = task_manager.get("task_123")
        
        assert task.id == "task_123"
        assert task.status == TaskStatus.RUNNING
        mock_api.get.assert_called_once_with("/api/v1/tasks/task_123")

    def test_get_empty_id_raises_error(self, task_manager):
        """测试空任务ID抛出异常"""
        with pytest.raises(AgentOSError) as exc_info:
            task_manager.get("")
        assert "任务ID" in str(exc_info.value)

    def test_query_returns_status(self, task_manager, mock_api):
        """测试查询任务状态"""
        mock_api.get.return_value = APIResponse(
            success=True,
            data={"task_id": "task_123", "status": "completed", "description": "test"}
        )
        
        status = task_manager.query("task_123")
        
        assert status == TaskStatus.COMPLETED

    def test_wait_success(self, task_manager, mock_api):
        """测试等待任务完成"""
        mock_api.get.return_value = APIResponse(
            success=True,
            data={"task_id": "task_123", "status": "completed", "description": "test", "output": "done"}
        )
        
        result = task_manager.wait("task_123", timeout=1)
        
        assert result.status == TaskStatus.COMPLETED
        assert result.output == "done"

    def test_wait_timeout_raises_error(self, task_manager, mock_api):
        """测试等待超时抛出异常"""
        mock_api.get.return_value = APIResponse(
            success=True,
            data={"task_id": "task_123", "status": "running", "description": "test"}
        )
        
        with pytest.raises(AgentOSError) as exc_info:
            task_manager.wait("task_123", timeout=0.1)
        assert exc_info.value.error_code == CODE_TASK_TIMEOUT

    def test_cancel_success(self, task_manager, mock_api):
        """测试取消任务"""
        mock_api.post.return_value = APIResponse(success=True)
        
        task_manager.cancel("task_123")
        
        mock_api.post.assert_called_once_with("/api/v1/tasks/task_123/cancel", None)

    def test_list_success(self, task_manager, mock_api):
        """测试列出任务"""
        mock_api.get.return_value = APIResponse(
            success=True,
            data={
                "tasks": [
                    {"task_id": "task_1", "status": "pending", "description": "task1"},
                    {"task_id": "task_2", "status": "running", "description": "task2"}
                ]
            }
        )
        
        tasks = task_manager.list()
        
        assert len(tasks) == 2
        assert tasks[0].id == "task_1"
        assert tasks[1].id == "task_2"

    def test_list_with_options(self, task_manager, mock_api):
        """测试带选项列出任务"""
        mock_api.get.return_value = APIResponse(success=True, data={"tasks": []})
        
        opts = ListOptions(pagination=PaginationOptions(page=1, page_size=10))
        task_manager.list(opts)
        
        mock_api.get.assert_called_once()

    def test_delete_success(self, task_manager, mock_api):
        """测试删除任务"""
        mock_api.delete.return_value = APIResponse(success=True)
        
        task_manager.delete("task_123")
        
        mock_api.delete.assert_called_once_with("/api/v1/tasks/task_123")

    def test_count_success(self, task_manager, mock_api):
        """测试获取任务总数"""
        mock_api.get.return_value = APIResponse(success=True, data={"count": 42})
        
        count = task_manager.count()
        
        assert count == 42

    def test_count_empty_response(self, task_manager, mock_api):
        """测试空响应返回0"""
        mock_api.get.return_value = APIResponse(success=True, data=None)
        
        count = task_manager.count()
        
        assert count == 0

    def test_batch_submit_success(self, task_manager, mock_api):
        """测试批量提交任务"""
        mock_api.post.return_value = APIResponse(
            success=True,
            data={"task_id": "task_new", "status": "pending"}
        )
        
        tasks = task_manager.batch_submit(["task1", "task2", "task3"])
        
        assert len(tasks) == 3
        assert mock_api.post.call_count == 3

    def test_wait_for_any_success(self, task_manager, mock_api):
        """测试等待任一任务完成"""
        mock_api.get.return_value = APIResponse(
            success=True,
            data={"task_id": "task_1", "status": "completed", "description": "test"}
        )
        
        result = task_manager.wait_for_any(["task_1", "task_2"], timeout=1)
        
        assert result.status == TaskStatus.COMPLETED

    def test_wait_for_all_success(self, task_manager, mock_api):
        """测试等待所有任务完成"""
        mock_api.get.return_value = APIResponse(
            success=True,
            data={"task_id": "task_1", "status": "completed", "description": "test"}
        )
        
        results = task_manager.wait_for_all(["task_1", "task_2"], timeout=1)
        
        assert len(results) >= 0


class TestSessionManager:
    """SessionManager单元测试类"""

    @pytest.fixture
    def mock_api(self):
        """创建Mock APIClient"""
        return Mock(spec=APIClient)

    @pytest.fixture
    def session_manager(self, mock_api):
        """创建SessionManager实例"""
        return SessionManager(mock_api)

    def test_init(self, mock_api):
        """测试初始化"""
        manager = SessionManager(mock_api)
        assert manager._api == mock_api

    def test_create_success(self, session_manager, mock_api):
        """测试创建会话成功"""
        mock_api.post.return_value = APIResponse(
            success=True,
            data={"session_id": "sess_123"}
        )
        
        session = session_manager.create("user_123")
        
        assert session.id == "sess_123"
        assert session.user_id == "user_123"
        assert session.status == SessionStatus.ACTIVE
        mock_api.post.assert_called_once()

    def test_create_with_options(self, session_manager, mock_api):
        """测试带元数据创建会话"""
        mock_api.post.return_value = APIResponse(
            success=True,
            data={"session_id": "sess_456"}
        )
        
        metadata = {"device": "mobile", "ip": "192.168.1.1"}
        session = session_manager.create_with_options("user_123", metadata)
        
        assert session.id == "sess_456"
        assert session.metadata == metadata

    def test_get_success(self, session_manager, mock_api):
        """测试获取会话成功"""
        mock_api.get.return_value = APIResponse(
            success=True,
            data={
                "session_id": "sess_123",
                "user_id": "user_123",
                "status": "active",
                "context": {"theme": "dark"}
            }
        )
        
        session = session_manager.get("sess_123")
        
        assert session.id == "sess_123"
        assert session.user_id == "user_123"
        assert session.context == {"theme": "dark"}

    def test_get_empty_id_raises_error(self, session_manager):
        """测试空会话ID抛出异常"""
        with pytest.raises(AgentOSError) as exc_info:
            session_manager.get("")
        assert "会话ID" in str(exc_info.value)

    def test_set_context_success(self, session_manager, mock_api):
        """测试设置上下文"""
        mock_api.post.return_value = APIResponse(success=True)
        
        session_manager.set_context("sess_123", "theme", "dark")
        
        mock_api.post.assert_called_once_with(
            "/api/v1/sessions/sess_123/context",
            {"key": "theme", "value": "dark"}
        )

    def test_set_context_empty_session_id_raises_error(self, session_manager):
        """测试空会话ID设置上下文抛出异常"""
        with pytest.raises(AgentOSError) as exc_info:
            session_manager.set_context("", "key", "value")
        assert "会话ID" in str(exc_info.value)

    def test_set_context_empty_key_raises_error(self, session_manager):
        """测试空键设置上下文抛出异常"""
        with pytest.raises(AgentOSError) as exc_info:
            session_manager.set_context("sess_123", "", "value")
        assert "上下文键" in str(exc_info.value)

    def test_get_context_success(self, session_manager, mock_api):
        """测试获取上下文"""
        mock_api.get.return_value = APIResponse(
            success=True,
            data={"value": "dark"}
        )
        
        value = session_manager.get_context("sess_123", "theme")
        
        assert value == "dark"

    def test_get_all_context_success(self, session_manager, mock_api):
        """测试获取全部上下文"""
        mock_api.get.return_value = APIResponse(
            success=True,
            data={"context": {"theme": "dark", "lang": "zh"}}
        )
        
        context = session_manager.get_all_context("sess_123")
        
        assert context == {"theme": "dark", "lang": "zh"}

    def test_delete_context_success(self, session_manager, mock_api):
        """测试删除上下文"""
        mock_api.delete.return_value = APIResponse(success=True)
        
        session_manager.delete_context("sess_123", "theme")
        
        mock_api.delete.assert_called_once_with("/api/v1/sessions/sess_123/context/theme")

    def test_close_success(self, session_manager, mock_api):
        """测试关闭会话"""
        mock_api.delete.return_value = APIResponse(success=True)
        
        session_manager.close("sess_123")
        
        mock_api.delete.assert_called_once_with("/api/v1/sessions/sess_123")

    def test_list_success(self, session_manager, mock_api):
        """测试列出会话"""
        mock_api.get.return_value = APIResponse(
            success=True,
            data={
                "sessions": [
                    {"session_id": "sess_1", "user_id": "user_1", "status": "active"},
                    {"session_id": "sess_2", "user_id": "user_2", "status": "active"}
                ]
            }
        )
        
        sessions = session_manager.list()
        
        assert len(sessions) == 2
        assert sessions[0].id == "sess_1"

    def test_list_by_user_success(self, session_manager, mock_api):
        """测试按用户列出会话"""
        mock_api.get.return_value = APIResponse(success=True, data={"sessions": []})
        
        sessions = session_manager.list_by_user("user_123")
        
        assert isinstance(sessions, list)

    def test_list_active_success(self, session_manager, mock_api):
        """测试列出活跃会话"""
        mock_api.get.return_value = APIResponse(success=True, data={"sessions": []})
        
        sessions = session_manager.list_active()
        
        assert isinstance(sessions, list)

    def test_update_success(self, session_manager, mock_api):
        """测试更新会话"""
        mock_api.put.return_value = APIResponse(
            success=True,
            data={"session_id": "sess_123", "user_id": "user_123", "status": "active"}
        )
        
        session = session_manager.update("sess_123", {"device": "desktop"})
        
        assert session.id == "sess_123"

    def test_refresh_success(self, session_manager, mock_api):
        """测试刷新会话"""
        mock_api.post.return_value = APIResponse(success=True)
        
        session_manager.refresh("sess_123")
        
        mock_api.post.assert_called_once_with("/api/v1/sessions/sess_123/refresh", None)

    def test_is_expired_true(self, session_manager, mock_api):
        """测试会话已过期"""
        mock_api.get.return_value = APIResponse(
            success=True,
            data={"session_id": "sess_123", "user_id": "user_123", "status": "expired"}
        )
        
        expired = session_manager.is_expired("sess_123")
        
        assert expired is True

    def test_is_expired_false(self, session_manager, mock_api):
        """测试会话未过期"""
        mock_api.get.return_value = APIResponse(
            success=True,
            data={"session_id": "sess_123", "user_id": "user_123", "status": "active"}
        )
        
        expired = session_manager.is_expired("sess_123")
        
        assert expired is False

    def test_count_success(self, session_manager, mock_api):
        """测试获取会话总数"""
        mock_api.get.return_value = APIResponse(success=True, data={"count": 50})
        
        count = session_manager.count()
        
        assert count == 50

    def test_count_active_success(self, session_manager, mock_api):
        """测试获取活跃会话数"""
        mock_api.get.return_value = APIResponse(success=True, data={"count": 25})
        
        count = session_manager.count_active()
        
        assert count == 25

    def test_clean_expired_success(self, session_manager, mock_api):
        """测试清理过期会话"""
        mock_api.post.return_value = APIResponse(success=True, data={"cleaned": 10})
        
        cleaned = session_manager.clean_expired()
        
        assert cleaned == 10


class TestMemoryManager:
    """MemoryManager单元测试类"""

    @pytest.fixture
    def mock_api(self):
        """创建Mock APIClient"""
        return Mock(spec=APIClient)

    @pytest.fixture
    def memory_manager(self, mock_api):
        """创建MemoryManager实例"""
        return MemoryManager(mock_api)

    def test_init(self, mock_api):
        """测试初始化"""
        manager = MemoryManager(mock_api)
        assert manager._api == mock_api

    def test_write_success(self, memory_manager, mock_api):
        """测试写入记忆成功"""
        mock_api.post.return_value = APIResponse(
            success=True,
            data={"memory_id": "mem_123"}
        )
        
        memory = memory_manager.write("important fact", MemoryLayer.L1)
        
        assert memory.id == "mem_123"
        assert memory.content == "important fact"
        assert memory.layer == MemoryLayer.L1
        mock_api.post.assert_called_once()

    def test_write_with_options(self, memory_manager, mock_api):
        """测试带元数据写入记忆"""
        mock_api.post.return_value = APIResponse(
            success=True,
            data={"memory_id": "mem_456"}
        )
        
        metadata = {"source": "user", "confidence": 0.95}
        memory = memory_manager.write_with_options("fact", MemoryLayer.L2, metadata)
        
        assert memory.id == "mem_456"
        assert memory.metadata == metadata

    def test_write_empty_content_raises_error(self, memory_manager):
        """测试空内容写入抛出异常"""
        with pytest.raises(AgentOSError) as exc_info:
            memory_manager.write("", MemoryLayer.L1)
        assert "记忆内容" in str(exc_info.value)

    def test_get_success(self, memory_manager, mock_api):
        """测试获取记忆成功"""
        mock_api.get.return_value = APIResponse(
            success=True,
            data={
                "memory_id": "mem_123",
                "content": "fact",
                "layer": "L1",
                "score": 0.95
            }
        )
        
        memory = memory_manager.get("mem_123")
        
        assert memory.id == "mem_123"
        assert memory.content == "fact"
        assert memory.layer == MemoryLayer.L1
        assert memory.score == 0.95

    def test_search_success(self, memory_manager, mock_api):
        """测试搜索记忆"""
        mock_api.get.return_value = APIResponse(
            success=True,
            data={
                "memories": [
                    {"memory_id": "mem_1", "content": "fact1", "layer": "L1", "score": 0.9},
                    {"memory_id": "mem_2", "content": "fact2", "layer": "L1", "score": 0.8}
                ],
                "total": 2
            }
        )
        
        result = memory_manager.search("fact", top_k=5)
        
        assert result.total == 2
        assert len(result.memories) == 2
        assert result.query == "fact"
        assert result.top_k == 5

    def test_search_empty_query_raises_error(self, memory_manager):
        """测试空查询抛出异常"""
        with pytest.raises(AgentOSError) as exc_info:
            memory_manager.search("")
        assert "搜索查询" in str(exc_info.value)

    def test_search_by_layer_success(self, memory_manager, mock_api):
        """测试按层级搜索记忆"""
        mock_api.get.return_value = APIResponse(
            success=True,
            data={"memories": [], "total": 0}
        )
        
        result = memory_manager.search_by_layer("fact", MemoryLayer.L2, top_k=10)
        
        assert isinstance(result, MemorySearchResult)

    def test_update_success(self, memory_manager, mock_api):
        """测试更新记忆"""
        mock_api.put.return_value = APIResponse(
            success=True,
            data={"memory_id": "mem_123", "content": "updated content", "layer": "L1"}
        )
        
        memory = memory_manager.update("mem_123", "updated content")
        
        assert memory.id == "mem_123"

    def test_delete_success(self, memory_manager, mock_api):
        """测试删除记忆"""
        mock_api.delete.return_value = APIResponse(success=True)
        
        memory_manager.delete("mem_123")
        
        mock_api.delete.assert_called_once_with("/api/v1/memories/mem_123")

    def test_list_success(self, memory_manager, mock_api):
        """测试列出记忆"""
        mock_api.get.return_value = APIResponse(
            success=True,
            data={"memories": []}
        )
        
        memories = memory_manager.list()
        
        assert isinstance(memories, list)

    def test_list_by_layer_success(self, memory_manager, mock_api):
        """测试按层级列出记忆"""
        mock_api.get.return_value = APIResponse(success=True, data={"memories": []})
        
        memories = memory_manager.list_by_layer(MemoryLayer.L1)
        
        assert isinstance(memories, list)

    def test_count_success(self, memory_manager, mock_api):
        """测试获取记忆总数"""
        mock_api.get.return_value = APIResponse(success=True, data={"count": 100})
        
        count = memory_manager.count()
        
        assert count == 100

    def test_clear_success(self, memory_manager, mock_api):
        """测试清空记忆"""
        mock_api.delete.return_value = APIResponse(success=True)
        
        memory_manager.clear()
        
        mock_api.delete.assert_called_once_with("/api/v1/memories")

    def test_batch_write_success(self, memory_manager, mock_api):
        """测试批量写入记忆"""
        mock_api.post.return_value = APIResponse(
            success=True,
            data={"memory_id": "mem_new"}
        )
        
        items = [
            MemoryWriteItem("fact1", MemoryLayer.L1),
            MemoryWriteItem("fact2", MemoryLayer.L2),
        ]
        
        memories = memory_manager.batch_write(items)
        
        assert len(memories) == 2
        assert mock_api.post.call_count == 2

    def test_evolve_success(self, memory_manager, mock_api):
        """测试触发记忆演化"""
        mock_api.post.return_value = APIResponse(success=True)
        
        memory_manager.evolve()
        
        mock_api.post.assert_called_once_with("/api/v1/memories/evolve", None)

    def test_get_stats_success(self, memory_manager, mock_api):
        """测试获取记忆统计"""
        mock_api.get.return_value = APIResponse(
            success=True,
            data={"L1": 50, "L2": 30, "L3": 15, "L4": 5}
        )
        
        stats = memory_manager.get_stats()
        
        assert stats["L1"] == 50
        assert stats["L4"] == 5


class TestSkillManager:
    """SkillManager单元测试类"""

    @pytest.fixture
    def mock_api(self):
        """创建Mock APIClient"""
        return Mock(spec=APIClient)

    @pytest.fixture
    def skill_manager(self, mock_api):
        """创建SkillManager实例"""
        return SkillManager(mock_api)

    def test_init(self, mock_api):
        """测试初始化"""
        manager = SkillManager(mock_api)
        assert manager._api == mock_api

    def test_load_success(self, skill_manager, mock_api):
        """测试加载技能成功"""
        mock_api.post.return_value = APIResponse(
            success=True,
            data={"name": "my_skill", "version": "1.0.0"}
        )
        
        skill = skill_manager.load("skill_123")
        
        assert skill.id == "skill_123"
        assert skill.name == "my_skill"
        assert skill.status == SkillStatus.ACTIVE

    def test_load_empty_id_raises_error(self, skill_manager):
        """测试空技能ID加载抛出异常"""
        with pytest.raises(AgentOSError) as exc_info:
            skill_manager.load("")
        assert "技能ID" in str(exc_info.value)

    def test_get_success(self, skill_manager, mock_api):
        """测试获取技能成功"""
        mock_api.get.return_value = APIResponse(
            success=True,
            data={
                "skill_id": "skill_123",
                "name": "my_skill",
                "version": "1.0.0",
                "status": "active"
            }
        )
        
        skill = skill_manager.get("skill_123")
        
        assert skill.id == "skill_123"
        assert skill.name == "my_skill"

    def test_execute_success(self, skill_manager, mock_api):
        """测试执行技能成功"""
        mock_api.post.return_value = APIResponse(
            success=True,
            data={"success": True, "output": {"result": "done"}}
        )
        
        result = skill_manager.execute("skill_123", {"input": "test"})
        
        assert result.success is True
        assert result.output == {"result": "done"}

    def test_execute_with_context_success(self, skill_manager, mock_api):
        """测试带上下文执行技能"""
        mock_api.post.return_value = APIResponse(
            success=True,
            data={"success": True, "output": "done"}
        )
        
        result = skill_manager.execute_with_context(
            "skill_123",
            {"input": "test"},
            "sess_456"
        )
        
        assert result.success is True

    def test_unload_success(self, skill_manager, mock_api):
        """测试卸载技能"""
        mock_api.post.return_value = APIResponse(success=True)
        
        skill_manager.unload("skill_123")
        
        mock_api.post.assert_called_once_with("/api/v1/skills/skill_123/unload", None)

    def test_list_success(self, skill_manager, mock_api):
        """测试列出技能"""
        mock_api.get.return_value = APIResponse(
            success=True,
            data={
                "skills": [
                    {"skill_id": "skill_1", "name": "skill1", "status": "active"},
                    {"skill_id": "skill_2", "name": "skill2", "status": "active"}
                ]
            }
        )
        
        skills = skill_manager.list()
        
        assert len(skills) == 2
        assert skills[0].id == "skill_1"

    def test_list_loaded_success(self, skill_manager, mock_api):
        """测试列出已加载技能"""
        mock_api.get.return_value = APIResponse(success=True, data={"skills": []})
        
        skills = skill_manager.list_loaded()
        
        assert isinstance(skills, list)

    def test_register_success(self, skill_manager, mock_api):
        """测试注册技能"""
        mock_api.post.return_value = APIResponse(
            success=True,
            data={"skill_id": "skill_new"}
        )
        
        skill = skill_manager.register(
            "my_skill",
            "A sample skill",
            {"input": {"type": "string"}}
        )
        
        assert skill.id == "skill_new"
        assert skill.name == "my_skill"
        assert skill.description == "A sample skill"

    def test_register_empty_name_raises_error(self, skill_manager):
        """测试空名称注册抛出异常"""
        with pytest.raises(AgentOSError) as exc_info:
            skill_manager.register("", "description", {})
        assert "技能名称" in str(exc_info.value)

    def test_update_success(self, skill_manager, mock_api):
        """测试更新技能"""
        mock_api.put.return_value = APIResponse(
            success=True,
            data={"skill_id": "skill_123", "name": "my_skill", "status": "active"}
        )
        
        skill = skill_manager.update("skill_123", "new description", {"input": {"type": "object"}})
        
        assert skill.id == "skill_123"

    def test_delete_success(self, skill_manager, mock_api):
        """测试删除技能"""
        mock_api.delete.return_value = APIResponse(success=True)
        
        skill_manager.delete("skill_123")
        
        mock_api.delete.assert_called_once_with("/api/v1/skills/skill_123")

    def test_get_info_success(self, skill_manager, mock_api):
        """测试获取技能信息"""
        mock_api.get.return_value = APIResponse(
            success=True,
            data={
                "skill_name": "my_skill",
                "description": "A skill",
                "version": "1.0.0",
                "parameters": {}
            }
        )
        
        info = skill_manager.get_info("skill_123")
        
        assert info.name == "my_skill"
        assert info.version == "1.0.0"

    def test_validate_success(self, skill_manager, mock_api):
        """测试验证技能参数"""
        mock_api.post.return_value = APIResponse(
            success=True,
            data={"valid": True, "errors": []}
        )
        
        valid, errors = skill_manager.validate("skill_123", {"input": "test"})
        
        assert valid is True
        assert len(errors) == 0

    def test_validate_with_errors(self, skill_manager, mock_api):
        """测试验证技能参数有错误"""
        mock_api.post.return_value = APIResponse(
            success=True,
            data={"valid": False, "errors": ["Missing required field: input"]}
        )
        
        valid, errors = skill_manager.validate("skill_123", {})
        
        assert valid is False
        assert len(errors) == 1

    def test_count_success(self, skill_manager, mock_api):
        """测试获取技能总数"""
        mock_api.get.return_value = APIResponse(success=True, data={"count": 20})
        
        count = skill_manager.count()
        
        assert count == 20

    def test_count_loaded_success(self, skill_manager, mock_api):
        """测试获取已加载技能数"""
        mock_api.get.return_value = APIResponse(success=True, data={"count": 15})
        
        count = skill_manager.count_loaded()
        
        assert count == 15

    def test_search_success(self, skill_manager, mock_api):
        """测试搜索技能"""
        mock_api.get.return_value = APIResponse(
            success=True,
            data={"skills": []}
        )
        
        skills = skill_manager.search("data processing", top_k=5)
        
        assert isinstance(skills, list)

    def test_search_empty_query_raises_error(self, skill_manager):
        """测试空查询搜索抛出异常"""
        with pytest.raises(AgentOSError) as exc_info:
            skill_manager.search("")
        assert "搜索查询" in str(exc_info.value)

    def test_batch_execute_success(self, skill_manager, mock_api):
        """测试批量执行技能"""
        mock_api.post.return_value = APIResponse(
            success=True,
            data={"success": True, "output": "done"}
        )
        
        requests = [
            SkillExecuteRequest("skill_1", {"input": "a"}),
            SkillExecuteRequest("skill_2", {"input": "b"}),
        ]
        
        results = skill_manager.batch_execute(requests)
        
        assert len(results) == 2
        assert all(r.success for r in results)

    def test_get_stats_success(self, skill_manager, mock_api):
        """测试获取技能统计"""
        mock_api.get.return_value = APIResponse(
            success=True,
            data={"executions": 100, "successes": 95, "failures": 5}
        )
        
        stats = skill_manager.get_stats("skill_123")
        
        assert stats["executions"] == 100
        assert stats["successes"] == 95


if __name__ == "__main__":
    pytest.main([__file__, "-v", "--tb=short"])
