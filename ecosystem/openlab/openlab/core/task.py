"""
OpenLab Core Task Module

Task scheduling and state machine core module
Following AgentOS architecture design principles V1.8
"""

from dataclasses import dataclass, field
from enum import Enum
from typing import Any, Dict, List, Optional, Callable, Awaitable
import asyncio
import time
import uuid


class TaskStatus(Enum):
    PENDING = "pending"
    QUEUED = "queued"
    RUNNING = "running"
    PAUSED = "paused"
    COMPLETED = "completed"
    FAILED = "failed"
    CANCELLED = "cancelled"
    TIMEOUT = "timeout"


class TaskCategory(Enum):
    IMMEDIATE = "immediate"
    SCHEDULED = "scheduled"
    PERIODIC = "periodic"
    LONG_RUNNING = "long_running"


@dataclass
class TaskDefinition:
    task_id: str = field(default_factory=lambda: str(uuid.uuid4()))
    name: str = ""
    description: str = ""
    category: TaskCategory = TaskCategory.IMMEDIATE
    priority: int = 0
    input_data: Optional[Dict[str, Any]] = None
    metadata: Dict[str, Any] = field(default_factory=dict)
    timeout: Optional[float] = None
    max_retries: int = 0
    created_at: float = field(default_factory=time.time)
    scheduled_at: Optional[float] = None

    def __post_init__(self):
        if not self.task_id:
            self.task_id = str(uuid.uuid4())


@dataclass
class TaskState:
    task_id: str
    status: TaskStatus = TaskStatus.PENDING
    progress: float = 0.0
    result: Optional[Any] = None
    error: Optional[str] = None
    error_code: Optional[str] = None
    retry_count: int = 0
    started_at: Optional[float] = None
    completed_at: Optional[float] = None
    checkpoint_data: Optional[Dict[str, Any]] = None
    metrics: Dict[str, Any] = field(default_factory=dict)

    def to_dict(self) -> Dict[str, Any]:
        return {
            "task_id": self.task_id,
            "status": self.status.value,
            "progress": self.progress,
            "result": self.result,
            "error": self.error,
            "error_code": self.error_code,
            "retry_count": self.retry_count,
            "started_at": self.started_at,
            "completed_at": self.completed_at,
            "checkpoint_data": self.checkpoint_data,
            "metrics": self.metrics,
        }

    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> "TaskState":
        return cls(
            task_id=data["task_id"],
            status=TaskStatus(data["status"]),
            progress=data.get("progress", 0.0),
            result=data.get("result"),
            error=data.get("error"),
            error_code=data.get("error_code"),
            retry_count=data.get("retry_count", 0),
            started_at=data.get("started_at"),
            completed_at=data.get("completed_at"),
            checkpoint_data=data.get("checkpoint_data"),
            metrics=data.get("metrics", {}),
        )


@dataclass
class ExecutionPlan:
    plan_id: str
    task_id: str
    steps: List[Dict[str, Any]] = field(default_factory=list)
    resources: Dict[str, Any] = field(default_factory=dict)
    estimated_duration: Optional[float] = None
    actual_duration: Optional[float] = None
    created_at: float = field(default_factory=time.time)

    def add_step(self, step: Dict[str, Any]) -> None:
        self.steps.append(step)

    def get_next_step(self, current_step_index: int) -> Optional[Dict[str, Any]]:
        if current_step_index < len(self.steps):
            return self.steps[current_step_index]
        return None


class TaskScheduler:

    def __init__(self, max_concurrent: int = 100, max_queue_size: int = 10000):
        self.max_concurrent = max_concurrent
        self.max_queue_size = max_queue_size
        self._queue: asyncio.PriorityQueue = asyncio.PriorityQueue()
        self._running_tasks: Dict[str, asyncio.Task] = {}
        self._task_states: Dict[str, TaskState] = {}
        self._execution_plans: Dict[str, ExecutionPlan] = {}
        self._lock = asyncio.Lock()
        self._semaphore: asyncio.Semaphore = asyncio.Semaphore(max_concurrent)
        self._shutdown = False

    async def submit(self, definition: TaskDefinition) -> str:
        if self._shutdown:
            raise RuntimeError("Scheduler is shutting down")

        if self._queue.qsize() >= self.max_queue_size:
            raise RuntimeError("Task queue is full")

        state = TaskState(task_id=definition.task_id)
        async with self._lock:
            self._task_states[definition.task_id] = state

        priority = -definition.priority
        await self._queue.put((priority, definition))

        return definition.task_id

    async def schedule(
        self,
        definition: TaskDefinition,
        executor: Callable[[TaskDefinition], Awaitable[Any]]
    ) -> asyncio.Task:

        async def run_task():
            async with self._semaphore:
                if self._shutdown:
                    return

                task_id = definition.task_id
                state = self._task_states.get(task_id)
                if not state:
                    return

                try:
                    state.status = TaskStatus.RUNNING
                    state.started_at = time.time()

                    result = await asyncio.wait_for(
                        executor(definition),
                        timeout=definition.timeout
                    )

                    state.status = TaskStatus.COMPLETED
                    state.result = result
                    state.progress = 1.0
                    state.completed_at = time.time()

                except asyncio.TimeoutError:
                    state.status = TaskStatus.TIMEOUT
                    state.error = "Task timeout"

                except asyncio.CancelledError:
                    state.status = TaskStatus.CANCELLED
                    raise

                except Exception as e:
                    state.status = TaskStatus.FAILED
                    state.error = str(e)

                finally:
                    state.completed_at = time.time()

        async_task = asyncio.create_task(run_task())

        async with self._lock:
            self._running_tasks[definition.task_id] = async_task

        return async_task

    async def get_state(self, task_id: str) -> Optional[TaskState]:
        async with self._lock:
            return self._task_states.get(task_id)

    async def cancel(self, task_id: str) -> bool:
        async with self._lock:
            task = self._running_tasks.get(task_id)
            if task and not task.done():
                task.cancel()
                return True
            return False

    async def save_checkpoint(self, task_id: str, checkpoint_data: Dict[str, Any]) -> bool:
        async with self._lock:
            state = self._task_states.get(task_id)
            if state:
                state.checkpoint_data = checkpoint_data
                return True
            return False

    async def load_checkpoint(self, task_id: str) -> Optional[Dict[str, Any]]:
        async with self._lock:
            state = self._task_states.get(task_id)
            if state:
                return state.checkpoint_data
            return None

    async def shutdown(self, wait: bool = True, timeout: Optional[float] = None) -> None:
        self._shutdown = True

        async with self._lock:
            for task_id, task in self._running_tasks.items():
                if not task.done():
                    task.cancel()

            if wait:
                if self._running_tasks:
                    await asyncio.wait(
                        self._running_tasks.values(),
                        timeout=timeout
                    )

    def get_stats(self) -> Dict[str, Any]:
        return {
            "queue_size": self._queue.qsize(),
            "running_tasks": len(self._running_tasks),
            "total_tasks": len(self._task_states),
            "max_concurrent": self.max_concurrent,
            "max_queue_size": self.max_queue_size,
        }


__all__ = [
    "TaskStatus",
    "TaskCategory",
    "TaskDefinition",
    "TaskState",
    "ExecutionPlan",
    "TaskScheduler",
]
