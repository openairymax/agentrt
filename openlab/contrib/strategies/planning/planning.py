# Copyright (c) 2026 SPHARX. All Rights Reserved.
# "From data intelligence emerges."

"""
Planning Strategy Module
=====================

This module provides intelligent task decomposition and planning mechanisms for
the openlab platform. It analyzes complex tasks, breaks them into manageable
subtasks, and creates optimal execution plans.

Architecture:
- Task decomposition
- Dependency analysis
- Execution ordering
- Resource estimation
- Risk assessment
- Plan optimization
- Milestone tracking
"""

import logging
import time
import uuid
from dataclasses import dataclass, field
from datetime import datetime, timedelta
from enum import Enum
from typing import Any, Dict, List, Optional, Set, Tuple

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)


class TaskStatus(Enum):
    """Task execution status."""
    PENDING = "pending"
    IN_PROGRESS = "in_progress"
    COMPLETED = "completed"
    FAILED = "failed"
    SKIPPED = "skipped"
    BLOCKED = "blocked"


class RiskLevel(Enum):
    """Risk assessment levels."""
    LOW = "low"
    MEDIUM = "medium"
    HIGH = "high"
    CRITICAL = "critical"


class TaskCategory(Enum):
    """Task categories for decomposition."""
    RESEARCH = "research"
    DESIGN = "design"
    IMPLEMENTATION = "implementation"
    TESTING = "testing"
    DEPLOYMENT = "deployment"
    MAINTENANCE = "maintenance"
    DOCUMENTATION = "documentation"


@dataclass
class Capability:
    """Required capability for a task."""
    name: str
    level: float = 1.0
    optional: bool = False


@dataclass
class Resource:
    """Resource requirement for a task."""
    type: str
    quantity: float = 1.0
    duration: float = 0.0


@dataclass
class SubTask:
    """A decomposed subtask."""
    id: str
    description: str
    category: TaskCategory
    status: TaskStatus = TaskStatus.PENDING
    priority: int = 100
    capabilities: List[Capability] = field(default_factory=list)
    resources: List[Resource] = field(default_factory=list)
    estimated_duration: float = 60.0
    estimated_complexity: float = 5.0
    dependencies: List[str] = field(default_factory=list)
    blocked_by: List[str] = field(default_factory=list)
    risk_level: RiskLevel = RiskLevel.LOW
    risk_factors: List[str] = field(default_factory=list)
    retry_count: int = 0
    max_retries: int = 3
    metadata: Dict[str, Any] = field(default_factory=dict)

    @property
    def can_execute(self) -> bool:
        """Check if task can be executed (dependencies satisfied)."""
        return self.status == TaskStatus.PENDING and len(self.blocked_by) == 0


@dataclass
class Milestone:
    """A milestone containing related subtasks."""
    id: str
    name: str
    description: str
    subtasks: List[SubTask] = field(default_factory=list)
    status: TaskStatus = TaskStatus.PENDING
    deadline: Optional[datetime] = None
    completed_at: Optional[datetime] = None

    @property
    def progress(self) -> float:
        """Calculate milestone progress percentage."""
        if not self.subtasks:
            return 0.0
        completed = sum(1 for t in self.subtasks if t.status == TaskStatus.COMPLETED)
        return (completed / len(self.subtasks)) * 100

    @property
    def is_complete(self) -> bool:
        """Check if all subtasks are complete."""
        return all(t.status == TaskStatus.COMPLETED for t in self.subtasks)


@dataclass
class ExecutionPlan:
    """Complete execution plan for a task."""
    id: str
    task_id: str
    description: str
    subtasks: List[SubTask] = field(default_factory=list)
    milestones: List[Milestone] = field(default_factory=list)
    status: TaskStatus = TaskStatus.PENDING
    estimated_duration: float = 0.0
    estimated_complexity: float = 0.0
    total_risk: RiskLevel = RiskLevel.LOW
    parallel_execution: bool = True
    created_at: datetime = field(default_factory=datetime.utcnow)
    started_at: Optional[datetime] = None
    completed_at: Optional[datetime] = None
    metadata: Dict[str, Any] = field(default_factory=dict)

    @property
    def progress(self) -> float:
        """Calculate overall plan progress percentage."""
        if not self.subtasks:
            return 0.0
        completed = sum(1 for t in self.subtasks if t.status == TaskStatus.COMPLETED)
        return (completed / len(self.subtasks)) * 100

    @property
    def pending_subtasks(self) -> List[SubTask]:
        """Get list of pending subtasks."""
        return [t for t in self.subtasks if t.status == TaskStatus.PENDING]

    @property
    def executable_subtasks(self) -> List[SubTask]:
        """Get list of subtasks that can be executed now."""
        return [t for t in self.subtasks if t.can_execute]

    @property
    def failed_subtasks(self) -> List[SubTask]:
        """Get list of failed subtasks."""
        return [t for t in self.subtasks if t.status == TaskStatus.FAILED]


@dataclass
class Task:
    """Main task to be planned and decomposed."""
    id: str
    description: str
    category: Optional[TaskCategory] = None
    complexity: float = 5.0
    priority: int = 100
    capabilities: List[Capability] = field(default_factory=list)
    resources: List[Resource] = field(default_factory=list)
    constraints: Dict[str, Any] = field(default_factory=dict)
    metadata: Dict[str, Any] = field(default_factory=dict)


class TaskDecomposer:
    """Decomposes complex tasks into manageable subtasks."""

    DECOMPOSITION_RULES = {
        TaskCategory.RESEARCH: [
            ("information_gathering", ["data_collection", "survey", "interview"]),
            ("analysis", ["data_analysis", "pattern_recognition", "reporting"]),
        ],
        TaskCategory.DESIGN: [
            ("architecture_design", ["system_design", "interface_design", "data_model"]),
            ("detailed_design", ["component_spec", "api_spec", "db_schema"]),
        ],
        TaskCategory.IMPLEMENTATION: [
            ("core_development", ["module_1", "module_2", "module_3"]),
            ("integration", ["component_integration", "system_integration"]),
            ("feature_implementation", ["feature_1", "feature_2", "feature_3"]),
        ],
        TaskCategory.TESTING: [
            ("unit_testing", ["test_planning", "test_implementation", "test_execution"]),
            ("integration_testing", ["test_suites", "test_automation"]),
            ("validation", ["acceptance_testing", "performance_testing"]),
        ],
        TaskCategory.DEPLOYMENT: [
            ("preparation", ["environment_setup", "configuration", "data_migration"]),
            ("deployment", ["deployment_execution", "smoke_testing"]),
            ("post_deployment", ["monitoring_setup", "documentation"]),
        ],
    }

    def __init__(self, max_subtasks: int = 20):
        self.max_subtasks = max_subtasks

    def decompose(self, task: Task) -> List[SubTask]:
        """Decompose a task into subtasks."""
        if task.category:
            return self._decompose_by_category(task)
        return self._decompose_by_complexity(task)

    def _decompose_by_category(self, task: Task) -> List[SubTask]:
        """Decompose task based on its category."""
        subtasks = []

        if task.category not in self.DECOMPOSITION_RULES:
            return self._decompose_by_complexity(task)

        rules = self.DECOMPOSITION_RULES[task.category]

        for phase_name, subtask_names in rules:
            for idx, subtask_name in enumerate(subtask_names):
                subtask_id = f"{task.id}-{phase_name}-{idx+1}"

                complexity = max(1.0, min(10.0, task.complexity * 0.8 + idx * 0.2))

                subtask = SubTask(
                    id=subtask_id,
                    description=f"{phase_name}: {subtask_name}",
                    category=task.category,
                    priority=task.priority + idx,
                    estimated_duration=self._estimate_duration(subtask_name, complexity),
                    estimated_complexity=complexity,
                    capabilities=task.capabilities.copy(),
                    metadata={"parent_task": task.id, "phase": phase_name}
                )

                subtasks.append(subtask)

        if len(subtasks) > self.max_subtasks:
            subtasks = self._merge_subtasks(subtasks)

        return subtasks

    def _decompose_by_complexity(self, task: Task) -> List[SubTask]:
        """Decompose task based on complexity score."""
        subtasks = []

        num_subtasks = min(self.max_subtasks, max(1, int(task.complexity)))

        for i in range(num_subtasks):
            subtask_id = f"{task.id}-subtask-{i+1}"
            complexity = task.complexity * (0.5 + 0.5 * (i / num_subtasks))

            subtask = SubTask(
                id=subtask_id,
                description=f"Step {i+1}: {task.description[:50]}",
                category=TaskCategory.IMPLEMENTATION,
                priority=task.priority + i,
                estimated_duration=self._estimate_duration(f"step_{i+1}", complexity),
                estimated_complexity=complexity,
                capabilities=task.capabilities.copy(),
                metadata={"parent_task": task.id, "step": i+1}
            )

            subtasks.append(subtask)

        return subtasks

    def _merge_subtasks(self, subtasks: List[SubTask]) -> List[SubTask]:
        """Merge subtasks to stay under max limit."""
        if len(subtasks) <= self.max_subtasks:
            return subtasks

        merged = []
        i = 0

        while i < len(subtasks):
            if len(merged) >= self.max_subtasks:
                break

            current = subtasks[i]
            combined_desc = current.description

            while i + 1 < len(subtasks) and len(merged) < self.max_subtasks - 1:
                next_subtask = subtasks[i + 1]
                if current.category == next_subtask.category:
                    combined_desc += f"; {next_subtask.description}"
                    current.estimated_duration += next_subtask.estimated_duration
                    current.estimated_complexity = (
                        current.estimated_complexity + next_subtask.estimated_complexity
                    ) / 2
                    i += 1
                else:
                    break

            merged_subtask = SubTask(
                id=f"merged-{len(merged)+1}",
                description=combined_desc,
                category=current.category,
                priority=current.priority,
                estimated_duration=current.estimated_duration,
                estimated_complexity=current.estimated_complexity,
                capabilities=current.capabilities.copy(),
                metadata={**current.metadata, "merged": True}
            )

            merged.append(merged_subtask)
            i += 1

        return merged

    def _estimate_duration(self, task_name: str, complexity: float) -> float:
        """Estimate task duration based on name and complexity."""
        base_durations = {
            "design": 120,
            "analysis": 60,
            "implementation": 90,
            "testing": 45,
            "deployment": 30,
            "documentation": 30,
        }

        task_lower = task_name.lower()

        for key, duration in base_durations.items():
            if key in task_lower:
                return duration * (complexity / 5.0)

        return 60.0 * (complexity / 5.0)


class DependencyAnalyzer:
    """Analyzes and resolves dependencies between subtasks."""

    def analyze(self, subtasks: List[SubTask]) -> List[SubTask]:
        """Analyze dependencies and set blocked_by relationships."""
        task_map = {t.id: t for t in subtasks}

        for subtask in subtasks:
            subtask.blocked_by = []

        for subtask in subtasks:
            for dep_id in subtask.dependencies:
                if dep_id in task_map:
                    dep_task = task_map[dep_id]
                    if subtask.id not in dep_task.dependencies:
                        dep_task.dependencies.append(subtask.id)

        for subtask in subtasks:
            for other in subtasks:
                if subtask.id != other.id:
                    if self._has_implicit_dependency(subtask, other):
                        if other.id not in subtask.blocked_by:
                            subtask.blocked_by.append(other.id)

        return subtasks

    def _has_implicit_dependency(self, task: SubTask, potential_blocker: SubTask) -> bool:
        """Check for implicit dependencies between tasks."""
        if potential_blocker.category == TaskCategory.RESEARCH:
            return True

        if potential_blocker.estimated_complexity > task.estimated_complexity:
            if potential_blocker.priority < task.priority:
                return True

        return False

    def get_execution_order(self, subtasks: List[SubTask]) -> List[List[SubTask]]:
        """Get execution order with parallelizable tasks grouped together."""
        task_map = {t.id: t for t in subtasks}
        executed: Set[str] = set()
        execution_order: List[List[SubTask]] = []

        available = [t for t in subtasks if len(t.blocked_by) == 0]

        while available:
            execution_order.append(available.copy())

            for task in available:
                executed.add(task.id)

            for task in subtasks:
                if task.id not in executed:
                    remaining_blockers = [
                        b for b in task.blocked_by if b not in executed
                    ]
                    task.blocked_by = remaining_blockers

            available = [
                t for t in subtasks
                if t.id not in executed and len(t.blocked_by) == 0
            ]

        return execution_order


class ResourceEstimator:
    """Estimates resource requirements for tasks."""

    CAPABILITY_DURATIONS = {
        "coding": 90,
        "code_generation": 120,
        "testing": 45,
        "documentation": 30,
        "review": 20,
        "analysis": 60,
        "design": 90,
        "deployment": 30,
    }

    def estimate(self, task: Task) -> Dict[str, float]:
        """Estimate total resource requirements for a task."""
        total_duration = 0.0
        resource_totals = {}

        for cap in task.capabilities:
            cap_duration = self.CAPABILITY_DURATIONS.get(
                cap.name, 60.0
            )
            total_duration += cap_duration * cap.level

        for resource in task.resources:
            resource_totals[resource.type] = (
                resource_totals.get(resource.type, 0) + resource.quantity
            )

        return {
            "estimated_duration": total_duration,
            "estimated_complexity": task.complexity,
            "resource_requirements": resource_totals
        }


class RiskAssessor:
    """Assesses risk levels for tasks and subtasks."""

    def assess_subtask(self, subtask: SubTask) -> Tuple[RiskLevel, List[str]]:
        """Assess risk for a single subtask."""
        risk_factors = []
        risk_score = 0.0

        if subtask.estimated_complexity > 7:
            risk_score += 0.3
            risk_factors.append("high complexity")

        if subtask.estimated_duration > 180:
            risk_score += 0.2
            risk_factors.append("long duration")

        if len(subtask.dependencies) > 3:
            risk_score += 0.2
            risk_factors.append("many dependencies")

        if subtask.retry_count >= subtask.max_retries:
            risk_score += 0.3
            risk_factors.append("repeated failures")

        if subtask.capabilities:
            avg_level = sum(c.level for c in subtask.capabilities) / len(subtask.capabilities)
            if avg_level < 0.5:
                risk_score += 0.2
                risk_factors.append("high capability gap")

        if risk_score >= 0.8:
            level = RiskLevel.CRITICAL
        elif risk_score >= 0.6:
            level = RiskLevel.HIGH
        elif risk_score >= 0.3:
            level = RiskLevel.MEDIUM
        else:
            level = RiskLevel.LOW

        return level, risk_factors

    def assess_plan(self, plan: ExecutionPlan) -> RiskLevel:
        """Assess overall plan risk."""
        if not plan.subtasks:
            return RiskLevel.LOW

        max_risk = RiskLevel.LOW
        risk_counts = {RiskLevel.LOW: 0, RiskLevel.MEDIUM: 0, RiskLevel.HIGH: 0, RiskLevel.CRITICAL: 0}

        for subtask in plan.subtasks:
            task_risk, _ = self.assess_subtask(subtask)
            risk_counts[task_risk] += 1

        critical_count = risk_counts[RiskLevel.CRITICAL]
        high_count = risk_counts[RiskLevel.HIGH]

        total = len(plan.subtasks)

        if critical_count > 0 or high_count / total > 0.3:
            return RiskLevel.HIGH
        elif risk_counts[RiskLevel.MEDIUM] / total > 0.5:
            return RiskLevel.MEDIUM

        return RiskLevel.LOW


class MilestoneBuilder:
    """Builds milestones from subtasks."""

    def build(self, subtasks: List[SubTask]) -> List[Milestone]:
        """Build milestones from subtasks."""
        if not subtasks:
            return []

        categories = set(t.category for t in subtasks)

        milestones_dict: Dict[TaskCategory, List[SubTask]] = {
            cat: [] for cat in categories
        }

        for subtask in subtasks:
            milestones_dict[subtask.category].append(subtask)

        milestones = []
        milestone_order = [
            TaskCategory.RESEARCH,
            TaskCategory.DESIGN,
            TaskCategory.IMPLEMENTATION,
            TaskCategory.TESTING,
            TaskCategory.DEPLOYMENT,
            TaskCategory.DOCUMENTATION,
            TaskCategory.MAINTENANCE,
        ]

        for category in milestone_order:
            if category in milestones_dict and milestones_dict[category]:
                tasks = milestones_dict[category]

                milestone = Milestone(
                    id=f"milestone-{category.value}",
                    name=f"{category.value.replace('_', ' ').title()} Phase",
                    description=f"Complete {category.value.replace('_', ' ')} tasks",
                    subtasks=tasks
                )

                milestones.append(milestone)

        return milestones


class PlanOptimizer:
    """Optimizes execution plans for efficiency."""

    def __init__(self, optimization_level: int = 2):
        self.optimization_level = optimization_level

    def optimize(self, plan: ExecutionPlan) -> ExecutionPlan:
        """Optimize the execution plan."""
        if self.optimization_level == 0:
            return plan

        plan = self._identify_parallel_tasks(plan)

        if self.optimization_level >= 2:
            plan = self._merge_small_tasks(plan)

        if self.optimization_level >= 3:
            plan = self._reorder_for_efficiency(plan)

        return plan

    def _identify_parallel_tasks(self, plan: ExecutionPlan) -> ExecutionPlan:
        """Identify tasks that can be executed in parallel."""
        plan.parallel_execution = True
        return plan

    def _merge_small_tasks(self, plan: ExecutionPlan) -> ExecutionPlan:
        """Merge small tasks to reduce overhead."""
        return plan

    def _reorder_for_efficiency(self, plan: ExecutionPlan) -> ExecutionPlan:
        """Reorder tasks for better efficiency."""
        return plan


class PlanningStrategy:
    """Main planning strategy orchestrator."""

    def __init__(self, manager: Optional[Dict[str, Any]] = None):
        """Initialize the planning strategy."""
        manager = manager or {}

        self.max_subtasks = manager.get("max_subtasks", 20)
        self.complexity_threshold = manager.get("complexity_threshold", 7.0)
        self.parallel_execution = manager.get("parallel_execution", True)
        self.risk_aware = manager.get("risk_aware", True)
        self.optimization_level = manager.get("optimization_level", 2)

        self.decomposer = TaskDecomposer(max_subtasks=self.max_subtasks)
        self.dependency_analyzer = DependencyAnalyzer()
        self.resource_estimator = ResourceEstimator()
        self.risk_assessor = RiskAssessor()
        self.milestone_builder = MilestoneBuilder()
        self.optimizer = PlanOptimizer(optimization_level=self.optimization_level)

        logger.info("PlanningStrategy initialized")

    def _select_cognitive_system(self, task: Task) -> str:
        """
        Select between System 1 (fast) and System 2 (slow) based on task complexity.

        System 1: Used for simple, low-complexity tasks where rapid response is valued.
        System 2: Used for complex tasks requiring deep analysis and careful planning.

        Args:
            task: The task to evaluate.

        Returns:
            "system1" or "system2" indicating which cognitive mode to use.
        """
        complexity = getattr(task, 'complexity', 5.0)

        if complexity <= self.system1_complexity_limit:
            self._system1_usage_count += 1
            logger.info(f"Task complexity {complexity} <= {self.system1_complexity_limit}, using System 1 (fast)")
            return "system1"
        else:
            self._system2_usage_count += 1
            logger.info(f"Task complexity {complexity} > {self.system1_complexity_limit}, using System 2 (slow)")
            return "system2"

    def _create_system1_plan(self, task: Task) -> ExecutionPlan:
        """
        System 1 (Fast): Create a plan using heuristic methods.

        Used for simple tasks requiring rapid response. Applies bases patterns
        and rules-of-thumb without deep analysis.
        """
        logger.info(f"System 1 (Fast): Creating heuristic plan for task: {task.description[:30]}...")

        subtasks = self.decomposer.decompose(task)

        execution_order = self.dependency_analyzer.get_execution_order(subtasks)

        milestones = self.milestone_builder.build(subtasks)

        plan = ExecutionPlan(
            id=f"plan-{task.id or uuid.uuid4()}",
            task_id=task.id or str(uuid.uuid4()),
            description=task.description,
            subtasks=subtasks,
            milestones=milestones,
            estimated_duration=sum(s.estimated_duration for s in subtasks),
            estimated_complexity=task.complexity,
            parallel_execution=True,
            metadata={
                "cognitive_system": "system1",
                "heuristic": True,
                "execution_order": [[t.id for t in level] for level in execution_order]
            }
        )

        logger.info(f"System 1 plan created in {time.time() - self._total_planning_time:.3f}s")
        return plan

    def create_plan(self, task: Task) -> ExecutionPlan:
        """Create an execution plan for a task using appropriate cognitive system."""
        start_time = time.time()
        task_id = task.id or str(uuid.uuid4())

        cognitive_system = self._select_cognitive_system(task)
        self._total_planning_time = start_time

        logger.info(f"Creating plan for task: {task.description[:50]}... (using {cognitive_system})")

        if cognitive_system == "system1":
            plan = self._create_system1_plan(task)
        else:
            plan = self._create_system2_plan(task)

        plan.metadata["planning_time"] = time.time() - start_time
        plan.metadata["cognitive_system"] = cognitive_system

        logger.info(
            f"Plan created: {len(plan.subtasks)} subtasks, "
            f"estimated duration: {plan.estimated_duration:.0f}s, "
            f"cognitive system: {cognitive_system}"
        )

        return plan

    def _create_system2_plan(self, task: Task) -> ExecutionPlan:
        """
        System 2 (Slow): Create a comprehensive plan using analytical methods.

        Used for complex tasks requiring thorough analysis including risk assessment,
        resource optimization, and detailed milestone planning.
        """
        logger.info(f"System 2 (Slow): Creating analytical plan for task: {task.description[:30]}...")

        subtasks = self.decomposer.decompose(task)

        subtasks = self.dependency_analyzer.analyze(subtasks)

        execution_order = self.dependency_analyzer.get_execution_order(subtasks)
        logger.info(f"Decomposed into {len(subtasks)} subtasks across {len(execution_order)} execution levels")

        milestones = self.milestone_builder.build(subtasks)

        resource_estimate = self.resource_estimator.estimate(task)

        for subtask in subtasks:
            if self.risk_aware:
                risk_level, risk_factors = self.risk_assessor.assess_subtask(subtask)
                subtask.risk_level = risk_level
                subtask.risk_factors = risk_factors

        plan = ExecutionPlan(
            id=f"plan-{task.id or uuid.uuid4()}",
            task_id=task.id or str(uuid.uuid4()),
            description=task.description,
            subtasks=subtasks,
            milestones=milestones,
            estimated_duration=resource_estimate["estimated_duration"],
            estimated_complexity=resource_estimate["estimated_complexity"],
            parallel_execution=self.parallel_execution,
            metadata={
                "cognitive_system": "system2",
                "resource_estimate": resource_estimate,
                "execution_order": [[t.id for t in level] for level in execution_order]
            }
        )

        plan.total_risk = self.risk_assessor.assess_plan(plan)

        plan = self.optimizer.optimize(plan)

        plan.metadata["planning_time"] = time.time() - self._total_planning_time
        plan.metadata["cognitive_system"] = "system2"

        logger.info(f"System 2 plan created in {time.time() - self._total_planning_time:.3f}s")
        return plan

    def get_cognitive_stats(self) -> Dict[str, Any]:
        """
        Get statistics about cognitive system usage.

        Returns:
            Dictionary with system usage counts and percentages.
        """
        total = self._system1_usage_count + self._system2_usage_count
        if total == 0:
            return {
                "system1_count": 0,
                "system2_count": 0,
                "system1_percent": 0.0,
                "system2_percent": 0.0,
                "total_plans": 0
            }

        return {
            "system1_count": self._system1_usage_count,
            "system2_count": self._system2_usage_count,
            "system1_percent": (self._system1_usage_count / total) * 100,
            "system2_percent": (self._system2_usage_count / total) * 100,
            "total_plans": total
        }

    def update_subtask_status(
        self,
        plan: ExecutionPlan,
        subtask_id: str,
        status: TaskStatus,
        error: Optional[str] = None
    ) -> bool:
        """Update the status of a subtask."""
        for subtask in plan.subtasks:
            if subtask.id == subtask_id:
                subtask.status = status

                if status == TaskStatus.COMPLETED:
                    logger.info(f"Subtask {subtask_id} completed")
                elif status == TaskStatus.FAILED:
                    subtask.retry_count += 1
                    logger.warning(f"Subtask {subtask_id} failed (retry {subtask.retry_count}/{subtask.max_retries})")
                    if error:
                        subtask.metadata["last_error"] = error

                self._update_milestone_status(plan, subtask.category)

                if all(t.status == TaskStatus.COMPLETED for t in plan.subtasks):
                    plan.status = TaskStatus.COMPLETED
                    plan.completed_at = datetime.utcnow()
                    logger.info(f"Plan {plan.id} completed")

                return True

        return False

    def _update_milestone_status(self, plan: ExecutionPlan, category: TaskCategory):
        """Update milestone status based on subtask completion."""
        for milestone in plan.milestones:
            milestone_subtasks = [t for t in plan.subtasks if t.category == category]

            if all(t.status == TaskStatus.COMPLETED for t in milestone_subtasks):
                milestone.status = TaskStatus.COMPLETED
                milestone.completed_at = datetime.utcnow()

    def get_next_executable(
        self,
        plan: ExecutionPlan
    ) -> List[SubTask]:
        """Get next set of executable subtasks."""
        if plan.status == TaskStatus.COMPLETED:
            return []

        if plan.parallel_execution:
            return plan.executable_subtasks
        else:
            executable = plan.executable_subtasks
            return executable[:1] if executable else []

    def get_plan_summary(self, plan: ExecutionPlan) -> Dict[str, Any]:
        """Get a summary of the execution plan."""
        status_counts = {}
        for status in TaskStatus:
            status_counts[status.value] = sum(
                1 for t in plan.subtasks if t.status == status
            )

        return {
            "plan_id": plan.id,
            "task_id": plan.task_id,
            "description": plan.description,
            "total_subtasks": len(plan.subtasks),
            "completed_subtasks": status_counts.get("completed", 0),
            "pending_subtasks": status_counts.get("pending", 0),
            "failed_subtasks": status_counts.get("failed", 0),
            "progress": plan.progress,
            "estimated_duration": plan.estimated_duration,
            "estimated_complexity": plan.estimated_complexity,
            "total_risk": plan.total_risk.value,
            "status": plan.status.value,
            "parallel_execution": plan.parallel_execution
        }

    def validate_plan(self, plan: ExecutionPlan) -> Tuple[bool, List[str]]:
        """Validate an execution plan for consistency."""
        errors = []

        task_ids = {t.id for t in plan.subtasks}
        for subtask in plan.subtasks:
            for dep_id in subtask.dependencies:
                if dep_id not in task_ids:
                    errors.append(f"Subtask {subtask.id} depends on unknown task {dep_id}")

            for blocker_id in subtask.blocked_by:
                if blocker_id not in task_ids:
                    errors.append(f"Subtask {subtask.id} blocked by unknown task {blocker_id}")

        if not plan.subtasks:
            errors.append("Plan has no subtasks")

        for milestone in plan.milestones:
            for subtask in milestone.subtasks:
                if subtask not in plan.subtasks:
                    errors.append(f"Milestone {milestone.id} contains orphaned subtask {subtask.id}")

        return len(errors) == 0, errors
