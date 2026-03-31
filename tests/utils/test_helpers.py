"""
AgentOS 公共测试工具模块
Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
Version: 1.0.0

提供统一的测试辅助函数，减少重复代码
"""

import pytest
from typing import Any, Dict, List, Optional, Callable
from unittest.mock import Mock, MagicMock, patch
from functools import wraps
import time


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
# 导出公共 API
# ============================================================

__all__ = [
    'create_mock_response',
    'create_mock_session',
    'with_mock_session',
    'performance_test',
    'parametrize_validation',
    'assert_dict_contains',
    'assert_error_contains',
    'TestDataBuilder',
    'ContractTestHelper',
    'TestIsolation',
]
