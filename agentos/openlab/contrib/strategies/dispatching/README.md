# Dispatching — 任务调度策略

**模块路径**: `agentos/openlab/contrib/strategies/dispatching/`
**版本**: v0.0.5

> **Status**: 本模块作为 AgentOS 的正式组成部分，API 持续演进中。本模块通过 JSON-RPC 2.0 协议与 AgentOS 核心运行时集成。

## 概述

Dispatching 策略模块提供智能体的任务调度能力，负责将任务高效地分发给最合适的执行者。当前实现为贡献骨架（Contrib Skeleton），提供基础调度接口，支持根据不同策略进行任务分配。

## 目录结构

```
dispatching/
├── __init__.py                 # 模块导出
├── dispatching.py              # DispatchingStrategy 核心实现
└── README.md                   # 本文件
```

## 核心组件

### DispatchingStrategy (`dispatching.py`)

| 类 | 说明 |
|----|------|
| `DispatchingStrategy` | 调度策略基类，接受 config 配置，提供 `dispatch()` 方法 |

## 调度策略

| 策略 | 说明 | 适用场景 |
|------|------|----------|
| 轮询调度 (Round Robin) | 依次分配给可用 Agent | 各 Agent 能力相同 |
| 能力匹配 (Capability Match) | 按能力匹配度分配 | 专业化 Agent 集群 |
| 最短队列 (Shortest Queue) | 分配给等待队列最短的 Agent | 高负载场景 |
| 加权分配 (Weighted) | 按权重比例分配 | 异构 Agent 集群 |
| 随机分配 (Random) | 随机选择 Agent | 负载测试 |

## 接口说明

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

## 依赖关系

- **核心依赖**: AgentOS OpenLab Core
- **Python**: >= 3.10, typing

## 使用示例

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

---

© 2026 SPHARX Ltd. All Rights Reserved.
