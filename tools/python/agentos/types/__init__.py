# AgentOS Python SDK - Type Definitions
# Version: 2.0.0.0

"""
Common type definitions and enums.

This package provides:
    - Common type aliases (TaskID, SessionID, etc.)
    - Task-related types (TaskStatus, TaskResult)
    - Memory-related types (MemoryInfo, MemoryRecordType)
    - Session-related types (SessionInfo)
"""

try:
    from .common import (
        TaskID,
        SessionID,
        MemoryRecordID,
        SkillID,
        Timestamp,
        ErrorCode,
        JSONValue,
        JSONObject,
    )
    from .task import TaskStatus, TaskResult
    from .memory import MemoryInfo, MemoryRecordType
    from .session import SessionInfo
    
    __all__ = [
        "TaskID",
        "SessionID",
        "MemoryRecordID",
        "SkillID",
        "Timestamp",
        "ErrorCode",
        "JSONValue",
        "JSONObject",
        "TaskStatus",
        "TaskResult",
        "MemoryInfo",
        "MemoryRecordType",
        "SessionInfo",
    ]
except ImportError:
    __all__ = []
