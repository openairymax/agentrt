# AgentOS Python SDK
# Version: 2.0.0
# Last updated: 2026-03-23

"""
AgentOS Python SDK - AgentOS 系统的生产级 Python 接口

功能特性：
    - 高级业务模块（任务、记忆、会话、技能）
    - 完整的类型注解和文档
    - 跨平台支持（Linux、macOS、Windows）
    - 异步编程支持
    - 上下文管理器（with/async with）

快速入门：
    >>> from agentos import AgentOS
    >>> client = AgentOS(endpoint="http://localhost:18789")
    >>> task = client.submit_task('{"input": "analyze this data"}')
    >>> result = task.wait(timeout=30)
"""

__version__ = "2.0.0"
__author__ = "AgentOS Team"
__license__ = "Apache-2.0"

# 导入异常类和错误码常量
from .exceptions import (
    AgentOSError,
    AgentOSMemoryError,
    AgentOSTimeoutError,
    InitializationError,
    ValidationError,
    NetworkError,
    TelemetryError,
    ConfigError,
    SyscallError,
    RateLimitError,
    TaskError,
    SessionError,
    SkillError,
    AuthenticationError,
    http_status_to_code,
    CODE_SUCCESS,
    CODE_UNKNOWN,
    CODE_INVALID_PARAMETER,
    CODE_MISSING_PARAMETER,
    CODE_TIMEOUT,
    CODE_NOT_FOUND,
    CODE_ALREADY_EXISTS,
    CODE_CONFLICT,
    CODE_INVALID_CONFIG,
    CODE_INVALID_ENDPOINT,
    CODE_NETWORK_ERROR,
    CODE_CONNECTION_REFUSED,
    CODE_SERVER_ERROR,
    CODE_UNAUTHORIZED,
    CODE_FORBIDDEN,
    CODE_RATE_LIMITED,
    CODE_INVALID_RESPONSE,
    CODE_PARSE_ERROR,
    CODE_VALIDATION_ERROR,
    CODE_NOT_SUPPORTED,
    CODE_INTERNAL,
    CODE_BUSY,
    CODE_LOOP_CREATE_FAILED,
    CODE_LOOP_START_FAILED,
    CODE_LOOP_STOP_FAILED,
    CODE_COGNITION_FAILED,
    CODE_DAG_BUILD_FAILED,
    CODE_AGENT_DISPATCH_FAILED,
    CODE_INTENT_PARSE_FAILED,
    CODE_TASK_FAILED,
    CODE_TASK_CANCELLED,
    CODE_TASK_TIMEOUT,
    CODE_MEMORY_NOT_FOUND,
    CODE_MEMORY_EVOLVE_FAILED,
    CODE_MEMORY_SEARCH_FAILED,
    CODE_SESSION_NOT_FOUND,
    CODE_SESSION_EXPIRED,
    CODE_SKILL_NOT_FOUND,
    CODE_SKILL_EXECUTION_FAILED,
    CODE_TELEMETRY_ERROR,
    CODE_PERMISSION_DENIED,
    CODE_CORRUPTED_DATA,
)

# 向后兼容别名（不推荐使用新名称）
TimeoutError = AgentOSTimeoutError
MemoryError = AgentOSMemoryError

# 导入客户端
from .agent import AgentOS, AsyncAgentOS

# 导入工具函数
from .utils import (
    generate_id,
    generate_timestamp,
    generate_hash,
    validate_json,
    sanitize_string,
    get_env_var,
    parse_timeout,
    merge_dicts,
    retry_with_backoff,
    Timer,
    RateLimiter,
)

# 导入遥测
from .telemetry import (
    Telemetry,
    Meter,
    Tracer,
    Span,
    SpanStatus,
    MetricPoint,
)

# 导入类型定义
from .types import (
    TaskStatus,
    TaskResult,
    MemoryInfo,
    MemoryRecordType,
    SessionInfo,
    SkillInfo,
    SkillResult,
    TelemetryMetrics,
    Priority,
    TaskID,
    SessionID,
    MemoryRecordID,
    SkillID,
    Timestamp,
    ErrorCode,
    JSONValue,
    JSONObject,
)

__all__ = [
    # 版本信息
    "__version__",
    "__author__",
    "__license__",

    # 异常
    "AgentOSError",
    "AgentOSMemoryError",
    "AgentOSTimeoutError",
    "InitializationError",
    "ValidationError",
    "NetworkError",
    "TelemetryError",
    "ConfigError",
    "SyscallError",
    "RateLimitError",
    "TaskError",
    "SessionError",
    "SkillError",
    "AuthenticationError",
    # 向后兼容别名
    "TimeoutError",
    "MemoryError",

    # 错误码常量
    "http_status_to_code",
    "CODE_SUCCESS",
    "CODE_UNKNOWN",
    "CODE_INVALID_PARAMETER",
    "CODE_MISSING_PARAMETER",
    "CODE_TIMEOUT",
    "CODE_NOT_FOUND",
    "CODE_ALREADY_EXISTS",
    "CODE_CONFLICT",
    "CODE_INVALID_CONFIG",
    "CODE_INVALID_ENDPOINT",
    "CODE_NETWORK_ERROR",
    "CODE_CONNECTION_REFUSED",
    "CODE_SERVER_ERROR",
    "CODE_UNAUTHORIZED",
    "CODE_FORBIDDEN",
    "CODE_RATE_LIMITED",
    "CODE_INVALID_RESPONSE",
    "CODE_PARSE_ERROR",
    "CODE_VALIDATION_ERROR",
    "CODE_NOT_SUPPORTED",
    "CODE_INTERNAL",
    "CODE_BUSY",
    "CODE_LOOP_CREATE_FAILED",
    "CODE_LOOP_START_FAILED",
    "CODE_LOOP_STOP_FAILED",
    "CODE_COGNITION_FAILED",
    "CODE_DAG_BUILD_FAILED",
    "CODE_AGENT_DISPATCH_FAILED",
    "CODE_INTENT_PARSE_FAILED",
    "CODE_TASK_FAILED",
    "CODE_TASK_CANCELLED",
    "CODE_TASK_TIMEOUT",
    "CODE_MEMORY_NOT_FOUND",
    "CODE_MEMORY_EVOLVE_FAILED",
    "CODE_MEMORY_SEARCH_FAILED",
    "CODE_SESSION_NOT_FOUND",
    "CODE_SESSION_EXPIRED",
    "CODE_SKILL_NOT_FOUND",
    "CODE_SKILL_EXECUTION_FAILED",
    "CODE_TELEMETRY_ERROR",
    "CODE_PERMISSION_DENIED",
    "CODE_CORRUPTED_DATA",

    # 客户端
    "AgentOS",
    "AsyncAgentOS",

    # 工具函数
    "generate_id",
    "generate_timestamp",
    "generate_hash",
    "validate_json",
    "sanitize_string",
    "get_env_var",
    "parse_timeout",
    "merge_dicts",
    "retry_with_backoff",
    "Timer",
    "RateLimiter",

    # 遥测
    "Telemetry",
    "Meter",
    "Tracer",
    "Span",
    "SpanStatus",
    "MetricPoint",

    # 类型定义
    "TaskStatus",
    "TaskResult",
    "MemoryInfo",
    "MemoryRecordType",
    "SessionInfo",
    "SkillInfo",
    "SkillResult",
    "TelemetryMetrics",
    "Priority",

    # 类型别名
    "TaskID",
    "SessionID",
    "MemoryRecordID",
    "SkillID",
    "Timestamp",
    "ErrorCode",
    "JSONValue",
    "JSONObject",
]
