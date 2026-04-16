# AgentOS Python SDK Exceptions
# Version: 3.0.0
# Last updated: 2026-04-04

"""
Exception classes for the AgentOS Python SDK.

This module defines a comprehensive exception hierarchy for all AgentOS operations.
All exceptions inherit from AgentOSError and include rich error information.
错误码常量与 Go SDK errors.go 保持一致的十六进制体系。
"""

from typing import Optional, Any, Dict


# ===== 十六进制错误码常量（与 Go SDK ErrorCodeReference.md 对齐） =====

CODE_SUCCESS = "0x0000"
CODE_UNKNOWN = "0x0001"
CODE_INVALID_PARAMETER = "0x0002"
CODE_MISSING_PARAMETER = "0x0003"
CODE_TIMEOUT = "0x0004"
CODE_NOT_FOUND = "0x0005"
CODE_ALREADY_EXISTS = "0x0006"
CODE_CONFLICT = "0x0007"
CODE_INVALID_CONFIG = "0x0008"
CODE_INVALID_ENDPOINT = "0x0009"
CODE_NETWORK_ERROR = "0x000A"
CODE_CONNECTION_REFUSED = "0x000B"
CODE_SERVER_ERROR = "0x000C"
CODE_UNAUTHORIZED = "0x000D"
CODE_FORBIDDEN = "0x000E"
CODE_RATE_LIMITED = "0x000F"
CODE_INVALID_RESPONSE = "0x0010"
CODE_PARSE_ERROR = "0x0011"
CODE_VALIDATION_ERROR = "0x0012"
CODE_NOT_SUPPORTED = "0x0013"
CODE_INTERNAL = "0x0014"
CODE_BUSY = "0x0015"

CODE_LOOP_CREATE_FAILED = "0x1001"
CODE_LOOP_START_FAILED = "0x1002"
CODE_LOOP_STOP_FAILED = "0x1003"

CODE_COGNITION_FAILED = "0x2001"
CODE_DAG_BUILD_FAILED = "0x2002"
CODE_AGENT_DISPATCH_FAILED = "0x2003"
CODE_INTENT_PARSE_FAILED = "0x2004"

CODE_TASK_FAILED = "0x3001"
CODE_TASK_TIMEOUT = "0x3002"
CODE_TASK_CANCELLED = "0x3003"
CODE_TASK_NOT_FOUND = "0x3004"

CODE_MEMORY_NOT_FOUND = "0x4001"
CODE_MEMORY_WRITE_FAILED = "0x4002"
CODE_MEMORY_EVOLVE_FAILED = "0x4003"
CODE_MEMORY_SEARCH_FAILED = "0x4004"
CODE_MEMORY_LEVEL_INVALID = "0x4005"

CODE_SESSION_NOT_FOUND = "0x4101"
CODE_SESSION_EXPIRED = "0x4102"
CODE_SKILL_NOT_FOUND = "0x4006"
CODE_SKILL_EXECUTION_FAILED = "0x4007"

CODE_SYSCALL_FAILED = "0x5001"
CODE_SYSCALL_TIMEOUT = "0x5002"
CODE_SYSCALL_INVALID_PARAMS = "0x5003"

CODE_TELEMETRY_ERROR = "0x5101"
CODE_SYSCALL_ERROR = "0x5102"

CODE_PERMISSION_DENIED = "0x6001"
CODE_CORRUPTED_DATA = "0x6002"


HTTP_STATUS_TO_ERROR_MAP = {
    400: lambda details: ValidationError(f"请求参数无效：{details}"),
    401: lambda details: AuthenticationError(f"未授权：{details}"),
    403: lambda details: AuthenticationError(f"禁止访问：{details}"),
    404: lambda details: AgentOSError(message=f"资源不存在：{details}", error_code=CODE_NOT_FOUND),
    408: lambda details: AgentOSTimeoutError(operation="请求"),
    409: lambda details: AgentOSError(message=f"资源冲突：{details}", error_code=CODE_CONFLICT),
    422: lambda details: ValidationError(f"验证失败：{details}"),
    429: lambda details: RateLimitError(f"请求频率超限：{details}"),
}

HTTP_STATUS_TO_CODE_MAP = {
    400: CODE_INVALID_PARAMETER,
    401: CODE_UNAUTHORIZED,
    403: CODE_FORBIDDEN,
    404: CODE_NOT_FOUND,
    408: CODE_TIMEOUT,
    409: CODE_CONFLICT,
    422: CODE_VALIDATION_ERROR,
    429: CODE_RATE_LIMITED,
    500: CODE_SERVER_ERROR,
    502: CODE_SERVER_ERROR,
    503: CODE_SERVER_ERROR,
    504: CODE_TIMEOUT,
}


def http_status_to_code(status: int) -> str:
    """将 HTTP 状态码映射为对应的错误码"""
    return HTTP_STATUS_TO_CODE_MAP.get(status, CODE_UNKNOWN)


def http_status_to_error(status: int, details: str = "") -> "AgentOSError":
    """
    将 HTTP 状态码转换为对应的异常实例，与 Go SDK HTTPStatusToError 一致。

    Args:
        status: HTTP 状态码
        details: 错误详细信息

    Returns:
        对应的异常实例
    """
    if status >= 500:
        return ServerError(f"服务器错误 ({status}): {details}")
    
    error_factory = HTTP_STATUS_TO_ERROR_MAP.get(status)
    if error_factory:
        return error_factory(details)
    
    return AgentOSError(message=f"HTTP 错误 ({status}): {details}", error_code=http_status_to_code(status))


class AgentOSError(Exception):
    """
    Base exception class for all AgentOS errors.

    Attributes:
        error_code (str): The hexadecimal error code (e.g., "0x0001")
        message (str): Human-readable error description
        cause (Exception): The underlying cause of this error, if any
    """

    def __init__(
        self,
        message: str = "",
        error_code: str = CODE_UNKNOWN,
        cause: Optional[Exception] = None,
    ):
        super().__init__(message)
        self.error_code = error_code
        self.message = message
        self.cause = cause

    def __str__(self) -> str:
        if self.cause:
            return f"[{self.error_code}] {self.message}: {self.cause}"
        return f"[{self.error_code}] {self.message}"

    def __repr__(self) -> str:
        return f"{self.__class__.__name__}(error_code={self.error_code!r}, message={self.message!r})"

    def __eq__(self, other: object) -> bool:
        if not isinstance(other, AgentOSError):
            return NotImplemented
        return self.error_code == other.error_code

    def __hash__(self) -> int:
        return hash(self.error_code)


class NetworkError(AgentOSError):
    """Network communication error."""

    def __init__(self, message: str = "网络连接失败", cause: Optional[Exception] = None):
        super().__init__(message=message, error_code=CODE_NETWORK_ERROR, cause=cause)


class TimeoutError(AgentOSError):
    """Operation timeout error."""

    def __init__(self, message: str = "操作超时", cause: Optional[Exception] = None):
        super().__init__(message=message, error_code=CODE_TIMEOUT, cause=cause)


class ValidationError(AgentOSError):
    """Input validation error."""

    def __init__(self, message: str = "输入验证失败", cause: Optional[Exception] = None):
        super().__init__(message=message, error_code=CODE_VALIDATION_ERROR, cause=cause)


class AuthenticationError(AgentOSError):
    """Authentication error."""

    def __init__(self, message: str = "认证失败", cause: Optional[Exception] = None):
        super().__init__(message=message, error_code=CODE_UNAUTHORIZED, cause=cause)


class ServerError(AgentOSError):
    """Server-side error."""

    def __init__(self, message: str = "服务器错误", cause: Optional[Exception] = None):
        super().__init__(message=message, error_code=CODE_SERVER_ERROR, cause=cause)


class RateLimitError(AgentOSError):
    """Rate limit exceeded error."""

    def __init__(self, message: str = "请求频率超限", cause: Optional[Exception] = None):
        super().__init__(message=message, error_code=CODE_RATE_LIMITED, cause=cause)


class TaskError(AgentOSError):
    """Task-related error."""

    def __init__(self, message: str = "", cause: Optional[Exception] = None):
        super().__init__(message=message, error_code=CODE_TASK_FAILED, cause=cause)


class MemoryError(AgentOSError):
    """Memory-related error."""

    def __init__(self, message: str = "", cause: Optional[Exception] = None):
        super().__init__(message=message, error_code=CODE_MEMORY_NOT_FOUND, cause=cause)


class SessionError(AgentOSError):
    """Session-related error."""

    def __init__(self, message: str = "", cause: Optional[Exception] = None):
        super().__init__(message=message, error_code=CODE_SESSION_NOT_FOUND, cause=cause)


class AgentOSTimeoutError(TimeoutError):
    """AgentOS-specific timeout error with operation context."""

    def __init__(self, operation: str = "", cause: Optional[Exception] = None):
        message = f"操作超时：{operation}" if operation else "操作超时"
        super().__init__(message=message, cause=cause)
        self.operation = operation


class ConfigurationError(AgentOSError):
    """Configuration error."""

    def __init__(self, message: str = "配置错误", cause: Optional[Exception] = None):
        super().__init__(message=message, error_code=CODE_INVALID_CONFIG, cause=cause)


class InitializationError(AgentOSError):
    """Initialization error."""

    def __init__(self, message: str = "初始化失败", cause: Optional[Exception] = None):
        super().__init__(message=message, error_code=CODE_INTERNAL, cause=cause)


class TelemetryError(AgentOSError):
    """Telemetry-related error."""

    def __init__(self, message: str = "遥测错误", cause: Optional[Exception] = None):
        super().__init__(message=message, error_code=CODE_TELEMETRY_ERROR, cause=cause)


class SyscallError(AgentOSError):
    """System call error."""

    def __init__(self, message: str = "系统调用失败", cause: Optional[Exception] = None):
        super().__init__(message=message, error_code=CODE_SYSCALL_FAILED, cause=cause)


class SkillError(AgentOSError):
    """Skill-related error."""

    def __init__(self, message: str = "", cause: Optional[Exception] = None):
        super().__init__(message=message, error_code=CODE_SKILL_EXECUTION_FAILED, cause=cause)


class InvalidResponseError(AgentOSError):
    """Invalid response error."""

    def __init__(self, message: str = "响应格式异常", cause: Optional[Exception] = None):
        super().__init__(message=message, error_code=CODE_INVALID_RESPONSE, cause=cause)


AgentOSMemoryError = MemoryError
ConfigError = ConfigurationError
