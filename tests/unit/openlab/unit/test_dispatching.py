# Copyright (c) 2026 SPHARX. All Rights Reserved.
# "From data intelligence emerges."

"""
Unit Tests for Dispatching Strategies
====================================
"""

import pytest
from typing import Dict, List, Optional
import sys
import os

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))


class TestAgentMetrics:
    """Tests for AgentMetrics dataclass."""

    def test_agent_metrics_creation(self):
        """Test AgentMetrics can be created."""
        from openlab.contrib.strategies.dispatching import AgentMetrics

        metrics = AgentMetrics(agent_id="agent-001")
        assert metrics.agent_id == "agent-001"
        assert metrics.weight == 1.0
        assert metrics.current_load == 0.0
        assert metrics.success_rate == 1.0

    def test_agent_metrics_with_values(self):
        """Test AgentMetrics with custom values."""
        from openlab.contrib.strategies.dispatching import AgentMetrics

        metrics = AgentMetrics(
            agent_id="agent-002",
            weight=2.0,
            current_load=0.5,
            avg_response_time=1.5,
            success_rate=0.95,
            priority=10,
            capabilities=["coding", "testing"]
        )

        assert metrics.weight == 2.0
        assert metrics.current_load == 0.5
        assert metrics.avg_response_time == 1.5
        assert metrics.success_rate == 0.95
        assert metrics.priority == 10
        assert len(metrics.capabilities) == 2


class TestTaskContext:
    """Tests for TaskContext dataclass."""

    def test_task_context_creation(self):
        """Test TaskContext can be created."""
        from openlab.contrib.strategies.dispatching import TaskContext

        context = TaskContext(task_id="task-001")
        assert context.task_id == "task-001"
        assert context.task_type == "default"
        assert context.priority == 0

    def test_task_context_with_requirements(self):
        """Test TaskContext with requirements."""
        from openlab.contrib.strategies.dispatching import TaskContext

        context = TaskContext(
            task_id="task-002",
            task_type="coding",
            priority=5,
            required_capabilities=["python", "testing"],
            estimated_duration=3600.0,
            deadline=1234567890.0
        )

        assert context.task_type == "coding"
        assert context.priority == 5
        assert "python" in context.required_capabilities
        assert context.estimated_duration == 3600.0
        assert context.deadline == 1234567890.0


class TestDispatchStrategy:
    """Tests for DispatchStrategy base class."""

    def test_strategy_stats(self):
        """Test strategy statistics tracking."""
        from openlab.contrib.strategies.dispatching import WeightedRoundRobinStrategy

        strategy = WeightedRoundRobinStrategy()
        stats = strategy.get_stats()

        assert stats["strategy"] == "weighted_round_robin"
        assert stats["total_selections"] == 0
        assert isinstance(stats["selection_distribution"], dict)


class TestWeightedRoundRobinStrategy:
    """Tests for WeightedRoundRobinStrategy class."""

    @pytest.fixture
    def strategy(self):
        """Create a test strategy."""
        from openlab.contrib.strategies.dispatching import WeightedRoundRobinStrategy
        return WeightedRoundRobinStrategy()

    def test_strategy_creation(self, strategy):
        """Test strategy can be created."""
        assert strategy.name == "weighted_round_robin"

    def test_select_single_candidate(self, strategy):
        """Test selecting from single candidate."""
        from openlab.contrib.strategies.dispatching import AgentMetrics

        candidates = [AgentMetrics(agent_id="agent-001")]
        selected = strategy.select(candidates)

        assert selected is not None
        assert selected.agent_id == "agent-001"

    def test_select_empty_candidates(self, strategy):
        """Test selecting from empty candidates."""
        selected = strategy.select([])
        assert selected is None

    def test_select_multiple_candidates(self, strategy):
        """Test selecting from multiple candidates."""
        from openlab.contrib.strategies.dispatching import AgentMetrics

        candidates = [
            AgentMetrics(agent_id="agent-001", weight=1.0),
            AgentMetrics(agent_id="agent-002", weight=2.0),
        ]

        selected = strategy.select(candidates)
        assert selected is not None
        assert selected.agent_id in ["agent-001", "agent-002"]

    def test_select_with_weights(self):
        """Test selection with custom weights."""
        from openlab.contrib.strategies.dispatching import (
            WeightedRoundRobinStrategy,
            AgentMetrics
        )

        strategy = WeightedRoundRobinStrategy(
            weights={"agent-001": 10.0, "agent-002": 1.0}
        )

        candidates = [
            AgentMetrics(agent_id="agent-001"),
            AgentMetrics(agent_id="agent-002"),
        ]

        # Run multiple selections and count
        counts = {"agent-001": 0, "agent-002": 0}
        for _ in range(100):
            selected = strategy.select(candidates)
            if selected:
                counts[selected.agent_id] += 1

        # Agent with higher weight should be selected more often
        assert counts["agent-001"] > counts["agent-002"]

    def test_stats_update(self, strategy):
        """Test statistics are updated after selection."""
        from openlab.contrib.strategies.dispatching import AgentMetrics

        candidates = [AgentMetrics(agent_id="agent-001")]
        strategy.select(candidates)

        stats = strategy.get_stats()
        assert stats["total_selections"] == 1
        assert stats["selection_distribution"]["agent-001"] == 1


class TestPriorityBasedStrategy:
    """Tests for PriorityBasedStrategy class."""

    @pytest.fixture
    def strategy(self):
        """Create a test strategy."""
        from openlab.contrib.strategies.dispatching import PriorityBasedStrategy
        return PriorityBasedStrategy()

    def test_strategy_creation(self, strategy):
        """Test strategy can be created."""
        assert strategy.name == "priority_based"

    def test_select_highest_priority(self, strategy):
        """Test selecting agent with highest priority."""
        from openlab.contrib.strategies.dispatching import AgentMetrics

        candidates = [
            AgentMetrics(agent_id="agent-001", priority=5),
            AgentMetrics(agent_id="agent-002", priority=10),
            AgentMetrics(agent_id="agent-003", priority=3),
        ]

        selected = strategy.select(candidates)
        assert selected is not None
        assert selected.agent_id == "agent-002"


class TestLeastLoadedStrategy:
    """Tests for LeastLoadedStrategy class."""

    @pytest.fixture
    def strategy(self):
        """Create a test strategy."""
        from openlab.contrib.strategies.dispatching import LeastLoadedStrategy
        return LeastLoadedStrategy()

    def test_strategy_creation(self, strategy):
        """Test strategy can be created."""
        assert strategy.name == "least_loaded"

    def test_select_least_loaded(self, strategy):
        """Test selecting agent with least load."""
        from openlab.contrib.strategies.dispatching import AgentMetrics

        candidates = [
            AgentMetrics(agent_id="agent-001", current_load=0.8),
            AgentMetrics(agent_id="agent-002", current_load=0.2),
            AgentMetrics(agent_id="agent-003", current_load=0.5),
        ]

        selected = strategy.select(candidates)
        assert selected is not None
        assert selected.agent_id == "agent-002"
        assert selected.current_load == 0.2


class TestAdaptiveMLStrategy:
    """Tests for AdaptiveMLStrategy class."""

    @pytest.fixture
    def strategy(self):
        """Create a test strategy."""
        from openlab.contrib.strategies.dispatching import AdaptiveMLStrategy
        return AdaptiveMLStrategy()

    def test_strategy_creation(self, strategy):
        """Test strategy can be created."""
        assert strategy.name == "adaptive_ml"

    def test_select_adaptive(self, strategy):
        """Test adaptive selection."""
        from openlab.contrib.strategies.dispatching import AgentMetrics, TaskContext

        candidates = [
            AgentMetrics(
                agent_id="agent-001",
                current_load=0.5,
                success_rate=0.9,
                avg_response_time=1.0
            ),
            AgentMetrics(
                agent_id="agent-002",
                current_load=0.3,
                success_rate=0.7,
                avg_response_time=2.0
            ),
        ]

        context = TaskContext(task_id="task-001", task_type="coding")
        selected = strategy.select(candidates, context)

        assert selected is not None
        assert selected.agent_id in ["agent-001", "agent-002"]


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
