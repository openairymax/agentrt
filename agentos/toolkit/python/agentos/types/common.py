# AgentOS Python SDK - Types Module
# Version: 0.1.0
# Last updated: 2026-03-24

"""
Type definitions for AgentOS Python SDK.

This module provides all enumeration types, domain models, and
request/response structures used throughout the SDK.

Corresponds to Go SDK: types/types.go
"""

from enum import Enum, IntEnum
from dataclasses import dataclass, field
from typing import Any, Dict, List, Optional
from datetime import datetime
import time


# ============================================================
# 枚举类型
# ============================================================

class TaskStatus(str, Enum):
    """
    任务状态枚举。

    对应 Go SDK: types.TaskStatus
    """
    PENDING = "pending"
    RUNNING = "running"
    COMPLETED = "completed"
    FAILED = "failed"
    CANCELLED = "cancelled"

    def is_terminal(self) -> bool:
        """
        判断任务是否处于终态。

        Returns:
            bool: 如果任务已完成、失败或取消，返回 True
        """
        return self in (TaskStatus.COMPLETED, TaskStatus.FAILED, TaskStatus.CANCELLED)


class MemoryLayer(str, Enum):
    """
    记忆层级枚举（对应认知深度分层）。

    对应 Go SDK: types.MemoryLayer
    """
    L1 = "L1"
    L2 = "L2"
    L3 = "L3"
    L4 = "L4"

    def is_valid(self) -> bool:
        """
        判断记忆层级是否合法。

        Returns:
            bool: 如果是有效的层级，返回 True
        """
        return self in (MemoryLayer.L1, MemoryLayer.L2, MemoryLayer.L3, MemoryLayer.L4)


class MemoryRecordType(IntEnum):
    """
    Types of memory records supported by AgentOS.

    对应 Go SDK: types.MemoryRecordType
    """
    RAW = 0
    FEATURE = 1
    STRUCTURE = 2
    PATTERN = 3


class SessionStatus(str, Enum):
    """
    会话状态枚举。

    对应 Go SDK: types.SessionStatus
    """
    ACTIVE = "active"
    INACTIVE = "inactive"
    EXPIRED = "expired"


class SkillStatus(str, Enum):
    """
    技能状态枚举。

    对应 Go SDK: types.SkillStatus
    """
    ACTIVE = "active"
    INACTIVE = "inactive"
    DEPRECATED = "deprecated"


class SpanStatus(str, Enum):
    """
    遥测 Span 状态枚举。

    对应 Go SDK: types.SpanStatus
    """
    OK = "ok"
    ERROR = "error"
    UNSET = "unset"


# ============================================================
# 领域模型
# ============================================================

@dataclass
class Task:
    """
    表示 AgentOS 系统中的一个执行任务。

    对应 Go SDK: types.Task
    """
    id: str
    description: str
    status: TaskStatus
    priority: int = 0
    output: str = ""
    error: str = ""
    metadata: Dict[str, Any] = field(default_factory=dict)
    created_at: Optional[datetime] = None
    updated_at: Optional[datetime] = None


@dataclass
class TaskResult:
    """
    表示已完成任务的结果快照。

    对应 Go SDK: types.TaskResult
    """
    id: str
    status: TaskStatus
    output: str = ""
    error: str = ""
    start_time: Optional[datetime] = None
    end_time: Optional[datetime] = None
    duration: float = 0.0


@dataclass
class Memory:
    """
    表示 AgentOS 系统中的一条记忆记录。

    对应 Go SDK: types.Memory
    """
    id: str
    content: str
    layer: MemoryLayer
    score: float = 1.0
    metadata: Dict[str, Any] = field(default_factory=dict)
    created_at: Optional[datetime] = None
    updated_at: Optional[datetime] = None


@dataclass
class MemorySearchResult:
    """
    表示记忆搜索的聚合结果。

    对应 Go SDK: types.MemorySearchResult
    """
    memories: List[Memory]
    total: int
    query: str
    top_k: int


@dataclass
class MemoryInfo:
    """
    Information about a memory record.

    对应 Go SDK: types.MemoryInfo
    """
    record_id: str
    record_type: MemoryRecordType
    data_size: int
    metadata: Optional[Dict[str, Any]] = None
    created_at: float = field(default_factory=time.time)
    access_count: int = 0
    importance_score: float = 0.0

    def to_dict(self) -> Dict[str, Any]:
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
class Session:
    """
    表示用户与 Agent 交互的有状态通道。

    对应 Go SDK: types.Session
    """
    id: str
    user_id: str
    status: SessionStatus
    context: Dict[str, Any] = field(default_factory=dict)
    metadata: Dict[str, Any] = field(default_factory=dict)
    created_at: Optional[datetime] = None
    last_activity: Optional[datetime] = None


@dataclass
class Skill:
    """
    表示 AgentOS 系统中的可插拔能力单元。

    对应 Go SDK: types.Skill
    """
    id: str
    name: str
    version: str = ""
    description: str = ""
    status: SkillStatus = SkillStatus.ACTIVE
    parameters: Dict[str, Any] = field(default_factory=dict)
    metadata: Dict[str, Any] = field(default_factory=dict)
    created_at: Optional[datetime] = None


@dataclass
class SkillResult:
    """
    表示技能执行的结果。

    对应 Go SDK: types.SkillResult
    """
    success: bool
    output: Any = None
    error: str = ""


@dataclass
class SkillInfo:
    """
    表示技能的只读元信息。

    对应 Go SDK: types.SkillInfo
    """
    name: str
    description: str
    version: str
    parameters: Dict[str, Any] = field(default_factory=dict)


# ============================================================
# 列表查询选项
# ============================================================

@dataclass
class PaginationOptions:
    """
    分页选项。

    对应 Go SDK: types.PaginationOptions
    """
    page: int = 1
    page_size: int = 20

    def to_query_params(self) -> Dict[str, str]:
        """
        将分页参数转换为查询参数字典。

        Returns:
            Dict[str, str]: 查询参数字典
        """
        params = {}
        if self.page > 0:
            params["page"] = str(self.page)
        if self.page_size > 0:
            params["page_size"] = str(self.page_size)
        return params


@dataclass
class SortOptions:
    """
    排序选项。

    对应 Go SDK: types.SortOptions
    """
    field: str = ""
    order: str = "asc"


@dataclass
class FilterOptions:
    """
    过滤选项。

    对应 Go SDK: types.FilterOptions
    """
    key: str = ""
    value: Any = None


@dataclass
class ListOptions:
    """
    列表查询的复合选项。

    对应 Go SDK: types.ListOptions
    """
    pagination: Optional[PaginationOptions] = None
    sort: Optional[SortOptions] = None
    filter: Optional[FilterOptions] = None

    def to_query_params(self) -> Dict[str, str]:
        """
        将列表选项转换为查询参数字典。

        Returns:
            Dict[str, str]: 查询参数字典
        """
        params = {}
        if self.pagination:
            params.update(self.pagination.to_query_params())
        if self.sort:
            if self.sort.field:
                params["sort_by"] = self.sort.field
            if self.sort.order:
                params["sort_order"] = self.sort.order
        if self.filter:
            if self.filter.key:
                params["filter_key"] = self.filter.key
                params["filter_value"] = str(self.filter.value)
        return params
