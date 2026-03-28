# AgentOS Python SDK - Utilities Module
# Version: 3.0.0
# Last updated: 2026-03-24

"""
Utility functions for AgentOS Python SDK.

Provides type-safe map data extraction, API response parsing,
URL building, and other bases utility functions.

Corresponds to Go SDK: utils/helpers.go
"""

import json
import secrets
import time
from datetime import datetime
from typing import Any, Dict, List, Optional, Union
from urllib.parse import urlencode


# ============================================================
# Map 类型安全提取函数
# ============================================================

def get_string(m: Dict[str, Any], key: str, default: str = "") -> str:
    """
    从字典中安全提取字符串值。

    Args:
        m: 源字典
        key: 键名
        default: 默认值

    Returns:
        str: 提取的字符串值或默认值

    Example:
        >>> data = {"name": "test"}
        >>> get_string(data, "name")
        'test'
        >>> get_string(data, "missing", "default")
        'default'
    """
    if key in m:
        value = m[key]
        if isinstance(value, str):
            return value
    return default


def get_int(m: Dict[str, Any], key: str, default: int = 0) -> int:
    """
    从字典中安全提取整数值。

    Args:
        m: 源字典
        key: 键名
        default: 默认值

    Returns:
        int: 提取的整数值或默认值

    Example:
        >>> data = {"count": 42}
        >>> get_int(data, "count")
        42
    """
    if key in m:
        value = m[key]
        if isinstance(value, int) and not isinstance(value, bool):
            return value
        if isinstance(value, float):
            return int(value)
    return default


def get_float(m: Dict[str, Any], key: str, default: float = 0.0) -> float:
    """
    从字典中安全提取浮点数值。

    Args:
        m: 源字典
        key: 键名
        default: 默认值

    Returns:
        float: 提取的浮点数值或默认值

    Example:
        >>> data = {"score": 0.95}
        >>> get_float(data, "score")
        0.95
    """
    if key in m:
        value = m[key]
        if isinstance(value, (int, float)) and not isinstance(value, bool):
            return float(value)
    return default


def get_bool(m: Dict[str, Any], key: str, default: bool = False) -> bool:
    """
    从字典中安全提取布尔值。

    Args:
        m: 源字典
        key: 键名
        default: 默认值

    Returns:
        bool: 提取的布尔值或默认值

    Example:
        >>> data = {"enabled": True}
        >>> get_bool(data, "enabled")
        True
    """
    if key in m:
        value = m[key]
        if isinstance(value, bool):
            return value
    return default


def get_dict(m: Dict[str, Any], key: str) -> Optional[Dict[str, Any]]:
    """
    从字典中安全提取嵌套字典。

    Args:
        m: 源字典
        key: 键名

    Returns:
        Optional[Dict[str, Any]]: 提取的字典或 None

    Example:
        >>> data = {"metadata": {"version": "1.0"}}
        >>> get_dict(data, "metadata")
        {'version': '1.0'}
    """
    if key in m:
        value = m[key]
        if isinstance(value, dict):
            return value
    return None


def get_list(m: Dict[str, Any], key: str) -> List[Any]:
    """
    从字典中安全提取列表。

    Args:
        m: 源字典
        key: 键名

    Returns:
        List[Any]: 提取的列表或空列表

    Example:
        >>> data = {"items": [1, 2, 3]}
        >>> get_list(data, "items")
        [1, 2, 3]
    """
    if key in m:
        value = m[key]
        if isinstance(value, list):
            return value
    return []


# ============================================================
# API 响应解析函数
# ============================================================

def extract_data_map(resp: Optional[Any]) -> Optional[Dict[str, Any]]:
    """
    从 APIResponse 中提取 Data 字段为字典。

    Args:
        resp: APIResponse 对象

    Returns:
        Optional[Dict[str, Any]]: 提取的数据字典或 None

    Example:
        >>> from agentos.client import APIResponse
        >>> resp = APIResponse(success=True, data={"id": "123"})
        >>> extract_data_map(resp)
        {'id': '123'}
    """
    if resp is None:
        return None
    if hasattr(resp, 'success') and not resp.success:
        return None
    if hasattr(resp, 'data') and resp.data is not None:
        if isinstance(resp.data, dict):
            return resp.data
    return None


def build_url(base_path: str, query_params: Optional[Dict[str, str]] = None) -> str:
    """
    拼接基础路径和查询参数，返回完整 URL。

    Args:
        base_path: 基础路径
        query_params: 查询参数字典

    Returns:
        str: 完整的 URL 字符串

    Example:
        >>> build_url("/api/v1/tasks", {"page": "1", "size": "10"})
        '/api/v1/tasks?page=1&size=10'
    """
    if not query_params:
        return base_path
    query_string = urlencode(query_params)
    if "?" in base_path:
        return f"{base_path}&{query_string}"
    return f"{base_path}?{query_string}"


def parse_time_from_map(m: Dict[str, Any], key: str) -> Optional[datetime]:
    """
    从字典中安全提取并解析时间。

    Args:
        m: 源字典
        key: 键名

    Returns:
        Optional[datetime]: 解析的时间对象或 None

    Example:
        >>> data = {"created_at": "2024-01-01T00:00:00Z"}
        >>> parse_time_from_map(data, "created_at")
        datetime.datetime(2024, 1, 1, 0, 0)
    """
    if key not in m:
        return None

    value = m[key]
    if isinstance(value, str):
        try:
            # 尝试解析 ISO 8601 格式
            return datetime.fromisoformat(value.replace('Z', '+00:00'))
        except ValueError:
            pass
    elif isinstance(value, (int, float)):
        # Unix 时间戳（秒）
        return datetime.fromtimestamp(value)

    return None


def extract_int_stats(data: Dict[str, Any]) -> Dict[str, int]:
    """
    从字典中提取所有整数类型的统计值。

    Args:
        data: 源字典

    Returns:
        Dict[str, int]: 统计值字典

    Example:
        >>> data = {"total": 100, "failed": 5, "rate": 0.95}
        >>> extract_int_stats(data)
        {'total': 100, 'failed': 5}
    """
    stats = {}
    for key, value in data.items():
        if isinstance(value, int) and not isinstance(value, bool):
            stats[key] = value
        elif isinstance(value, float):
            stats[key] = int(value)
    return stats


# ============================================================
# ID/时间戳生成
# ============================================================

def generate_id(prefix: str = "aos") -> str:
    """
    生成唯一的 AgentOS ID（时间戳+密码学安全随机数）。

    Args:
        prefix: ID 前缀

    Returns:
        str: 唯一标识符

    Example:
        >>> generate_id()
        'aos_1709251200000000_a1b2c3d4'
    """
    timestamp = int(time.time() * 1_000_000)  # 微秒级时间戳
    random_hex = secrets.token_hex(4)  # 8 位随机十六进制
    return f"{prefix}_{timestamp}_{random_hex}"


def generate_timestamp() -> int:
    """
    生成当前 Unix 时间戳（秒）。

    Returns:
        int: Unix 时间戳

    Example:
        >>> generate_timestamp()
        1709251200
    """
    return int(time.time())


# ============================================================
# 验证和清理
# ============================================================

def validate_json(s: str) -> bool:
    """
    验证字符串是否为合法 JSON。

    Args:
        s: 待验证的字符串

    Returns:
        bool: 是否为合法 JSON

    Example:
        >>> validate_json('{"key": "value"}')
        True
        >>> validate_json('not json')
        False
    """
    try:
        json.loads(s)
        return True
    except (json.JSONDecodeError, TypeError):
        return False


def sanitize_string(s: str) -> str:
    """
    清理字符串中的危险字符。

    Args:
        s: 待清理的字符串

    Returns:
        str: 清理后的字符串

    Example:
        >>> sanitize_string("  hello\\x00world  ")
        'hello world'
    """
    s = s.strip()
    s = s.replace('\x00', '')
    s = s.replace('\r\n', '\n')
    return s


def append_pagination(params: Optional[Dict[str, str]], page: int, page_size: int) -> Dict[str, str]:
    """
    向查询参数追加分页信息。

    Args:
        params: 现有查询参数
        page: 页码
        page_size: 每页大小

    Returns:
        Dict[str, str]: 更新后的查询参数

    Example:
        >>> append_pagination({"status": "active"}, 1, 20)
        {'status': 'active', 'page': '1', 'page_size': '20'}
    """
    if params is None:
        params = {}
    if page > 0:
        params["page"] = str(page)
    if page_size > 0:
        params["page_size"] = str(page_size)
    return params
