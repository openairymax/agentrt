# AgentOS 测试基类
# Version: 1.0.0.6
# Last updated: 2026-03-22

"""
测试基类模块。

提供通用的测试功能和工具类，减少重复代码。
"""

import os
import sys
import json
import time
import tempfile
import asyncio
from typing import Dict, Any, List, Optional, Union, Callable
from unittest.mock import Mock, MagicMock, patch, AsyncMock
from pathlib import Path
import pytest

# 添加项目根目录到路径
PROJECT_ROOT = Path(__file__).parent.parent.parent
sys.path.insert(0, str(PROJECT_ROOT / "toolkit" / "python"))


class BaseTestCase:
    """测试基类"""

    @classmethod
    def setup_class(cls):
        """类级别的设置"""
        cls.test_data_dir = PROJECT_ROOT / "tests" / "fixtures" / "data"
        cls.temp_dir = Path(tempfile.gettempdir()) / "agentos_tests"
        cls.temp_dir.mkdir(parents=True, exist_ok=True)

        # 加载测试数据
        cls.load_test_data()

    @classmethod
    def teardown_class(cls):
        """类级别的清理"""
        # 清理临时文件
        import shutil
        if cls.temp_dir.exists():
            shutil.rmtree(cls.temp_dir, ignore_errors=True)

    def setup_method(self, method):
        """方法级别的设置"""
        self.method_start_time = time.time()

    def teardown_method(self, method):
        """方法级别的清理"""
        method_duration = time.time() - self.method_start_time
        print(f"\n⏱️  {method.__name__} 执行时间: {method_duration:.3f}s")

    @classmethod
    def load_test_data(cls):
        """加载测试数据"""
        cls.test_data = {}

        data_types = ["tasks", "memories", "sessions", "skills"]
        for data_type in data_types:
            data_file = cls.test_data_dir / data_type / f"sample_{data_type}.json"
            if data_file.exists():
                try:
                    with open(data_file, 'r', encoding='utf-8') as f:
                        cls.test_data[data_type] = json.load(f)
                except (json.JSONDecodeError, IOError):
                    cls.test_data[data_type] = {}
            else:
                cls.test_data[data_type] = {}

    def get_test_data(self, data_type: str, key: str = None) -> Any:
        """
        获取测试数据。

        Args:
            data_type: 数据类型
            key: 数据键，如果为None则返回全部数据

        Returns:
            测试数据
        """
        if data_type not in self.test_data:
            return None

        if key is None:
            return self.test_data[data_type]

        # 支持嵌套键访问，如 "tasks.0"
        keys = key.split('.')
        data = self.test_data[data_type]

        try:
            for k in keys:
                if isinstance(k, str) and k.isdigit():
                    k = int(k)
                data = data[k]
            return data
        except (KeyError, IndexError, TypeError):
            return None

    def create_mock_response(self, status_code: int = 200,
                           json_data: Dict = None,
                           text: str = "") -> Mock:
        """
        创建模拟HTTP响应。

        Args:
            status_code: HTTP状态码
            json_data: JSON响应数据
            text: 文本响应数据

        Returns:
            模拟的响应对象
        """
        response = Mock()
        response.status_code = status_code
        response.json.return_value = json_data or {}
        response.text = text
        response.ok = status_code < 400
        return response

    def create_mock_session(self, responses: List[Mock] = None) -> Mock:
        """
        创建模拟会话对象。

        Args:
            responses: 预设的响应列表

        Returns:
            模拟的会话对象
        """
        session = Mock()

        if responses:
            session.get.side_effect = responses
            session.post.side_effect = responses
            session.put.side_effect = responses
            session.delete.side_effect = responses

        return session

    def assert_performance(self, operation: Callable, max_time: float, *args, **kwargs):
        """
        断言操作性能。

        Args:
            operation: 要测试的操作
            max_time: 最大允许时间（秒）
            *args: 操作参数
            **kwargs: 操作关键字参数
        """
        start_time = time.perf_counter()
        result = operation(*args, **kwargs)
        end_time = time.perf_counter()

        elapsed = end_time - start_time
        assert elapsed <= max_time, f"操作耗时 {elapsed:.3f}s 超过限制 {max_time:.3f}s"

        return result

    def assert_async_performance(self, operation: Callable, max_time: float, *args, **kwargs):
        """
        断言异步操作性能。

        Args:
            operation: 要测试的异步操作
            max_time: 最大允许时间（秒）
            *args: 操作参数
            **kwargs: 操作关键字参数
        """
        async def _run_test():
            start_time = time.perf_counter()
            result = await operation(*args, **kwargs)
            end_time = time.perf_counter()

            elapsed = end_time - start_time
            assert elapsed <= max_time, f"异步操作耗时 {elapsed:.3f}s 超过限制 {max_time:.3f}s"

            return result

        return asyncio.run(_run_test())


class SDKTestCase(BaseTestCase):
    """SDK测试基类"""

    def setup_method(self, method):
        """SDK测试方法级别设置"""
        super().setup_method(method)

        # 导入SDK模块
        try:
            from agentos import AgentOS, AsyncAgentOS
            from agentos.exceptions import (
                AgentOSError, NetworkError, TimeoutError as AgentOSTimeoutError,
                TaskError, MemoryError as AgentOSMemoryError, SessionError, SkillError
            )

            self.AgentOS = AgentOS
            self.AsyncAgentOS = AsyncAgentOS
            self.AgentOSError = AgentOSError
            self.NetworkError = NetworkError
            self.AgentOSTimeoutError = AgentOSTimeoutError
            self.TaskError = TaskError
            self.AgentOSMemoryError = AgentOSMemoryError
            self.SessionError = SessionError
            self.SkillError = SkillError

        except ImportError as e:
            pytest.skip(f"无法导入SDK模块: {e}")

    def create_mock_client(self, endpoint: str = "http://localhost:18789") -> Mock:
        """
        创建模拟客户端。

        Args:
            endpoint: 服务端点

        Returns:
            模拟的客户端对象
        """
        with patch('agentos.agent.requests.Session') as mock_session_class:
            mock_session = Mock()
            mock_session_class.return_value = mock_session

            # 设置默认响应
            mock_response = self.create_mock_response()
            mock_session.get.return_value = mock_response
            mock_session.post.return_value = mock_response
            mock_session.put.return_value = mock_response
            mock_session.delete.return_value = mock_response

            client = self.AgentOS(endpoint=endpoint)
            client._session = mock_session

            return client

    def create_mock_async_client(self, endpoint: str = "http://localhost:18789") -> Mock:
        """
        创建模拟异步客户端。

        Args:
            endpoint: 服务端点

        Returns:
            模拟的异步客户端对象
        """
        with patch('agentos.agent.aiohttp.ClientSession') as mock_session_class:
            mock_session = AsyncMock()
            mock_session_class.return_value = mock_session

            # 设置默认响应
            mock_response = AsyncMock()
            mock_response.status = 200
            mock_response.json.return_value = {}
            mock_session.get.return_value.__aenter__.return_value = mock_response
            mock_session.post.return_value.__aenter__.return_value = mock_response
            mock_session.put.return_value.__aenter__.return_value = mock_response
            mock_session.delete.return_value.__aenter__.return_value = mock_response

            client = self.AsyncAgentOS(endpoint=endpoint)
            client._session = mock_session

            return client


class APITestCase(BaseTestCase):
    """API测试基类"""

    def setup_method(self, method):
        """API测试方法级别设置"""
        super().setup_method(method)

        self.base_url = "http://localhost:18789"
        self.api_version = "v1"
        self.headers = {
            "Content-Type": "application/json",
            "User-Agent": "AgentOS-Test/1.0.0"
        }

    def get_api_url(self, endpoint: str) -> str:
        """
        获取完整的API URL。

        Args:
            endpoint: API端点

        Returns:
            完整的API URL
        """
        return f"{self.base_url}/{self.api_version}/{endpoint.lstrip('/')}"

    def create_test_payload(self, data_type: str, **overrides) -> Dict[str, Any]:
        """
        创建测试载荷。

        Args:
            data_type: 数据类型
            **overrides: 覆盖字段

        Returns:
            测试载荷
        """
        base_data = self.get_test_data(data_type)
        if not base_data:
            return overrides

        # 获取第一个数据项作为模板
        if isinstance(base_data, dict) and data_type in base_data:
            template = base_data[data_type][0] if base_data[data_type] else {}
        elif isinstance(base_data, list):
            template = base_data[0] if base_data else {}
        else:
            template = base_data

        # 合并覆盖数据
        result = template.copy()
        result.update(overrides)

        return result


class IntegrationTestCase(BaseTestCase):
    """集成测试基类"""

    def setup_method(self, method):
        """集成测试方法级别设置"""
        super().setup_method(method)

        # 检查外部依赖
        self.check_external_dependencies()

    def check_external_dependencies(self):
        """检查外部依赖"""
        # 检查AgentOS服务是否可用
        import socket
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(5)
            result = sock.connect_ex(('localhost', 18789))
            sock.close()
            self.agentos_available = result == 0
        except Exception:
            self.agentos_available = False

        if not self.agentos_available:
            pytest.skip("AgentOS服务不可用，跳过集成测试")

    def wait_for_service(self, host: str = "localhost", port: int = 18789,
                        timeout: int = 30) -> bool:
        """
        等待服务启动。

        Args:
            host: 服务主机
            port: 服务端口
            timeout: 超时时间（秒）

        Returns:
            服务是否可用
        """
        import socket

        start_time = time.time()
        while time.time() - start_time < timeout:
            try:
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                sock.settimeout(1)
                result = sock.connect_ex((host, port))
                sock.close()
                if result == 0:
                    return True
            except Exception:
                pass
            time.sleep(1)

        return False


# 测试工具函数
def create_test_file(file_path: Path, content: str = "test content") -> Path:
    """
    创建测试文件。

    Args:
        file_path: 文件路径
        content: 文件内容

    Returns:
        创建的文件路径
    """
    file_path.parent.mkdir(parents=True, exist_ok=True)
    file_path.write_text(content, encoding='utf-8')
    return file_path


def assert_file_exists(file_path: Path, should_exist: bool = True):
    """
    断言文件是否存在。

    Args:
        file_path: 文件路径
        should_exist: 是否应该存在
    """
    if should_exist:
        assert file_path.exists(), f"文件应该存在: {file_path}"
    else:
        assert not file_path.exists(), f"文件不应该存在: {file_path}"


def assert_file_content(file_path: Path, expected_content: str):
    """
    断言文件内容。

    Args:
        file_path: 文件路径
        expected_content: 期望内容
    """
    assert file_path.exists(), f"文件不存在: {file_path}"
    actual_content = file_path.read_text(encoding='utf-8')
    assert actual_content == expected_content, f"文件内容不匹配:\n期望: {expected_content}\n实际: {actual_content}"
