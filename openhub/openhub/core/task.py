"""
Copyright (c) 2026 SPHARX. All Rights Reserved.
"From data intelligence emerges."

OpenHub Task Management Module
==============================

This module provides task abstraction and scheduling for OpenHub:
- Task definition and state machine
- Priority-based scheduling
- Task dependency DAG support
- Asynchronous execution with cancellation

Usage:
    from openhub.core.task import Task, TaskStatus, TaskPriority, TaskScheduler

Author: AgentOS Architecture Committee
Version: 1.0.0.6
"""

from __future__ import annotations

import asyncio
import logging
import time
import uuid
from abc import ABC, abstractmethod
from collections import defaultdict
from dataclasses import dataclass, field
from enum import Enum, auto
from typing import (
    Any,
    Callable,
    Dict,
    List,
    Optional,
    Set,
    Tuple,
    Awaitable,
    TypeVar,
)

logger = logging.getLogger(__name__)


class TaskStatus(Enum):
    """Task lifecycle states."""
    PENDING = auto()
    SCHEDULED = auto()
    RUNNING = auto()
    COMPLETED = auto()
    FAILED = auto()
    CANCELLED = auto()
    TIMED_OUT = auto()


class TaskPriority(Enum):
    """Task priority levels (higher value = higher priority)."""
    LOW = 0
    NORMAL = 1
    HIGH = 2
    CRITICAL = 3


class TaskEvent(Enum):
    """Task lifecycle events for state machine transitions."""
    SUBMIT = auto()
    SCHEDULE = auto()
    START = auto()
    COMPLETE = auto()
    FAIL = auto()
    CANCEL = auto()
    TIMEOUT = auto()
    RETRY = auto()


@dataclass
class TaskDefinition:
    """
    Immutable task definition containing all task metadata.

    Attributes:
        task_id: Unique task identifier.
        name: Human-readable task name.
        description: Task description.
        priority: Task priority level.
        agent_role: Required agent role for execution.
        timeout_seconds: Maximum execution time.
        retry_count: Number of retries on failure.
        dependencies: Set of task IDs that must complete first.
        input_data: Task input payload.
        metadata: Additional task metadata.
        created_at: Task creation timestamp.
    """
    task_id: str
    name: str
    description: str
    priority: TaskPriority
    agent_role: str
    timeout_seconds: int
    retry_count: int
    dependencies: Set[str]
    input_data: Any
    metadata: Dict[str, Any]
    created_at: float = field(default_factory=time.time)

    def __post_init__(self) -> None:
        """Validate task definition."""
        if not self.task_id:
            raise ValueError("task_id cannot be empty")
        if self.timeout_seconds <= 0:
            raise ValueError("timeout_seconds must be positive")
        if self.retry_count < 0:
            raise ValueError("retry_count cannot be negative")


@dataclass
class TaskState:
    """
    Mutable task state tracking current execution status.

    Attributes:
        task_id: Reference to task definition ID.
        status: Current task status.
        attempt: Current execution attempt number.
        scheduled_at: When task was scheduled.
        started_at: When task execution started.
        completed_at: When task completed (success or failure).
        error: Error message if failed.
        error_code: Standardized error code.
        output: Task output payload.
        trace_id: Distributed tracing identifier.
    """
    task_id: str
    status: TaskStatus
    attempt: int = 0
    scheduled_at: Optional[float] = None
    started_at: Optional[float] = None
    completed_at: Optional[float] = None
    error: Optional[str] = None
    error_code: Optional[str] = None
    output: Any = None
    trace_id: Optional[str] = None

    def transition(self, event: TaskEvent) -> bool:
        """
        Attempt state transition based on event.

        Args:
            event: Event triggering transition.

        Returns:
            True if transition valid, False otherwise.
        """
        valid_transitions = {
            TaskStatus.PENDING: {
                TaskEvent.SUBMIT: TaskStatus.PENDING,
                TaskEvent.SCHEDULE: TaskStatus.SCHEDULED,
                TaskEvent.CANCEL: TaskStatus.CANCELLED,
            },
            TaskStatus.SCHEDULED: {
                TaskEvent.START: TaskStatus.RUNNING,
                TaskEvent.CANCEL: TaskStatus.CANCELLED,
                TaskEvent.TIMEOUT: TaskStatus.TIMED_OUT,
            },
            TaskStatus.RUNNING: {
                TaskEvent.COMPLETE: TaskStatus.COMPLETED,
                TaskEvent.FAIL: TaskStatus.FAILED,
                TaskEvent.CANCEL: TaskStatus.CANCELLED,
                TaskEvent.TIMEOUT: TaskStatus.TIMED_OUT,
                TaskEvent.RETRY: TaskStatus.SCHEDULED,
            },
            TaskStatus.COMPLETED: {},
            TaskStatus.FAILED: {
                TaskEvent.RETRY: TaskStatus.SCHEDULED,
            },
            TaskStatus.CANCELLED: {},
            TaskStatus.TIMED_OUT: {
                TaskEvent.RETRY: TaskStatus.SCHEDULED,
            },
        }

        next_status = valid_transitions.get(self.status, {}).get(event)
        if next_status is None:
            logger.warning(
                f"Invalid transition: {self.status} + {event}",
                extra={"task_id": self.task_id},
            )
            return False

        self.status = next_status
        return True

    def get_duration_ms(self) -> Optional[float]:
        """
        Get task execution duration in milliseconds.

        Returns:
            Duration if task completed, None otherwise.
        """
        if self.started_at is None:
            return None
        end_time = self.completed_at or time.time()
        return (end_time - self.started_at) * 1000


@dataclass
class TaskResult:
    """
    Final task result payload.

    Attributes:
        task_id: Reference to task ID.
        success: Whether task completed successfully.
        output: Task output data.
        error: Error message if failed.
        error_code: Standardized error code.
        duration_ms: Execution duration in milliseconds.
        metrics: Additional execution metrics.
    """
    task_id: str
    success: bool
    output: Optional[Any] = None
    error: Optional[str] = None
    error_code: Optional[str] = None
    duration_ms: Optional[float] = None
    metrics: Dict[str, Any] = field(default_factory=dict)


class Task:
    """
    Combined task definition and state container.

    This is the primary interface for task submission and tracking.

    Example:
        task = Task(
            name="generate-api-docs",
            description="Generate API documentation",
            priority=TaskPriority.HIGH,
            agent_role="backend",
            input_data={"spec_path": "/api/openapi.yaml"},
        )

        await scheduler.submit(task)
        result = await scheduler.wait_for_completion(task.task_id)
    """

    def __init__(
        self,
        name: str,
        description: str = "",
        priority: TaskPriority = TaskPriority.NORMAL,
        agent_role: str = "default",
        timeout_seconds: int = 300,
        retry_count: int = 0,
        dependencies: Optional[Set[str]] = None,
        input_data: Any = None,
        metadata: Optional[Dict[str, Any]] = None,
    ) -> None:
        """
        Create a new task.

        Args:
            name: Human-readable task name.
            description: Task description.
            priority: Task priority level.
            agent_role: Required agent role.
            timeout_seconds: Execution timeout.
            retry_count: Number of retries on failure.
            dependencies: Set of task IDs that must complete first.
            input_data: Task input payload.
            metadata: Additional metadata.
        """
        self.task_id = str(uuid.uuid4())
        self.definition = TaskDefinition(
            task_id=self.task_id,
            name=name,
            description=description,
            priority=priority,
            agent_role=agent_role,
            timeout_seconds=timeout_seconds,
            retry_count=retry_count,
            dependencies=dependencies or set(),
            input_data=input_data,
            metadata=metadata or {},
        )
        self.state = TaskState(
            task_id=self.task_id,
            status=TaskStatus.PENDING,
        )
        self._result: Optional[TaskResult] = None
        self._completion_event = asyncio.Event()

        logger.info(
            "Task created",
            extra={
                "task_id": self.task_id,
                "name": name,
                "priority": priority.name,
                "agent_role": agent_role,
            },
        )

    @property
    def is_complete(self) -> bool:
        """Check if task has reached a terminal state."""
        return self.state.status in (
            TaskStatus.COMPLETED,
            TaskStatus.FAILED,
            TaskStatus.CANCELLED,
            TaskStatus.TIMED_OUT,
        )

    @property
    def is_success(self) -> bool:
        """Check if task completed successfully."""
        return self.state.status == TaskStatus.COMPLETED

    def get_result(self, timeout: Optional[float] = None) -> TaskResult:
        """
        Get task result, blocking until complete.

        Args:
            timeout: Maximum wait time in seconds.

        Returns:
            TaskResult object.

        Raises:
            asyncio.TimeoutError: If timeout reached.
        """
        if not self._completion_event.wait(timeout=timeout):
            raise asyncio.TimeoutError(f"Task {self.task_id} result timeout")
        return self._result

    def _set_result(self, result: TaskResult) -> None:
        """Internal method to set result and signal completion."""
        self._result = result
        self._completion_event.set()


TaskHandler = Callable[["Task"], Awaitable[TaskResult]]
"""Type alias for task execution handler function."""


class TaskScheduler:
    """
    Priority-based task scheduler with dependency support.

    Features:
    - Priority queue with weighted fair scheduling
    - DAG-based dependency resolution
    - Automatic retry with exponential backoff
    - Task timeout enforcement
    - Resource-aware scheduling

    Example:
        scheduler = TaskScheduler(max_concurrent=10)

        async def handler(task: Task) -> TaskResult:
            result = await my_agent.execute(task)
            return TaskResult(
                task_id=task.task_id,
                success=True,
                output=result,
            )

        scheduler.set_handler(handler)
        await scheduler.start()

        task = Task(name="example", agent_role="backend")
        await scheduler.submit(task)
        result = await scheduler.wait_for_completion(task.task_id)

        await scheduler.stop()
    """

    def __init__(
        self,
        max_concurrent: int = 100,
        max_queue_size: int = 10000,
        default_timeout: float = 300.0,
        retry_base_delay: float = 1.0,
        retry_max_delay: float = 60.0,
    ) -> None:
        """
        Initialize the task scheduler.

        Args:
            max_concurrent: Maximum concurrent running tasks.
            max_queue_size: Maximum queued tasks before rejection.
            default_timeout: Default task timeout in seconds.
            retry_base_delay: Base delay for exponential backoff (seconds).
            retry_max_delay: Maximum retry delay cap (seconds).
        """
        self.max_concurrent = max_concurrent
        self.max_queue_size = max_queue_size
        self.default_timeout = default_timeout
        self.retry_base_delay = retry_base_delay
        self.retry_max_delay = retry_max_delay

        self._tasks: Dict[str, Task] = {}
        self._pending_queue: List[Task] = []
        self._running: Dict[str, Task] = {}
        self._completed: Dict[str, Task] = {}

        self._dependency_graph: Dict[str, Set[str]] = defaultdict(set)
        self._dependents_graph: Dict[str, Set[str]] = defaultdict(set)

        self._handler: Optional[TaskHandler] = None
        self._running_tasks: asyncio.Semaphore
        self._lock = asyncio.Lock()
        self._scheduler_task: Optional[asyncio.Task] = None
        self._shutdown = False

        logger.info(
            "TaskScheduler initialized",
            extra={
                "max_concurrent": max_concurrent,
                "max_queue_size": max_queue_size,
            },
        )

    def set_handler(self, handler: TaskHandler) -> None:
        """
        Set the task execution handler.

        Args:
            handler: Async function that processes tasks and returns results.

        Raises:
            ValueError: If handler is not callable.
        """
        if not callable(handler):
            raise ValueError("Handler must be a callable")
        self._handler = handler

    async def submit(self, task: Task) -> str:
        """
        Submit a task for execution.

        Args:
            task: Task to submit.

        Returns:
            Task ID.

        Raises:
            asyncio.QueueFull: If queue at capacity.
            ValueError: If task already submitted.
        """
        if task.task_id in self._tasks:
            raise ValueError(f"Task {task.task_id} already submitted")

        async with self._lock:
            if len(self._tasks) >= self.max_queue_size:
                raise asyncio.QueueFull(
                    f"Scheduler queue at capacity ({self.max_queue_size})"
                )

            self._tasks[task.task_id] = task

            for dep_id in task.definition.dependencies:
                self._dependency_graph[dep_id].add(task.task_id)
                self._dependents_graph[task.task_id].add(dep_id)

        task.state.transition(TaskEvent.SUBMIT)
        self._enqueue_by_priority(task)

        logger.info(
            "Task submitted",
            extra={
                "task_id": task.task_id,
                "name": task.definition.name,
                "priority": task.definition.priority.name,
                "dependencies": list(task.definition.dependencies),
            },
        )

        return task.task_id

    async def cancel(self, task_id: str) -> bool:
        """
        Cancel a pending or running task.

        Args:
            task_id: Task identifier.

        Returns:
            True if cancelled, False if not found or already terminal.
        """
        async with self._lock:
            task = self._tasks.get(task_id)
            if task is None:
                return False

            if task.is_complete:
                return False

            if task.state.status == TaskStatus.RUNNING:
                task.state.transition(TaskEvent.CANCEL)
                task._set_result(TaskResult(
                    task_id=task_id,
                    success=False,
                    error="Task cancelled by user",
                    error_code="TASK_CANCELLED",
                ))
            else:
                task.state.transition(TaskEvent.CANCEL)
                self._remove_from_queue(task)
                self._completed[task_id] = task

        logger.info(f"Task {task_id} cancelled")
        return True

    async def wait_for_completion(
        self, task_id: str, timeout: Optional[float] = None
    ) -> TaskResult:
        """
        Wait for task completion and get result.

        Args:
            task_id: Task identifier.
            timeout: Maximum wait time in seconds.

        Returns:
            TaskResult object.

        Raises:
            KeyError: If task not found.
            asyncio.TimeoutError: If timeout reached.
        """
        async with self._lock:
            task = self._tasks.get(task_id)
            if task is None:
                raise KeyError(f"Task {task_id} not found")

        result = task.get_result(timeout=timeout)

        if not result.success and task.definition.retry_count > 0:
            if task.state.attempt < task.definition.retry_count:
                logger.info(
                    f"Task {task_id} will retry",
                    extra={"attempt": task.state.attempt + 1},
                )

        return result

    def get_task_state(self, task_id: str) -> Optional[TaskStatus]:
        """
        Get current task status.

        Args:
            task_id: Task identifier.

        Returns:
            TaskStatus if found, None otherwise.
        """
        task = self._tasks.get(task_id)
        return task.state.status if task else None

    async def get_stats(self) -> Dict[str, Any]:
        """
        Get scheduler statistics.

        Returns:
            Dictionary with queue and execution stats.
        """
        async with self._lock:
            return {
                "total_tasks": len(self._tasks),
                "pending": len(self._pending_queue),
                "running": len(self._running),
                "completed": len(self._completed),
                "max_concurrent": self.max_concurrent,
                "queue_capacity": self.max_queue_size,
            }

    async def start(self) -> None:
        """
        Start the scheduler's background processing loop.

        This method must be called before submitting tasks.
        """
        if self._handler is None:
            raise RuntimeError("Task handler not set")
        if self._scheduler_task is not None:
            raise RuntimeError("Scheduler already started")

        self._running_tasks = asyncio.Semaphore(self.max_concurrent)
        self._scheduler_task = asyncio.create_task(self._scheduler_loop())
        logger.info("TaskScheduler started")

    async def stop(self, timeout: float = 30.0) -> None:
        """
        Stop the scheduler gracefully.

        Args:
            timeout: Maximum time to wait for running tasks.

        Raises:
            asyncio.TimeoutError: If graceful shutdown times out.
        """
        if self._shutdown:
            return

        self._shutdown = True
        logger.info("TaskScheduler stopping")

        if self._scheduler_task:
            self._scheduler_task.cancel()
            try:
                await asyncio.wait_for(self._scheduler_task, timeout=timeout)
            except asyncio.CancelledError:
                pass

        async with self._lock:
            for task in list(self._running.values()):
                await self.cancel(task.task_id)

        logger.info("TaskScheduler stopped")

    def _enqueue_by_priority(self, task: Task) -> None:
        """
        Insert task into priority queue maintaining order.

        Args:
            task: Task to enqueue.
        """
        priority = task.definition.priority.value
        insert_pos = len(self._pending_queue)

        for i, queued_task in enumerate(self._pending_queue):
            if queued_task.definition.priority.value < priority:
                insert_pos = i
                break

        self._pending_queue.insert(insert_pos, task)

    def _remove_from_queue(self, task: Task) -> None:
        """Remove task from pending queue if present."""
        try:
            self._pending_queue.remove(task)
        except ValueError:
            pass

    def _are_dependencies_met(self, task: Task) -> bool:
        """
        Check if all task dependencies are satisfied.

        Args:
            task: Task to check.

        Returns:
            True if all dependencies completed successfully.
        """
        for dep_id in task.definition.dependencies:
            dep_task = self._tasks.get(dep_id)
            if dep_task is None or not dep_task.is_success:
                return False
        return True

    async def _scheduler_loop(self) -> None:
        """
        Main scheduler loop processing pending tasks.

        This loop:
        1. Checks for tasks with met dependencies
        2. Schedules tasks respecting concurrency limits
        3. Monitors running tasks for completion
        4. Handles retries and timeouts
        """
        while not self._shutdown:
            try:
                await self._process_completed()
                await self._schedule_ready_tasks()
                await asyncio.sleep(0.01)
            except asyncio.CancelledError:
                break
            except Exception as e:
                logger.error(f"Scheduler loop error: {e}")

    async def _process_completed(self) -> None:
        """Process completed tasks and trigger dependent scheduling."""
        completed_ids = [
            tid for tid, task in self._running.items()
            if task.is_complete
        ]

        for task_id in completed_ids:
            task = self._running.pop(task_id)
            self._completed[task_id] = task
            self._running_tasks.release()

            async with self._lock:
                dependents = self._dependency_graph.get(task_id, set())
                for dependent_id in dependents:
                    dependent = self._tasks.get(dependent_id)
                    if dependent and self._are_dependencies_met(dependent):
                        self._enqueue_by_priority(dependent)

    async def _schedule_ready_tasks(self) -> None:
        """Schedule pending tasks that have all dependencies met."""
        while self._pending_queue and self._running_tasks.locked() == False:
            task = self._pending_queue.pop(0)

            if task.is_complete:
                continue

            if not self._are_dependencies_met(task):
                self._pending_queue.insert(0, task)
                break

            task.state.attempt += 1
            task.state.transition(TaskEvent.START)
            task.state.started_at = time.time()

            self._running[task.task_id] = task

            asyncio.create_task(self._execute_task(task))

    async def _execute_task(self, task: Task) -> None:
        """
        Execute a single task with timeout and retry handling.

        Args:
            task: Task to execute.
        """
        logger.info(
            "Task execution started",
            extra={
                "task_id": task.task_id,
                "attempt": task.state.attempt,
            },
        )

        try:
            timeout = min(
                task.definition.timeout_seconds,
                self.default_timeout,
            )

            result = await asyncio.wait_for(
                self._handler(task),
                timeout=timeout,
            )

            task.state.transition(TaskEvent.COMPLETE)
            task.state.completed_at = time.time()
            task.state.output = result.output
            task._set_result(result)

            logger.info(
                "Task completed successfully",
                extra={
                    "task_id": task.task_id,
                    "duration_ms": task.state.get_duration_ms(),
                },
            )

        except asyncio.TimeoutError:
            task.state.transition(TaskEvent.TIMEOUT)
            task.state.completed_at = time.time()
            task.state.error = f"Task timed out after {timeout}s"
            task.state.error_code = "TASK_TIMEOUT"
            task._set_result(TaskResult(
                task_id=task.task_id,
                success=False,
                error=task.state.error,
                error_code=task.state.error_code,
                duration_ms=task.state.get_duration_ms(),
            ))

            logger.warning(
                "Task timed out",
                extra={"task_id": task.task_id, "timeout_s": timeout},
            )

        except Exception as e:
            task.state.transition(TaskEvent.FAIL)
            task.state.completed_at = time.time()
            task.state.error = str(e)
            task.state.error_code = "TASK_EXECUTION_ERROR"

            should_retry = (
                task.state.attempt <= task.definition.retry_count
            )

            if should_retry:
                delay = min(
                    self.retry_base_delay * (2 ** (task.state.attempt - 1)),
                    self.retry_max_delay,
                )
                logger.info(
                    f"Task will retry in {delay}s",
                    extra={
                        "task_id": task.task_id,
                        "attempt": task.state.attempt,
                        "delay": delay,
                    },
                )
                await asyncio.sleep(delay)
                task.state.transition(TaskEvent.RETRY)
                self._enqueue_by_priority(task)
            else:
                task._set_result(TaskResult(
                    task_id=task.task_id,
                    success=False,
                    error=task.state.error,
                    error_code=task.state.error_code,
                    duration_ms=task.state.get_duration_ms(),
                ))

            logger.error(
                "Task execution failed",
                extra={
                    "task_id": task.task_id,
                    "error": str(e),
                    "will_retry": should_retry,
                },
            )
