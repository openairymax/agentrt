# AgentOS Python SDK Types
# Version: 1.0.0.5
# Last updated: 2026-03-21

"""
Type definitions for the AgentOS Python SDK.
"""

from enum import Enum
from typing import Dict, Any, Optional, List

class TaskStatus(Enum):
    """Task status enumeration."""
    PENDING = "pending"
    RUNNING = "running"
    COMPLETED = "completed"
    FAILED = "failed"
    CANCELLED = "cancelled"

class TaskResult:
    """Task result class."""
    def __init__(self, task_id: str, status: TaskStatus, output: Optional[str] = None, error: Optional[str] = None):
        self.task_id = task_id
        self.status = status
        self.output = output
        self.error = error

    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary."""
        return {
            "task_id": self.task_id,
            "status": self.status.value,
            "output": self.output,
            "error": self.error
        }

class MemoryInfo:
    """Memory information class."""
    def __init__(self, memory_id: str, content: str, created_at: str, metadata: Optional[Dict[str, Any]] = None):
        self.memory_id = memory_id
        self.content = content
        self.created_at = created_at
        self.metadata = metadata or {}

    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary."""
        return {
            "memory_id": self.memory_id,
            "content": self.content,
            "created_at": self.created_at,
            "metadata": self.metadata
        }

class SkillInfo:
    """Skill information class."""
    def __init__(self, skill_name: str, description: str, version: str, parameters: Optional[Dict[str, Any]] = None):
        self.skill_name = skill_name
        self.description = description
        self.version = version
        self.parameters = parameters or {}

    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary."""
        return {
            "skill_name": self.skill_name,
            "description": self.description,
            "version": self.version,
            "parameters": self.parameters
        }

class SkillResult:
    """Skill execution result class."""
    def __init__(self, success: bool, output: Optional[Any] = None, error: Optional[str] = None):
        self.success = success
        self.output = output
        self.error = error

    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary."""
        return {
            "success": self.success,
            "output": self.output,
            "error": self.error
        }
