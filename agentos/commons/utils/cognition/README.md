# Cognition — 认知管理

`commons/utils/cognition/` 提供 Agent 的认知管理能力，包括 Agent 信息管理、任务调度和计划生成。

## 设计目标

- **Agent 信息管理**：统一管理 Agent 的标识、能力、状态和元数据
- **任务调度**：基于 Agent 能力和当前负载的任务分发
- **计划生成**：根据目标自动生成执行计划，拆解为可执行的任务序列

## 核心能力

| 接口 | 说明 |
|------|------|
| `agent_info_register` | 注册 Agent 信息 |
| `agent_info_query` | 查询 Agent 信息 |
| `task_dispatch` | 分发任务到最优 Agent |
| `plan_generate` | 根据目标生成执行计划 |
| `plan_optimize` | 优化已有执行计划 |
| `capability_match` | 能力匹配计算 |

## Agent 信息结构

```json
{
    "agent_id": "agent-001",
    "name": "CodeAssistant",
    "capabilities": ["code_review", "refactoring", "testing"],
    "status": "idle",
    "load": 0.3,
    "metadata": {
        "version": "1.0.0",
        "language": "python"
    }
}
```

## 使用示例

### C API

```c
#include "cognition/cognition_common.h"

agent_info_t agents[3];
agent_info_init(&agents[0], "agent-001");
agent_info_init(&agents[1], "agent-002");
agent_info_init(&agents[2], "agent-003");

agent_info_update_stats(&agents[0], true, 120);
agent_info_update_stats(&agents[1], false, 350);
agent_info_update_stats(&agents[2], true, 80);

task_info_t task;
task_info_init(&task, "task-001", "code_review", "Review PR #42");

dispatch_result_t dispatch;
cognition_select_best_agent(agents, 3, &task, &dispatch);
if (dispatch.success) {
    printf("Selected: %s (confidence: %.2f)\n",
           dispatch.selected_agent, dispatch.confidence);
}

plan_result_t plan;
cognition_generate_plan(&task, &plan);
if (plan.success) {
    printf("Plan: %s\n", plan.plan);
}

dispatch_result_cleanup(&dispatch);
plan_result_cleanup(&plan);
task_info_cleanup(&task);
for (int i = 0; i < 3; i++) agent_info_cleanup(&agents[i]);
```

### Python API

```python
from cognition import AgentInfoManager

manager = AgentInfoManager()

# 注册 Agent
manager.register(
    agent_id="agent-001",
    name="CodeAssistant",
    capabilities=["code_review", "refactoring"],
    metadata={"version": "1.0.0"}
)

# 查询 Agent
agent = manager.query("agent-001")
print(f"Agent: {agent.name}, Status: {agent.status}")

# 按能力查找
agents = manager.find_by_capability("code_review")
for agent in agents:
    print(f"Found: {agent.name}")
```

### 任务分发

```python
from cognition import TaskDispatcher

dispatcher = TaskDispatcher()

# 分发任务
task = {
    "type": "code_review",
    "priority": "high",
    "payload": {"repo": "agentos", "pr": 42}
}

selected_agent = dispatcher.dispatch(task)
print(f"Task dispatched to: {selected_agent.name}")
```

## 配置选项

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| match_threshold | float | 0.7 | 能力匹配阈值 |
| load_balance | bool | true | 是否启用负载均衡 |
| max_plan_depth | int | 10 | 计划最大深度 |
| cache_ttl | int | 300 | 缓存 TTL（秒） |

---

*AgentOS Commons Utils — Cognition*
