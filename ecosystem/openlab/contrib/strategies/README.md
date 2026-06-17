# Strategies — 调度与规划策略

**模块路径**: `ecosystem/openlab/contrib/strategies/`
**版本**: v0.1.0

> **Status**: 本模块作为 AgentRT 的正式组成部分，API 持续演进中。本模块通过 JSON-RPC 2.0 协议与 AgentRT 核心运行时集成。

## 概述

Strategies 是 OpenLab 社区贡献的调度与规划策略集合，定义 Agent 协作的模式和任务编排方式。该模块包含两大策略：Dispatching（任务调度）和 Planning（任务规划），分别解决"任务分发给谁"和"任务如何分解"两个核心问题，共同构建多智能体协作的编排基础。

## 架构定位

```
+-------------------------------------------------------------------+
|                         OpenLab 开放生态                            |
+-------------------------------------------------------------------+
|                     Contributions (contrib/)                       |
|  +------------------+  +------------------+  +------------------+ |
|  |     Skills       |  |    Strategies    |  |     Agents       | |
|  |  能力单元         |  |  协作模式        |  |  角色化智能体     | |
|  |                  |  |                  |  |                  | |
|  | • Browser        |  | • Dispatching    |  | • Architect      | |
|  | • Database       |  | • Planning       |  | • Backend        | |
|  | • GitHub         |  |                  |  | • Frontend       | |
|  +------------------+  +------------------+  | • ...            | |
|                                              +------------------+ |
+-------------------------------------------------------------------+
|                     OpenLab Core (openlab/)                        |
|  Agent Registry | Task Scheduler | Tool Executor | Storage         |
+-------------------------------------------------------------------+
```

Strategies 作为 Contributions 三大子模块之一，是 Agent 协作的编排层。Dispatching 策略决定任务如何分配给合适的 Agent，Planning 策略决定复杂任务如何分解为可执行步骤，两者协同工作实现高效的多智能体编排。

## 目录结构

```
strategies/
├── __init__.py                     # 策略包导出
├── dispatching/                    # 任务调度策略
│   ├── __init__.py                 # 导出 DispatchingStrategy
│   ├── dispatching.py              # DispatchingStrategy 核心实现
│   └── README.md                   # Dispatching 详细文档
├── planning/                       # 任务规划策略
│   ├── __init__.py                 # 导出 PlanningStrategy, PlanStep
│   ├── planning.py                 # PlanningStrategy/PlanStep 核心实现
│   └── README.md                   # Planning 详细文档
└── README.md                       # 本文件
```

## 策略列表

| Strategy | 路径 | 核心问题 | 核心类 |
|----------|------|----------|--------|
| **Dispatching** | `dispatching/` | 任务分发给谁 | `DispatchingStrategy` |
| **Planning** | `planning/` | 任务如何分解 | `PlanningStrategy`、`PlanStep` |

## 策略体系

### Dispatching — 任务调度策略

负责任务到 Agent 的分配决策，根据不同策略将任务分发给最合适的执行者。

#### 核心接口

```python
class DispatchingStrategy:
    def __init__(self, config: Optional[Dict[str, Any]] = None)

    async def dispatch(self, task: Any, agents: list = None) -> Dict[str, Any]:
        """分发任务到合适的 Agent

        Args:
            task: 任务对象
            agents: 可用 Agent 列表

        Returns:
            Dict: 包含 status/strategy/assigned_agents/task 信息
        """
```

#### 支持的调度策略

| 策略 | 说明 | 适用场景 |
|------|------|----------|
| 轮询调度 (Round Robin) | 依次分配给可用 Agent | 各 Agent 能力相同 |
| 能力匹配 (Capability Match) | 按能力匹配度分配 | 专业化 Agent 集群 |
| 最短队列 (Shortest Queue) | 分配给等待队列最短的 Agent | 高负载场景 |
| 加权分配 (Weighted) | 按权重比例分配 | 异构 Agent 集群 |
| 随机分配 (Random) | 随机选择 Agent | 负载测试 |

> 详细接口和配置请参阅 `dispatching/README.md`。

### Planning — 任务规划策略

负责将复杂目标分解为可执行的步骤序列，支持目标分解、依赖分析和计划生成。

#### 核心接口

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

#### 规划流程

```
目标输入 → 目标分解 → 依赖分析 → 资源评估 → 计划生成 → 计划执行
    ↓          ↓          ↓          ↓          ↓          ↓
 用户需求   子任务列表   DAG 图    资源分配   时间线    执行引擎
```

#### PlanStep 数据结构

| 字段 | 类型 | 说明 |
|------|------|------|
| `step_id` | `str` | 步骤唯一标识 |
| `description` | `str` | 步骤描述 |
| `dependencies` | `List[str]` | 依赖步骤 ID 列表 |
| `assigned_agent` | `Optional[str]` | 分配的 Agent 标识 |

> 详细接口和配置请参阅 `planning/README.md`。

## 使用指南

### 调度与规划协同

Dispatching 和 Planning 策略通常协同使用：先通过 Planning 将复杂任务分解为步骤，再通过 Dispatching 将每个步骤分配给合适的 Agent。

```python
from contrib.strategies.planning import PlanningStrategy
from contrib.strategies.dispatching import DispatchingStrategy

# 1. 规划：将复杂任务分解为步骤
planner = PlanningStrategy()
plan = await planner.plan(task={
    "description": "开发一个 REST API 服务",
    "constraints": {
        "team_size": 3,
        "tech_stack": ["Python", "FastAPI"]
    }
})

# 2. 调度：将每个步骤分配给合适的 Agent
dispatcher = DispatchingStrategy()
for step in plan["steps"]:
    result = await dispatcher.dispatch(step, agents=available_agents)
    print(f"Step {step.step_id} → {result['assigned_agents']}")
```

### 单独使用 Dispatching

```python
from contrib.strategies.dispatching import DispatchingStrategy

dispatcher = DispatchingStrategy(strategy="capability_match")

task = {
    "id": "task-001",
    "type": "code_review",
    "priority": "high",
    "payload": {"repo": "agentos", "branch": "main"}
}

result = await dispatcher.dispatch(task, agents=available_agents)
print(f"Task dispatched to: {result['assigned_agents']}")
```

### 单独使用 Planning

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

## 开发指南

### 创建新的社区策略

1. 在 `contrib/strategies/` 下创建新目录（使用小写字母）
2. 实现策略核心类，提供标准化的异步接口
3. 创建 `__init__.py` 导出策略类
4. 编写 `README.md`，包含概述、核心组件、接口说明和使用示例
5. 在 `strategies/__init__.py` 中注册新策略

### 策略接口规范

所有策略应遵循以下接口规范：

- 构造函数接受 `config: Optional[Dict[str, Any]]` 配置参数
- 核心方法为异步方法，返回 `Dict[str, Any]` 结果
- 结果中包含 `status` 和 `strategy` 字段标识执行状态和策略类型

## 与其他模块的关系

| 模块 | 关系 |
|------|------|
| **contrib/agents/** | Dispatching 策略将任务分配给 Agent；Planning 策略在 PlanStep 中指定 `assigned_agent` |
| **contrib/skills/** | Strategies 根据 Agent 拥有的 Skills 进行能力匹配调度 |
| **openlab/core/** | Strategies 与 Core 的 TaskScheduler 协同工作，实现任务的调度和编排 |
| **app/** | 应用层使用 Strategies 编排多 Agent 协作流程 |

## 依赖关系

- **核心依赖**: Python >= 3.10, openlab.core, typing, dataclasses
- **协议依赖**: AgentRT protocols 层（JSON-RPC 2.0）

---

© 2026 SPHARX Ltd. All Rights Reserved.
