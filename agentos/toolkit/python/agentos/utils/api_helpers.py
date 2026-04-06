# AgentOS Python SDK - API 响应处理工具
# Version: 3.0.0
# Last updated: 2026-04-05
#
# 提供统一的 API 响应处理函数，消除重复代码
# 遵循 DRY 原则和 ARCHITECTURAL_PRINCIPLES.md E-3（资源确定性）

import time
from typing import Dict, Any, Optional, Tuple
import requests

from agentos.exceptions import (
    AgentOSError,
    InvalidResponseError,
    NetworkError,
    ServerError,
    http_status_to_error,
)


def create_api_error(
    status_code: int, 
    message: str, 
    error_code: Optional[str] = None
) -> Dict[str, Any]:
    """
    创建标准化的 API 错误响应
    
    Args:
        status_code: HTTP 状态码
        message: 错误消息
        error_code: 可选的错误码
    
    Returns:
        dict: 标准化错误字典
    
    Example:
        >>> error = create_api_error(400, "参数无效", "0x0002")
        >>> assert error["error"] == True
        >>> assert error["code"] == "0x0002"
    """
    code = error_code or http_status_to_code(status_code)
    return {
        "error": True,
        "success": False,
        "code": code,
        "message": message,
        "status_code": status_code,
        "timestamp": time.time()
    }


def handle_api_response(
    response: requests.Response, 
    expected_codes: Tuple[int, ...] = (200, 201)
) -> Optional[Dict[str, Any]]:
    """
    处理 API 响应的统一函数
    
    Args:
        response: requests.Response 对象
        expected_codes: 期望的 HTTP 状态码元组
    
    Returns:
        dict/None: 解析后的 JSON 数据
    
    Raises:
        AgentOSError: 当状态码不在 expected_codes 中
        InvalidResponseError: JSON 解析失败
    
    Example:
        >>> response = requests.get("http://localhost:18789/api/v1/tasks")
        >>> data = handle_api_response(response)
        >>> assert isinstance(data, dict)
    """
    if response.status_code not in expected_codes:
        error_msg = f"API请求失败: {response.status_code}"
        try:
            error_data = response.json()
            if "error" in error_data:
                error_msg = error_data["error"].get("message", error_msg)
        except (ValueError, KeyError):
            error_msg = f"{error_msg} - {response.text[:200]}"
        
        raise http_status_to_error(
            response.status_code, 
            error_msg
        )
    
    if response.status_code == 204:  # No Content
        return None
    
    try:
        data = response.json()
        
        # 检查响应结构
        if not isinstance(data, dict):
            raise InvalidResponseError(f"响应应为字典，实际为 {type(data).__name__}")
        
        # 检查 success 字段
        if "success" in data and not data["success"]:
            error_info = data.get("error", {})
            error_code = error_info.get("code", "0x0001")
            error_message = error_info.get("message", "未知错误")
            raise AgentOSError(error_code, error_message)
        
        return data
    
    except ValueError as e:
        raise InvalidResponseError(f"JSON解析失败: {str(e)}")


def http_status_to_code(status_code: int) -> str:
    """
    将 HTTP 状态码映射到 AgentOS 错误码
    
    Args:
        status_code: HTTP 状态码
    
    Returns:
        str: AgentOS 错误码
    
    Mapping:
        400 -> 0x0002 (InvalidParameter)
        401 -> 0x000D (Unauthorized)
        403 -> 0x000E (Forbidden)
        404 -> 0x0005 (NotFound)
        408 -> 0x0004 (Timeout)
        429 -> 0x000F (RateLimited)
        500-504 -> 0x000C (ServerError)
        Other -> 0x0001 (Unknown)
    """
    mapping = {
        400: "0x0002",  # InvalidParameter
        401: "0x000D",  # Unauthorized
        403: "0x000E",  # Forbidden
        404: "0x0005",  # NotFound
        405: "0x0013",  # NotSupported
        408: "0x0004",  # Timeout
        409: "0x0007",  # Conflict
        422: "0x0012",  # ValidationError
        429: "0x000F",  # RateLimited
        500: "0x000C",  # ServerError
        502: "0x000C",  # ServerError
        503: "0x000C",  # ServerError
        504: "0x0004",  # Timeout
    }
    
    return mapping.get(status_code, "0x0001")  # Unknown


def validate_response_data(
    data: Dict[str, Any],
    required_fields: list,
    response_type: str = "response"
) -> None:
    """
    验证响应数据包含必需字段
    
    Args:
        data: 响应数据字典
        required_fields: 必需字段列表
        response_type: 响应类型描述（用于错误消息）
    
    Raises:
        InvalidResponseError: 缺少必需字段
    
    Example:
        >>> data = {"id": "123", "status": "pending"}
        >>> validate_response_data(data, ["id", "status"], "Task")
    """
    missing_fields = [field for field in required_fields if field not in data]
    
    if missing_fields:
        raise InvalidResponseError(
            f"{response_type}响应缺少必需字段: {', '.join(missing_fields)}"
        )


def extract_data_field(
    response_data: Dict[str, Any],
    field_name: str = "data",
    default: Any = None
) -> Any:
    """
    从响应数据中提取指定字段
    
    Args:
        response_data: 响应数据字典
        field_name: 字段名称
        default: 默认值
    
    Returns:
        Any: 字段值或默认值
    
    Example:
        >>> response = {"success": True, "data": {"id": "123"}}
        >>> data = extract_data_field(response)
        >>> assert data["id"] == "123"
    """
    if not isinstance(response_data, dict):
        return default
    
    return response_data.get(field_name, default)


def is_retryable_status(status_code: int) -> bool:
    """
    判断 HTTP 状态码是否可重试
    
    Args:
        status_code: HTTP 状态码
    
    Returns:
        bool: 是否可重试
    
    Retryable:
        - 408 (Request Timeout)
        - 429 (Too Many Requests)
        - 500-504 (Server Errors)
    """
    retryable_codes = {408, 429, 500, 502, 503, 504}
    return status_code in retryable_codes


def build_error_context(
    error: Exception,
    context: Optional[Dict[str, Any]] = None
) -> Dict[str, Any]:
    """
    构建错误上下文信息
    
    Args:
        error: 异常对象
        context: 额外上下文信息
    
    Returns:
        dict: 错误上下文字典
    
    Example:
        >>> try:
        ...     raise ValueError("测试错误")
        ... except Exception as e:
        ...     ctx = build_error_context(e, {"operation": "submit_task"})
        ...     assert ctx["error_type"] == "ValueError"
    """
    ctx = {
        "error_type": type(error).__name__,
        "error_message": str(error),
        "timestamp": time.time(),
    }
    
    if context:
        ctx.update(context)
    
    if isinstance(error, AgentOSError):
        ctx["error_code"] = error.code
    
    return ctx
