# AgentOS Python SDK Exceptions
# Version: 2.0.0
# Last updated: 2026-03-23

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
CODE_TASK_CANCELLED = "0x3002"
CODE_TASK_TIMEOUT = "0x3003"

CODE_MEMORY_NOT_FOUND = "0x4001"
CODE_MEMORY_EVOLVE_FAILED = "0x4002"
CODE_MEMORY_SEARCH_FAILED = "0x4003"
CODE_SESSION_NOT_FOUND = "0x4004"
CODE_SESSION_EXPIRED = "0x4005"
CODE_SKILL_NOT_FOUND = "0x4006"
CODE_SKILL_EXECUTION_FAILED = "0x4007"

CODE_TELEMETRY_ERROR = "0x5001"

CODE_PERMISSION_DENIED = "0x6001"
CODE_CORRUPTED_DATA = "0x6002"


def http_status_to_code(status: int) -> str:
    """将 HTTP 状态码映射到 AgentOS 错误码，与 Go SDK HTTPStatusToError 一致。"""
    mapping = {
        400: CODE_INVALID_PARAMETER,
        401: CODE_UNAUTHORIZED,
        403: CODE_FORBIDDEN,
        404: CODE_NOT_FOUND,
        408: CODE_TIMEOUT,
        409: CODE_CONFLICT,
        429: CODE_RATE_LIMITED,
        422: CODE_VALIDATION_ERROR,
        500: CODE_SERVER_ERROR,
        502: CODE_SERVER_ERROR,
        503: CODE_SERVER_ERROR,
        504: CODE_TIMEOUT,
    }
    return mapping.get(status, CODE_UNKNOWN)


class AgentOSError(Exception):
    """
    Base exception class for all AgentOS errors.
    
    Attributes:
        error_code (str): The hexadecimal error code (e.g., "0x0001")
        message (str): Human-readable error description
        details (Optional[Dict]): Additional error context
    """
    
    def __init__(
        self,
        message: str = "",
        error_code: str = CODE_UNKNOWN,
        details: Optional[Dict[str, Any]] = None
    ):
        self.error_code = error_code
        self.message = message
        self.details = details or {}
        super().__init__(self._format_message())
    
    def _format_message(self) -> str:
        """Format the error message with error code and details."""
        base_msg = f"[{self.error_code}] {self.message}"
        
        if self.details:
            details_str = ", ".join(f"{k}={v}" for k, v in self.details.items())
            return f"{base_msg} ({details_str})"
        
        return base_msg
    
    def to_dict(self) -> Dict[str, Any]:
        """Convert exception to dictionary for serialization."""
        return {
            "error_code": self.error_code,
            "message": self.message,
            "details": self.details,
            "type": self.__class__.__name__
        }


class TaskError(AgentOSError):
    """Exception raised for task-related errors."""
    
    def __init__(self, message: str = "", **kwargs):
        super().__init__(message=message, error_code=CODE_TASK_FAILED, **kwargs)


class AgentOSMemoryError(AgentOSError):
    """Exception raised for memory operation errors."""
    
    def __init__(self, message: str = "", **kwargs):
        super().__init__(message=message, error_code=CODE_MEMORY_NOT_FOUND, **kwargs)


class SessionError(AgentOSError):
    """Exception raised for session management errors."""
    
    def __init__(self, message: str = "", **kwargs):
        super().__init__(message=message, error_code=CODE_SESSION_NOT_FOUND, **kwargs)


class SkillError(AgentOSError):
    """Exception raised for skill loading and execution errors."""
    
    def __init__(self, message: str = "", **kwargs):
        super().__init__(message=message, error_code=CODE_SKILL_EXECUTION_FAILED, **kwargs)


class TelemetryError(AgentOSError):
    """Exception raised for telemetry and observability errors."""
    
    def __init__(self, message: str = "", **kwargs):
        super().__init__(message=message, error_code=CODE_TELEMETRY_ERROR, **kwargs)


class InitializationError(AgentOSError):
    """Exception raised when SDK initialization fails."""
    pass


class AgentOSTimeoutError(AgentOSError):
    """
    Exception raised when an operation times out.
    
    Attributes:
        timeout_ms (int): The timeout value that was exceeded
        operation (str): The operation that timed out
    """
    
    def __init__(self, timeout_ms: int = 0, operation: str = "operation", **kwargs):
        super().__init__(
            message=f"{operation.capitalize()} timed out after {timeout_ms}ms",
            error_code=CODE_TIMEOUT,
            **kwargs
        )
        self.timeout_ms = timeout_ms
        self.operation = operation


class ValidationError(AgentOSError):
    """Exception raised when input validation fails."""
    
    def __init__(self, message: str = "", **kwargs):
        super().__init__(message=message, error_code=CODE_VALIDATION_ERROR, **kwargs)


class NetworkError(AgentOSError):
    """Exception raised for network-related errors."""
    
    def __init__(self, message: str = "", **kwargs):
        super().__init__(message=message, error_code=CODE_NETWORK_ERROR, **kwargs)


class AuthenticationError(AgentOSError):
    """Exception raised for authentication errors."""
    
    def __init__(self, message: str = "", **kwargs):
        super().__init__(message=message, error_code=CODE_UNAUTHORIZED, **kwargs)


class ConfigError(AgentOSError):
    """Exception raised for configuration errors."""
    
    def __init__(self, message: str = "", **kwargs):
        super().__init__(message=message, error_code=CODE_INVALID_CONFIG, **kwargs)


class SyscallError(AgentOSError):
    """Exception raised for system call errors."""
    
    def __init__(self, message: str = "", **kwargs):
        super().__init__(message=message, error_code=CODE_TELEMETRY_ERROR, **kwargs)


class RateLimitError(AgentOSError):
    """Exception raised when rate limit is exceeded."""
    
    def __init__(self, message: str = "Rate limit exceeded", **kwargs):
        super().__init__(message=message, error_code=CODE_RATE_LIMITED, **kwargs)


# 向后兼容别名（不推荐使用，将在未来版本中移除）
MemoryError = AgentOSMemoryError
TimeoutError = AgentOSTimeoutError
