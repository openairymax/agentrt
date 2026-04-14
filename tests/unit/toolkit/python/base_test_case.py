# AgentOS Python SDK - 测试基类
# Version: 3.0.0
# Last updated: 2026-04-05
#
# 提供统一的测试基类，消除重复初始化代码
# 遵循 DRY 原则和 ARCHITECTURAL_PRINCIPLES.md E-3（资源确定性）

import pytest
from unittest.mock import MagicMock, AsyncMock
from typing import Dict, Any, Optional
from datetime import datetime

from agentos.client.client import APIClient, APIResponse, manager
from agentos.exceptions import AgentOSError


class BaseTestCase:
    """
    统一测试基类，消除重复初始化代码
    
    遵循 ARCHITECTURAL_PRINCIPLES.md:
        - E-3 (资源确定性): fixture 生命周期明确
        - E-8 (可测试性): 提供统一的测试工具
        - A-1 (简约至上): 最小化测试代码
    
    Usage:
        class TestTaskManager(BaseTestCase):
            def test_submit(self):
                mgr = TaskManager(self.mock_client, self.config)
                self.mock_client.post.return_value = self.create_task_response()
                
                result = mgr.submit("test task")
                assert result is not None
    """
    
    @pytest.fixture(autouse=True)
    def setup_method(self):
        """
        每个测试方法前的统一初始化
        
        自动应用（autouse=True），无需在每个测试类中重复定义
        """
        self.mock_client = MagicMock(spec=APIClient)
        self.config = manager(
            endpoint="http://localhost:18789",
            timeout=30.0,
            max_retries=3,
            api_key="test-api-key"
        )
        self.test_timestamp = datetime(2026, 4, 5, 12, 0, 0)
    
    def create_mock_response(
        self, 
        status_code: int = 200, 
        json_data: Optional[Dict[str, Any]] = None,
        success: bool = True
    ) -> APIResponse:
        """
        创建标准 Mock 响应
        
        Args:
            status_code: HTTP 状态码
            json_data: JSON 数据
            success: 是否成功
        
        Returns:
            APIResponse: 标准响应对象
        """
        response = MagicMock()
        response.status_code = status_code
        response.json.return_value = json_data or {}
        response.success = success
        response.data = json_data.get("data", {}) if json_data else {}
        return response
    
    def create_task_response(
        self, 
        task_id: str = "task-123",
        status: str = "pending",
        description: str = "test task"
    ) -> APIResponse:
        """
        创建任务提交的标准成功响应
        
        Args:
            task_id: 任务ID
            status: 任务状态
            description: 任务描述
        
        Returns:
            APIResponse: 任务响应对象
        """
        return self.create_mock_response(200, {
            "success": True,
            "data": {
                "id": task_id,
                "task_id": task_id,
                "status": status,
                "description": description,
                "created_at": self.test_timestamp.isoformat(),
                "updated_at": self.test_timestamp.isoformat()
            }
        })
    
    def create_memory_response(
        self,
        memory_id: str = "memory-456",
        level: str = "L1",
        content: str = "test content"
    ) -> APIResponse:
        """
        创建记忆写入的标准成功响应
        
        Args:
            memory_id: 记忆ID
            level: 记忆层级
            content: 记忆内容
        
        Returns:
            APIResponse: 记忆响应对象
        """
        return self.create_mock_response(201, {
            "success": True,
            "data": {
                "id": memory_id,
                "level": level,
                "content": content,
                "created_at": self.test_timestamp.isoformat()
            }
        })
    
    def create_session_response(
        self,
        session_id: str = "session-789",
        status: str = "active"
    ) -> APIResponse:
        """
        创建会话创建的标准成功响应
        
        Args:
            session_id: 会话ID
            status: 会话状态
        
        Returns:
            APIResponse: 会话响应对象
        """
        return self.create_mock_response(201, {
            "success": True,
            "data": {
                "id": session_id,
                "session_id": session_id,
                "status": status,
                "created_at": self.test_timestamp.isoformat(),
                "expires_at": "2026-04-06T12:00:00"
            }
        })
    
    def create_skill_response(
        self,
        skill_id: str = "skill-abc",
        name: str = "test-skill",
        status: str = "loaded"
    ) -> APIResponse:
        """
        创建技能加载的标准成功响应
        
        Args:
            skill_id: 技能ID
            name: 技能名称
            status: 技能状态
        
        Returns:
            APIResponse: 技能响应对象
        """
        return self.create_mock_response(200, {
            "success": True,
            "data": {
                "id": skill_id,
                "name": name,
                "status": status,
                "loaded_at": self.test_timestamp.isoformat()
            }
        })
    
    def create_error_response(
        self,
        error_code: str = "0x0002",
        message: str = "参数无效",
        status_code: int = 400
    ) -> APIResponse:
        """
        创建标准错误响应
        
        Args:
            error_code: 错误码
            message: 错误消息
            status_code: HTTP 状态码
        
        Returns:
            APIResponse: 错误响应对象
        """
        return self.create_mock_response(status_code, {
            "success": False,
            "error": {
                "code": error_code,
                "message": message
            }
        }, success=False)
    
    def create_list_response(
        self,
        items: list,
        total: Optional[int] = None,
        page: int = 1,
        page_size: int = 20
    ) -> APIResponse:
        """
        创建列表查询的标准响应
        
        Args:
            items: 数据项列表
            total: 总数
            page: 页码
            page_size: 每页大小
        
        Returns:
            APIResponse: 列表响应对象
        """
        return self.create_mock_response(200, {
            "success": True,
            "data": {
                "items": items,
                "total": total or len(items),
                "page": page,
                "page_size": page_size
            }
        })
    
    def assert_api_called(
        self,
        method: str,
        path: str,
        call_count: int = 1
    ):
        """
        断言 API 方法被调用
        
        Args:
            method: HTTP 方法（get/post/put/delete）
            path: 请求路径
            call_count: 调用次数
        """
        mock_method = getattr(self.mock_client, method.lower())
        assert mock_method.call_count == call_count, \
            f"期望 {method.upper()} {path} 被调用 {call_count} 次，实际 {mock_method.call_count} 次"
    
    def assert_error_code(self, error: AgentOSError, expected_code: str):
        """
        断言错误码匹配
        
        Args:
            error: 错误对象
            expected_code: 期望的错误码
        """
        assert hasattr(error, 'code'), "错误对象应包含 code 属性"
        assert error.code == expected_code, \
            f"期望错误码 {expected_code}，实际 {error.code}"


class AsyncTestCase(BaseTestCase):
    """
    异步测试基类
    
    继承自 BaseTestCase，额外提供异步测试支持
    """
    
    @pytest.fixture(autouse=True)
    def setup_async_method(self, setup_method):
        """
        异步测试初始化
        """
        self.async_mock_client = MagicMock(spec=APIClient)
        # 为异步方法设置 AsyncMock
        for method in ['get', 'post', 'put', 'delete']:
            setattr(self.async_mock_client, method, AsyncMock())
    
    @pytest.mark.asyncio
    async def async_test_wrapper(self, test_func):
        """
        异步测试包装器
        """
        await test_func()
