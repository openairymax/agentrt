# Cognition — 认知管理

**模块路径**: `agentos/commons/utils/cognition/`
**版本**: v0.1.0

## 概述

Cognition 模块提供 Agent 的认知管理能力，包括 Agent 信息管理、任务调度、计划生成和多 Agent 协调。该模块是 AgentRT 智能调度体系的核心，负责在多个 Agent 之间进行最优选择与任务分发，并支持根据目标自动生成执行计划。

## 设计目标

- **Agent 信息管理**：统一管理 Agent 的标识、能力、状态和元数据，支持性能统计的动态更新
- **任务调度**：基于 Agent 能力、权重和当前负载的任务分发，返回最优 Agent 及置信度
- **计划生成**：根据目标自动生成执行计划，拆解为可执行的任务序列
- **多 Agent 协调**：协调多个 Agent 的执行结果，合并为统一决策

## 目录结构

```
cognition/
├── include/
│   └── cognition_common.h       # 认知模块公共接口定义
├── src/
│   └── cognition_common.c       # 认知模块实现
└── README.md                    # 本文档
```

## 核心数据结构

### agent_info_t — Agent 信息

| 字段 | 类型 | 说明 |
|------|------|------|
| `agent_id` | `char *` | Agent 唯一标识 |
| `weight` | `double` | 当前权重（由 `agent_info_calculate_weight` 动态计算） |
| `success_rate` | `double` | 历史成功率 |
| `total_tasks` | `uint64_t` | 总任务数 |
| `successful_tasks` | `uint64_t` | 成功任务数 |
| `avg_latency` | `double` | 平均延迟（毫秒） |
| `last_used` | `uint64_t` | 最后使用时间戳 |

### task_info_t — 任务信息

| 字段 | 类型 | 说明 |
|------|------|------|
| `task_id` | `char *` | 任务唯一标识 |
| `task_type` | `char *` | 任务类型（如 `code_review`） |
| `task_content` | `char *` | 任务内容描述 |
| `priority` | `uint64_t` | 优先级 |
| `deadline` | `uint64_t` | 截止时间 |

### dispatch_result_t — 调度结果

| 字段 | 类型 | 说明 |
|------|------|------|
| `success` | `bool` | 调度是否成功 |
| `selected_agent` | `char *` | 选中的 Agent ID |
| `confidence` | `double` | 置信度（0.0~1.0） |
| `error` | `char *` | 错误信息 |

### plan_result_t — 计划结果

| 字段 | 类型 | 说明 |
|------|------|------|
| `success` | `bool` | 生成是否成功 |
| `plan` | `char *` | 计划内容 |
| `plan_size` | `size_t` | 计划大小 |
| `error` | `char *` | 错误信息 |

### coordination_result_t — 协调结果

| 字段 | 类型 | 说明 |
|------|------|------|
| `success` | `bool` | 协调是否成功 |
| `decision` | `char *` | 决策结果 |
| `decision_size` | `size_t` | 决策大小 |
| `error` | `char *` | 错误信息 |

## 接口说明

### Agent 信息管理

| 函数 | 说明 |
|------|------|
| `agent_info_init(agent, agent_id)` | 初始化 Agent 信息，返回 0 成功 |
| `agent_info_cleanup(agent)` | 清理 Agent 信息，释放内存 |
| `agent_info_update_stats(agent, success, latency)` | 更新性能统计（成功/失败、延迟） |
| `agent_info_calculate_weight(agent)` | 基于成功率与延迟动态计算权重 |

### 任务管理

| 函数 | 说明 |
|------|------|
| `task_info_init(task, task_id, task_type, task_content)` | 初始化任务信息 |
| `task_info_cleanup(task)` | 清理任务信息 |

### 调度与计划

| 函数 | 说明 |
|------|------|
| `cognition_select_best_agent(agents, count, task, result)` | 从 Agent 数组中选择最优 Agent |
| `cognition_generate_plan(task, result)` | 根据任务生成执行计划 |
| `cognition_coordinate_results(agent_results, count, result)` | 协调多个 Agent 的结果 |
| `cognition_calculate_task_priority(task)` | 计算任务优先级 |
| `cognition_evaluate_plan_quality(plan, task)` | 评估计划质量（返回 0~100 分） |

### 结果管理

| 函数 | 说明 |
|------|------|
| `dispatch_result_init / cleanup` | 初始化/清理调度结果 |
| `plan_result_init / cleanup` | 初始化/清理计划结果 |
| `coordination_result_init / cleanup` | 初始化/清理协调结果 |

## 使用示例

```c
#include "cognition_common.h"

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

## 依赖关系

| 依赖 | 说明 |
|------|------|
| `strategy_common.h` | 加权评分算法用于 Agent 选择 |
| `memory_compat.h` | 统一内存管理宏 |
| `string_compat.h` | 字符串操作兼容层 |

## 配置选项

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| match_threshold | float | 0.7 | 能力匹配阈值 |
| load_balance | bool | true | 是否启用负载均衡 |
| max_plan_depth | int | 10 | 计划最大深度 |
| cache_ttl | int | 300 | 缓存 TTL（秒） |

---

© 2026 SPHARX Ltd. All Rights Reserved.
