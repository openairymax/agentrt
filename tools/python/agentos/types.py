# AgentOS Python SDK - Types and Data Structures
# Version: 1.0.0.6
# Last updated: 2026-03-21

"""
Type definitions and data structures for the AgentOS Python SDK.

This module provides type-safe data structures for interacting with AgentOS,
including enums, dataclasses, and type aliases.
"""

from enum import Enum, IntEnum
from dataclasses import dataclass, field
from typing import Optional, Dict, Any, List, Union
import time


class TaskStatus(IntEnum):
    """
    Enumeration of possible task states.
    
    Matches the C API status codes for direct mapping.
    """
    PENDING = 0      # Task submitted but not started
    RUNNING = 1      # Task currently executing
    SUCCEEDED = 2    # Task completed successfully
    FAILED = 3       # Task failed with error
    CANCELLED = 4    # Task was cancelled


class MemoryRecordType(IntEnum):
    """
    Types of memory records supported by AgentOS.
    """
    RAW = 0              # Raw unprocessed data
    FEATURE = 1          # Extracted features (embeddings)
    STRUCTURE = 2        # Structured relationships
    PATTERN = 3          # Abstracted patterns


class Priority(Enum):
    """
    Task priority levels.
    """
    LOW = 0
    NORMAL = 1
    HIGH = 2
    CRITICAL = 3


@dataclass
class TaskResult:
    """
    Result of a task execution.
    
    Attributes:
        task_id: Unique identifier for the task
        status: Current status of the task
        result: Optional result data (JSON object)
        error: Optional error message if task failed
        created_at: Timestamp when task was created
        completed_at: Timestamp when task completed
    """
    task_id: str
    status: TaskStatus
    result: Optional[Dict[str, Any]] = None
    error: Optional[str] = None
    created_at: float = field(default_factory=time.time)
    completed_at: Optional[float] = None
    
    def is_success(self) -> bool:
        """Check if task succeeded."""
        return self.status == TaskStatus.SUCCEEDED
    
    def is_failed(self) -> bool:
        """Check if task failed."""
        return self.status == TaskStatus.FAILED
    
    def is_pending(self) -> bool:
        """Check if task is still pending."""
        return self.status == TaskStatus.PENDING
    
    def is_running(self) -> bool:
        """Check if task is running."""
        return self.status == TaskStatus.RUNNING
    
    def is_cancelled(self) -> bool:
        """Check if task was cancelled."""
        return self.status == TaskStatus.CANCELLED
    
    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary representation."""
        return {
            "task_id": self.task_id,
            "status": self.status.name,
            "status_code": self.status.value,
            "result": self.result,
            "error": self.error,
            "created_at": self.created_at,
            "completed_at": self.completed_at
        }
    
    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> "TaskResult":
        """Create TaskResult from dictionary."""
        raw_status = data.get("status", 0)
        if isinstance(raw_status, str):
            status = TaskStatus[raw_status]
        elif isinstance(raw_status, int):
            status = TaskStatus(raw_status)
        else:
            status = TaskStatus.PENDING
        return cls(
            task_id=data["task_id"],
            status=status,
            result=data.get("result"),
            error=data.get("error"),
            created_at=data.get("created_at", time.time()),
            completed_at=data.get("completed_at")
        )


@dataclass
class MemoryInfo:
    """
    Information about a memory record.
    
    Attributes:
        record_id: Unique identifier for the memory record
        record_type: Type of memory record
        data_size: Size of the data in bytes
        metadata: Optional metadata associated with the record
        created_at: Timestamp when record was created
        access_count: Number of times this record has been accessed
        importance_score: Importance score (0.0-1.0)
    """
    record_id: str
    record_type: MemoryRecordType
    data_size: int
    metadata: Optional[Dict[str, Any]] = None
    created_at: float = field(default_factory=time.time)
    access_count: int = 0
    importance_score: float = 0.0
    
    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary representation."""
        return {
            "record_id": self.record_id,
            "record_type": self.record_type.name,
            "data_size": self.data_size,
            "metadata": self.metadata,
            "created_at": self.created_at,
            "access_count": self.access_count,
            "importance_score": self.importance_score
        }
    
    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> "MemoryInfo":
        """Create MemoryInfo from dictionary."""
        raw_type = data.get("record_type", 0)
        if isinstance(raw_type, str):
            record_type = MemoryRecordType[raw_type]
        elif isinstance(raw_type, int):
            record_type = MemoryRecordType(raw_type)
        else:
            record_type = MemoryRecordType.RAW
        return cls(
            record_id=data["record_id"],
            record_type=record_type,
            data_size=data["data_size"],
            metadata=data.get("metadata"),
            created_at=data.get("created_at", time.time()),
            access_count=data.get("access_count", 0),
            importance_score=data.get("importance_score", 0.0)
        )


@dataclass
class SessionInfo:
    """
    Information about an active session.
    
    Attributes:
        session_id: Unique identifier for the session
        metadata: Session metadata (JSON object)
        created_at: Timestamp when session was created
        last_active: Timestamp of last activity
        task_count: Number of tasks in this session
        status: Session status (active/closed)
    """
    session_id: str
    metadata: Optional[Dict[str, Any]] = None
    created_at: float = field(default_factory=time.time)
    last_active: float = field(default_factory=time.time)
    task_count: int = 0
    status: str = "active"
    
    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary representation."""
        return {
            "session_id": self.session_id,
            "metadata": self.metadata,
            "created_at": self.created_at,
            "last_active": self.last_active,
            "task_count": self.task_count,
            "status": self.status
        }
    
    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> "SessionInfo":
        """Create SessionInfo from dictionary."""
        return cls(
            session_id=data["session_id"],
            metadata=data.get("metadata"),
            created_at=data.get("created_at", time.time()),
            last_active=data.get("last_active", time.time()),
            task_count=data.get("task_count", 0),
            status=data.get("status", "active")
        )


@dataclass
class SkillInfo:
    """
    Information about a skill.
    
    Attributes:
        skill_id: Unique identifier for the skill
        name: Human-readable skill name
        version: Skill version
        description: Skill description
        enabled: Whether the skill is enabled
    """
    skill_id: str
    name: str
    version: str
    description: Optional[str] = None
    enabled: bool = True
    
    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary representation."""
        return {
            "skill_id": self.skill_id,
            "name": self.name,
            "version": self.version,
            "description": self.description,
            "enabled": self.enabled
        }
    
    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> "SkillInfo":
        """Create SkillInfo from dictionary."""
        return cls(
            skill_id=data["skill_id"],
            name=data["name"],
            version=data["version"],
            description=data.get("description"),
            enabled=data.get("enabled", True)
        )


@dataclass
class SkillResult:
    """
    Result of skill execution.
    
    Attributes:
        success: Whether the execution was successful
        output: Output data from skill execution
        error: Error message if execution failed
        execution_time_ms: Execution time in milliseconds
    """
    success: bool
    output: Optional[Any] = None
    error: Optional[str] = None
    execution_time_ms: Optional[float] = None
    
    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary representation."""
        return {
            "success": self.success,
            "output": self.output,
            "error": self.error,
            "execution_time_ms": self.execution_time_ms
        }
    
    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> "SkillResult":
        """Create SkillResult from dictionary."""
        return cls(
            success=data.get("success", False),
            output=data.get("output"),
            error=data.get("error"),
            execution_time_ms=data.get("execution_time_ms")
        )


@dataclass
class TelemetryMetrics:
    """
    System telemetry metrics.
    
    Attributes:
        timestamp: Timestamp when metrics were collected
        cpu_usage_percent: CPU usage percentage
        memory_usage_bytes: Memory usage in bytes
        active_tasks: Number of active tasks
        queued_tasks: Number of queued tasks
        memory_records: Total number of memory records
        sessions_active: Number of active sessions
        uptime_seconds: System uptime in seconds
    """
    timestamp: float = field(default_factory=time.time)
    cpu_usage_percent: float = 0.0
    memory_usage_bytes: int = 0
    active_tasks: int = 0
    queued_tasks: int = 0
    memory_records: int = 0
    sessions_active: int = 0
    uptime_seconds: float = 0.0
    
    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary representation."""
        return {
            "timestamp": self.timestamp,
            "cpu_usage_percent": self.cpu_usage_percent,
            "memory_usage_bytes": self.memory_usage_bytes,
            "active_tasks": self.active_tasks,
            "queued_tasks": self.queued_tasks,
            "memory_records": self.memory_records,
            "sessions_active": self.sessions_active,
            "uptime_seconds": self.uptime_seconds
        }
    
    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> "TelemetryMetrics":
        """Create TelemetryMetrics from dictionary."""
        return cls(
            timestamp=data.get("timestamp", time.time()),
            cpu_usage_percent=data.get("cpu_usage_percent", 0.0),
            memory_usage_bytes=data.get("memory_usage_bytes", 0),
            active_tasks=data.get("active_tasks", 0),
            queued_tasks=data.get("queued_tasks", 0),
            memory_records=data.get("memory_records", 0),
            sessions_active=data.get("sessions_active", 0),
            uptime_seconds=data.get("uptime_seconds", 0.0)
        )


# Type aliases for common types
TaskID = str
SessionID = str
MemoryRecordID = str
SkillID = str
Timestamp = float
ErrorCode = int

# JSON-compatible types
JSONValue = Union[str, int, float, bool, None, Dict[str, Any], List[Any]]
JSONObject = Dict[str, JSONValue]
