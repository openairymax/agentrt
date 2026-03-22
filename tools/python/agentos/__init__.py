# AgentOS Python SDK
# Version: 2.0.0.0
# Last updated: 2026-03-21

"""
AgentOS Python SDK - Production-ready interface to AgentOS system.

This SDK provides a clean, Pythonic interface to interact with the AgentOS system,
featuring:
    - High-level business logic modules (task, memory, session, skill)
    - Low-level FFI bindings with comprehensive error handling
    - Full type annotations and detailed documentation
    - Cross-platform support (Linux, macOS, Windows)
    - Asynchronous programming support

Architecture:
    agentos/
    ├── core/           # Core layer (FFI bindings)
    ├── modules/        # Business modules (task, memory, session, skill)
    ├── client/         # Client classes (sync, async)
    ├── utils/          # Utility functions
    ├── telemetry/      # Observability utilities
    └── types/          # Type definitions

Quick Start:
    >>> from agentos import AgentOS
    >>> client = AgentOS(endpoint="http://localhost:18789")
    >>> 
    >>> # Submit a task
    >>> task = client.submit_task('{"input": "analyze this data"}')
    >>> result = task.wait(timeout=30)
    >>> print(result)

Example:
    >>> # Using context managers
    >>> with AgentOS() as client:
    ...     session = client.create_session()
    ...     with session:
    ...         result = client.execute_skill("my_skill", {"param": "value"})

For more information, see the documentation at https://agentos.dev
"""

__version__ = "2.0.0.0"
__author__ = "AgentOS Team"
__license__ = "Apache-2.0"

# Import version info
try:
    from ._version import (
        __version__,
        __version_info__,
        __author__,
        __license__,
        get_version_string,
        get_version_tuple,
    )
except ImportError:
    pass

# Import exceptions
from .exceptions import (
    AgentOSError,
    InitializationError,
    ValidationError,
    NetworkError,
    TimeoutError,
    TelemetryError,
)

# Import core components (FFI layer)
try:
    from .core import (
        SyscallBinding,
        SyscallProxy,
        get_default_proxy,
    )
except ImportError:
    pass

# Import business modules
try:
    from .modules.task import TaskManager, TaskInfo, TaskResult, TaskStatus, TaskError
    from .modules.memory import MemoryManager, MemoryInfo, MemoryRecordType, MemoryError
    from .modules.session import SessionManager, SessionInfo, SessionError
    from .modules.skill import SkillManager, SkillInfo, SkillResult, SkillError
except ImportError:
    pass

# Import clients
try:
    from .client import AgentOS, AsyncAgentOS
except ImportError:
    # Fallback to old location for backward compatibility
    try:
        from .agent import AgentOS, AsyncAgentOS
    except ImportError:
        pass

# Import utilities
try:
    from .utils import (
        generate_id,
        generate_timestamp,
        generate_hash,
        validate_json,
        sanitize_string,
        Timer,
        RateLimiter,
    )
except ImportError:
    pass

# Import telemetry
try:
    from .telemetry import (
        Telemetry,
        Meter,
        Tracer,
        Span,
        SpanStatus,
        MetricPoint,
    )
except ImportError:
    pass

# Import types
try:
    from .types import (
        TaskID,
        SessionID,
        MemoryRecordID,
        SkillID,
        Timestamp,
        ErrorCode,
        JSONValue,
        JSONObject,
        TaskStatus,
        TaskResult,
        MemoryInfo,
        MemoryRecordType,
        SessionInfo,
        SkillInfo,
        SkillResult,
    )
except ImportError:
    pass
__all__ = [
    # Version info
    "__version__",
    "__author__",
    "__license__",
    
    # Exceptions
    "AgentOSError",
    "InitializationError",
    "ValidationError",
    "NetworkError",
    "TimeoutError",
    "TelemetryError",
    "TaskError",
    "MemoryError",
    "SessionError",
    "SkillError",
    
    # Core (FFI layer)
    "SyscallBinding",
    "SyscallProxy",
    "get_default_proxy",
    
    # Clients
    "AgentOS",
    "AsyncAgentOS",
    
    # Managers
    "TaskManager",
    "MemoryManager",
    "SessionManager",
    "SkillManager",
    
    # Models
    "TaskInfo",
    "TaskResult",
    "TaskStatus",
    "MemoryInfo",
    "MemoryRecordType",
    "SessionInfo",
    "SkillInfo",
    "SkillResult",
    
    # Utilities
    "generate_id",
    "generate_timestamp",
    "generate_hash",
    "validate_json",
    "sanitize_string",
    "Timer",
    "RateLimiter",
    
    # Telemetry
    "Telemetry",
    "Meter",
    "Tracer",
    "Span",
    "SpanStatus",
    "MetricPoint",
    
    # Types
    "TaskID",
    "SessionID",
    "MemoryRecordID",
    "SkillID",
    "Timestamp",
    "ErrorCode",
    "JSONValue",
    "JSONObject",
]
