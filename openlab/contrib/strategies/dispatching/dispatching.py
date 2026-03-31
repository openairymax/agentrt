"""
Copyright (c) 2026 SPHARX. All Rights Reserved.
"From data intelligence emerges."

openlab Contrib - Dispatching Strategies
========================================

Task dispatching strategies for intelligent agent selection.

Available Strategies:
    - WeightedRoundRobinStrategy: Weight-based fair scheduling
    - PriorityBasedStrategy: Priority-driven scheduling
    - AdaptiveMLStrategy: ML-driven adaptive scheduling

Example:
    from openlab.contrib.strategies.dispatching import (
        WeightedRoundRobinStrategy,
        PriorityBasedStrategy,
    )

    strategy = WeightedRoundRobinStrategy(weights=[1, 2, 3])
    selected_agent = strategy.select(candidates)

Author: AgentOS Architecture Committee
Version: 1.0.0.6
"""

from __future__ import annotations

import logging
import random
from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from enum import Enum, auto
from typing import Any, Dict, List, Optional, Tuple

logger = logging.getLogger(__name__)


class DispatchStrategyType(Enum):
    """Supported dispatch strategy types."""
    WEIGHTED_ROUND_ROBIN = auto()
    PRIORITY_BASED = auto()
    LEAST_LOADED = auto()
    ADAPTIVE_ML = auto()


@dataclass
class AgentMetrics:
    """Agent performance metrics for dispatching decisions.

    Attributes:
        agent_id: Unique agent identifier.
        weight: Relative weight for weighted strategies.
        current_load: Current task load (0.0 to 1.0).
        avg_response_time: Average response time in seconds.
        success_rate: Task success rate (0.0 to 1.0).
        priority: Agent priority level.
        capabilities: Set of agent capabilities.
    """
    agent_id: str
    weight: float = 1.0
    current_load: float = 0.0
    avg_response_time: float = 0.0
    success_rate: float = 1.0
    priority: int = 0
    capabilities: List[str] = field(default_factory=list)


@dataclass
class TaskContext:
    """Task context for dispatching decisions.

    Attributes:
        task_id: Unique task identifier.
        task_type: Type of task.
        priority: Task priority level.
        required_capabilities: Required agent capabilities.
        estimated_duration: Estimated task duration in seconds.
        deadline: Optional task deadline timestamp.
    """
    task_id: str
    task_type: str = "default"
    priority: int = 0
    required_capabilities: List[str] = field(default_factory=list)
    estimated_duration: float = 0.0
    deadline: Optional[float] = None


class DispatchStrategy(ABC):
    """Abstract base class for dispatch strategies.

    All dispatch strategies must implement the select() method
    to choose the best agent for a given task.
    """

    def __init__(self, name: str = "base"):
        """Initialize dispatch strategy.

        Args:
            name: Strategy name for logging.
        """
        self.name = name
        self._selection_count: Dict[str, int] = {}
        self._total_selections = 0

    @abstractmethod
    def select(
        self,
        candidates: List[AgentMetrics],
        context: Optional[TaskContext] = None,
    ) -> Optional[AgentMetrics]:
        """Select the best agent from candidates.

        Args:
            candidates: List of candidate agents with metrics.
            context: Optional task context for decision.

        Returns:
            Selected agent metrics, or None if no suitable agent.
        """
        pass

    def _record_selection(self, agent_id: str) -> None:
        """Record agent selection for statistics.

        Args:
            agent_id: Selected agent ID.
        """
        self._selection_count[agent_id] = self._selection_count.get(agent_id, 0) + 1
        self._total_selections += 1

    def get_stats(self) -> Dict[str, Any]:
        """Get strategy statistics.

        Returns:
            Dictionary with selection statistics.
        """
        return {
            "strategy": self.name,
            "total_selections": self._total_selections,
            "selection_distribution": dict(self._selection_count),
        }


class WeightedRoundRobinStrategy(DispatchStrategy):
    """Weighted round-robin dispatch strategy.

    Distributes tasks based on agent weights, ensuring
    fair distribution proportional to each agent's capacity.

    Example:
        strategy = WeightedRoundRobinStrategy(weights={"agent-1": 2, "agent-2": 1})
        selected = strategy.select(candidates)
    """

    def __init__(
        self,
        weights: Optional[Dict[str, float]] = None,
        name: str = "weighted_round_robin",
    ):
        """Initialize weighted round-robin strategy.

        Args:
            weights: Optional weight mapping (agent_id -> weight).
            name: Strategy name.
        """
        super().__init__(name)
        self._weights = weights or {}
        self._current_index = 0
        self._counter = 0

    def select(
        self,
        candidates: List[AgentMetrics],
        context: Optional[TaskContext] = None,
    ) -> Optional[AgentMetrics]:
        """Select agent using weighted round-robin.

        Args:
            candidates: List of candidate agents.
            context: Optional task context.

        Returns:
            Selected agent or None.
        """
        if not candidates:
            return None

        if len(candidates) == 1:
            self._record_selection(candidates[0].agent_id)
            return candidates[0]

        effective_weights = []
        for agent in candidates:
            weight = self._weights.get(agent.agent_id, agent.weight)
            effective_weights.append(max(weight, 0.1))

        total_weight = sum(effective_weights)
        thresholds = []
        cumulative = 0.0
        for w in effective_weights:
            cumulative += w / total_weight
            thresholds.append(cumulative)

        self._counter += 1
        selection_point = (self._counter - 1) % 100 / 100.0

        for i, threshold in enumerate(thresholds):
            if selection_point < threshold:
                self._record_selection(candidates[i].agent_id)
                return candidates[i]

        self._record_selection(candidates[-1].agent_id)
        return candidates[-1]

    def set_weight(self, agent_id: str, weight: float) -> None:
        """Set weight for an agent.

        Args:
            agent_id: Agent identifier.
            weight: New weight value.
        """
        self._weights[agent_id] = weight


class PriorityBasedStrategy(DispatchStrategy):
    """Priority-based dispatch strategy.

    Selects agents based on combined priority of task and agent,
    with optional capability matching.

    Example:
        strategy = PriorityBasedStrategy()
        selected = strategy.select(candidates, context=task_context)
    """

    def __init__(
        self,
        capability_match_bonus: float = 10.0,
        load_penalty_factor: float = 5.0,
        name: str = "priority_based",
    ):
        """Initialize priority-based strategy.

        Args:
            capability_match_bonus: Bonus for capability matching.
            load_penalty_factor: Penalty factor for high load.
            name: Strategy name.
        """
        super().__init__(name)
        self.capability_match_bonus = capability_match_bonus
        self.load_penalty_factor = load_penalty_factor

    def select(
        self,
        candidates: List[AgentMetrics],
        context: Optional[TaskContext] = None,
    ) -> Optional[AgentMetrics]:
        """Select agent based on priority scoring.

        Args:
            candidates: List of candidate agents.
            context: Optional task context.

        Returns:
            Selected agent or None.
        """
        if not candidates:
            return None

        if len(candidates) == 1:
            self._record_selection(candidates[0].agent_id)
            return candidates[0]

        scored_candidates = []
        for agent in candidates:
            score = self._calculate_score(agent, context)
            scored_candidates.append((score, agent))

        scored_candidates.sort(key=lambda x: x[0], reverse=True)
        selected = scored_candidates[0][1]
        self._record_selection(selected.agent_id)
        return selected

    def _calculate_score(
        self,
        agent: AgentMetrics,
        context: Optional[TaskContext],
    ) -> float:
        """Calculate priority score for an agent.

        Args:
            agent: Agent metrics.
            context: Task context.

        Returns:
            Priority score.
        """
        score = float(agent.priority)

        score += agent.success_rate * 10

        score -= agent.current_load * self.load_penalty_factor

        if agent.avg_response_time > 0:
            score -= min(agent.avg_response_time / 10.0, 5.0)

        if context and context.required_capabilities:
            matched = set(agent.capabilities) & set(context.required_capabilities)
            match_ratio = len(matched) / len(context.required_capabilities)
            score += match_ratio * self.capability_match_bonus

        return max(score, 0.0)


class LeastLoadedStrategy(DispatchStrategy):
    """Least-loaded dispatch strategy.

    Selects the agent with the lowest current load.

    Example:
        strategy = LeastLoadedStrategy()
        selected = strategy.select(candidates)
    """

    def __init__(self, name: str = "least_loaded"):
        """Initialize least-loaded strategy.

        Args:
            name: Strategy name.
        """
        super().__init__(name)

    def select(
        self,
        candidates: List[AgentMetrics],
        context: Optional[TaskContext] = None,
    ) -> Optional[AgentMetrics]:
        """Select agent with lowest load.

        Args:
            candidates: List of candidate agents.
            context: Optional task context.

        Returns:
            Selected agent or None.
        """
        if not candidates:
            return None

        if len(candidates) == 1:
            self._record_selection(candidates[0].agent_id)
            return candidates[0]

        sorted_candidates = sorted(
            candidates,
            key=lambda a: (a.current_load, -a.success_rate, a.avg_response_time),
        )

        selected = sorted_candidates[0]
        self._record_selection(selected.agent_id)
        return selected


class AdaptiveMLStrategy(DispatchStrategy):
    """Machine learning-driven adaptive dispatch strategy.

    Uses historical performance data to make optimal selections.
    Falls back to priority-based selection when insufficient data.

    Example:
        strategy = AdaptiveMLStrategy()
        selected = strategy.select(candidates, context=task_context)
    """

    def __init__(
        self,
        learning_rate: float = 0.1,
        history_size: int = 100,
        name: str = "adaptive_ml",
    ):
        """Initialize adaptive ML strategy.

        Args:
            learning_rate: Learning rate for score updates.
            history_size: Maximum history entries per agent.
            name: Strategy name.
        """
        super().__init__(name)
        self.learning_rate = learning_rate
        self.history_size = history_size
        self._agent_scores: Dict[str, float] = {}
        self._fallback = PriorityBasedStrategy()

    def select(
        self,
        candidates: List[AgentMetrics],
        context: Optional[TaskContext] = None,
    ) -> Optional[AgentMetrics]:
        """Select agent using adaptive scoring.

        Args:
            candidates: List of candidate agents.
            context: Optional task context.

        Returns:
            Selected agent or None.
        """
        if not candidates:
            return None

        if len(candidates) == 1:
            self._record_selection(candidates[0].agent_id)
            return candidates[0]

        has_learned_scores = any(
            agent.agent_id in self._agent_scores for agent in candidates
        )

        if not has_learned_scores:
            selected = self._fallback.select(candidates, context)
            if selected:
                self._record_selection(selected.agent_id)
            return selected

        scored_candidates = []
        for agent in candidates:
            base_score = self._agent_scores.get(agent.agent_id, 50.0)
            load_factor = 1.0 - agent.current_load
            success_factor = agent.success_rate
            adjusted_score = base_score * load_factor * success_factor
            scored_candidates.append((adjusted_score, agent))

        scored_candidates.sort(key=lambda x: x[0], reverse=True)
        selected = scored_candidates[0][1]
        self._record_selection(selected.agent_id)
        return selected

    def update_score(
        self,
        agent_id: str,
        success: bool,
        response_time: float,
    ) -> None:
        """Update agent score based on task outcome.

        Args:
            agent_id: Agent identifier.
            success: Whether task succeeded.
            response_time: Task response time in seconds.
        """
        current_score = self._agent_scores.get(agent_id, 50.0)

        if success:
            time_bonus = max(0, 10 - response_time)
            delta = 5 + time_bonus
        else:
            delta = -10

        new_score = current_score + self.learning_rate * delta
        self._agent_scores[agent_id] = max(0, min(100, new_score))

    def get_agent_scores(self) -> Dict[str, float]:
        """Get current agent scores.

        Returns:
            Dictionary of agent scores.
        """
        return dict(self._agent_scores)


__all__ = [
    "DispatchStrategyType",
    "AgentMetrics",
    "TaskContext",
    "DispatchStrategy",
    "WeightedRoundRobinStrategy",
    "PriorityBasedStrategy",
    "LeastLoadedStrategy",
    "AdaptiveMLStrategy",
]
