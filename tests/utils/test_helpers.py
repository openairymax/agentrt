"""
AgentOS 公共测试工具模块
Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
Version: 1.0.0.9

提供统一的测试辅助函数，减少重复代码
"""

import pytest
import json
import hashlib
import secrets
import string
import re
from typing import Any, Dict, List, Optional, Callable, Type, Union
from unittest.mock import Mock, MagicMock, patch, AsyncMock
from functools import wraps
import time
import tracemalloc
import asyncio
from contextlib import contextmanager


# ============================================================
# Mock 工厂函数
# ============================================================

def create_mock_response(
    status_code: int = 200,
    json_data: Optional[Dict[str, Any]] = None,
    text: str = ""
) -> Mock:
    """
    创建标准 Mock 响应对象

    Args:
        status_code: HTTP 状态码
        json_data: JSON 响应数据
        text: 文本响应

    Returns:
        Mock: 配置好的 Mock 响应对象
    """
    mock_response = Mock()
    mock_response.status_code = status_code
    mock_response.json.return_value = json_data or {}
    mock_response.text = text
    return mock_response


def create_mock_session(
    response_data: Optional[Dict[str, Any]] = None,
    status_code: int = 200
) -> Mock:
    """
    创建标准 Mock Session 对象

    Args:
        response_data: 响应数据
        status_code: HTTP 状态码

    Returns:
        Mock: 配置好的 Mock Session 对象
    """
    mock_session_instance = Mock()
    mock_response = create_mock_response(status_code, response_data)

    mock_session_instance.post.return_value = mock_response
    mock_session_instance.get.return_value = mock_response
    mock_session_instance.put.return_value = mock_response
    mock_session_instance.delete.return_value = mock_response

    return mock_session_instance


# ============================================================
# 测试装饰器
# ============================================================

def with_mock_session(func: Callable) -> Callable:
    """
    装饰器：自动为测试函数添加 Mock Session

    用法:
        @with_mock_session
        def test_something(mock_session):
            client = AgentOS()
            # mock_session 已自动配置
    """
    @wraps(func)
    @patch('agentos.agent.requests.Session')
    def wrapper(*args, **kwargs):
        mock_session = create_mock_session()
        return func(*args, mock_session, **kwargs)
    return wrapper


def performance_test(max_duration: float = 0.1):
    """
    装饰器：性能测试，确保函数执行时间不超过阈值

    Args:
        max_duration: 最大执行时间（秒）

    用法:
        @performance_test(max_duration=0.2)
        def test_performance():
            # 测试代码
    """
    def decorator(func: Callable) -> Callable:
        @wraps(func)
        def wrapper(*args, **kwargs):
            start = time.perf_counter()
            result = func(*args, **kwargs)
            elapsed = time.perf_counter() - start

            assert elapsed < max_duration, \
                f"Performance test failed: {elapsed:.3f}s > {max_duration:.3f}s"

            return result
        return wrapper
    return decorator


# ============================================================
# 参数化测试辅助函数
# ============================================================

def parametrize_validation(
    test_cases: List[Dict[str, Any]]
) -> Callable:
    """
    参数化验证测试

    Args:
        test_cases: 测试用例列表，每个用例包含:
            - name: 测试名称
            - input: 输入数据
            - expected_valid: 是否期望验证通过
            - expected_errors: 期望的错误信息列表（可选）

    用法:
        @parametrize_validation([
            {"name": "valid", "input": {...}, "expected_valid": True},
            {"name": "invalid", "input": {...}, "expected_valid": False, "expected_errors": ["field required"]}
        ])
        def test_validation(test_case):
            # 测试代码
    """
    return pytest.mark.parametrize(
        "test_case",
        test_cases,
        ids=[tc["name"] for tc in test_cases]
    )


# ============================================================
# 断言辅助函数
# ============================================================

def assert_dict_contains(
    actual: Dict[str, Any],
    expected: Dict[str, Any],
    path: str = ""
) -> None:
    """
    递归断言字典包含期望的键值对

    Args:
        actual: 实际字典
        expected: 期望字典
        path: 当前路径（用于错误信息）

    Raises:
        AssertionError: 如果断言失败
    """
    for key, expected_value in expected.items():
        current_path = f"{path}.{key}" if path else key

        assert key in actual, f"Missing key: {current_path}"

        actual_value = actual[key]

        if isinstance(expected_value, dict):
            assert isinstance(actual_value, dict), \
                f"Expected dict at {current_path}, got {type(actual_value).__name__}"
            assert_dict_contains(actual_value, expected_value, current_path)
        else:
            assert actual_value == expected_value, \
                f"Value mismatch at {current_path}: expected {expected_value}, got {actual_value}"


def assert_error_contains(
    errors: List[str],
    expected_substring: str
) -> None:
    """
    断言错误列表包含期望的子串

    Args:
        errors: 错误列表
        expected_substring: 期望的子串

    Raises:
        AssertionError: 如果没有找到匹配的错误
    """
    assert any(expected_substring in error for error in errors), \
        f"Expected error containing '{expected_substring}' not found in {errors}"


# ============================================================
# 测试数据生成器
# ============================================================

class TestDataBuilder:
    """
    测试数据构建器

    提供流式 API 构建测试数据
    """

    def __init__(self):
        self._data = {}

    def with_field(self, name: str, value: Any) -> 'TestDataBuilder':
        """添加字段"""
        self._data[name] = value
        return self

    def with_fields(self, **kwargs) -> 'TestDataBuilder':
        """批量添加字段"""
        self._data.update(kwargs)
        return self

    def without_field(self, name: str) -> 'TestDataBuilder':
        """移除字段"""
        self._data.pop(name, None)
        return self

    def build(self) -> Dict[str, Any]:
        """构建最终数据"""
        return self._data.copy()

    def build_invalid(self, missing_field: str) -> Dict[str, Any]:
        """构建缺少字段的数据"""
        data = self._data.copy()
        data.pop(missing_field, None)
        return data


# ============================================================
# 契约测试辅助类
# ============================================================

class ContractTestHelper:
    """
    契约测试辅助类

    提供契约验证的通用方法
    """

    @staticmethod
    def create_valid_contract() -> Dict[str, Any]:
        """创建有效的契约数据"""
        return {
            "schema_version": "1.0.0",
            "agent_id": "com.agentos.test.v1",
            "agent_name": "Test Agent",
            "version": "1.0.0",
            "role": "software_engineer",
            "description": "测试 Agent",
            "capabilities": [
                {
                    "name": "test_capability",
                    "description": "测试能力",
                    "input_schema": {"type": "object"},
                    "output_schema": {"type": "object"}
                }
            ],
            "models": {
                "system1": "gpt-3.5-turbo",
                "system2": "gpt-4"
            },
            "required_permissions": ["read_project_context"],
            "cost_profile": {
                "token_per_task_avg": 1000,
                "api_cost_per_task": 0.01,
                "maintenance_level": "community"
            },
            "trust_metrics": {
                "install_count": 0,
                "rating": 3.0,
                "verified_provider": False,
                "last_audit": "2026-03-01"
            }
        }

    @staticmethod
    def create_invalid_contract(
        missing_field: Optional[str] = None,
        invalid_field: Optional[str] = None,
        invalid_value: Any = None
    ) -> Dict[str, Any]:
        """
        创建无效的契约数据

        Args:
            missing_field: 要移除的字段
            invalid_field: 要设置无效值的字段
            invalid_value: 无效值

        Returns:
            无效的契约数据
        """
        contract = ContractTestHelper.create_valid_contract()

        if missing_field:
            contract.pop(missing_field, None)

        if invalid_field and invalid_value is not None:
            contract[invalid_field] = invalid_value

        return contract


# ============================================================
# 测试隔离辅助
# ============================================================

class TestIsolation:
    """
    测试隔离辅助类

    确保测试之间相互隔离
    """

    def __init__(self):
        self._saved_state = {}

    def save_env(self, key: str) -> 'TestIsolation':
        """保存环境变量"""
        import os
        self._saved_state[f"env_{key}"] = os.environ.get(key)
        return self

    def restore_env(self, key: str) -> 'TestIsolation':
        """恢复环境变量"""
        import os
        saved_key = f"env_{key}"
        if saved_key in self._saved_state:
            if self._saved_state[saved_key] is None:
                os.environ.pop(key, None)
            else:
                os.environ[key] = self._saved_state[saved_key]
        return self

    def restore_all(self) -> None:
        """恢复所有保存的状态"""
        for key in list(self._saved_state.keys()):
            if key.startswith("env_"):
                self.restore_env(key[4:])


# ============================================================
# 异步测试辅助
# ============================================================

def async_test(func: Callable) -> Callable:
    """
    装饰器：标记异步测试函数

    用法:
        @async_test
        async def test_async():
            result = await some_async_function()
            assert result is not None
    """
    @wraps(func)
    def wrapper(*args, **kwargs):
        loop = asyncio.new_event_loop()
        try:
            return loop.run_until_complete(func(*args, **kwargs))
        finally:
            loop.close()
    return wrapper


@contextmanager
def async_timeout(seconds: float):
    """
    上下文管理器：异步超时控制

    用法:
        async with async_timeout(5.0):
            await long_running_operation()

    Raises:
        asyncio.TimeoutError: 操作超时
    """
    async def _timeout():
        await asyncio.sleep(seconds)
        raise asyncio.TimeoutError(f"Operation timed out after {seconds}s")

    async def _run_with_timeout(coro):
        task = asyncio.create_task(coro)
        timeout_task = asyncio.create_task(_timeout())
        try:
            result = await asyncio.shield(asyncio.gather(task, timeout_task, return_exceptions=True))[0]
            return result
        except asyncio.TimeoutError:
            task.cancel()
            raise
        finally:
            timeout_task.cancel()

    loop = asyncio.new_event_loop()
    try:
        yield _run_with_timeout
    finally:
        loop.close()


# ============================================================
# 内存分析辅助
# ============================================================

@contextmanager
def memory_profile():
    """
    上下文管理器：内存使用分析

    用法:
        with memory_profile() as mp:
            # 执行代码
            data = process_large_data()
        print(f"峰值内存: {mp.peak_mb:.2f} MB")
        print(f"当前内存: {mp.current_mb:.2f} MB")

    Yields:
        MemorySnapshot: 内存快照对象
    """
    class MemorySnapshot:
        def __init__(self):
            self.start_mb = 0
            self.peak_mb = 0
            self.current_mb = 0

        def update(self):
            tracemalloc.stop()
            tracemalloc.start()
            snapshot = tracemalloc.take_snapshot()
            stats = snapshot.statistics('lineno')
            total = sum(stat.size for stat in stats)
            self.current_mb = total / 1024 / 1024
            if self.current_mb > self.peak_mb:
                self.peak_mb = self.current_mb

    snapshot = MemorySnapshot()
    snapshot.start_mb = 0
    tracemalloc.start()
    try:
        yield snapshot
    finally:
        snapshot.update()
        tracemalloc.stop()


# ============================================================
# 随机数据生成器
# ============================================================

class RandomDataGenerator:
    """
    随机数据生成器

    用于生成各种测试数据
    """

    @staticmethod
    def random_string(length: int = 10, include_special: bool = False) -> str:
        """生成随机字符串"""
        chars = string.ascii_letters + string.digits
        if include_special:
            chars += "!@#$%^&*()_+-=[]{}|;:,.<>?"
        return ''.join(secrets.choice(chars) for _ in range(length))

    @staticmethod
    def random_email() -> str:
        """生成随机邮箱"""
        username = RandomDataGenerator.random_string(8).lower()
        domain = secrets.choice(['gmail.com', 'outlook.com', 'test.com'])
        return f"{username}@{domain}"

    @staticmethod
    def random_url() -> str:
        """生成随机URL"""
        scheme = secrets.choice(['http', 'https'])
        domain = RandomDataGenerator.random_string(10).lower()
        path = '/'.join(RandomDataGenerator.random_string(5).lower() for _ in range(3))
        return f"{scheme}://{domain}.com/{path}"

    @staticmethod
    def random_json(depth: int = 3, max_items: int = 5) -> Dict[str, Any]:
        """生成随机JSON结构"""
        if depth <= 0:
            return {"value": RandomDataGenerator.random_string(10)}

        result = {}
        num_items = secrets.randbelow(max_items) + 1

        for i in range(num_items):
            key = f"field_{i}"
            choice = secrets.randbelow(4)

            if choice == 0:
                result[key] = RandomDataGenerator.random_string(20)
            elif choice == 1:
                result[key] = secrets.randbelow(10000)
            elif choice == 2:
                result[key] = secrets.choice([True, False])
            else:
                result[key] = RandomDataGenerator.random_json(depth - 1, max_items)

        return result

    @staticmethod
    def random_ip() -> str:
        """生成随机IP地址"""
        return ".".join(str(secrets.randbelow(256)) for _ in range(4))


# ============================================================
# JSON Schema 验证辅助（简化版）
# ============================================================

class JSONSchemaValidator:
    """
    简化的 JSON Schema 验证器

    支持常用验证规则
    """

    TYPE_MAP = {
        "object": dict,
        "string": str,
        "number": (int, float),
        "boolean": bool,
        "array": list,
    }

    @staticmethod
    def validate(data: Any, schema: Dict[str, Any]) -> List[str]:
        """
        验证数据是否符合 Schema

        Args:
            data: 要验证的数据
            schema: JSON Schema 定义

        Returns:
            错误列表，空表示验证通过
        """
        errors = []
        JSONSchemaValidator._validate_recursive(data, schema, "", errors)
        return errors

    @staticmethod
    def _validate_type(data: Any, expected_type: str, path: str, errors: List[str]) -> bool:
        """
        验证数据类型是否匹配。

        Args:
            data: 要验证的数据
            expected_type: 期望的类型
            path: 字段路径
            errors: 错误列表

        Returns:
            bool: 类型是否匹配
        """
        if expected_type not in JSONSchemaValidator.TYPE_MAP:
            return True

        expected_python_type = JSONSchemaValidator.TYPE_MAP[expected_type]
        if not isinstance(data, expected_python_type):
            errors.append(f"{path}: expected {expected_type}, got {type(data).__name__}")
            return False
        return True

    @staticmethod
    def _validate_required_fields(data: Dict, required: List[str], path: str, errors: List[str]) -> None:
        """
        验证必需字段。

        Args:
            data: 数据字典
            required: 必需字段列表
            path: 字段路径
            errors: 错误列表
        """
        for field in required:
            if field not in data:
                errors.append(f"{path}.{field}: required field missing")

    @staticmethod
    def _validate_properties(data: Dict, properties: Dict, path: str, errors: List[str]) -> None:
        """
        验证对象属性。

        Args:
            data: 数据字典
            properties: 属性schema定义
            path: 字段路径
            errors: 错误列表
        """
        for key, field_schema in properties.items():
            if key in data:
                JSONSchemaValidator._validate_recursive(
                    data[key], field_schema,
                    f"{path}.{key}" if path else key,
                    errors
                )

    @staticmethod
    def _validate_items(data: List, items_schema: Dict, path: str, errors: List[str]) -> None:
        """
        验证数组元素。

        Args:
            data: 数据列表
            items_schema: 元素schema定义
            path: 字段路径
            errors: 错误列表
        """
        for i, item in enumerate(data):
            JSONSchemaValidator._validate_recursive(
                item, items_schema,
                f"{path}[{i}]",
                errors
            )

    @staticmethod
    def _validate_recursive(data: Any, schema: Dict[str, Any], path: str, errors: List[str]) -> None:
        """
        递归验证数据。

        Args:
            data: 要验证的数据
            schema: JSON Schema 定义
            path: 字段路径
            errors: 错误列表
        """
        if "type" in schema:
            expected_type = schema["type"]
            if not JSONSchemaValidator._validate_type(data, expected_type, path, errors):
                return

        if "required" in schema and isinstance(data, dict):
            JSONSchemaValidator._validate_required_fields(
                data, schema["required"], path, errors
            )

        if "properties" in schema and isinstance(data, dict):
            JSONSchemaValidator._validate_properties(
                data, schema["properties"], path, errors
            )

        if "items" in schema and isinstance(data, list):
            JSONSchemaValidator._validate_items(
                data, schema["items"], path, errors
            )


# ============================================================
# 性能基准测试辅助
# ============================================================

class PerformanceBenchmark:
    """
    性能基准测试类

    用于测量和比较性能
    """

    def __init__(self, name: str = "Benchmark"):
        self.name = name
        self.results = []
        self._start_time = None

    def start(self):
        """开始计时"""
        self._start_time = time.perf_counter()

    def stop(self) -> float:
        """停止计时并记录结果"""
        if self._start_time is None:
            raise RuntimeError("Benchmark not started")
        elapsed = time.perf_counter() - self._start_time
        self.results.append(elapsed)
        self._start_time = None
        return elapsed

    def run(self, func: Callable, iterations: int = 100, warmup: int = 10) -> Dict[str, float]:
        """
        运行基准测试

        Args:
            func: 要测试的函数
            iterations: 测试迭代次数
            warmup: 预热迭代次数

        Returns:
            性能统计字典
        """
        for _ in range(warmup):
            func()

        self.results = []
        for _ in range(iterations):
            start = time.perf_counter()
            func()
            self.results.append(time.perf_counter() - start)

        return self.get_stats()

    def get_stats(self) -> Dict[str, float]:
        """获取性能统计"""
        if not self.results:
            return {}

        sorted_results = sorted(self.results)
        return {
            "name": self.name,
            "iterations": len(self.results),
            "min": min(self.results) * 1000,
            "max": max(self.results) * 1000,
            "mean": sum(self.results) / len(self.results) * 1000,
            "median": sorted_results[len(sorted_results) // 2] * 1000,
            "p95": sorted_results[int(len(sorted_results) * 0.95)] * 1000,
            "p99": sorted_results[int(len(sorted_results) * 0.99)] * 1000,
        }


# ============================================================
# 边界条件测试辅助
# ============================================================

class BoundaryTestCases:
    """
    边界条件测试用例生成器

    生成各种边界条件测试数据
    """

    @staticmethod
    def get_string_boundaries() -> List[str]:
        """获取字符串边界值"""
        return [
            "",  # 空字符串
            "a",  # 单字符
            "a" * 255,  # 最大短字符串
            "a" * 1000,  # 长字符串
            "\0",  # null字符
            "\n\t\r",  # 空白字符
            "<script>alert('xss')</script>",  # XSS攻击
            "' OR '1'='1",  # SQL注入
            "../../../etc/passwd",  # 路径遍历
            "\u0000",  # Unicode null
            "\uFFFD",  # Unicode替换字符
        ]

    @staticmethod
    def get_number_boundaries() -> List[Union[int, float]]:
        """获取数字边界值"""
        return [
            0,
            -1,
            1,
            127,  # 8位最大值
            128,  # 8位溢出
            255,  # 无符号8位最大值
            256,  # 8位溢出
            32767,  # 16位有符号最大值
            -32768,  # 16位有符号最小值
            2147483647,  # 32位有符号最大值
            -2147483648,  # 32位有符号最小值
            0.0,
            -0.0,
            float('inf'),
            float('-inf'),
            float('nan'),
            1e308,  # 接近浮点数最大值
            1e-308,  # 接近浮点数最小值
        ]

    @staticmethod
    def get_collection_boundaries() -> List[Any]:
        """获取集合边界值"""
        return [
            [],  # 空列表
            [None],  # 单元素列表
            [{}],  # 嵌套空字典
            [{"a": None}],  # 深层嵌套
            list(range(1000)),  # 大列表
            {chr(i): i for i in range(100)},  # 大字典
        ]

    @staticmethod
    def get_json_boundaries() -> List[Dict[str, Any]]:
        """获取JSON边界值"""
        return [
            {},  # 空对象
            {"key": ""},  # 空字符串值
            {"key": None},  # null值
            {"key": [None, {}, {"nested": ""}]},  # 深层嵌套
            {"key": "value" * 1000},  # 长字符串值
            {f"key_{i}": i for i in range(100)},  # 大对象
        ]


# ============================================================
# 导出公共 API
# ============================================================

__all__ = [
    # Mock 工厂
    'create_mock_response',
    'create_mock_session',

    # 装饰器
    'with_mock_session',
    'performance_test',
    'async_test',

    # 上下文管理器
    'async_timeout',
    'memory_profile',

    # 参数化测试
    'parametrize_validation',

    # 断言辅助
    'assert_dict_contains',
    'assert_error_contains',

    # 数据构建器
    'TestDataBuilder',
    'RandomDataGenerator',
    'BoundaryTestCases',

    # 契约测试
    'ContractTestHelper',

    # 验证器
    'JSONSchemaValidator',

    # 性能测试
    'PerformanceBenchmark',

    # 测试隔离
    'TestIsolation',
]
