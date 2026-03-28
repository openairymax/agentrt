# Copyright (c) 2026 SPHARX. All Rights Reserved.
# "From data intelligence emerges."

"""
Dispatching Strategy Module
=========================

This module provides intelligent task routing and agent selection mechanisms
for the openlab platform. It analyzes task requirements and matches them
with suitable agents based on capabilities, availability, and performance metrics.

Architecture:
- Capability-based matching
- Load balancing
- Priority routing
- Performance tracking
- Affinity-based routing
- Resource-aware scheduling
"""

import logging
import time
import uuid
from dataclasses import dataclass, field
from datetime import datetime
from enum import Enum
from typing import Any, Dict, List, Optional, Tuple

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)


class Priority(Enum):
    """Task priority levels."""
    CRITICAL = 1
    HIGH = 2
    MEDIUM = 3
    LOW = 4
    BACKGROUND = 5


class LoadBalanceMode(Enum):
    """Load balancing modes."""
    ROUND_ROBIN = "round_robin"
    LEAST_LOADED = "least_loaded"
    CAPABILITY_MATCH = "capability_match"
    PERFORMANCE = "performance"
    AFFINITY = "affinity"


@dataclass
class Capability:
    """Agent capability descriptor."""
    name: str
    level: float = 1.0
    metadata: Dict[str, Any] = field(default_factory=dict)


@dataclass
class AgentMetrics:
    """Agent performance metrics."""
    agent_id: str
    total_tasks: int = 0
    successful_tasks: int = 0
    failed_tasks: int = 0
    avg_execution_time: float = 0.0
    current_load: int = 0
    last_updated: datetime = field(default_factory=datetime.utcnow)

    @property
    def success_rate(self) -> float:
        """Calculate success rate."""
        if self.total_tasks == 0:
            return 0.0
        return self.successful_tasks / self.total_tasks

    @property
    def failure_rate(self) -> float:
        """Calculate failure rate."""
        if self.total_tasks == 0:
            return 0.0
        return self.failed_tasks / self.total_tasks


@dataclass
class AgentInfo:
    """Information about a registered agent."""
    agent_id: str
    name: str
    capabilities: List[Capability]
    is_available: bool = True
    max_concurrent_tasks: int = 5
    current_tasks: int = 0
    priority: int = 100
    affinity_tags: List[str] = field(default_factory=list)
    metadata: Dict[str, Any] = field(default_factory=dict)
    metrics: AgentMetrics = field(default_factory=None)

    def __post_init__(self):
        """Initialize default values."""
        if self.metrics is None:
            self.metrics = AgentMetrics(agent_id=self.agent_id)

    @property
    def available_slots(self) -> int:
        """Calculate available task slots."""
        return max(0, self.max_concurrent_tasks - self.current_tasks)

    def has_capability(self, capability_name: str, min_level: float = 0.5) -> bool:
        """Check if agent has a capability at specified level."""
        for cap in self.capabilities:
            if cap.name == capability_name and cap.level >= min_level:
                return True
        return False

    def get_capability_level(self, capability_name: str) -> float:
        """Get agent's level for a specific capability."""
        for cap in self.capabilities:
            if cap.name == capability_name:
                return cap.level
        return 0.0


@dataclass
class TaskRequirements:
    """Requirements for a task to be dispatched."""
    required_capabilities: List[str] = field(default_factory=list)
    preferred_capabilities: List[str] = field(default_factory=list)
    preferred_agent: Optional[str] = None
    excluded_agents: List[str] = field(default_factory=list)
    priority: Priority = Priority.MEDIUM
    estimated_complexity: float = 5.0
    affinity_tags: List[str] = field(default_factory=list)
    estimated_duration: float = 60.0
    metadata: Dict[str, Any] = field(default_factory=dict)


@dataclass
class DispatchResult:
    """Result of a dispatch operation."""
    success: bool
    task_id: Optional[str] = None
    agent_id: Optional[str] = None
    agent_name: Optional[str] = None
    error: Optional[str] = None
    dispatch_time: float = 0.0
    retry_count: int = 0
    alternative_agents: List[str] = field(default_factory=list)


class TaskAnalyzer:
    """Analyzes tasks to extract routing requirements."""

    @staticmethod
    def analyze(task_data: Dict[str, Any]) -> TaskRequirements:
        """Analyze task data and extract requirements."""
        metadata = task_data.get("metadata", {})

        required_caps = metadata.get("required_capabilities", [])
        if not required_caps and "type" in task_data:
            caps_from_type = TaskAnalyzer._infer_capabilities_from_task_type(task_data["type"])
            required_caps.extend(caps_from_type)

        preferred_caps = metadata.get("preferred_capabilities", [])
        affinity_tags = metadata.get("affinity_tags", [])
        if not affinity_tags and "tags" in task_data:
            affinity_tags.extend(task_data["tags"])

        priority_str = metadata.get("priority", "medium")
        try:
            priority = Priority[priority_str.upper()]
        except KeyError:
            priority = Priority.MEDIUM

        return TaskRequirements(
            required_capabilities=required_caps,
            preferred_capabilities=preferred_caps,
            preferred_agent=metadata.get("preferred_agent"),
            excluded_agents=metadata.get("excluded_agents", []),
            priority=priority,
            estimated_complexity=metadata.get("complexity", 5.0),
            affinity_tags=affinity_tags,
            estimated_duration=metadata.get("estimated_duration", 60.0),
            metadata=metadata
        )

    @staticmethod
    def _infer_capabilities_from_task_type(task_type: str) -> List[str]:
        """Infer required capabilities from task type."""
        capability_map = {
            "code_generation": ["coding", "code_generation"],
            "code_review": ["code_analysis", "review"],
            "testing": ["testing", "qa"],
            "documentation": ["documentation", "writing"],
            "data_analysis": ["data_analysis", "analytics"],
            "web_scraping": ["web_scraping", "browser"],
            "database": ["database", "sql"],
            "deployment": ["devops", "deployment"],
            "security": ["security", "analysis"],
            "planning": ["planning", "reasoning"],
        }

        task_lower = task_type.lower()
        for key, capabilities in capability_map.items():
            if key in task_lower:
                return capabilities

        return []


class CapabilityMatcher:
    """Matches tasks to agents based on capabilities."""

    def __init__(self, weight: float = 0.4):
        self.weight = weight

    def calculate_match_score(
        self,
        requirements: TaskRequirements,
        agent: AgentInfo
    ) -> float:
        """Calculate capability match score for an agent."""
        if not requirements.required_capabilities:
            return 1.0

        required_count = len(requirements.required_capabilities)
        matched_level = 0.0

        for cap_name in requirements.required_capabilities:
            level = agent.get_capability_level(cap_name)
            matched_level += level

        base_score = matched_level / required_count if required_count > 0 else 1.0

        preferred_count = len(requirements.preferred_capabilities)
        if preferred_count > 0:
            preferred_matched = 0.0
            for cap_name in requirements.preferred_capabilities:
                level = agent.get_capability_level(cap_name)
                preferred_matched += level
            preferred_score = preferred_matched / preferred_count
            base_score = (base_score * 0.7) + (preferred_score * 0.3)

        return min(1.0, base_score)


class PriorityRouter:
    """Routes tasks based on priority."""

    def __init__(self, weight: float = 0.3):
        self.weight = weight

    def calculate_priority_score(
        self,
        requirements: TaskRequirements,
        agent: AgentInfo
    ) -> float:
        """Calculate priority-based routing score."""
        priority_score = 1.0 - ((requirements.priority.value - 1) / 4)

        agent_priority_score = 1.0 - (agent.priority / 200)

        combined = (priority_score * 0.6) + (agent_priority_score * 0.4)

        return min(1.0, max(0.0, combined))


class LoadBalancer:
    """Balances load across agents."""

    def __init__(self, weight: float = 0.3, mode: LoadBalanceMode = LoadBalanceMode.LEAST_LOADED):
        self.weight = weight
        self.mode = mode
        self._round_robin_index = 0
        self._agent_order = []

    def calculate_load_score(
        self,
        requirements: TaskRequirements,
        agent: AgentInfo
    ) -> float:
        """Calculate load-based score for an agent."""
        if agent.current_tasks >= agent.max_concurrent_tasks:
            return 0.0

        available_ratio = agent.available_slots / agent.max_concurrent_tasks

        if self.mode == LoadBalanceMode.ROUND_ROBIN:
            return 1.0 if agent.agent_id in self._agent_order else 0.5

        elif self.mode == LoadBalanceMode.LEAST_LOADED:
            return available_ratio

        elif self.mode == LoadBalanceMode.PERFORMANCE:
            perf_score = agent.metrics.success_rate * 0.7 + (1.0 - agent.metrics.failure_rate) * 0.3
            return available_ratio * perf_score

        elif self.mode == LoadBalanceMode.AFFINITY:
            if not requirements.affinity_tags:
                return available_ratio

            agent_tags = set(agent.affinity_tags)
            required_tags = set(requirements.affinity_tags)
            overlap = len(agent_tags & required_tags)
            affinity_score = overlap / len(required_tags) if required_tags else 0.5

            return available_ratio * (0.4 + 0.6 * affinity_score)

        return available_ratio

    def select_next_agent(self, agents: List[AgentInfo]) -> Optional[AgentInfo]:
        """Select next agent using round-robin."""
        if not agents:
            return None

        if self._agent_order != [a.agent_id for a in agents]:
            self._agent_order = [a.agent_id for a in agents]
            self._round_robin_index = 0

        for _ in range(len(agents)):
            idx = self._round_robin_index % len(agents)
            agent = agents[idx]
            self._round_robin_index += 1

            if agent.is_available and agent.available_slots > 0:
                return agent

        return None


class AffinityRouter:
    """Routes based on task-agent affinity."""

    def calculate_affinity_score(
        self,
        requirements: TaskRequirements,
        agent: AgentInfo
    ) -> float:
        """Calculate affinity score between task and agent."""
        if not requirements.affinity_tags:
            return 0.5

        agent_tags = set(agent.affinity_tags)
        task_tags = set(requirements.affinity_tags)

        if not agent_tags:
            return 0.5

        overlap = len(agent_tags & task_tags)
        union = len(agent_tags | task_tags)

        jaccard = overlap / union if union > 0 else 0.0

        return 0.5 + 0.5 * jaccard


class AgentSelector:
    """Selects the best agent for a task."""

    def __init__(
        self,
        capability_weight: float = 0.4,
        priority_weight: float = 0.3,
        load_weight: float = 0.3
    ):
        self.capability_weight = capability_weight
        self.priority_weight = priority_weight
        self.load_weight = load_weight

        self.capability_matcher = CapabilityMatcher(capability_weight)
        self.priority_router = PriorityRouter(priority_weight)
        self.load_balancer = LoadBalancer(load_weight)
        self.affinity_router = AffinityRouter()

    def select_agent(
        self,
        requirements: TaskRequirements,
        available_agents: List[AgentInfo]
    ) -> Optional[AgentInfo]:
        """Select the best agent for the given requirements."""
        if not available_agents:
            logger.warning("No agents available for selection")
            return None

        candidate_agents = [
            a for a in available_agents
            if a.is_available
            and a.available_slots > 0
            and a.agent_id not in requirements.excluded_agents
        ]

        if not candidate_agents:
            logger.warning("No suitable agents found after filtering")
            return None

        if requirements.preferred_agent:
            preferred = next(
                (a for a in candidate_agents if a.agent_id == requirements.preferred_agent),
                None
            )
            if preferred:
                return preferred

        for cap_name in requirements.required_capabilities:
            cap_candidates = [
                a for a in candidate_agents
                if a.has_capability(cap_name, min_level=0.5)
            ]
            if cap_candidates:
                candidate_agents = cap_candidates
                break

        scored_agents = []
        for agent in candidate_agents:
            cap_score = self.capability_matcher.calculate_match_score(requirements, agent)
            prio_score = self.priority_router.calculate_priority_score(requirements, agent)
            load_score = self.load_balancer.calculate_load_score(requirements, agent)
            affinity_score = self.affinity_router.calculate_affinity_score(requirements, agent)

            total_score = (
                cap_score * self.capability_weight +
                prio_score * self.priority_weight +
                load_score * self.load_weight +
                affinity_score * 0.1
            )

            scored_agents.append((agent, total_score, cap_score, prio_score, load_score))

        scored_agents.sort(key=lambda x: x[1], reverse=True)

        best_agent = scored_agents[0][0] if scored_agents else None

        if best_agent:
            logger.info(
                f"Selected agent: {best_agent.name} (score: {scored_agents[0][1]:.3f}, "
                f"cap: {scored_agents[0][2]:.3f}, prio: {scored_agents[0][3]:.3f}, "
                f"load: {scored_agents[0][4]:.3f})"
            )

        return best_agent


class DispatchingStrategy:
    """Main dispatching strategy orchestrator."""

    def __init__(self, manager: Optional[Dict[str, Any]] = None):
        """Initialize the dispatching strategy."""
        manager = manager or {}

        self.capability_weight = manager.get("capability_weight", 0.4)
        self.priority_weight = manager.get("priority_weight", 0.3)
        self.load_weight = manager.get("load_weight", 0.3)
        self.max_retry_attempts = manager.get("max_retry_attempts", 3)
        self.timeout_seconds = manager.get("timeout_seconds", 300)

        self.agent_selector = AgentSelector(
            capability_weight=self.capability_weight,
            priority_weight=self.priority_weight,
            load_weight=self.load_weight
        )

        self.task_analyzer = TaskAnalyzer()

        self.registered_agents: Dict[str, AgentInfo] = {}

        logger.info("DispatchingStrategy initialized")

    def register_agent(self, agent: AgentInfo) -> bool:
        """Register an agent with the dispatcher."""
        if agent.agent_id in self.registered_agents:
            logger.warning(f"Agent {agent.agent_id} already registered, updating")
            return False

        self.registered_agents[agent.agent_id] = agent
        logger.info(f"Agent registered: {agent.name} ({agent.agent_id})")
        return True

    def unregister_agent(self, agent_id: str) -> bool:
        """Unregister an agent from the dispatcher."""
        if agent_id in self.registered_agents:
            del self.registered_agents[agent_id]
            logger.info(f"Agent unregistered: {agent_id}")
            return True
        return False

    def get_available_agents(self) -> List[AgentInfo]:
        """Get list of currently available agents."""
        return [
            agent for agent in self.registered_agents.values()
            if agent.is_available and agent.available_slots > 0
        ]

    def select_agent(self, requirements: TaskRequirements) -> Optional[AgentInfo]:
        """Select the best agent for the given requirements."""
        available = self.get_available_agents()
        return self.agent_selector.select_agent(requirements, available)

    def dispatch(
        self,
        task_data: Dict[str, Any],
        selected_agent: Optional[AgentInfo] = None
    ) -> DispatchResult:
        """Dispatch a task to an appropriate agent."""
        start_time = time.time()

        task_id = task_data.get("id") or str(uuid.uuid4())

        requirements = self.task_analyzer.analyze(task_data)

        if selected_agent is None:
            selected_agent = self.select_agent(requirements)

        if selected_agent is None:
            return DispatchResult(
                success=False,
                task_id=task_id,
                error="No suitable agent available",
                dispatch_time=time.time() - start_time
            )

        alternative_agents = []
        available = self.get_available_agents()
        for agent in available:
            if agent.agent_id != selected_agent.agent_id:
                if all(agent.has_capability(cap) for cap in requirements.required_capabilities):
                    alternative_agents.append(agent.agent_id)

        result = DispatchResult(
            success=True,
            task_id=task_id,
            agent_id=selected_agent.agent_id,
            agent_name=selected_agent.name,
            alternative_agents=alternative_agents[:3],
            dispatch_time=time.time() - start_time
        )

        logger.info(
            f"Task {task_id} dispatched to {selected_agent.name} "
            f"(alternatives: {alternative_agents})"
        )

        return result

    def dispatch_with_retry(
        self,
        task_data: Dict[str, Any]
    ) -> DispatchResult:
        """Dispatch a task with automatic retry on failure."""
        last_error = None
        retry_count = 0

        while retry_count < self.max_retry_attempts:
            result = self.dispatch(task_data)

            if result.success:
                result.retry_count = retry_count
                return result

            last_error = result.error
            retry_count += 1

            if retry_count < self.max_retry_attempts:
                logger.warning(
                    f"Dispatch attempt {retry_count} failed for task {result.task_id}, "
                    f"retrying..."
                )
                time.sleep(min(2 ** retry_count, 30))

        return DispatchResult(
            success=False,
            task_id=task_data.get("id"),
            error=f"Failed after {self.max_retry_attempts} attempts: {last_error}",
            retry_count=retry_count,
            dispatch_time=0
        )

    def update_agent_metrics(
        self,
        agent_id: str,
        success: bool,
        execution_time: float
    ) -> bool:
        """Update metrics for an agent after task completion."""
        if agent_id not in self.registered_agents:
            return False

        agent = self.registered_agents[agent_id]
        metrics = agent.metrics

        metrics.total_tasks += 1
        if success:
            metrics.successful_tasks += 1
        else:
            metrics.failed_tasks += 1

        if metrics.total_tasks > 0:
            metrics.avg_execution_time = (
                (metrics.avg_execution_time * (metrics.total_tasks - 1) + execution_time)
                / metrics.total_tasks
            )

        metrics.current_load = max(0, metrics.current_load - 1)
        metrics.last_updated = datetime.utcnow()

        logger.debug(
            f"Updated metrics for {agent.name}: "
            f"total={metrics.total_tasks}, success_rate={metrics.success_rate:.2f}"
        )

        return True

    def increment_agent_load(self, agent_id: str) -> bool:
        """Increment the current task load for an agent."""
        if agent_id not in self.registered_agents:
            return False

        agent = self.registered_agents[agent_id]
        agent.current_tasks += 1
        agent.metrics.current_load = agent.current_tasks
        return True

    def decrement_agent_load(self, agent_id: str) -> bool:
        """Decrement the current task load for an agent."""
        if agent_id not in self.registered_agents:
            return False

        agent = self.registered_agents[agent_id]
        agent.current_tasks = max(0, agent.current_tasks - 1)
        agent.metrics.current_load = agent.current_tasks
        return True

    def set_agent_availability(self, agent_id: str, available: bool) -> bool:
        """Set agent availability status."""
        if agent_id not in self.registered_agents:
            return False

        self.registered_agents[agent_id].is_available = available
        logger.info(f"Agent {agent_id} availability set to {available}")
        return True

    def get_dispatch_stats(self) -> Dict[str, Any]:
        """Get dispatcher statistics."""
        total_agents = len(self.registered_agents)
        available_agents = len(self.get_available_agents())

        total_tasks = sum(a.metrics.total_tasks for a in self.registered_agents.values())
        total_success = sum(a.metrics.successful_tasks for a in self.registered_agents.values())
        total_failed = sum(a.metrics.failed_tasks for a in self.registered_agents.values())

        return {
            "total_agents": total_agents,
            "available_agents": available_agents,
            "total_tasks": total_tasks,
            "successful_tasks": total_success,
            "failed_tasks": total_failed,
            "overall_success_rate": total_success / total_tasks if total_tasks > 0 else 0.0
        }
