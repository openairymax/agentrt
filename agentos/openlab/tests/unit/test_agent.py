# Copyright (c) 2026 SPHARX. All Rights Reserved.
# "From data intelligence emerges."

"""
Unit Tests for Core Agent Module
================================

Tests for openlab.core.agent module.
These tests verify the Agent, AgentRegistry, and related classes.
"""

import pytest
import asyncio
from typing import Set
from unittest.mock import AsyncMock, MagicMock, patch

import sys
import os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))


class TestAgentStatus:
    """Tests for AgentStatus enum."""

    def test_agent_status_values(self):
        """Test AgentStatus enum has correct values."""
        from openlab.core.agent import AgentStatus

        assert AgentStatus.CREATED.value == "created"
        assert AgentStatus.INITIALIZING.value == "initializing"
        assert AgentStatus.READY.value == "ready"
        assert AgentStatus.RUNNING.value == "running"
        assert AgentStatus.PAUSED.value == "paused"
        assert AgentStatus.SHUTTING_DOWN.value == "shutting_down"
        assert AgentStatus.SHUTDOWN.value == "shutdown"
        assert AgentStatus.ERROR.value == "error"


class TestAgentCapability:
    """Tests for AgentCapability enum."""

    def test_capability_values(self):
        """Test AgentCapability enum has correct values."""
        from openlab.core.agent import AgentCapability

        assert AgentCapability.ARCHITECTURE_DESIGN.value == "architecture_design"
        assert AgentCapability.CODE_GENERATION.value == "code_generation"
        assert AgentCapability.TEST_GENERATION.value == "test_generation"
        assert AgentCapability.DOCUMENTATION.value == "documentation"
        assert AgentCapability.DEBUGGING.value == "debugging"
        assert AgentCapability.OPTIMIZATION.value == "optimization"


class TestAgentContext:
    """Tests for AgentContext dataclass."""

    def test_context_creation(self):
        """Test AgentContext can be created."""
        from openlab.core.agent import AgentContext

        context = AgentContext(agent_id="test-agent-001")
        assert context.agent_id == "test-agent-001"
        assert context.task_id is None
        assert context.session_id is None
        assert isinstance(context.metadata, dict)
        assert context.created_at > 0

    def test_context_with_task(self):
        """Test AgentContext with task_id."""
        from openlab.core.agent import AgentContext

        context = AgentContext(
            agent_id="test-agent-001",
            task_id="task-001",
            session_id="session-001"
        )
        assert context.task_id == "task-001"
        assert context.session_id == "session-001"

    def test_context_with_timeout(self):
        """Test AgentContext with timeout."""
        from openlab.core.agent import AgentContext

        context = AgentContext(
            agent_id="test-agent-001",
            timeout=60.0
        )
        assert context.timeout == 60.0


class TestTaskResult:
    """Tests for TaskResult dataclass."""

    def test_task_result_success(self):
        """Test successful TaskResult creation."""
        from openlab.core.agent import TaskResult

        result = TaskResult(success=True, output={"data": "test"})
        assert result.success is True
        assert result.output == {"data": "test"}
        assert result.error is None
        assert result.error_code is None

    def test_task_result_failure(self):
        """Test failed TaskResult creation."""
        from openlab.core.agent import TaskResult

        result = TaskResult(
            success=False,
            error="Something went wrong",
            error_code="ERR_001"
        )
        assert result.success is False
        assert result.error == "Something went wrong"
        assert result.error_code == "ERR_001"

    def test_task_result_with_metrics(self):
        """Test TaskResult with metrics."""
        from openlab.core.agent import TaskResult

        result = TaskResult(
            success=True,
            output="done",
            metrics={"execution_time": 1.5, "items_processed": 10}
        )
        assert result.metrics["execution_time"] == 1.5
        assert result.metrics["items_processed"] == 10


class TestMessage:
    """Tests for Message class."""

    def test_message_creation(self):
        """Test Message creation."""
        from openlab.core.agent import Message

        msg = Message(
            message_type="task",
            content={"task": "design"},
            sender="agent-001",
            receiver="agent-002"
        )
        assert msg.type == "task"
        assert msg.content == {"task": "design"}
        assert msg.sender == "agent-001"
        assert msg.receiver == "agent-002"
        assert msg.timestamp > 0

    def test_message_repr(self):
        """Test Message string representation."""
        from openlab.core.agent import Message

        msg = Message(
            message_type="task",
            content={},
            sender="sender-001",
            receiver="receiver-001"
        )
        repr_str = repr(msg)
        assert "task" in repr_str
        assert "sender-001" in repr_str
        assert "receiver-001" in repr_str


class SimpleTestAgent:
    """Concrete implementation of Agent for testing."""

    def __init__(
        self,
        agent_id: str,
        capabilities: Set = None,
        manager=None,
        workbench_id=None
    ):
        self.agent_id = agent_id
        self.capabilities = capabilities or set()
        self.manager = manager
        self.workbench_id = workbench_id
        self._status = None
        self._context = None
        self._tools = {}
        self._tool_executor = None
        self._created_at = 0
        self._last_activity = 0
        self._status = self._get_initial_status()

    def _get_initial_status(self):
        from openlab.core.agent import AgentStatus
        return AgentStatus.CREATED

    async def initialize(self) -> None:
        from openlab.core.agent import AgentStatus
        self._status = AgentStatus.READY

    async def execute(self, input_data, context) -> "TaskResult":
        from openlab.core.agent import TaskResult, AgentStatus
        self._status = AgentStatus.RUNNING
        await asyncio.sleep(0.01)  # Simulate work
        self._status = AgentStatus.READY
        return TaskResult(success=True, output=f"Processed: {input_data}")

    async def shutdown(self) -> None:
        from openlab.core.agent import AgentStatus
        self._status = AgentStatus.SHUTDOWN

    def register_tool(self, name: str, tool) -> None:
        self._tools[name] = tool

    def get_tool(self, name: str):
        return self._tools.get(name)

    @property
    def status(self):
        return self._status

    @property
    def context(self):
        return self._context


class TestAgent:
    """Tests for Agent abstract base class functionality."""

    @pytest.fixture
    def agent(self):
        """Create a test agent."""
        from openlab.core.agent import AgentCapability
        return SimpleTestAgent(
            agent_id="test-agent-001",
            capabilities={AgentCapability.ARCHITECTURE_DESIGN}
        )

    @pytest.mark.asyncio
    async def test_agent_initialization(self, agent):
        """Test agent can be initialized."""
        await agent.initialize()
        assert agent.status.value == "ready"

    @pytest.mark.asyncio
    async def test_agent_execution(self, agent):
        """Test agent can execute tasks."""
        from openlab.core.agent import AgentContext, TaskResult

        context = AgentContext(agent_id="test-agent-001")
        result = await agent.execute("test input", context)

        assert isinstance(result, TaskResult)
        assert result.success is True
        assert "Processed" in result.output

    @pytest.mark.asyncio
    async def test_agent_shutdown(self, agent):
        """Test agent can shutdown gracefully."""
        await agent.initialize()
        await agent.shutdown()
        assert agent.status.value == "shutdown"

    @pytest.mark.asyncio
    async def test_agent_status_transitions(self, agent):
        """Test agent status transitions correctly."""
        from openlab.core.agent import AgentStatus

        assert agent.status.value == "created"

        await agent.initialize()
        assert agent.status.value == "ready"

        await agent.shutdown()
        assert agent.status.value == "shutdown"

    def test_agent_register_tool(self, agent):
        """Test tool registration."""
        mock_tool = MagicMock()
        agent.register_tool("test_tool", mock_tool)

        retrieved_tool = agent.get_tool("test_tool")
        assert retrieved_tool is mock_tool

    def test_agent_get_nonexistent_tool(self, agent):
        """Test getting non-existent tool returns None."""
        tool = agent.get_tool("nonexistent")
        assert tool is None


class TestAgentRegistry:
    """Tests for AgentRegistry class."""

    @pytest.fixture
    def registry(self):
        """Create a test registry."""
        from openlab.core.agent import AgentRegistry
        return AgentRegistry()

    @pytest.mark.asyncio
    async def test_registry_initialization(self, registry):
        """Test registry can be initialized."""
        count = await registry.count()
        assert count == 0

    @pytest.mark.asyncio
    async def test_register_agent(self, registry):
        """Test registering an agent."""
        from openlab.core.agent import AgentCapability

        agent = SimpleTestAgent(
            agent_id="reg-test-001",
            capabilities={AgentCapability.ARCHITECTURE_DESIGN}
        )

        success = await registry.register(agent)
        assert success is True
        assert await registry.count() == 1

    @pytest.mark.asyncio
    async def test_register_duplicate_agent(self, registry):
        """Test registering duplicate agent fails."""
        from openlab.core.agent import AgentCapability

        agent = SimpleTestAgent(
            agent_id="dup-test-001",
            capabilities={AgentCapability.ARCHITECTURE_DESIGN}
        )

        await registry.register(agent)
        success = await registry.register(agent)  # Second registration

        assert success is False
        assert await registry.count() == 1

    @pytest.mark.asyncio
    async def test_unregister_agent(self, registry):
        """Test unregistering an agent."""
        from openlab.core.agent import AgentCapability

        agent = SimpleTestAgent(
            agent_id="unreg-test-001",
            capabilities={AgentCapability.ARCHITECTURE_DESIGN}
        )

        await registry.register(agent)
        success = await registry.unregister("unreg-test-001")

        assert success is True
        assert await registry.count() == 0

    @pytest.mark.asyncio
    async def test_unregister_nonexistent(self, registry):
        """Test unregistering non-existent agent fails."""
        success = await registry.unregister("nonexistent-id")
        assert success is False

    @pytest.mark.asyncio
    async def test_get_agent(self, registry):
        """Test getting an agent by ID."""
        from openlab.core.agent import AgentCapability

        agent = SimpleTestAgent(
            agent_id="get-test-001",
            capabilities={AgentCapability.ARCHITECTURE_DESIGN}
        )

        await registry.register(agent)
        retrieved = await registry.get("get-test-001")

        assert retrieved is not None
        assert retrieved.agent_id == "get-test-001"

    @pytest.mark.asyncio
    async def test_get_nonexistent_agent(self, registry):
        """Test getting non-existent agent returns None."""
        agent = await registry.get("nonexistent-id")
        assert agent is None

    @pytest.mark.asyncio
    async def test_list_agents(self, registry):
        """Test listing all agents."""
        from openlab.core.agent import AgentCapability

        agent1 = SimpleTestAgent(
            agent_id="list-test-001",
            capabilities={AgentCapability.ARCHITECTURE_DESIGN}
        )
        agent2 = SimpleTestAgent(
            agent_id="list-test-002",
            capabilities={AgentCapability.CODE_GENERATION}
        )

        await registry.register(agent1)
        await registry.register(agent2)

        agents = await registry.list_agents()
        assert len(agents) == 2
        agent_ids = {a.agent_id for a in agents}
        assert "list-test-001" in agent_ids
        assert "list-test-002" in agent_ids


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
