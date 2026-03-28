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

    def test_agent_initialization(self):
        """Test agent can be initialized."""
        from openlab.core.agent import Agent, AgentStatus

        agent = Agent(
            agent_id="test-agent-001",
            name="TestAgent",
            capabilities=["coding", "testing"]
        )

        assert agent.agent_id == "test-agent-001"
        assert agent.name == "TestAgent"
        assert agent.status == AgentStatus.IDLE
        assert len(agent.capabilities) == 2

    def test_agent_with_capabilities(self, sample_agent_data):
        """Test agent initialization with capabilities."""
        from openlab.core.agent import Agent, Capability

        capabilities = [
            Capability(name=c["name"], level=c["level"])
            for c in sample_agent_data["capabilities"]
        ]

        agent = Agent(
            agent_id="test-agent-002",
            name=sample_agent_data["name"],
            capabilities=capabilities
        )

        assert len(agent.capabilities) == 2
        assert agent.get_capability_level("coding") == 0.9
        assert agent.get_capability_level("testing") == 0.8

    def test_agent_status_transitions(self):
        """Test agent status can be changed."""
        from openlab.core.agent import Agent, AgentStatus

        agent = Agent(agent_id="test-agent-003", name="StatusTestAgent")

        agent.status = AgentStatus.BUSY
        assert agent.status == AgentStatus.BUSY

        agent.status = AgentStatus.IDLE
        assert agent.status == AgentStatus.IDLE

    def test_agent_task_counting(self):
        """Test agent tracks current tasks correctly."""
        from openlab.core.agent import Agent

        agent = Agent(agent_id="test-agent-004", name="TaskCountAgent")

        assert agent.current_tasks == 0

        agent.increment_tasks()
        assert agent.current_tasks == 1

        agent.increment_tasks()
        assert agent.current_tasks == 2

        agent.decrement_tasks()
        assert agent.current_tasks == 1


class TestAgentRegistry:
    """Tests for agent registry functionality."""

    def test_registry_initialization(self):
        """Test registry can be initialized."""
        from openlab.core.agent import AgentRegistry

        registry = AgentRegistry()
        assert registry is not None
        assert len(registry.list_agents()) == 0

    def test_register_agent(self):
        """Test registering an agent."""
        from openlab.core.agent import Agent, AgentRegistry

        registry = AgentRegistry()
        agent = Agent(agent_id="reg-test-001", name="RegisteredAgent")

        success = registry.register(agent)
        assert success is True
        assert len(registry.list_agents()) == 1

    def test_unregister_agent(self):
        """Test unregistering an agent."""
        from openlab.core.agent import Agent, AgentRegistry

        registry = AgentRegistry()
        agent = Agent(agent_id="reg-test-002", name="UnregisterAgent")

        registry.register(agent)
        assert len(registry.list_agents()) == 1

        success = registry.unregister("reg-test-002")
        assert success is True
        assert len(registry.list_agents()) == 0

    def test_find_agent_by_id(self):
        """Test finding an agent by ID."""
        from openlab.core.agent import Agent, AgentRegistry

        registry = AgentRegistry()
        agent = Agent(agent_id="find-test-001", name="FindableAgent")

        registry.register(agent)
        found = registry.find_by_id("find-test-001")

        assert found is not None
        assert found.name == "FindableAgent"

    def test_find_agents_by_capability(self):
        """Test finding agents by capability."""
        from openlab.core.agent import Agent, Capability, AgentRegistry

        registry = AgentRegistry()

        agent1 = Agent(
            agent_id="cap-test-001",
            name="AgentWithCoding",
            capabilities=[Capability(name="coding", level=0.9)]
        )

        agent2 = Agent(
            agent_id="cap-test-002",
            name="AgentWithTesting",
            capabilities=[Capability(name="testing", level=0.8)]
        )

        registry.register(agent1)
        registry.register(agent2)

        coding_agents = registry.find_by_capability("coding", min_level=0.5)
        assert len(coding_agents) == 1
        assert coding_agents[0].name == "AgentWithCoding"


class TestAgentMessaging:
    """Tests for agent messaging functionality."""

    def test_send_message(self):
        """Test sending a message to an agent."""
        from openlab.core.agent import Agent, AgentRegistry, Message

        registry = AgentRegistry()
        agent = Agent(agent_id="msg-test-001", name="ReceiverAgent")
        registry.register(agent)

        message = Message(
            sender_id="sender-001",
            receiver_id="msg-test-001",
            content={"type": "task", "data": "test"}
        )

        success = registry.send_message(message)
        assert success is True

    def test_get_messages(self):
        """Test retrieving messages for an agent."""
        from openlab.core.agent import Agent, AgentRegistry, Message

        registry = AgentRegistry()
        agent = Agent(agent_id="get-msg-test-001", name="InboxAgent")
        registry.register(agent)

        message = Message(
            sender_id="sender-001",
            receiver_id="get-msg-test-001",
            content={"type": "task"}
        )
        registry.send_message(message)

        messages = registry.get_messages("get-msg-test-001")
        assert len(messages) == 1


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
