# Copyright (c) 2026 SPHARX. All Rights Reserved.
# "From data intelligence emerges."

"""
Unit Tests for Dispatching Strategy
=================================
"""

import pytest
import sys
import os

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))


class TestDispatchingStrategy:
    """Tests for dispatching strategy."""

    def test_strategy_initialization(self):
        """Test strategy can be initialized."""
        from contrib.strategies.dispatching import DispatchingStrategy

        strategy = DispatchingStrategy()
        assert strategy is not None
        assert strategy.capability_weight == 0.4
        assert strategy.priority_weight == 0.3
        assert strategy.load_weight == 0.3

    def test_register_agent(self):
        """Test agent registration."""
        from contrib.strategies.dispatching import DispatchingStrategy, AgentInfo, Capability

        strategy = DispatchingStrategy()
        agent = AgentInfo(
            agent_id="agent-001",
            name="TestAgent",
            capabilities=[Capability(name="coding", level=0.9)]
        )

        success = strategy.register_agent(agent)
        assert success is True

    def test_unregister_agent(self):
        """Test agent unregistration."""
        from contrib.strategies.dispatching import DispatchingStrategy, AgentInfo, Capability

        strategy = DispatchingStrategy()
        agent = AgentInfo(
            agent_id="agent-002",
            name="TestAgent2",
            capabilities=[Capability(name="testing", level=0.8)]
        )

        strategy.register_agent(agent)
        success = strategy.unregister_agent("agent-002")
        assert success is True

    def test_select_agent_with_no_agents(self):
        """Test agent selection with no registered agents."""
        from contrib.strategies.dispatching import DispatchingStrategy, TaskRequirements

        strategy = DispatchingStrategy()
        requirements = TaskRequirements(required_capabilities=["coding"])

        selected = strategy.select_agent(requirements)
        assert selected is None

    def test_select_agent_by_capability(self):
        """Test agent selection by capability."""
        from contrib.strategies.dispatching import (
            DispatchingStrategy, AgentInfo, Capability, TaskRequirements
        )

        strategy = DispatchingStrategy()

        agent1 = AgentInfo(
            agent_id="cap-agent-001",
            name="CodingAgent",
            capabilities=[Capability(name="coding", level=0.9)]
        )

        agent2 = AgentInfo(
            agent_id="cap-agent-002",
            name="DesignAgent",
            capabilities=[Capability(name="design", level=0.8)]
        )

        strategy.register_agent(agent1)
        strategy.register_agent(agent2)

        requirements = TaskRequirements(required_capabilities=["coding"])
        selected = strategy.select_agent(requirements)

        assert selected is not None
        assert selected.name == "CodingAgent"

    def test_dispatch_task(self):
        """Test task dispatching."""
        from contrib.strategies.dispatching import (
            DispatchingStrategy, AgentInfo, Capability, TaskRequirements, Priority
        )

        strategy = DispatchingStrategy()

        agent = AgentInfo(
            agent_id="dispatch-agent-001",
            name="DispatchAgent",
            capabilities=[Capability(name="coding", level=0.9)]
        )

        strategy.register_agent(agent)

        task_data = {
            "id": "task-001",
            "description": "Test task",
            "type": "code_generation",
            "metadata": {
                "required_capabilities": ["coding"],
                "priority": "high"
            }
        }

        result = strategy.dispatch(task_data)

        assert result.success is True
        assert result.agent_id == "dispatch-agent-001"

    def test_dispatch_with_no_suitable_agent(self):
        """Test dispatching when no suitable agent exists."""
        from contrib.strategies.dispatching import DispatchingStrategy, TaskRequirements

        strategy = DispatchingStrategy()
        task_data = {
            "id": "task-002",
            "description": "Test task",
            "metadata": {"required_capabilities": ["quantum_computing"]}
        }

        result = strategy.dispatch(task_data)

        assert result.success is False
        assert result.error is not None


class TestTaskRequirements:
    """Tests for task requirements."""

    def test_task_requirements_creation(self):
        """Test creating task requirements."""
        from contrib.strategies.dispatching import TaskRequirements, Priority

        requirements = TaskRequirements(
            required_capabilities=["coding", "testing"],
            preferred_agent="agent-001",
            priority=Priority.HIGH,
            estimated_complexity=7.0
        )

        assert len(requirements.required_capabilities) == 2
        assert requirements.priority == Priority.HIGH
        assert requirements.estimated_complexity == 7.0


class TestAgentInfo:
    """Tests for agent info."""

    def test_agent_info_creation(self):
        """Test creating agent info."""
        from contrib.strategies.dispatching import AgentInfo, Capability

        agent = AgentInfo(
            agent_id="info-agent-001",
            name="InfoTestAgent",
            capabilities=[Capability(name="design", level=0.85)]
        )

        assert agent.agent_id == "info-agent-001"
        assert agent.available_slots == 5

    def test_has_capability(self):
        """Test capability checking."""
        from contrib.strategies.dispatching import AgentInfo, Capability

        agent = AgentInfo(
            agent_id="cap-check-001",
            name="CapabilityCheckAgent",
            capabilities=[
                Capability(name="coding", level=0.9),
                Capability(name="testing", level=0.6)
            ]
        )

        assert agent.has_capability("coding", min_level=0.8) is True
        assert agent.has_capability("coding", min_level=0.95) is False
        assert agent.has_capability("design", min_level=0.5) is False

    def test_get_capability_level(self):
        """Test getting capability level."""
        from contrib.strategies.dispatching import AgentInfo, Capability

        agent = AgentInfo(
            agent_id="level-agent-001",
            name="LevelTestAgent",
            capabilities=[Capability(name="analysis", level=0.75)]
        )

        assert agent.get_capability_level("analysis") == 0.75
        assert agent.get_capability_level("unknown") == 0.0


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
