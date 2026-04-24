# Dispatching — 调度策略

`openlab/contrib/strategies/dispatching/` 提供智能体的任务调度策略，负责将任务高效地分发给最合适的执行者。

## 核心能力

- **任务分发**：根据任务类型和 Agent 能力进行智能分发
- **负载均衡**：基于 Agent 当前负载动态分配任务
- **优先级调度**：支持基于优先级的任务队列管理
- **故障转移**：Agent 异常时的自动任务重分配

## 调度策略

| 策略 | 说明 | 适用场景 |
|------|------|----------|
| 轮询调度 | 依次分配给可用 Agent | 各 Agent 能力相同 |
| 能力匹配 | 按能力匹配度分配 | 专业化 Agent 集群 |
| 最短队列 | 分配给等待队列最短的 Agent | 高负载场景 |
| 加权分配 | 按权重比例分配 | 异构 Agent 集群 |

## 使用方式

```python
from contrib.strategies.dispatching import DispatchingStrategy

dispatcher = DispatchingStrategy(strategy="capability_match")

# 分发任务
task = {
    "id": "task-001",
    "type": "code_review",
    "priority": "high",
    "payload": {"repo": "agentos", "branch": "main"}
}

agent = dispatcher.dispatch(task)
print(f"Task dispatched to: {agent.id}")
```

---

*AgentOS OpenLab — Dispatching Strategy*
