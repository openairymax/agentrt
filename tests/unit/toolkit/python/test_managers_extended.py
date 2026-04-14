# AgentOS Python SDK - Manager Classes Extended Tests
# Version: 1.0.0
# Last updated: 2026-04-05
# Author: Spharx AgentOS Team

"""
Manager类的扩展测试，补充边界条件、异常路径和并发场景测试。

This module contains extended unit tests for TaskManager, SessionManager,
MemoryManager, and SkillManager classes with focus on:
- Boundary conditions (边界条件)
- Error paths (异常路径)
- Concurrent scenarios (并发场景)
- Mock isolation (Mock隔离)

遵循 ARCHITECTURAL_PRINCIPLES.md 的 E-8（可测试性原则）。

Run with: pytest tests/test_managers_extended.py -v --cov=agentos.modules --cov-report=term-missing
"""

import pytest
from unittest.mock import Mock, MagicMock, patch, call
from datetime import datetime, timedelta
import threading
import time
import sys
import os

# 添加父目录到路径
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

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


# ============================================================================
# TaskManager 扩展测试
# ============================================================================

class TestTaskManagerBoundaryConditions:
    """TaskManager边界条件测试"""

    @pytest.fixture
    def mock_api(self):
        """创建Mock APIClient"""
        return Mock(spec=APIClient)

    @pytest.fixture
    def task_manager(self, mock_api):
        """创建TaskManager实例"""
        return TaskManager(mock_api)

    def test_submit_with_very_long_description(self, task_manager, mock_api):
        """测试超长任务描述"""
        long_desc = "test " * 10000  # 50000字符
        mock_api.post.return_value = APIResponse(
            success=True,
            data={"task_id": "task_long", "status": "pending", "description": long_desc[:100]}
        )
        
        task = task_manager.submit(long_desc)
        
        assert task is not None
        assert task.id == "task_long"

    def test_submit_with_special_characters(self, task_manager, mock_api):
        """测试特殊字符任务描述"""
        special_descs = [
            "task with emoji 🎉",
            "task\nwith\nnewlines",
            "task\twith\ttabs",
            'task with "quotes"',
            "task with <html> tags</html>",
            "task with 'single quotes'",
            "task with \\ backslash",
            "task with / forward slash",
        ]
        
        mock_api.post.return_value = APIResponse(
            success=True,
            data={"task_id": "task_special", "status": "pending"}
        )
        
        for desc in special_descs:
            task = task_manager.submit(desc)
            assert task is not None

    def test_submit_with_unicode_content(self, task_manager, mock_api):
        """测试Unicode内容"""
        unicode_descs = [
            "任务 with 中文",
            "タスク with 日本語",
            "작업 with 한국어",
            "مهمة with العربية",
            "משימה with עברית",
        ]
        
        mock_api.post.return_value = APIResponse(
            success=True,
            data={"task_id": "task_unicode", "status": "pending"}
        )
        
        for desc in unicode_descs:
            task = task_manager.submit(desc)
            assert task is not None

    def test_concurrent_submissions(self, task_manager, mock_api):
        """测试并发任务提交"""
        results = []
        errors = []
        
        def submit_task(i):
            try:
                mock_api.post.return_value = APIResponse(
                    success=True,
                    data={"task_id": f"task_{i}", "status": "pending"}
                )
                task = task_manager.submit(f"concurrent-task-{i}")
                results.append(task)
            except Exception as e:
                errors.append(e)
        
        threads = [
            threading.Thread(target=submit_task, args=(i,))
            for i in range(10)
        ]
        
        for t in threads:
            t.start()
        for t in threads:
            t.join()
        
        assert len(results) == 10
        assert len(errors) == 0

    def test_submit_with_none_description(self, task_manager):
        """测试None任务描述"""
        with pytest.raises((AgentOSError, TypeError)):
            task_manager.submit(None)

    def test_get_with_none_id(self, task_manager):
        """测试None任务ID"""
        with pytest.raises((AgentOSError, TypeError)):
            task_manager.get(None)

    def test_cancel_with_none_id(self, task_manager):
        """测试None任务ID取消"""
        with pytest.raises((AgentOSError, TypeError)):
            task_manager.cancel(None)


class TestTaskManagerErrorPaths:
    """TaskManager异常路径测试"""

    @pytest.fixture
    def mock_api(self):
        """创建Mock APIClient"""
        return Mock(spec=APIClient)

    @pytest.fixture
    def task_manager(self, mock_api):
        """创建TaskManager实例"""
        return TaskManager(mock_api)

    def test_api_connection_timeout(self, task_manager, mock_api):
        """测试API连接超时"""
        import requests.exceptions
        
        mock_api.post.side_effect = requests.exceptions.Timeout()
        
        with pytest.raises(AgentOSError) as exc_info:
            task_manager.submit("timeout test")
        
        assert "timeout" in str(exc_info.value).lower() or "连接" in str(exc_info.value).lower() or "超时" in str(exc_info.value)

    def test_api_server_error(self, task_manager, mock_api):
        """测试API服务器错误"""
        mock_api.post.return_value = APIResponse(
            success=False,
            error={"code": 500, "message": "Internal Server Error"}
        )
        
        with pytest.raises(AgentOSError):
            task_manager.submit("server error test")

    def test_api_not_found_error(self, task_manager, mock_api):
        """测试API 404错误"""
        mock_api.get.return_value = APIResponse(
            success=False,
            error={"code": 404, "message": "Task not found"}
        )
        
        with pytest.raises(AgentOSError):
            task_manager.get("nonexistent_task")

    def test_malformed_response(self, task_manager, mock_api):
        """测试畸形响应处理"""
        mock_api.post.return_value = {"invalid": "format"}  # 不是APIResponse
        
        # 应该抛出异常或返回None
        with pytest.raises((AgentOSError, AttributeError, TypeError)):
            task_manager.submit("malformed response")

    def test_empty_response_data(self, task_manager, mock_api):
        """测试空响应数据"""
        mock_api.post.return_value = APIResponse(success=True, data=None)
        
        with pytest.raises((AgentOSError, TypeError, KeyError)):
            task_manager.submit("empty response")

    def test_invalid_task_status(self, task_manager, mock_api):
        """测试无效任务状态"""
        mock_api.get.return_value = APIResponse(
            success=True,
            data={"task_id": "task_123", "status": "INVALID_STATUS", "description": "test"}
        )
        
        task = task_manager.get("task_123")
        
        # 应该能处理未知状态
        assert task.id == "task_123"


class TestTaskManagerMockIsolation:
    """TaskManager Mock隔离测试"""

    @pytest.fixture
    def isolated_task_manager(self):
        """完全隔离的TaskManager实例"""
        with patch('agentos.modules.task.manager.APIClient') as MockClient:
            mock_client = MockClient.return_value
            mock_client.post = MagicMock()
            mock_client.get = MagicMock()
            mock_client.put = MagicMock()
            mock_client.delete = MagicMock()
            
            from agentos.modules.task.manager import TaskManager
            mgr = TaskManager(client=mock_client)
            yield mgr, mock_client

    def test_independent_instance_creation(self, isolated_task_manager):
        """独立实例创建"""
        mgr, _ = isolated_task_manager
        assert mgr is not None

    def test_mock_api_not_called_on_creation(self, isolated_task_manager):
        """创建时不调用API"""
        _, mock_client = isolated_task_manager
        mock_client.post.assert_not_called()
        mock_client.get.assert_not_called()

    def test_submit_calls_post_once(self, isolated_task_manager):
        """提交任务调用post一次"""
        mgr, mock_client = isolated_task_manager
        
        mock_client.post.return_value = APIResponse(
            success=True,
            data={"task_id": "task_123", "status": "pending"}
        )
        
        mgr.submit("test task")
        
        mock_client.post.assert_called_once()

    def test_get_calls_get_once(self, isolated_task_manager):
        """获取任务调用get一次"""
        mgr, mock_client = isolated_task_manager
        
        mock_client.get.return_value = APIResponse(
            success=True,
            data={"task_id": "task_123", "status": "pending"}
        )
        
        mgr.get("task_123")
        
        mock_client.get.assert_called_once()


# ============================================================================
# SessionManager 扩展测试
# ============================================================================

class TestSessionManagerBoundaryConditions:
    """SessionManager边界条件测试"""

    @pytest.fixture
    def mock_api(self):
        """创建Mock APIClient"""
        return Mock(spec=APIClient)

    @pytest.fixture
    def session_manager(self, mock_api):
        """创建SessionManager实例"""
        return SessionManager(mock_api)

    def test_rapid_create_delete_cycle(self, session_manager, mock_api):
        """快速创建删除循环"""
        mock_api.post.return_value = APIResponse(
            success=True,
            data={"session_id": "sess_temp"}
        )
        mock_api.delete.return_value = APIResponse(success=True)
        
        for i in range(100):
            session = session_manager.create(f"user_{i}")
            session_manager.close(session.id)
        
        assert mock_api.post.call_count == 100
        assert mock_api.delete.call_count == 100

    def test_context_with_none_values(self, session_manager, mock_api):
        """Context包含None值"""
        mock_api.post.return_value = APIResponse(success=True)
        mock_api.get.return_value = APIResponse(success=True, data={"value": None})
        
        session_manager.set_context("sess_123", "key1", None)
        context = session_manager.get_context("sess_123", "key1")
        
        assert context is None

    def test_context_with_complex_objects(self, session_manager, mock_api):
        """Context包含复杂对象"""
        mock_api.post.return_value = APIResponse(success=True)
        
        complex_context = {
            "nested": {"key": "value"},
            "list": [1, 2, 3],
            "mixed": {"a": 1, "b": [2, 3]}
        }
        
        session_manager.set_context("sess_123", "complex", complex_context)
        
        mock_api.post.assert_called_once()

    def test_create_with_empty_user_id(self, session_manager):
        """测试空用户ID创建会话"""
        with pytest.raises(AgentOSError) as exc_info:
            session_manager.create("")
        assert "用户ID" in str(exc_info.value)

    def test_create_with_special_characters_in_user_id(self, session_manager, mock_api):
        """测试特殊字符用户ID"""
        mock_api.post.return_value = APIResponse(
            success=True,
            data={"session_id": "sess_special"}
        )
        
        special_user_ids = [
            "user@email.com",
            "user+tag",
            "user_name",
            "用户 ID",
            "ユーザー ID",
        ]
        
        for user_id in special_user_ids:
            session = session_manager.create(user_id)
            assert session is not None


class TestSessionManagerErrorPaths:
    """SessionManager异常路径测试"""

    @pytest.fixture
    def mock_api(self):
        """创建Mock APIClient"""
        return Mock(spec=APIClient)

    @pytest.fixture
    def session_manager(self, mock_api):
        """创建SessionManager实例"""
        return SessionManager(mock_api)

    def test_api_connection_refused(self, session_manager, mock_api):
        """测试API连接拒绝"""
        import requests.exceptions
        
        mock_api.post.side_effect = requests.exceptions.ConnectionError()
        
        with pytest.raises(AgentOSError):
            session_manager.create("user_123")

    def test_get_nonexistent_session(self, session_manager, mock_api):
        """测试获取不存在的会话"""
        mock_api.get.return_value = APIResponse(
            success=False,
            error={"code": 404, "message": "Session not found"}
        )
        
        with pytest.raises(AgentOSError):
            session_manager.get("nonexistent_session")

    def test_set_context_on_closed_session(self, session_manager, mock_api):
        """测试在已关闭会话上设置上下文"""
        mock_api.get.return_value = APIResponse(
            success=True,
            data={"session_id": "sess_123", "status": "closed"}
        )
        mock_api.post.return_value = APIResponse(
            success=False,
            error={"code": 400, "message": "Session is closed"}
        )
        
        # 先获取会话（显示已关闭）
        session = session_manager.get("sess_123")
        assert session.status == SessionStatus.CLOSED
        
        # 尝试设置上下文应该失败
        with pytest.raises(AgentOSError):
            session_manager.set_context("sess_123", "key", "value")


# ============================================================================
# MemoryManager 扩展测试
# ============================================================================

class TestMemoryManagerBoundaryConditions:
    """MemoryManager边界条件测试"""

    @pytest.fixture
    def mock_api(self):
        """创建Mock APIClient"""
        return Mock(spec=APIClient)

    @pytest.fixture
    def memory_manager(self, mock_api):
        """创建MemoryManager实例"""
        return MemoryManager(mock_api)

    def test_write_with_very_long_content(self, memory_manager, mock_api):
        """测试超长记忆内容"""
        long_content = "fact " * 10000  # 50000字符
        
        mock_api.post.return_value = APIResponse(
            success=True,
            data={"memory_id": "mem_long"}
        )
        
        memory = memory_manager.write(long_content, MemoryLayer.L1)
        
        assert memory is not None
        assert memory.id == "mem_long"

    def test_write_with_unicode_content(self, memory_manager, mock_api):
        """测试Unicode记忆内容"""
        unicode_contents = [
            "记忆 with 中文",
            "メモリ with 日本語",
            "메모리 with 한국어",
        ]
        
        mock_api.post.return_value = APIResponse(
            success=True,
            data={"memory_id": "mem_unicode"}
        )
        
        for content in unicode_contents:
            memory = memory_manager.write(content, MemoryLayer.L1)
            assert memory is not None

    def test_search_with_special_characters(self, memory_manager, mock_api):
        """测试特殊字符搜索"""
        mock_api.get.return_value = APIResponse(
            success=True,
            data={"memories": [], "total": 0}
        )
        
        special_queries = [
            "query with spaces",
            "query\nwith\nnewlines",
            "query\twith\ttabs",
            "query with \"quotes\"",
            "查询 with 中文",
        ]
        
        for query in special_queries:
            result = memory_manager.search(query)
            assert isinstance(result, MemorySearchResult)

    def test_batch_write_empty_list(self, memory_manager, mock_api):
        """测试批量写入空列表"""
        memories = memory_manager.batch_write([])
        
        assert len(memories) == 0
        mock_api.post.assert_not_called()

    def test_search_top_k_zero(self, memory_manager, mock_api):
        """测试top_k=0的搜索"""
        mock_api.get.return_value = APIResponse(
            success=True,
            data={"memories": [], "total": 0}
        )
        
        result = memory_manager.search("query", top_k=0)
        
        assert isinstance(result, MemorySearchResult)
        assert result.total == 0


class TestMemoryManagerErrorPaths:
    """MemoryManager异常路径测试"""

    @pytest.fixture
    def mock_api(self):
        """创建Mock APIClient"""
        return Mock(spec=APIClient)

    @pytest.fixture
    def memory_manager(self, mock_api):
        """创建MemoryManager实例"""
        return MemoryManager(mock_api)

    def test_api_timeout(self, memory_manager, mock_api):
        """测试API超时"""
        import requests.exceptions
        
        mock_api.post.side_effect = requests.exceptions.Timeout()
        
        with pytest.raises(AgentOSError):
            memory_manager.write("test content", MemoryLayer.L1)

    def test_search_invalid_layer(self, memory_manager, mock_api):
        """测试无效层级"""
        mock_api.get.return_value = APIResponse(
            success=False,
            error={"code": 400, "message": "Invalid layer"}
        )
        
        # 使用无效的层级
        with pytest.raises(AgentOSError):
            memory_manager.search_by_layer("query", "INVALID_LAYER")

    def test_write_invalid_layer(self, memory_manager, mock_api):
        """测试写入无效层级"""
        mock_api.post.return_value = APIResponse(
            success=False,
            error={"code": 400, "message": "Invalid layer"}
        )
        
        # 尝试写入无效层级
        with pytest.raises(AgentOSError):
            memory_manager.write("content", "INVALID_LAYER")


# ============================================================================
# SkillManager 扩展测试
# ============================================================================

class TestSkillManagerBoundaryConditions:
    """SkillManager边界条件测试"""

    @pytest.fixture
    def mock_api(self):
        """创建Mock APIClient"""
        return Mock(spec=APIClient)

    @pytest.fixture
    def skill_manager(self, mock_api):
        """创建SkillManager实例"""
        return SkillManager(mock_api)

    def test_load_with_special_characters_in_name(self, skill_manager, mock_api):
        """测试特殊字符技能名"""
        mock_api.get.return_value = APIResponse(
            success=True,
            data={"skill_id": "skill_special", "name": "test/skill", "status": "loaded"}
        )
        
        skill = skill_manager.load("test/skill")
        
        assert skill is not None
        assert skill.name == "test/skill"

    def test_execute_with_complex_input(self, skill_manager, mock_api):
        """测试复杂输入执行"""
        mock_api.post.return_value = APIResponse(
            success=True,
            data={"result": "success", "output": {"nested": {"key": "value"}}}
        )
        
        complex_input = {
            "nested": {"key": "value"},
            "list": [1, 2, 3],
            "mixed": {"a": 1, "b": [2, 3]}
        }
        
        result = skill_manager.execute("skill_123", complex_input)
        
        assert result is not None

    def test_batch_execute_empty_list(self, skill_manager, mock_api):
        """测试批量执行空列表"""
        results = skill_manager.batch_execute([])
        
        assert len(results) == 0


class TestSkillManagerErrorPaths:
    """SkillManager异常路径测试"""

    @pytest.fixture
    def mock_api(self):
        """创建Mock APIClient"""
        return Mock(spec=APIClient)

    @pytest.fixture
    def skill_manager(self, mock_api):
        """创建SkillManager实例"""
        return SkillManager(mock_api)

    def test_load_nonexistent_skill(self, skill_manager, mock_api):
        """测试加载不存在的技能"""
        mock_api.get.return_value = APIResponse(
            success=False,
            error={"code": 404, "message": "Skill not found"}
        )
        
        with pytest.raises(AgentOSError):
            skill_manager.load("nonexistent_skill")

    def test_execute_unloaded_skill(self, skill_manager, mock_api):
        """测试执行未加载的技能"""
        mock_api.post.return_value = APIResponse(
            success=False,
            error={"code": 400, "message": "Skill not loaded"}
        )
        
        with pytest.raises(AgentOSError):
            skill_manager.execute("unloaded_skill", {})

    def test_execute_with_invalid_input(self, skill_manager, mock_api):
        """测试无效输入执行"""
        mock_api.post.return_value = APIResponse(
            success=False,
            error={"code": 400, "message": "Invalid input"}
        )
        
        with pytest.raises(AgentOSError):
            skill_manager.execute("skill_123", None)


# ============================================================================
# 并发和性能测试
# ============================================================================

class TestConcurrentScenarios:
    """并发场景测试"""

    @pytest.fixture
    def mock_api(self):
        """创建Mock APIClient"""
        return Mock(spec=APIClient)

    def test_concurrent_task_and_session_operations(self, mock_api):
        """测试并发任务和会话操作"""
        from agentos.modules.task.manager import TaskManager
        from agentos.modules.session.manager import SessionManager
        
        task_mgr = TaskManager(mock_api)
        session_mgr = SessionManager(mock_api)
        
        mock_api.post.return_value = APIResponse(
            success=True,
            data={"task_id": "task_concurrent", "status": "pending"}
        )
        mock_api.get.return_value = APIResponse(
            success=True,
            data={"session_id": "sess_concurrent", "status": "active"}
        )
        
        results = []
        
        def create_task():
            task = task_mgr.submit("concurrent task")
            results.append(("task", task))
        
        def create_session():
            session = session_mgr.create("user_concurrent")
            results.append(("session", session))
        
        threads = []
        for i in range(5):
            t1 = threading.Thread(target=create_task)
            t2 = threading.Thread(target=create_session)
            threads.extend([t1, t2])
        
        for t in threads:
            t.start()
        for t in threads:
            t.join()
        
        assert len(results) == 10
        assert sum(1 for r in results if r[0] == "task") == 5
        assert sum(1 for r in results if r[0] == "session") == 5


# ============================================================================
# 主函数（用于直接运行测试）
# ============================================================================

if __name__ == "__main__":
    pytest.main([__file__, "-v", "--tb=short"])
