"""Dispatching strategy module — contrib skeleton."""

from typing import Any, Dict, List, Optional, Set
from dataclasses import dataclass, field
import random


@dataclass
class AgentMetrics:
    """Metrics for an agent used in dispatching decisions."""
    agent_id: str
    weight: float = 1.0
    current_load: float = 0.0
    avg_response_time: float = 0.0
    success_rate: float = 1.0
    priority: int = 0
    capabilities: List[str] = field(default_factory=list)


@dataclass
class TaskContext:
    """Context for a task to be dispatched."""
    task_id: str
    task_type: str = "default"
    priority: int = 0
    required_capabilities: List[str] = field(default_factory=list)
    estimated_duration: float = 0.0
    deadline: Optional[float] = None


class DispatchingStrategy:
    """Dispatching strategy for task assignment — contrib skeleton."""

    def __init__(self, config: Optional[Dict[str, Any]] = None):
        self.config = config or {}

    async def dispatch(self, task: Any, agents: list = None) -> Dict[str, Any]:
        return {
            "status": "completed",
            "strategy": "dispatching",
            "assigned_agents": [],
            "task": str(task) if task else "",
        }


class WeightedRoundRobinStrategy(DispatchingStrategy):
    """Weighted round-robin agent selection strategy."""

    def __init__(self, weights: Optional[Dict[str, float]] = None, config: Optional[Dict[str, Any]] = None):
        super().__init__(config)
        self.name = "weighted_round_robin"
        self.weights = weights or {}
        self._total_selections = 0
        self._selection_distribution: Dict[str, int] = {}
        self._current_index = 0

    def select(self, candidates: List[AgentMetrics], context: Optional[TaskContext] = None) -> Optional[AgentMetrics]:
        """Select an agent from candidates using weighted round-robin."""
        if not candidates:
            return None

        if len(candidates) == 1:
            selected = candidates[0]
        else:
            # Weighted selection
            total_weight = sum(self.weights.get(c.agent_id, c.weight) for c in candidates)
            if total_weight <= 0:
                selected = candidates[self._current_index % len(candidates)]
                self._current_index += 1
            else:
                r = random.uniform(0, total_weight)
                cumulative = 0
                selected = candidates[0]
                for c in candidates:
                    cumulative += self.weights.get(c.agent_id, c.weight)
                    if r <= cumulative:
                        selected = c
                        break

        self._total_selections += 1
        self._selection_distribution[selected.agent_id] = (
            self._selection_distribution.get(selected.agent_id, 0) + 1
        )
        return selected

    def get_stats(self) -> Dict[str, Any]:
        return {
            "strategy": self.name,
            "total_selections": self._total_selections,
            "selection_distribution": dict(self._selection_distribution),
        }


class PriorityBasedStrategy(DispatchingStrategy):
    """Priority-based agent selection strategy."""

    def __init__(self, config: Optional[Dict[str, Any]] = None):
        super().__init__(config)
        self.name = "priority_based"
        self._total_selections = 0
        self._selection_distribution: Dict[str, int] = {}

    def select(self, candidates: List[AgentMetrics], context: Optional[TaskContext] = None) -> Optional[AgentMetrics]:
        """Select the agent with the highest priority."""
        if not candidates:
            return None

        selected = max(candidates, key=lambda c: c.priority)

        self._total_selections += 1
        self._selection_distribution[selected.agent_id] = (
            self._selection_distribution.get(selected.agent_id, 0) + 1
        )
        return selected

    def get_stats(self) -> Dict[str, Any]:
        return {
            "strategy": self.name,
            "total_selections": self._total_selections,
            "selection_distribution": dict(self._selection_distribution),
        }


class LeastLoadedStrategy(DispatchingStrategy):
    """Least-loaded agent selection strategy."""

    def __init__(self, config: Optional[Dict[str, Any]] = None):
        super().__init__(config)
        self.name = "least_loaded"
        self._total_selections = 0
        self._selection_distribution: Dict[str, int] = {}

    def select(self, candidates: List[AgentMetrics], context: Optional[TaskContext] = None) -> Optional[AgentMetrics]:
        """Select the agent with the lowest current load."""
        if not candidates:
            return None

        selected = min(candidates, key=lambda c: c.current_load)

        self._total_selections += 1
        self._selection_distribution[selected.agent_id] = (
            self._selection_distribution.get(selected.agent_id, 0) + 1
        )
        return selected

    def get_stats(self) -> Dict[str, Any]:
        return {
            "strategy": self.name,
            "total_selections": self._total_selections,
            "selection_distribution": dict(self._selection_distribution),
        }


class AdaptiveMLStrategy(DispatchingStrategy):
    """Adaptive ML-based agent selection strategy."""

    def __init__(self, config: Optional[Dict[str, Any]] = None):
        super().__init__(config)
        self.name = "adaptive_ml"
        self._total_selections = 0
        self._selection_distribution: Dict[str, int] = {}

    def select(self, candidates: List[AgentMetrics], context: Optional[TaskContext] = None) -> Optional[AgentMetrics]:
        """Select an agent using adaptive scoring based on multiple metrics."""
        if not candidates:
            return None

        if len(candidates) == 1:
            selected = candidates[0]
        else:
            def score(agent: AgentMetrics) -> float:
                return (agent.success_rate * 0.4 +
                        (1.0 - agent.current_load) * 0.3 +
                        (1.0 / (1.0 + agent.avg_response_time)) * 0.3)

            selected = max(candidates, key=score)

        self._total_selections += 1
        self._selection_distribution[selected.agent_id] = (
            self._selection_distribution.get(selected.agent_id, 0) + 1
        )
        return selected

    def get_stats(self) -> Dict[str, Any]:
        return {
            "strategy": self.name,
            "total_selections": self._total_selections,
            "selection_distribution": dict(self._selection_distribution),
        }


__all__ = [
    "DispatchingStrategy",
    "AgentMetrics",
    "TaskContext",
    "WeightedRoundRobinStrategy",
    "PriorityBasedStrategy",
    "LeastLoadedStrategy",
    "AdaptiveMLStrategy",
]