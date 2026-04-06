# AgentOS Python SDK - Task Manager Tests (Using BaseTestCase)
# Version: 3.0.0
# Last updated: 2026-04-05
#
# 使用 BaseTestCase 的任务管理器测试
# 演示如何通过基类消除重复代码

import pytest
from tests.base_test_case import BaseTestCase

from agentos.modules.task.manager import TaskManager
from agentos.types import TaskStatus
from agentos.exceptions import AgentOSError, CODE_MISSING_PARAMETER


class TestTaskManager(BaseTestCase):
    """TaskManager单元测试类 - 使用 BaseTestCase"""
    
    def test_submit_success(self):
        """测试任务提交成功"""
        mgr = TaskManager(self.mock_client, self.config)
        self.mock_client.post.return_value = self.create_task_response()
        
        result = mgr.submit("test task")
        
        assert result is not None
        assert result["id"] == "task-123"
        self.mock_client.post.assert_called_once()
    
    def test_submit_with_options(self):
        """测试带选项的任务提交"""
        mgr = TaskManager(self.mock_client, self.config)
        self.mock_client.post.return_value = self.create_task_response(
            task_id="task-456",
            status="pending"
        )
        
        result = mgr.submit("test task", priority=10)
        
        assert result is not None
        assert result["id"] == "task-456"
    
    def test_submit_empty_description_raises_error(self):
        """测试空任务描述抛出异常"""
        mgr = TaskManager(self.mock_client, self.config)
        
        with pytest.raises(AgentOSError) as exc_info:
            mgr.submit("")
        
        self.assert_error_code(exc_info.value, CODE_MISSING_PARAMETER)
    
    def test_get_success(self):
        """测试获取任务成功"""
        mgr = TaskManager(self.mock_client, self.config)
        self.mock_client.get.return_value = self.create_task_response(
            task_id="task-789",
            status="running"
        )
        
        result = mgr.get("task-789")
        
        assert result is not None
        assert result["id"] == "task-789"
        self.assert_api_called("get", "/api/v1/tasks/task-789")
    
    def test_list_success(self):
        """测试任务列表查询"""
        mgr = TaskManager(self.mock_client, self.config)
        
        tasks = [
            {"id": "task-1", "status": "pending"},
            {"id": "task-2", "status": "running"},
        ]
        self.mock_client.get.return_value = self.create_list_response(tasks, total=2)
        
        result = mgr.list()
        
        assert result is not None
        assert len(result["items"]) == 2
    
    def test_cancel_success(self):
        """测试任务取消"""
        mgr = TaskManager(self.mock_client, self.config)
        self.mock_client.delete.return_value = self.create_mock_response(200, {
            "success": True,
            "data": {"id": "task-123", "status": "cancelled"}
        })
        
        result = mgr.cancel("task-123")
        
        assert result is not None
        self.assert_api_called("delete", "/api/v1/tasks/task-123")


class TestTaskManagerErrorHandling(BaseTestCase):
    """TaskManager 错误处理测试"""
    
    def test_network_error(self):
        """测试网络错误处理"""
        mgr = TaskManager(self.mock_client, self.config)
        self.mock_client.post.side_effect = ConnectionError("网络连接失败")
        
        with pytest.raises(Exception):
            mgr.submit("test task")
    
    def test_timeout_error(self):
        """测试超时错误处理"""
        mgr = TaskManager(self.mock_client, self.config)
        self.mock_client.post.side_effect = TimeoutError("请求超时")
        
        with pytest.raises(Exception):
            mgr.submit("test task")
    
    def test_invalid_response(self):
        """测试无效响应处理"""
        mgr = TaskManager(self.mock_client, self.config)
        self.mock_client.post.return_value = self.create_mock_response(200, None)
        
        with pytest.raises(Exception):
            mgr.submit("test task")
    
    def test_server_error(self):
        """测试服务器错误处理"""
        mgr = TaskManager(self.mock_client, self.config)
        self.mock_client.post.return_value = self.create_error_response(
            error_code="0x000C",
            message="服务器内部错误",
            status_code=500
        )
        
        with pytest.raises(AgentOSError) as exc_info:
            mgr.submit("test task")
        
        self.assert_error_code(exc_info.value, "0x000C")
