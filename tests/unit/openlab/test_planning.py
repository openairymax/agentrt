# Copyright (c) 2026 SPHARX. All Rights Reserved.
# "From data intelligence emerges."

"""
Unit Tests for Planning Strategies
==================================
"""

import pytest
from typing import Dict, List, Set
import sys
import os

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))


class TestTaskNode:
    """Tests for TaskNode dataclass."""

    def test_task_node_creation(self):
        """Test TaskNode can be created."""
        from openlab.contrib.strategies.planning import TaskNode

        node = TaskNode(
            id="task-001",
            name="Design Database",
            description="Design the database schema"
        )
        assert node.id == "task-001"
        assert node.name == "Design Database"
        assert node.status == "pending"
        assert node.priority == 50

    def test_task_node_with_dependencies(self):
        """Test TaskNode with dependencies."""
        from openlab.contrib.strategies.planning import TaskNode

        node = TaskNode(
            id="task-002",
            name="Implement API",
            dependencies={"task-001"}  # Depends on Design Database
        )
        assert "task-001" in node.dependencies
        assert len(node.dependencies) == 1

    def test_task_node_hash(self):
        """Test TaskNode can be hashed."""
        from openlab.contrib.strategies.planning import TaskNode

        node1 = TaskNode(id="task-001", name="Task 1")
        node2 = TaskNode(id="task-001", name="Task 1")

        assert hash(node1) == hash(node2)

    def test_task_node_equality(self):
        """Test TaskNode equality comparison."""
        from openlab.contrib.strategies.planning import TaskNode

        node1 = TaskNode(id="task-001", name="Task 1")
        node2 = TaskNode(id="task-001", name="Different Name")
        node3 = TaskNode(id="task-002", name="Task 1")

        assert node1 == node2  # Same ID
        assert node1 != node3  # Different ID


class TestTaskDAG:
    """Tests for TaskDAG class."""

    def test_dag_creation(self):
        """Test TaskDAG can be created."""
        from openlab.contrib.strategies.planning import TaskDAG

        dag = TaskDAG(root_goal="Build a web app")
        assert dag.root_goal == "Build a web app"
        assert len(dag.nodes) == 0
        assert len(dag.edges) == 0

    def test_add_node(self):
        """Test adding nodes to DAG."""
        from openlab.contrib.strategies.planning import TaskDAG, TaskNode

        dag = TaskDAG(root_goal="Build a web app")
        node = TaskNode(id="task-001", name="Design")
        dag.add_node(node)

        assert "task-001" in dag.nodes
        assert len(dag.edges) == 0  # No dependencies

    def test_add_node_with_dependencies(self):
        """Test adding node with dependencies."""
        from openlab.contrib.strategies.planning import TaskDAG, TaskNode

        dag = TaskDAG(root_goal="Build a web app")

        # Add parent node first
        parent = TaskNode(id="task-001", name="Design")
        dag.add_node(parent)

        # Add child node with dependency
        child = TaskNode(id="task-002", name="Implement", dependencies={"task-001"})
        dag.add_node(child)

        assert len(dag.edges) == 1
        assert ("task-001", "task-002") in dag.edges

    def test_get_execution_order(self):
        """Test topological sort for execution order."""
        from openlab.contrib.strategies.planning import TaskDAG, TaskNode

        dag = TaskDAG(root_goal="Build a web app")

        # Create dependency chain: design -> implement -> test
        design = TaskNode(id="design", name="Design")
        implement = TaskNode(id="implement", name="Implement", dependencies={"design"})
        test = TaskNode(id="test", name="Test", dependencies={"implement"})

        dag.add_node(design)
        dag.add_node(implement)
        dag.add_node(test)

        layers = dag.get_execution_order()

        assert len(layers) == 3
        assert layers[0][0].id == "design"
        assert layers[1][0].id == "implement"
        assert layers[2][0].id == "test"

    def test_get_ready_tasks(self):
        """Test getting ready tasks."""
        from openlab.contrib.strategies.planning import TaskDAG, TaskNode

        dag = TaskDAG(root_goal="Build a web app")

        design = TaskNode(id="design", name="Design")
        implement = TaskNode(id="implement", name="Implement", dependencies={"design"})

        dag.add_node(design)
        dag.add_node(implement)

        # Initially only design is ready
        ready = dag.get_ready_tasks(set())
        assert len(ready) == 1
        assert ready[0].id == "design"

        # After design is done, implement is ready
        ready = dag.get_ready_tasks({"design"})
        assert len(ready) == 1
        assert ready[0].id == "implement"

    def test_validate_dag_no_cycle(self):
        """Test DAG validation with no cycles."""
        from openlab.contrib.strategies.planning import TaskDAG, TaskNode

        dag = TaskDAG(root_goal="Build a web app")
        node1 = TaskNode(id="task-001", name="Task 1")
        node2 = TaskNode(id="task-002", name="Task 2", dependencies={"task-001"})
        dag.add_node(node1)
        dag.add_node(node2)

        is_valid, errors = dag.validate()
        assert is_valid is True
        assert len(errors) == 0


class TestPlanningContext:
    """Tests for PlanningContext dataclass."""

    def test_context_creation(self):
        """Test PlanningContext can be created."""
        from openlab.contrib.strategies.planning import PlanningContext

        context = PlanningContext(
            goal="Build a web app",
            max_depth=5,
            timeout=60.0
        )
        assert context.goal == "Build a web app"
        assert context.max_depth == 5
        assert context.timeout == 60.0

    def test_context_with_constraints(self):
        """Test PlanningContext with constraints."""
        from openlab.contrib.strategies.planning import PlanningContext

        context = PlanningContext(
            goal="Build a web app",
            constraints={"budget": "medium", "timeline": 12}
        )
        assert context.constraints["budget"] == "medium"
        assert context.constraints["timeline"] == 12


class TestHierarchicalPlanner:
    """Tests for HierarchicalPlanner class."""

    @pytest.fixture
    def planner(self):
        """Create a test planner."""
        from openlab.contrib.strategies.planning import HierarchicalPlanner
        return HierarchicalPlanner(max_depth=3)

    def test_planner_creation(self, planner):
        """Test planner can be created."""
        assert planner.max_depth == 3
        assert planner.name == "HierarchicalPlanner"

    def test_simple_planning(self, planner):
        """Test simple goal planning."""
        from openlab.contrib.strategies.planning import PlanningContext

        context = PlanningContext(
            goal="Build a simple web app",
            max_depth=2
        )

        dag = planner.plan("Build a simple web app", context)

        assert dag is not None
        assert dag.root_goal == "Build a simple web app"
        assert len(dag.nodes) > 0

    def test_plan_returns_dag(self, planner):
        """Test plan() returns TaskDAG."""
        from openlab.contrib.strategies.planning import TaskDAG

        dag = planner.plan("Test goal")

        assert isinstance(dag, TaskDAG)

    def test_planner_counts_plans(self, planner):
        """Test planner tracks number of plans."""
        initial_count = planner._plan_count

        planner.plan("Goal 1")
        assert planner._plan_count == initial_count + 1

        planner.plan("Goal 2")
        assert planner._plan_count == initial_count + 2


class TestReactivePlanner:
    """Tests for ReactivePlanner class."""

    @pytest.fixture
    def planner(self):
        """Create a test planner."""
        from openlab.contrib.strategies.planning import ReactivePlanner
        return ReactivePlanner()

    def test_planner_creation(self, planner):
        """Test planner can be created."""
        assert planner.name == "ReactivePlanner"

    def test_reactive_planning(self, planner):
        """Test reactive goal planning."""
        dag = planner.plan("Handle user request")

        assert dag is not None
        assert isinstance(dag.root_goal, str)


class TestReflectivePlanner:
    """Tests for ReflectivePlanner class."""

    @pytest.fixture
    def planner(self):
        """Create a test planner."""
        from openlab.contrib.strategies.planning import ReflectivePlanner
        return ReflectivePlanner()

    def test_planner_creation(self, planner):
        """Test planner can be created."""
        assert planner.name == "ReflectivePlanner"

    def test_reflective_planning(self, planner):
        """Test reflective goal planning."""
        dag = planner.plan("Complex system design")

        assert dag is not None


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
