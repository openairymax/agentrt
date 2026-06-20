"""Planning strategy module — contrib skeleton."""

from typing import Any, Dict, List, Optional, Set
from dataclasses import dataclass, field
from collections import deque


@dataclass
class PlanStep:
    step_id: str = ""
    description: str = ""
    dependencies: List[str] = field(default_factory=list)
    assigned_agent: Optional[str] = None


@dataclass
class TaskNode:
    """A node in the task dependency graph."""
    id: str
    name: str = ""
    description: str = ""
    status: str = "pending"
    priority: int = 50
    dependencies: Set[str] = field(default_factory=set)

    def __hash__(self):
        return hash(self.id)

    def __eq__(self, other):
        if not isinstance(other, TaskNode):
            return NotImplemented
        return self.id == other.id


class TaskDAG:
    """Directed Acyclic Graph for task dependencies."""

    def __init__(self, root_goal: str = ""):
        self.root_goal = root_goal
        self.nodes: Dict[str, TaskNode] = {}
        self.edges: Set[tuple] = set()

    def add_node(self, node: TaskNode) -> None:
        self.nodes[node.id] = node
        for dep_id in node.dependencies:
            self.edges.add((dep_id, node.id))

    def get_execution_order(self) -> List[List[TaskNode]]:
        """Topological sort returning layers of parallel-executable tasks."""
        in_degree = {node_id: 0 for node_id in self.nodes}
        adj = {node_id: [] for node_id in self.nodes}

        for parent, child in self.edges:
            if parent in adj and child in in_degree:
                adj[parent].append(child)
                in_degree[child] += 1

        queue = deque([nid for nid, deg in in_degree.items() if deg == 0])
        layers = []

        while queue:
            layer = []
            for _ in range(len(queue)):
                nid = queue.popleft()
                layer.append(self.nodes[nid])
                for neighbor in adj.get(nid, []):
                    in_degree[neighbor] -= 1
                    if in_degree[neighbor] == 0:
                        queue.append(neighbor)
            layers.append(layer)

        return layers

    def get_ready_tasks(self, completed: Set[str]) -> List[TaskNode]:
        """Get tasks whose dependencies are all completed."""
        ready = []
        for node in self.nodes.values():
            if node.id in completed:
                continue
            if node.dependencies.issubset(completed):
                ready.append(node)
        return ready

    def validate(self) -> tuple:
        """Validate the DAG has no cycles."""
        try:
            self.get_execution_order()
            return True, []
        except Exception as e:
            return False, [str(e)]


@dataclass
class PlanningContext:
    """Context for planning operations."""
    goal: str = ""
    max_depth: int = 5
    timeout: float = 60.0
    constraints: Dict[str, Any] = field(default_factory=dict)


class PlanningStrategy:
    """Planning strategy for task decomposition — contrib skeleton."""

    def __init__(self, config: Optional[Dict[str, Any]] = None):
        self.config = config or {}

    async def plan(self, task: Any) -> Dict[str, Any]:
        if isinstance(task, dict):
            task_desc = task.get("description", str(task))
        else:
            task_desc = str(task)
        return {
            "status": "completed",
            "strategy": "planning",
            "task": task_desc,
            "steps": [],
        }


class HierarchicalPlanner(PlanningStrategy):
    """Hierarchical task decomposition planner."""

    def __init__(self, max_depth: int = 3, config: Optional[Dict[str, Any]] = None):
        super().__init__(config)
        self.max_depth = max_depth
        self.name = "HierarchicalPlanner"
        self._plan_count = 0

    def plan(self, goal: str, context: Optional[PlanningContext] = None) -> TaskDAG:
        """Decompose a goal into a TaskDAG."""
        self._plan_count += 1
        dag = TaskDAG(root_goal=goal)

        # Generate hierarchical decomposition steps
        depth = min(self.max_depth, context.max_depth if context else 3)
        for i in range(depth):
            node = TaskNode(
                id=f"step-{i + 1}",
                name=f"Step {i + 1}",
                description=f"Decomposition step {i + 1} for: {goal}",
            )
            if i > 0:
                node.dependencies = {f"step-{i}"}
            dag.add_node(node)

        return dag


class ReactivePlanner(PlanningStrategy):
    """Reactive planning strategy."""

    def __init__(self, config: Optional[Dict[str, Any]] = None):
        super().__init__(config)
        self.name = "ReactivePlanner"
        self._plan_count = 0

    def plan(self, goal: str) -> TaskDAG:
        """Create a reactive plan for the given goal."""
        self._plan_count += 1
        dag = TaskDAG(root_goal=goal)
        node = TaskNode(
            id="reactive-1",
            name="Reactive Response",
            description=f"Reactive plan for: {goal}",
        )
        dag.add_node(node)
        return dag


class ReflectivePlanner(PlanningStrategy):
    """Reflective planning strategy with self-evaluation."""

    def __init__(self, config: Optional[Dict[str, Any]] = None):
        super().__init__(config)
        self.name = "ReflectivePlanner"
        self._plan_count = 0

    def plan(self, goal: str) -> TaskDAG:
        """Create a reflective plan for the given goal."""
        self._plan_count += 1
        dag = TaskDAG(root_goal=goal)
        node = TaskNode(
            id="reflect-1",
            name="Reflective Analysis",
            description=f"Reflective plan for: {goal}",
        )
        dag.add_node(node)
        return dag


__all__ = [
    "PlanningStrategy",
    "PlanStep",
    "TaskNode",
    "TaskDAG",
    "PlanningContext",
    "HierarchicalPlanner",
    "ReactivePlanner",
    "ReflectivePlanner",
]