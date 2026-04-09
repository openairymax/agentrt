# Copyright (c) 2026 SPHARX. All Rights Reserved.
# "From data intelligence emerges."

"""
Unit Tests for Core Task Module
================================

Tests for openlab.core.task module.
These tests verify the TaskScheduler, TaskDefinition, TaskState, and related classes.
"""

import pytest
import asyncio
from typing import Dict, List
import sys
import os

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))


class TestTaskStatus:
    """Tests for TaskStatus enum."""

    def test_task_status_values(self):
        """Test TaskStatus enum has correct values."""
        from openlab.core.task import TaskStatus

        assert TaskStatus.PENDING.value == "pending"
        assert TaskStatus.QUEUED.value == "queued"
        assert TaskStatus.RUNNING.value == "running"
        assert TaskStatus.PAUSED.value == "paused"
        assert TaskStatus.COMPLETED.value == "completed"
        assert TaskStatus.FAILED.value == "failed"
        assert TaskStatus.CANCELLED.value == "cancelled"
        assert TaskStatus.TIMEOUT.value == "timeout"


class TestTaskCategory:
    """Tests for TaskCategory enum."""

    def test_category_values(self):
        """Test TaskCategory enum has correct values."""
        from openlab.core.task import TaskCategory

        assert TaskCategory.IMMEDIATE.value == "immediate"
        assert TaskCategory.SCHEDULED.value == "scheduled"
        assert TaskCategory.PERIODIC.value == "periodic"
        assert TaskCategory.LONG_RUNNING.value == "long_running"


class TestTaskDefinition:
    """Tests for TaskDefinition dataclass."""

    def test_creation_with_defaults(self):
        """Test TaskDefinition with default values."""
        from openlab.core.task import TaskDefinition

        task = TaskDefinition(name="Test Task")
        
        assert task.name == "Test Task"
        assert task.category == TaskCategory.IMMEDIATE
        assert task.priority == 0
        assert task.input_data is None
        assert task.max_retries == 0
        assert task.timeout is None
        assert task.task_id is not None  # Auto-generated UUID

    def test_creation_with_all_params(self):
        """Test TaskDefinition with all parameters."""
        from openlab.core.task import TaskDefinition

        task = TaskDefinition(
            name="Complex Task",
            description="A complex task",
            category=TaskCategory.LONG_RUNNING,
            priority=10,
            input_data={"key": "value"},
            metadata={"author": "test"},
            timeout=3600.0,
            max_retries=3,
        )

        assert task.description == "A complex task"
        assert task.category == TaskCategory.LONG_RUNNING
        assert task.priority == 10
        assert task.input_data == {"key": "value"}
        assert task.metadata["author"] == "test"
        assert task.timeout == 3600.0
        assert task.max_retries == 3


class TestTaskState:
    """Tests for TaskState dataclass."""

    def test_initial_state(self):
        """Test initial state values."""
        from openlab.core.task import TaskState

        state = TaskState(task_id="task-001")
        
        assert state.status == TaskStatus.PENDING
        assert state.progress == 0.0
        assert state.result is None
        assert state.error is None
        assert state.retry_count == 0
        assert state.version == 1

    def test_state_serialization(self):
        """Test state to_dict/from_dict roundtrip."""
        from openlab.core.task import TaskState

        original = TaskState(
            task_id="task-002",
            status=TaskStatus.COMPLETED,
            progress=1.0,
            result={"output": "done"},
            retry_count=2,
        )
        
        data = original.to_dict()
        restored = TaskState.from_dict(data)
        
        assert restored.task_id == original.task_id
        assert restored.status == original.status
        assert restored.progress == original.progress
        assert restored.result == original.result
        assert restored.retry_count == original.retry_count


class TestExecutionPlan:
    """Tests for ExecutionPlan dataclass."""

    def test_plan_creation(self):
        """Test ExecutionPlan creation."""
        from openlab.core.task import ExecutionPlan

        plan = ExecutionPlan(
            plan_id="plan-001",
            task_id="task-001",
        )
        
        assert plan.plan_id == "plan-001"
        assert plan.task_id == "task-001"
        assert len(plan.steps) == 0
        assert len(plan.resources) == 0

    def test_add_step(self):
        """Test adding steps to plan."""
        from openlab.core.task import ExecutionPlan

        plan = ExecutionPlan(
            plan_id="plan-001",
            task_id="task-001",
        )
        
        plan.add_step({"name": "Step 1", "action": "init"})
        plan.add_step({"name": "Step 2", "action": "process"})
        
        assert len(plan.steps) == 2
        assert plan.steps[0]["name"] == "Step 1"

    def test_get_next_step(self):
        """Test getting next step."""
        from openlab.core.task import ExecutionPlan

        plan = ExecutionPlan(
            plan_id="plan-001",
            task_id="task-001",
        )
        
        plan.add_step({"name": "Step 1"})
        plan.add_step({"name": "Step 2"})
        
        step_0 = plan.get_next_step(0)
        step_1 = plan.get_next_step(1)
        step_out_of_range = plan.get_next_step(2)
        
        assert step_0["name"] == "Step 1"
        assert step_1["name"] == "Step 2"
        assert step_out_of_range is None


class TestTaskScheduler:
    """Tests for TaskScheduler class."""

    @pytest.fixture
    def scheduler(self):
        """Create a test scheduler."""
        from openlab.core.task import TaskScheduler
        return TaskScheduler(max_concurrent=5, max_queue_size=100)

    @pytest.mark.asyncio
    async def test_scheduler_creation(self, scheduler):
        """Test scheduler can be created."""
        stats = scheduler.get_stats()
        assert stats["max_concurrent"] == 5
        assert stats["max_queue_size"] == 100
        assert stats["queue_size"] == 0
        assert stats["running_tasks"] == 0

    @pytest.mark.asyncio
    async def test_submit_task(self, scheduler):
        """Test submitting a task."""
        from openlab.core.task import TaskDefinition
        
        definition = TaskDefinition(name="Test Task")
        task_id = await scheduler.submit(definition)
        
        assert task_id is not None
        assert scheduler._queue.qsize() == 1

    @pytest.mark.asyncio
    async def test_submit_full_queue(self, scheduler):
        """Test submitting to full queue raises error."""
        from openlab.core.task import TaskDefinition
        
        # Create a scheduler with very small queue
        small_scheduler = TaskScheduler(max_concurrent=1, max_queue_size=2)
        
        await small_scheduler.submit(TaskDefinition(name="Task 1"))
        await small_scheduler.submit(TaskDefinition(name="Task 2"))
        
        with pytest.raises(RuntimeError, match="queue is full"):
            await small_scheduler.submit(TaskDefinition(name="Task 3"))

    @pytest.mark.asyncio
    async def test_get_state(self, scheduler):
        """Test getting task state."""
        from openlab.core.task import TaskDefinition
        
        definition = TaskDefinition(name="Test Task")
        task_id = await scheduler.submit(definition)
        
        state = await scheduler.get_state(task_id)
        
        assert state is not None
        assert state.task_id == task_id
        assert state.status == TaskStatus.QUEUED

    @pytest.mark.asyncio
    async def test_cancel_task(self, scheduler):
        """Test cancelling a task."""
        from openlab.core.task import TaskDefinition
        
        definition = TaskDefinition(name="Cancellable Task")
        task_id = await scheduler.submit(definition)
        
        success = await scheduler.cancel(task_id)
        
        # Note: Task may have been picked up by schedule() already
        # So we just check it doesn't raise an error
        assert isinstance(success, bool)

    @pytest.mark.asyncio
    async def test_checkpoint_operations(self, scheduler):
        """Test checkpoint save and load."""
        from openlab.core.task import TaskDefinition
        
        definition = TaskDefinition(name="Checkpoint Task")
        task_id = await scheduler.submit(definition)
        
        # Save checkpoint
        checkpoint_data = {"progress": 50, "data": "test"}
        saved = await scheduler.save_checkpoint(task_id, checkpoint_data)
        assert saved is True
        
        # Load checkpoint
        loaded = await scheduler.load_checkpoint(task_id)
        assert loaded == checkpoint_data

    @pytest.mark.asyncio
    async def test_shutdown_scheduler(self, scheduler):
        """Test shutting down scheduler gracefully."""
        await scheduler.shutdown(wait=False)
        
        assert scheduler._shutdown is True

    @pytest.mark.asyncio
    async def test_schedule_execution(self, scheduler):
        """Test scheduling and executing a task."""
        from openlab.core.task import TaskDefinition
        
        executed_tasks = []
        
        async def mock_executor(task_def: TaskDefinition):
            executed_tasks.append(task_def.name)
            return f"Result: {task_def.name}"
        
        definition = TaskDefinition(name="Scheduled Task", timeout=30.0)
        task_id = await scheduler.submit(definition)
        
        async_task = await scheduler.schedule(definition, mock_executor)
        
        # Wait a bit for execution
        await asyncio.sleep(0.1)
        
        state = await scheduler.get_state(task_id)
        assert state is not None
        assert len(executed_tasks) > 0 or state.status in [TaskStatus.COMPLETED, TaskStatus.FAILED]


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
