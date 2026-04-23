# Planning — 规划策略

`openlab/contrib/strategies/planning/` 提供智能体的任务规划策略，负责将复杂目标分解为可执行的步骤序列。

## 核心能力

- **目标分解**：将高层次目标拆解为可执行的子任务
- **依赖分析**：识别任务间的依赖关系，确定执行顺序
- **资源评估**：评估执行每个子任务所需的资源
- **计划优化**：基于约束条件优化执行计划

## 规划流程

```
目标输入 → 目标分解 → 依赖分析 → 资源评估 → 计划生成 → 计划执行
    ↓          ↓          ↓          ↓          ↓          ↓
 用户需求   子任务列表   DAG 图    资源分配   时间线    执行引擎
```

## 使用方式

```python
from contrib.strategies.planning import PlanningStrategy

planner = PlanningStrategy()

# 生成执行计划
plan = planner.create_plan(
    goal="开发一个 REST API 服务",
    constraints={
        "deadline": "2024-02-01",
        "team_size": 3,
        "tech_stack": ["Python", "FastAPI"]
    }
)

# 查看计划步骤
for step in plan.steps:
    print(f"{step.id}: {step.description} ({step.estimated_hours}h)")
```

---

*AgentOS OpenLab — Planning Strategy*
