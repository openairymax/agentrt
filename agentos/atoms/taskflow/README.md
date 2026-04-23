# TaskFlow — 任务流引擎

**路径**: `agentos/atoms/taskflow/`

TaskFlow 是 AgentOS 的**任务流引擎**，基于 DAG（有向无环图）提供复杂任务的编排、调度、执行和监控能力。它与 CoreLoopThree 的执行循环紧密配合，将执行循环中的行动分解为可管理的任务单元，并按照依赖关系有序执行。

---

## 设计理念

- **DAG 编排**: 任务间依赖关系通过有向无环图定义，自动进行拓扑排序
- **生命周期管理**: 每个任务具有清晰的状态转换路径
- **多级优先级**: 支持 5 级优先级队列，确保关键任务优先执行
- **持久化**: 任务状态持久化到 SQLite，支持系统重启后恢复
- **可观测性**: 每个任务的生命周期事件均可追踪和审计

---

## 架构总览

```
┌─────────────────────────────────────────────────────┐
│                   TaskFlow Engine                     │
│                                                       │
│  ┌──────────────┐  ┌──────────────┐  ┌────────────┐ │
│  │   DAG Graph   │  │   Priority   │  │   State    │ │
│  │    Manager    │  │    Queue     │  │   Machine  │ │
│  └──────┬───────┘  └──────┬───────┘  └─────┬──────┘ │
│         │                 │                 │         │
│  ┌──────▼─────────────────▼─────────────────▼──────┐ │
│  │              Scheduler Engine                    │ │
│  │  ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐   │ │
│  │  │  Topo  │ │  Exec  │ │  Retry │ │  Event │   │ │
│  │  │  Sort  │ │  Pool  │ │ Manager│ │  Bus   │   │ │
│  │  └────────┘ └────────┘ └────────┘ └────────┘   │ │
│  └─────────────────────────────────────────────────┘ │
│                                                       │
│  ┌─────────────────────────────────────────────────┐ │
│  │              Persistence (SQLite)               │ │
│  └─────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────┘
```

---

## 任务生命周期

每个任务经历完整的生命周期状态转换：

```
                             ┌──────────┐
                             │  Pending  │
                             │  (待处理)  │
                             └────┬─────┘
                                  │
                                  ▼
                             ┌──────────┐
                    ┌────────│  Ready   │
                    │        │  (就绪)   │
                    │        └────┬─────┘
                    │             │
                    ▼             ▼
             ┌──────────┐  ┌──────────┐
             │ Blocked  │  │ Running  │
             │ (阻塞)    │  │ (运行中)  │
             └──────────┘  └────┬─────┘
                                │
                    ┌───────────┼───────────┐
                    │           │           │
                    ▼           ▼           ▼
             ┌──────────┐ ┌──────────┐ ┌──────────┐
             │ Success  │ │  Failed  │ │ Cancelled│
             │ (成功)    │ │ (失败)    │ │ (取消)    │
             └──────────┘ └────┬─────┘ └──────────┘
                               │
                               ▼
                        ┌──────────┐
                        │  Retry   │──────────→ Ready
                        │  (重试)   │
                        └──────────┘
```

---

## 多级优先级队列

TaskFlow 使用 5 级优先级队列，确保高优先级任务优先执行：

| 优先级 | 级别值 | 说明 | 示例 |
|--------|--------|------|------|
| CRITICAL | 0 | 关键任务，立即执行 | 错误恢复、安全操作 |
| HIGH | 1 | 高优先级任务 | 用户交互响应 |
| NORMAL | 2 | 普通任务 | 标准业务处理 |
| LOW | 3 | 低优先级任务 | 后台数据处理 |
| BACKGROUND | 4 | 后台任务 | 日志归档、统计 |

---

## DAG 依赖管理

任务通过 DAG 定义依赖关系，引擎自动进行拓扑排序：

```python
# 任务 DAG 定义示例
taskflow = TaskFlow()

@taskflow.task(depends_on=["fetch_data"])
def process_data(ctx):
    data = ctx.get("fetch_data")
    return transform(data)

@taskflow.task(depends_on=["process_data", "validate_schema"])
def generate_report(ctx):
    data = ctx.get("process_data")
    schema = ctx.get("validate_schema")
    return create_report(data, schema)

# 引擎自动拓扑排序并执行
taskflow.run()
```

---

## 配置接口

```json
{
  "taskflow": {
    "max_concurrent_tasks": 10,
    "default_priority": "NORMAL",
    "retry": {
      "max_retries": 3,
      "backoff": "exponential",
      "initial_delay_ms": 1000
    },
    "persistence": {
      "enabled": true,
      "backend": "sqlite",
      "path": "/var/lib/agentos/taskflow.db"
    },
    "timeout": {
      "default_seconds": 300,
      "max_seconds": 86400
    }
  }
}
```

---

## 与相关模块的关系

- **CoreLoopThree**: 在执行循环中集成 TaskFlow，将行动计划转换为 DAG 任务流
- **CoreKern**: 利用微内核的任务调度能力
- **Syscall**: 通过系统调用接口管理任务生命周期
- **Daemon - sched_d**: sched_d 守护进程使用 TaskFlow 进行跨服务任务调度

---

© 2026 SPHARX Ltd. All Rights Reserved.
