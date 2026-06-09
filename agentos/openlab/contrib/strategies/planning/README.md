# Planning — 任务规划策略

**模块路径**: `agentos/openlab/contrib/strategies/planning/`
**版本**: v0.1.0

> **Status**: 本模块作为 AgentOS 的正式组成部分，API 持续演进中。本模块通过 JSON-RPC 2.0 协议与 AgentOS 核心运行时集成。

## 概述

Planning 策略模块提供智能体的任务规划能力，负责将复杂目标分解为可执行的步骤序列。当前实现为贡献骨架（Contrib Skeleton），提供基础规划接口，支持目标分解、依赖分析和计划生成。

## 目录结构

```
planning/
├── __init__.py                 # 模块导出
├── planning.py                 # PlanningStrategy/PlanStep 核心实现
└── README.md                   # 本文件
```

## 核心组件

### PlanningStrategy (`planning.py`)

| 类 | 说明 |
|----|------|
| `PlanningStrategy` | 规划策略类，接受 config 配置，提供 `plan()` 方法 |
| `PlanStep` | 计划步骤数据类，包含 step_id/description/dependencies/assigned_agent |

## 规划流程

```
目标输入 → 目标分解 → 依赖分析 → 资源评估 → 计划生成 → 计划执行
    ↓          ↓          ↓          ↓          ↓          ↓
 用户需求   子任务列表   DAG 图    资源分配   时间线    执行引擎
```

## 接口说明

```python
@dataclass
class PlanStep:
    step_id: str = ""
    description: str = ""
    dependencies: List[str] = field(default_factory=list)
    assigned_agent: Optional[str] = None

class PlanningStrategy:
    def __init__(self, config: Optional[Dict[str, Any]] = None)

    async def plan(self, task: Any) -> Dict[str, Any]:
        """将任务分解为执行计划

        Args:
            task: 任务对象（dict 或其他类型）

        Returns:
            Dict: 包含 status/strategy/task/steps 信息
        """
```

## 依赖关系

- **核心依赖**: AgentOS OpenLab Core
- **Python**: >= 3.10, typing, dataclasses

## 使用示例

```python
from contrib.strategies.planning import PlanningStrategy, PlanStep

planner = PlanningStrategy()

plan = await planner.plan(
    task={
        "description": "开发一个 REST API 服务",
        "constraints": {
            "deadline": "2024-02-01",
            "team_size": 3,
            "tech_stack": ["Python", "FastAPI"]
        }
    }
)

for step in plan["steps"]:
    print(f"{step.step_id}: {step.description}")
```

---

© 2026 SPHARX Ltd. All Rights Reserved.
