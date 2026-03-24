# Copyright (c) 2026 SPHARX. All Rights Reserved.
# "From data intelligence emerges."

"""
Unit Tests for Planning Strategy
=============================
"""

import pytest
import sys
import os

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))


class TestPlanningStrategy:
    """Tests for planning strategy."""

    def test_strategy_initialization(self):
        """Test strategy can be initialized."""
        from contrib.strategies.planning import PlanningStrategy

        strategy = PlanningStrategy()
        assert strategy is not None
        assert strategy.max_subtasks == 20
        assert strategy.complexity_threshold == 7.0

    def test_create_simple_plan(self):
        """Test creating a simple execution plan."""
        from contrib.strategies.planning import PlanningStrategy, Task, TaskCategory

        strategy = PlanningStrategy()
        task = Task(
            id="plan-test-001",
            description="Test task for planning",
            complexity=5.0
        )

        plan = strategy.create_plan(task)

        assert plan is not None
        assert plan.task_id == "plan-test-001"
        assert len(plan.subtasks) > 0

    def test_create_plan_with_category(self):
        """Test creating a plan with specific category."""
        from contrib.strategies.planning import PlanningStrategy, Task, TaskCategory

        strategy = PlanningStrategy()
        task = Task(
            id="plan-test-002",
            description="Implementation task",
            category=TaskCategory.IMPLEMENTATION,
            complexity=8.0
        )

        plan = strategy.create_plan(task)

        assert plan is not None
        assert len(plan.subtasks) > 0
        assert plan.estimated_complexity > 0

    def test_get_next_executable(self):
        """Test getting next executable subtasks."""
        from contrib.strategies.planning import PlanningStrategy, Task

        strategy = PlanningStrategy()
        task = Task(id="exec-test-001", description="Execution test", complexity=5.0)

        plan = strategy.create_plan(task)
        next_tasks = strategy.get_next_executable(plan)

        assert isinstance(next_tasks, list)


class TestTaskDecomposition:
    """Tests for task decomposition."""

    def test_decompose_simple_task(self):
        """Test decomposing a simple task."""
        from contrib.strategies.planning import TaskDecomposer, Task, TaskCategory

        decomposer = TaskDecomposer(max_subtasks=20)
        task = Task(
            id="decomp-test-001",
            description="Test decomposition",
            complexity=5.0
        )

        subtasks = decomposer.decompose(task)

        assert len(subtasks) > 0
        assert len(subtasks) <= 20

    def test_decompose_complex_task(self):
        """Test decomposing a complex task."""
        from contrib.strategies.planning import TaskDecomposer, Task, TaskCategory

        decomposer = TaskDecomposer(max_subtasks=20)
        task = Task(
            id="decomp-test-002",
            description="Complex implementation",
            category=TaskCategory.IMPLEMENTATION,
            complexity=10.0
        )

        subtasks = decomposer.decompose(task)

        assert len(subtasks) > 0
        for subtask in subtasks:
            assert subtask.category == TaskCategory.IMPLEMENTATION


class TestDependencyAnalysis:
    """Tests for dependency analysis."""

    def test_analyze_dependencies(self):
        """Test analyzing dependencies between subtasks."""
        from contrib.strategies.planning import DependencyAnalyzer, SubTask, TaskCategory

        analyzer = DependencyAnalyzer()

        subtask1 = SubTask(
            id="dep-001",
            description="First task",
            category=TaskCategory.RESEARCH
        )

        subtask2 = SubTask(
            id="dep-002",
            description="Second task",
            category=TaskCategory.DESIGN,
            dependencies=["dep-001"]
        )

        subtasks = analyzer.analyze([subtask1, subtask2])

        assert len(subtasks) == 2

    def test_get_execution_order(self):
        """Test getting execution order."""
        from contrib.strategies.planning import DependencyAnalyzer, SubTask, TaskCategory

        analyzer = DependencyAnalyzer()

        subtask1 = SubTask(id="order-001", description="First", category=TaskCategory.RESEARCH)
        subtask2 = SubTask(
            id="order-002",
            description="Second",
            category=TaskCategory.DESIGN,
            dependencies=["order-001"]
        )

        subtasks = analyzer.analyze([subtask1, subtask2])
        execution_order = analyzer.get_execution_order(subtasks)

        assert len(execution_order) > 0
        assert execution_order[0][0].id == "order-001"


class TestRiskAssessment:
    """Tests for risk assessment."""

    def test_assess_low_risk_subtask(self):
        """Test assessing low risk subtask."""
        from contrib.strategies.planning import RiskAssessor, SubTask, TaskCategory, RiskLevel

        assessor = RiskAssessor()
        subtask = SubTask(
            id="risk-001",
            description="Low risk task",
            category=TaskCategory.DOCUMENTATION,
            estimated_complexity=3.0
        )

        risk_level, factors = assessor.assess_subtask(subtask)

        assert risk_level in [RiskLevel.LOW, RiskLevel.MEDIUM]

    def test_assess_high_risk_subtask(self):
        """Test assessing high risk subtask."""
        from contrib.strategies.planning import RiskAssessor, SubTask, TaskCategory, RiskLevel

        assessor = RiskAssessor()
        subtask = SubTask(
            id="risk-002",
            description="High risk task",
            category=TaskCategory.IMPLEMENTATION,
            estimated_complexity=9.0,
            estimated_duration=300.0,
            dependencies=["dep-001", "dep-002", "dep-003", "dep-004"]
        )

        risk_level, factors = assessor.assess_subtask(subtask)

        assert risk_level in [RiskLevel.HIGH, RiskLevel.CRITICAL]


class TestMilestoneBuilder:
    """Tests for milestone building."""

    def test_build_milestones(self):
        """Test building milestones from subtasks."""
        from contrib.strategies.planning import MilestoneBuilder, SubTask, TaskCategory

        builder = MilestoneBuilder()

        subtasks = [
            SubTask(id="ms-001", description="Research task", category=TaskCategory.RESEARCH),
            SubTask(id="ms-002", description="Design task", category=TaskCategory.DESIGN),
            SubTask(id="ms-003", description="Implementation task", category=TaskCategory.IMPLEMENTATION),
        ]

        milestones = builder.build(subtasks)

        assert len(milestones) > 0
        milestone_names = [m.name for m in milestones]
        assert "Research Phase" in milestone_names


class TestPlanValidation:
    """Tests for plan validation."""

    def test_validate_valid_plan(self):
        """Test validating a valid plan."""
        from contrib.strategies.planning import PlanningStrategy, Task, TaskStatus

        strategy = PlanningStrategy()
        task = Task(id="val-test-001", description="Validation test", complexity=5.0)

        plan = strategy.create_plan(task)
        is_valid, errors = strategy.validate_plan(plan)

        assert is_valid is True
        assert len(errors) == 0

    def test_validate_empty_plan(self):
        """Test validating an empty plan."""
        from contrib.strategies.planning import PlanningStrategy, ExecutionPlan

        strategy = PlanningStrategy()
        plan = ExecutionPlan(id="empty-plan", task_id="empty-task", description="Empty plan")

        is_valid, errors = strategy.validate_plan(plan)

        assert is_valid is False
        assert len(errors) > 0


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
