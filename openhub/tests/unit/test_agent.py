# Copyright (c) 2026 SPHARX. All Rights Reserved.
# "From data intelligence emerges."

"""
Unit Tests for Core Agent Module
=============================
"""

import pytest
import sys
import os
from unittest.mock import Mock, MagicMock, patch

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


class TestAgentBasics:
    """Basic agent functionality tests."""

    @pytest.mark.asyncio
    async def test_agent_initialization(self):
        """Test agent can be initialized."""
        from openhub.core.agent import Agent, AgentStatus, AgentContext, TaskResult

        class TestAgent(Agent):
            async def _do_initialize(self, config):
                pass

            async def _do_execute(self, context, input_data):
                return TaskResult(success=True, output={})

            async def _do_shutdown(self):
                pass

        agent = TestAgent(
            agent_id="test-agent-001",
            agent_type="test"
        )

        assert agent.agent_id == "test-agent-001"
        assert agent.status == AgentStatus.CREATED

    @pytest.mark.asyncio
    async def test_agent_status_transitions(self):
        """Test agent status can be changed."""
        from openhub.core.agent import Agent, AgentStatus, AgentContext, TaskResult

        class TestAgent(Agent):
            async def _do_initialize(self, config):
                pass

            async def _do_execute(self, context, input_data):
                return TaskResult(success=True, output={})

            async def _do_shutdown(self):
                pass

        agent = TestAgent(
            agent_id="test-agent-003",
            agent_type="test"
        )

        await agent.initialize()
        assert agent.status == AgentStatus.IDLE

    def test_agent_health_check(self):
        """Test agent health check."""
        from openhub.core.agent import Agent, AgentStatus

        class TestAgent(Agent):
            async def _do_initialize(self, config):
                pass

            async def _do_execute(self, context, input_data):
                pass

            async def _do_shutdown(self):
                pass

        agent = TestAgent(
            agent_id="test-agent-004",
            agent_type="test"
        )

        health = agent.get_health()
        assert "agent_id" in health
        assert "status" in health
        assert health["agent_id"] == "test-agent-004"


class TestAgentRegistry:
    """Tests for agent registry functionality."""

    @pytest.mark.asyncio
    async def test_registry_initialization(self):
        """Test registry can be initialized."""
        from openhub.core.agent import AgentRegistry

        registry = AgentRegistry()
        assert registry is not None

    @pytest.mark.asyncio
    async def test_register_agent(self):
        """Test registering an agent."""
        from openhub.core.agent import Agent, AgentRegistry, AgentMetadata, AgentCapability, AgentStatus, TaskResult

        class TestAgent(Agent):
            async def _do_initialize(self, config):
                pass

            async def _do_execute(self, context, input_data):
                return TaskResult(success=True, output={})

            async def _do_shutdown(self):
                pass

        registry = AgentRegistry()
        agent = TestAgent(agent_id="reg-test-001", agent_type="test")

        await registry.register(agent.metadata)
        agents = await registry.list_all()
        assert len(agents) == 1

    @pytest.mark.asyncio
    async def test_unregister_agent(self):
        """Test unregistering an agent."""
        from openhub.core.agent import Agent, AgentRegistry, AgentMetadata, AgentCapability, AgentStatus, TaskResult

        class TestAgent(Agent):
            async def _do_initialize(self, config):
                pass

            async def _do_execute(self, context, input_data):
                return TaskResult(success=True, output={})

            async def _do_shutdown(self):
                pass

        registry = AgentRegistry()
        agent = TestAgent(agent_id="reg-test-002", agent_type="test")

        await registry.register(agent.metadata)
        await registry.unregister("reg-test-002")
        agents = await registry.list_all()
        assert len(agents) == 0

    @pytest.mark.asyncio
    async def test_find_by_capability(self):
        """Test finding agents by capability."""
        from openhub.core.agent import Agent, AgentRegistry, AgentMetadata, AgentCapability, AgentStatus, TaskResult

        class TestAgent(Agent):
            async def _do_initialize(self, config):
                pass

            async def _do_execute(self, context, input_data):
                return TaskResult(success=True, output={})

            async def _do_shutdown(self):
                pass

        registry = AgentRegistry()
        agent = TestAgent(agent_id="cap-test-001", agent_type="test")

        await registry.register(agent.metadata)
        agents = await registry.find_by_capability(AgentCapability.CODE_GENERATION)
        assert isinstance(agents, list)


class TestAgentManager:
    """Tests for agent manager functionality."""

    @pytest.mark.asyncio
    async def test_manager_initialization(self):
        """Test manager can be initialized."""
        from openhub.core.agent import AgentRegistry, AgentManager

        registry = AgentRegistry()
        manager = AgentManager(registry)

        assert manager is not None
        assert manager.max_instances == 100

    @pytest.mark.asyncio
    async def test_get_health_report(self):
        """Test getting health report."""
        from openhub.core.agent import AgentRegistry, AgentManager

        registry = AgentRegistry()
        manager = AgentManager(registry)

        report = await manager.get_health_report()
        assert "manager_status" in report
        assert "total_instances" in report


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
