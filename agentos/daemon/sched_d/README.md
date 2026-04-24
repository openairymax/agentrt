# Scheduler Daemon — 任务调度守护进程

`daemon/sched_d/` 是 AgentOS 的任务调度守护进程，负责任务的分发、调度策略执行和任务状态管理。

## 核心职责

- **任务分发**：根据调度策略将任务分发给合适的 Agent
- **调度策略**：支持多种调度算法（轮询、加权、优先级、亲和性）
- **任务队列**：管理全局任务队列，支持优先级和依赖
- **状态追踪**：跟踪任务的生命周期状态
- **重试机制**：失败任务自动重试，可配置重试策略

## 调度策略

| 策略 | 说明 | 适用场景 |
|------|------|----------|
| 轮询 | 依次分配给可用 Agent | 负载均衡 |
| 加权 | 按 Agent 权重分配 | 异构 Agent 集群 |
| 优先级 | 优先处理高优先级任务 | 实时任务 |
| 亲和性 | 同类任务分配给同一 Agent | 有状态任务 |
| 自定义 | 用户自定义调度规则 | 特殊需求 |

## 核心能力

| 能力 | 说明 |
|------|------|
| `sched.submit` | 提交任务到队列 |
| `sched.dispatch` | 分发任务到 Agent |
| `sched.cancel` | 取消待执行任务 |
| `sched.status` | 查询任务执行状态 |
| `sched.queue.info` | 查询队列状态 |
| `sched.policy.set` | 设置调度策略 |

## 使用方式

```bash
# 启动调度守护进程
./sched_d --config sched_config.json

# 设置调度策略
./sched_d --policy weighted

# 指定工作线程数
./sched_d --workers 8
```

---

*AgentOS Daemon — Scheduler Daemon*
