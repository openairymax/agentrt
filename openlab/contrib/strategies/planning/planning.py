"""
Copyright (c) 2026 SPHARX. All Rights Reserved.
"From data intelligence emerges."

openlab Contrib - Planning Strategies
=====================================

Task planning strategies for intelligent task decomposition.

Available Strategies:
    - HierarchicalPlanner: Top-down task decomposition
    - ReactivePlanner: Real-time environment response
    - ReflectivePlanner: Self-adjusting after failures

Example:
    from openlab.contrib.strategies.planning import (
        HierarchicalPlanner,
        ReactivePlanner,
    )

    planner = HierarchicalPlanner(max_depth=5)
    dag = planner.plan("Build a web app", requirements)

Author: AgentOS Architecture Committee
Version: 1.0.0.6
"""

from __future__ import annotations

import logging
from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from enum import Enum, auto
from typing import (
    Any,
    Dict,
    List,
    Optional,
    Set,
    Tuple,
    Callable,
)

logger = logging.getLogger(__name__)


class PlannerType(Enum):
    """Available planner types."""
    HIERARCHICAL = auto()
    REACTIVE = auto()
    REFLECTIVE = auto()
    INCREMENTAL = auto()


@dataclass
class TaskNode:
    """Node in a task DAG.

    Attributes:
        id: Unique node identifier.
        name: Task name.
        description: Task description.
        dependencies: Set of dependency node IDs.
        status: Current task status.
        priority: Task priority (0-100).
        estimated_duration: Estimated duration in seconds.
        required_capabilities: Required agent capabilities.
        metadata: Additional metadata.
    """
    id: str
    name: str
    description: str = ""
    dependencies: Set[str] = field(default_factory=set)
    status: str = "pending"
    priority: int = 50
    estimated_duration: float = 0.0
    required_capabilities: List[str] = field(default_factory=list)
    metadata: Dict[str, Any] = field(default_factory=dict)

    def __hash__(self) -> int:
        return hash(self.id)

    def __eq__(self, other: object) -> bool:
        if isinstance(other, TaskNode):
            return self.id == other.id
        return False


@dataclass
class TaskDAG:
    """Directed Acyclic Graph for task planning.

    Attributes:
        root_goal: Original goal description.
        nodes: Dictionary of task nodes.
        edges: List of dependency edges (from_id, to_id).
        metadata: Additional DAG metadata.
    """
    root_goal: str
    nodes: Dict[str, TaskNode] = field(default_factory=dict)
    edges: List[Tuple[str, str]] = field(default_factory=list)
    metadata: Dict[str, Any] = field(default_factory=dict)

    def add_node(self, node: TaskNode) -> None:
        """Add a node to the DAG.

        Args:
            node: Task node to add.
        """
        self.nodes[node.id] = node
        for dep_id in node.dependencies:
            self.edges.append((dep_id, node.id))

    def get_execution_order(self) -> List[List[TaskNode]]:
        """Get topological execution order.

        Returns:
            List of execution layers (parallelizable tasks).
        """
        in_degree: Dict[str, int] = {nid: 0 for nid in self.nodes}
        for from_id, to_id in self.edges:
            in_degree[to_id] += 1

        layers: List[List[TaskNode]] = []
        remaining = set(in_degree.keys())

        while remaining:
            layer = [
                self.nodes[nid]
                for nid in remaining
                if in_degree[nid] == 0
            ]
            if not layer:
                break

            layers.append(layer)

            for node in layer:
                remaining.remove(node.id)
                for from_id, to_id in self.edges:
                    if from_id == node.id:
                        in_degree[to_id] -= 1

        return layers

    def get_ready_tasks(self, completed: Set[str]) -> List[TaskNode]:
        """Get tasks ready for execution.

        Args:
            completed: Set of completed task IDs.

        Returns:
            List of ready tasks.
        """
        ready = []
        for node in self.nodes.values():
            if node.id in completed:
                continue
            if node.status == "completed":
                continue
            if node.dependencies <= completed:
                ready.append(node)
        return ready

    def validate(self) -> Tuple[bool, List[str]]:
        """Validate DAG for cycles and orphan nodes.

        Returns:
            Tuple of (is_valid, list of errors).
        """
        errors = []

        visited = set()
        rec_stack = set()

        def has_cycle(node_id: str) -> bool:
            visited.add(node_id)
            rec_stack.add(node_id)

            for from_id, to_id in self.edges:
                if from_id == node_id:
                    if to_id not in visited:
                        if has_cycle(to_id):
                            return True
                    elif to_id in rec_stack:
                        return True

            rec_stack.remove(node_id)
            return False

        for node_id in self.nodes:
            if node_id not in visited:
                if has_cycle(node_id):
                    errors.append("Cycle detected in DAG")
                    break

        for from_id, to_id in self.edges:
            if from_id not in self.nodes:
                errors.append(f"Edge references missing node: {from_id}")
            if to_id not in self.nodes:
                errors.append(f"Edge references missing node: {to_id}")

        return len(errors) == 0, errors


@dataclass
class PlanningContext:
    """Context for planning operations.

    Attributes:
        goal: Planning goal description.
        constraints: Planning constraints.
        available_agents: Available agent capabilities.
        max_depth: Maximum decomposition depth.
        timeout: Planning timeout in seconds.
        metadata: Additional context data.
    """
    goal: str
    constraints: Dict[str, Any] = field(default_factory=dict)
    available_agents: List[str] = field(default_factory=list)
    max_depth: int = 5
    timeout: float = 60.0
    metadata: Dict[str, Any] = field(default_factory=dict)


class PlanningStrategy(ABC):
    """Abstract base class for planning strategies.

    All planning strategies must implement the plan() method
    to generate a task DAG from a goal.
    """

    def __init__(self, name: str = "base"):
        """Initialize planning strategy.

        Args:
            name: Strategy name.
        """
        self.name = name
        self._plan_count = 0

    @abstractmethod
    def plan(
        self,
        goal: str,
        context: Optional[PlanningContext] = None,
    ) -> TaskDAG:
        """Generate task DAG from goal.

        Args:
            goal: Goal description.
            context: Planning context.

        Returns:
            Generated task DAG.
        """
        pass

    def _record_planning(self, goal: str, dag: TaskDAG) -> None:
        """Record planning operation for metrics.

        Args:
            goal: Goal description.
            dag: Generated DAG.
        """
        self._plan_count += 1
        logger.info(
            f"Planning completed: {dag.root_goal}",
            extra={
                "strategy": self.name,
                "node_count": len(dag.nodes),
                "edge_count": len(dag.edges),
            },
        )


class HierarchicalPlanner(PlanningStrategy):
    """Hierarchical top-down task planner.

    Decomposes complex goals into subtasks recursively,
    creating a hierarchical task DAG.

    Example:
        planner = HierarchicalPlanner(max_depth=5)
        dag = planner.plan("Build a web application")
    """

    def __init__(
        self,
        max_depth: int = 5,
        min_granularity: int = 3,
        name: str = "hierarchical",
    ):
        """Initialize hierarchical planner.

        Args:
            max_depth: Maximum decomposition depth.
            min_granularity: Minimum tasks per level.
            name: Strategy name.
        """
        super().__init__(name)
        self.max_depth = max_depth
        self.min_granularity = min_granularity

    def plan(
        self,
        goal: str,
        context: Optional[PlanningContext] = None,
    ) -> TaskDAG:
        """Generate hierarchical task DAG.

        Args:
            goal: Goal description.
            context: Planning context.

        Returns:
            Generated task DAG.
        """
        dag = TaskDAG(root_goal=goal)
        root_node = TaskNode(
            id="root",
            name=goal,
            description=f"Root task: {goal}",
            priority=100,
        )
        dag.add_node(root_node)

        self._decompose_recursive(
            dag=dag,
            parent_id="root",
            goal=goal,
            depth=0,
            context=context,
        )

        self._record_planning(goal, dag)
        return dag

    def _decompose_recursive(
        self,
        dag: TaskDAG,
        parent_id: str,
        goal: str,
        depth: int,
        context: Optional[PlanningContext],
    ) -> None:
        """Recursively decompose task.

        Args:
            dag: Task DAG being built.
            parent_id: Parent task ID.
            goal: Current goal.
            depth: Current depth.
            context: Planning context.
        """
        max_depth = context.max_depth if context else self.max_depth
        if depth >= max_depth:
            return

        subtasks = self._generate_subtasks(goal, context)
        if not subtasks:
            return

        for i, subtask in enumerate(subtasks):
            node_id = f"{parent_id}.{i}"
            node = TaskNode(
                id=node_id,
                name=subtask["name"],
                description=subtask.get("description", ""),
                dependencies={parent_id},
                priority=subtask.get("priority", 50),
                estimated_duration=subtask.get("duration", 0.0),
                required_capabilities=subtask.get("capabilities", []),
            )
            dag.add_node(node)

            self._decompose_recursive(
                dag=dag,
                parent_id=node_id,
                goal=subtask["name"],
                depth=depth + 1,
                context=context,
            )

    def _generate_subtasks(
        self,
        goal: str,
        context: Optional[PlanningContext],
    ) -> List[Dict[str, Any]]:
        """Generate subtasks for a goal.

        Args:
            goal: Goal description.
            context: Planning context.

        Returns:
            List of subtask definitions.
        """
        keywords = {
            "build": ["design", "implement", "test", "deploy"],
            "create": ["plan", "develop", "verify", "release"],
            "analyze": ["collect", "process", "interpret", "report"],
            "develop": ["design", "code", "review", "integrate"],
        }

        goal_lower = goal.lower()
        for key, phases in keywords.items():
            if key in goal_lower:
                return [
                    {
                        "name": f"{phase.capitalize()} phase for: {goal}",
                        "description": f"Execute {phase} phase",
                        "priority": 100 - i * 10,
                        "duration": 60.0,
                        "capabilities": [phase],
                    }
                    for i, phase in enumerate(phases)
                ]

        return [
            {
                "name": f"Step {i+1}: {goal}",
                "description": f"Execute step {i+1}",
                "priority": 50,
                "duration": 30.0,
            }
            for i in range(self.min_granularity)
        ]


class ReactivePlanner(PlanningStrategy):
    """Reactive task planner for real-time adaptation.

    Adjusts plans dynamically based on environment changes
    and execution feedback.

    Example:
        planner = ReactivePlanner()
        dag = planner.plan("Process user requests", context)
    """

    def __init__(
        self,
        adaptation_threshold: float = 0.5,
        name: str = "reactive",
    ):
        """Initialize reactive planner.

        Args:
            adaptation_threshold: Threshold for plan adaptation.
            name: Strategy name.
        """
        super().__init__(name)
        self.adaptation_threshold = adaptation_threshold
        self._execution_history: List[Dict[str, Any]] = []

    def plan(
        self,
        goal: str,
        context: Optional[PlanningContext] = None,
    ) -> TaskDAG:
        """Generate reactive task DAG.

        Args:
            goal: Goal description.
            context: Planning context.

        Returns:
            Generated task DAG.
        """
        dag = TaskDAG(root_goal=goal)

        initial_tasks = self._create_initial_tasks(goal, context)
        for task in initial_tasks:
            dag.add_node(task)

        self._record_planning(goal, dag)
        return dag

    def adapt(
        self,
        dag: TaskDAG,
        feedback: Dict[str, Any],
    ) -> TaskDAG:
        """Adapt plan based on execution feedback.

        Args:
            dag: Current task DAG.
            feedback: Execution feedback.

        Returns:
            Adapted task DAG.
        """
        if feedback.get("success_rate", 1.0) < self.adaptation_threshold:
            failed_task_id = feedback.get("task_id")
            if failed_task_id and failed_task_id in dag.nodes:
                recovery_tasks = self._create_recovery_tasks(
                    dag.nodes[failed_task_id],
                    feedback,
                )
                for task in recovery_tasks:
                    dag.add_node(task)

        self._execution_history.append(feedback)
        return dag

    def _create_initial_tasks(
        self,
        goal: str,
        context: Optional[PlanningContext],
    ) -> List[TaskNode]:
        """Create initial task set.

        Args:
            goal: Goal description.
            context: Planning context.

        Returns:
            List of initial tasks.
        """
        return [
            TaskNode(
                id="init",
                name=f"Initialize: {goal}",
                description="Initialize resources and context",
                priority=100,
                estimated_duration=10.0,
            ),
            TaskNode(
                id="execute",
                name=f"Execute: {goal}",
                description="Main execution phase",
                dependencies={"init"},
                priority=80,
                estimated_duration=60.0,
            ),
            TaskNode(
                id="verify",
                name=f"Verify: {goal}",
                description="Verify results and quality",
                dependencies={"execute"},
                priority=60,
                estimated_duration=20.0,
            ),
            TaskNode(
                id="finalize",
                name=f"Finalize: {goal}",
                description="Cleanup and report",
                dependencies={"verify"},
                priority=40,
                estimated_duration=10.0,
            ),
        ]

    def _create_recovery_tasks(
        self,
        failed_task: TaskNode,
        feedback: Dict[str, Any],
    ) -> List[TaskNode]:
        """Create recovery tasks for failed task.

        Args:
            failed_task: The failed task.
            feedback: Failure feedback.

        Returns:
            List of recovery tasks.
        """
        recovery_id = f"{failed_task.id}_recovery"
        return [
            TaskNode(
                id=f"{recovery_id}_analyze",
                name=f"Analyze failure: {failed_task.name}",
                description="Analyze failure cause",
                priority=90,
                estimated_duration=15.0,
            ),
            TaskNode(
                id=f"{recovery_id}_retry",
                name=f"Retry: {failed_task.name}",
                description="Retry with adjusted approach",
                dependencies={f"{recovery_id}_analyze"},
                priority=85,
                estimated_duration=failed_task.estimated_duration * 1.5,
            ),
        ]


class ReflectivePlanner(PlanningStrategy):
    """Reflective planner with self-adjustment capability.

    Learns from execution failures and adjusts planning strategy.

    Example:
        planner = ReflectivePlanner()
        dag = planner.plan("Complex task")
        adjusted_dag = planner.reflect_and_adjust(dag, failure_info)
    """

    def __init__(
        self,
        reflection_threshold: int = 3,
        name: str = "reflective",
    ):
        """Initialize reflective planner.

        Args:
            reflection_threshold: Failures before reflection.
            name: Strategy name.
        """
        super().__init__(name)
        self.reflection_threshold = reflection_threshold
        self._failure_patterns: Dict[str, int] = {}
        self._base_planner = HierarchicalPlanner()

    def plan(
        self,
        goal: str,
        context: Optional[PlanningContext] = None,
    ) -> TaskDAG:
        """Generate reflective task DAG.

        Args:
            goal: Goal description.
            context: Planning context.

        Returns:
            Generated task DAG.
        """
        dag = self._base_planner.plan(goal, context)

        self._apply_learned_patterns(dag)

        self._record_planning(goal, dag)
        return dag

    def reflect_and_adjust(
        self,
        dag: TaskDAG,
        failure_info: Dict[str, Any],
    ) -> TaskDAG:
        """Reflect on failure and adjust plan.

        Args:
            dag: Current task DAG.
            failure_info: Information about the failure.

        Returns:
            Adjusted task DAG.
        """
        failure_type = failure_info.get("type", "unknown")
        failed_task_id = failure_info.get("task_id")

        pattern_key = f"{failure_type}:{failed_task_id}"
        self._failure_patterns[pattern_key] = (
            self._failure_patterns.get(pattern_key, 0) + 1
        )

        if self._failure_patterns[pattern_key] >= self.reflection_threshold:
            dag = self._add_preventive_tasks(dag, failure_type)

        if failed_task_id and failed_task_id in dag.nodes:
            alternative_tasks = self._generate_alternatives(
                dag.nodes[failed_task_id],
                failure_info,
            )
            for task in alternative_tasks:
                dag.add_node(task)

        return dag

    def _apply_learned_patterns(self, dag: TaskDAG) -> None:
        """Apply learned failure patterns to DAG.

        Args:
            dag: Task DAG to modify.
        """
        for pattern, count in self._failure_patterns.items():
            if count >= self.reflection_threshold:
                failure_type = pattern.split(":")[0]
                self._add_preventive_tasks(dag, failure_type)

    def _add_preventive_tasks(
        self,
        dag: TaskDAG,
        failure_type: str,
    ) -> TaskDAG:
        """Add preventive tasks based on failure type.

        Args:
            dag: Task DAG.
            failure_type: Type of failure.

        Returns:
            Modified DAG.
        """
        preventive_tasks = {
            "timeout": TaskNode(
                id="prevent_timeout",
                name="Prevent timeout",
                description="Add checkpoint and recovery mechanisms",
                priority=95,
                estimated_duration=5.0,
            ),
            "resource": TaskNode(
                id="prevent_resource",
                name="Check resources",
                description="Verify resource availability",
                priority=95,
                estimated_duration=3.0,
            ),
            "dependency": TaskNode(
                id="prevent_dependency",
                name="Verify dependencies",
                description="Check dependency availability",
                priority=95,
                estimated_duration=3.0,
            ),
        }

        if failure_type in preventive_tasks:
            dag.add_node(preventive_tasks[failure_type])

        return dag

    def _generate_alternatives(
        self,
        failed_task: TaskNode,
        failure_info: Dict[str, Any],
    ) -> List[TaskNode]:
        """Generate alternative approaches for failed task.

        Args:
            failed_task: The failed task.
            failure_info: Failure information.

        Returns:
            List of alternative tasks.
        """
        alt_id = f"{failed_task.id}_alt"
        return [
            TaskNode(
                id=alt_id,
                name=f"Alternative: {failed_task.name}",
                description=f"Alternative approach for {failed_task.name}",
                dependencies=failed_task.dependencies.copy(),
                priority=failed_task.priority,
                estimated_duration=failed_task.estimated_duration * 1.2,
                metadata={"original_task": failed_task.id},
            ),
        ]


__all__ = [
    "PlannerType",
    "TaskNode",
    "TaskDAG",
    "PlanningContext",
    "PlanningStrategy",
    "HierarchicalPlanner",
    "ReactivePlanner",
    "ReflectivePlanner",
]
