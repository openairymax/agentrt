# Planning Strategy

Task planning and decomposition strategy for OpenHub AgentOS.

## Overview

The planning strategy module provides intelligent task decomposition and planning mechanisms for the OpenHub platform. It analyzes complex tasks, breaks them into manageable subtasks, and creates optimal execution plans.

## Features

- Task decomposition
- Dependency analysis
- Execution ordering
- Resource estimation
- Risk assessment
- Plan optimization
- Milestone tracking

## Architecture

```
                    ┌─────────────────┐
                    │   Task Input    │
                    └────────┬────────┘
                             │
                    ┌────────▼────────┐
                    │  Task Analyzer  │
                    └────────┬────────┘
                             │
        ┌────────────────────┼────────────────────┐
        │                    │                    │
┌───────▼───────┐   ┌───────▼───────┐   ┌───────▼───────┐
│ Decomposer    │   │  Dependency   │   │   Resource    │
│              │   │  Analyzer    │   │  Estimator   │
└───────┬───────┘   └───────┬───────┘   └───────┬───────┘
        │                    │                    │
        └────────────────────┼────────────────────┘
                             │
                    ┌────────▼────────┐
                    │  Plan Builder  │
                    └────────┬────────┘
                             │
                    ┌────────▼────────┐
                    │  Plan Optimizer │
                    └────────┬────────┘
                             │
                    ┌────────▼────────┐
                    │   Execution     │
                    │     Plan       │
                    └─────────────────┘
```

## Usage

```python
from planning import PlanningStrategy, Task, SubTask, ExecutionPlan

# Create a complex task
task = Task(
    id="task-001",
    description="Build a web application",
    complexity=8,
    required_capabilities=["frontend", "backend", "database"]
)

# Create planning strategy
strategy = PlanningStrategy()

# Create execution plan
plan = strategy.create_plan(task)
print(f"Plan has {len(plan.subtasks)} subtasks")
print(f"Estimated duration: {plan.estimated_duration}")

# Execute plan step by step
for milestone in plan.milestones:
    print(f"Milestone: {milestone.name}")
    for subtask in milestone.subtasks:
        print(f"  - {subtask.description}")
```

## Configuration

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| max_subtasks | integer | 20 | Maximum subtasks per plan |
| complexity_threshold | float | 7.0 | Threshold for task decomposition |
| parallel_execution | boolean | true | Allow parallel subtask execution |
| risk_aware | boolean | true | Enable risk assessment |
| optimization_level | integer | 2 | Plan optimization level (0-3) |

## License

MIT
