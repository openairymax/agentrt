# TaskFlow — 任务流引擎

`agentos/atoms/taskflow/`

**版本**: v0.0.5

---

## 概述

TaskFlow 是 AgentOS 的**任务流引擎**，基于 Pregel 超步模型提供大规模图计算的分布式执行框架。它将复杂工作流建模为有向无环图（DAG），支持任务的编排、调度、执行和监控。TaskFlow 不仅提供传统的 DAG 依赖管理，还实现了 Pregel 超步迭代计算模型、分布式图分区与消息传递、检查点容错和高性能批处理与流水线。

TaskFlow 以 C11 标准实现，与 CoreLoopThree 的执行循环紧密配合，将执行循环中的行动分解为可管理的任务单元，并按照依赖关系有序执行。引擎支持同步/异步双模式执行、消息广播与点对点传递、增量检查点和故障恢复。

---

## 设计理念

| 原则 | 说明 |
|------|------|
| **Pregel 超步模型** | 基于 BSP（Bulk Synchronous Parallel）的迭代计算模型，每个超步包含计算-通信-同步三个阶段 |
| **DAG 编排** | 任务间依赖关系通过有向无环图定义，自动进行拓扑排序 |
| **分布式分区** | 图可按哈希/范围/自定义策略分区，支持分布式执行 |
| **容错与检查点** | 定期保存执行状态快照，故障后可从最近检查点恢复 |
| **高性能** | 消息合并、边缓存、批处理等优化策略 |
| **可观测性** | 每个任务和超步的生命周期事件均可追踪和审计 |

---

## 目录结构

```
taskflow/
├── include/
│   ├── taskflow.h                 # 主头文件（引擎/图/分区/执行/消息/检查点 API）
│   ├── taskflow_types.h           # 核心类型定义（顶点/边/消息/分区/检查点/配置）
│   ├── taskflow_advanced.h        # 高级功能接口
│   ├── taskflow_integration.h     # 集成接口
│   ├── graph_engine.h             # 图引擎接口
│   ├── pregel_engine.h            # Pregel 引擎接口
│   ├── workflow_patterns.h        # 工作流模式接口
│   └── taskflow_version.h.in      # 版本信息模板
├── src/
│   ├── taskflow_core.c            # 核心引擎实现
│   ├── taskflow_advanced.c        # 高级功能实现
│   ├── taskflow_execution_unit.c  # 执行单元实现
│   ├── graph_engine.c             # 图引擎实现
│   ├── pregel_engine.c            # Pregel 超步引擎实现
│   └── workflow_patterns.c        # 工作流模式实现
├── tests/
│   ├── unit/
│   │   └── test_taskflow_advanced.c  # 高级功能测试
│   └── CMakeLists.txt
└── CMakeLists.txt
```

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
│  │           Pregel Superstep Engine                │ │
│  │  Compute → Message → Synchronize → Next Step    │ │
│  └─────────────────────────────────────────────────┘ │
│                                                       │
│  ┌─────────────────────────────────────────────────┐ │
│  │           Checkpoint & Fault Tolerance           │ │
│  │  Full / Incremental Snapshot → Restore           │ │
│  └─────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────┘
```

---

## 核心类型定义

### 基础类型

| 类型 | 定义 | 说明 |
|------|------|------|
| `vertex_id_t` | `uint64_t` | 顶点唯一 ID |
| `edge_id_t` | `uint64_t` | 边唯一 ID |
| `partition_id_t` | `uint32_t` | 分区 ID |
| `superstep_t` | `uint32_t` | 超步 ID |
| `task_id_t` | `uint64_t` | 任务 ID |
| `message_id_t` | `uint64_t` | 消息 ID |
| `taskflow_error_t` | `int32_t` | 错误码 |

### 顶点状态

| 状态 | 说明 |
|------|------|
| `VERTEX_ACTIVE` | 活跃状态，参与计算 |
| `VERTEX_INACTIVE` | 非活跃状态，跳过计算 |
| `VERTEX_HALTED` | 停止状态，计算完成 |
| `VERTEX_FAULTED` | 故障状态，需要恢复 |
| `VERTEX_SUSPENDED` | 挂起状态，等待条件 |

### 分区策略

| 策略 | 说明 |
|------|------|
| `PARTITION_HASH` | 哈希分区 |
| `PARTITION_RANGE` | 范围分区 |
| `PARTITION_CUSTOM` | 自定义分区策略 |

---

## 接口说明

### 引擎管理

| 接口 | 功能 |
|------|------|
| `taskflow_engine_create()` | 创建引擎实例（传入配置） |
| `taskflow_engine_destroy()` | 销毁引擎实例 |
| `taskflow_engine_init()` | 初始化引擎 |
| `taskflow_engine_start()` | 启动引擎 |
| `taskflow_engine_stop()` | 停止引擎 |
| `taskflow_engine_pause()` | 暂停引擎 |
| `taskflow_engine_resume()` | 恢复引擎 |

### 图管理

| 接口 | 功能 |
|------|------|
| `taskflow_graph_create()` | 创建空图 |
| `taskflow_graph_destroy()` | 销毁图 |
| `taskflow_graph_add_vertex()` | 添加顶点 |
| `taskflow_graph_remove_vertex()` | 移除顶点 |
| `taskflow_graph_add_edge()` | 添加边 |
| `taskflow_graph_remove_edge()` | 移除边 |
| `taskflow_graph_get_vertex_count()` | 获取顶点数量 |
| `taskflow_graph_get_edge_count()` | 获取边数量 |

### 分区管理

| 接口 | 功能 |
|------|------|
| `taskflow_graph_partition()` | 对图进行分区（指定策略和数量） |
| `taskflow_graph_get_partitions()` | 获取分区列表 |

### 执行控制

| 接口 | 功能 |
|------|------|
| `taskflow_execute_sync()` | 同步执行图计算（指定最大超步数） |
| `taskflow_execute_async()` | 异步执行图计算（带完成回调） |
| `taskflow_execute_cancel()` | 取消正在执行的计算 |
| `taskflow_execute_wait()` | 等待计算完成（带超时） |

### 消息传递

| 接口 | 功能 |
|------|------|
| `taskflow_send_message()` | 发送消息到指定顶点 |
| `taskflow_broadcast_message()` | 广播消息到所有顶点 |
| `taskflow_get_incoming_messages()` | 获取顶点的入站消息 |
| `taskflow_clear_messages()` | 清空顶点的消息队列 |

### 检查点与容错

| 接口 | 功能 |
|------|------|
| `taskflow_create_checkpoint()` | 创建检查点（返回检查点 ID） |
| `taskflow_restore_checkpoint()` | 恢复检查点 |
| `taskflow_delete_checkpoint()` | 删除检查点 |
| `taskflow_list_checkpoints()` | 获取可用检查点列表 |

### 统计与监控

| 接口 | 功能 |
|------|------|
| `taskflow_get_stats()` | 获取执行统计信息 |
| `taskflow_reset_stats()` | 重置统计信息 |
| `taskflow_get_current_superstep()` | 获取当前超步 |
| `taskflow_get_active_vertex_count()` | 获取活跃顶点数量 |
| `taskflow_get_queued_message_count()` | 获取队列中的消息数量 |

### 工具函数

| 接口 | 功能 |
|------|------|
| `taskflow_error_to_string()` | 错误码转描述字符串 |
| `taskflow_get_version()` | 获取版本信息 |
| `taskflow_set_log_callback()` | 设置日志回调 |

---

## 引擎配置

```c
typedef struct {
    size_t max_vertices;                     // 最大顶点数
    size_t max_edges;                        // 最大边数
    size_t max_messages;                     // 最大消息数
    size_t worker_threads;                   // 工作线程数
    size_t partition_count;                  // 分区数量
    partition_strategy_t partition_strategy; // 分区策略

    size_t max_supersteps;                   // 最大超步数
    uint32_t superstep_timeout_ms;           // 超步超时时间

    size_t checkpoint_interval;              // 检查点间隔（超步数）
    bool enable_incremental_checkpoint;      // 增量检查点

    size_t message_buffer_size;              // 消息缓冲区大小
    size_t vertex_buffer_size;               // 顶点缓冲区大小
    size_t edge_buffer_size;                 // 边缓冲区大小

    bool enable_fault_tolerance;             // 容错开关
    size_t max_failures;                     // 最大故障数

    bool enable_message_combining;           // 消息合并
    bool enable_edge_caching;                // 边缓存
    size_t batch_size;                       // 批处理大小

    vertex_compute_func_t compute_func;      // 顶点计算函数
    message_send_func_t send_func;           // 消息发送函数
    aggregator_func_t aggregator_func;       // 聚合器函数
    void *user_context;                      // 用户上下文
} taskflow_config_t;
```

---

## 执行统计

```c
typedef struct {
    size_t total_vertices;                // 总顶点数
    size_t total_edges;                   // 总边数
    size_t total_messages;                // 总消息数
    size_t active_supersteps;             // 活跃超步数
    size_t completed_supersteps;          // 已完成超步数
    size_t checkpoints_taken;             // 已创建的检查点数
    size_t failures_recovered;            // 已恢复的故障数
    uint64_t total_compute_time_ms;       // 总计算时间
    uint64_t total_communication_time_ms; // 总通信时间
    size_t peak_memory_usage;             // 峰值内存使用量
} execution_stats_t;
```

---

## 错误码

| 错误码 | 值 | 说明 |
|--------|-----|------|
| `TASKFLOW_SUCCESS` | 0 | 成功 |
| `TASKFLOW_ERROR_INVALID_ARG` | 1 | 无效参数 |
| `TASKFLOW_ERROR_MEMORY` | 2 | 内存不足 |
| `TASKFLOW_ERROR_NOT_INITIALIZED` | 3 | 未初始化 |
| `TASKFLOW_ERROR_ALREADY_INITIALIZED` | 4 | 已初始化 |
| `TASKFLOW_ERROR_GRAPH_TOO_LARGE` | 5 | 图过大 |
| `TASKFLOW_ERROR_PARTITION` | 6 | 分区错误 |
| `TASKFLOW_ERROR_CHECKPOINT` | 7 | 检查点错误 |
| `TASKFLOW_ERROR_TIMEOUT` | 8 | 超时 |
| `TASKFLOW_ERROR_FAULT_DETECTED` | 9 | 检测到故障 |
| `TASKFLOW_ERROR_COMMUNICATION` | 10 | 通信错误 |
| `TASKFLOW_ERROR_INTERNAL` | 11 | 内部错误 |
| `TASKFLOW_ERROR_NO_ACTIVE_VERTICES` | 12 | 无活跃顶点（计算完成） |
| `TASKFLOW_ERROR_ALREADY_RUNNING` | 13 | 引擎已在运行 |
| `TASKFLOW_ERROR_INIT_FAILED` | 14 | 初始化失败 |

---

## Pregel 超步模型

TaskFlow 采用 Pregel 超步迭代计算模型，每个超步包含三个阶段：

```
┌─────────────────────────────────────────────┐
│              Superstep N                      │
│                                               │
│  1. Compute: 每个活跃顶点执行计算函数        │
│     - 处理入站消息                           │
│     - 更新顶点值                             │
│     - 发送出站消息                           │
│     - 可选择投票停止（halt）                  │
│                                               │
│  2. Message: 消息传递阶段                    │
│     - 点对点消息传递                         │
│     - 消息合并（可选）                       │
│                                               │
│  3. Synchronize: 全局同步屏障                │
│     - 等待所有分区完成                       │
│     - 检查是否还有活跃顶点                   │
│     - 无活跃顶点则计算完成                   │
└─────────────────────────────────────────────┘
```

---

## 依赖关系

| 依赖项 | 来源 | 用途 |
|--------|------|------|
| CoreKern | atoms/corekern | 微内核调度能力 |
| Syscall | atoms/syscall | 系统调用接口 |
| commons | agentos/commons | 统一类型和平台抽象 |

---

## 构建说明

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

cmake -B build -DBUILD_TESTS=ON
ctest --test-dir build -R taskflow
```

---

## 使用示例

```c
#include "taskflow.h"

int main(void) {
    taskflow_config_t config = {0};
    config.max_vertices = 10000;
    config.max_edges = 50000;
    config.worker_threads = 4;
    config.partition_count = 2;
    config.max_supersteps = 100;
    config.enable_fault_tolerance = true;
    config.checkpoint_interval = 10;

    taskflow_handle_t engine = taskflow_engine_create(&config);
    taskflow_engine_init(engine);
    taskflow_engine_start(engine);

    taskflow_graph_handle_t graph = taskflow_graph_create(engine);

    graph_vertex_t v1 = {0};
    v1.id = 1;
    v1.state = VERTEX_ACTIVE;
    taskflow_graph_add_vertex(graph, &v1);

    graph_vertex_t v2 = {0};
    v2.id = 2;
    v2.state = VERTEX_ACTIVE;
    taskflow_graph_add_vertex(graph, &v2);

    graph_edge_t edge = {0};
    edge.id = 1;
    edge.source = 1;
    edge.target = 2;
    taskflow_graph_add_edge(graph, &edge);

    taskflow_error_t err = taskflow_execute_sync(engine, graph, 100);

    execution_stats_t stats;
    taskflow_get_stats(engine, &stats);

    taskflow_graph_destroy(graph);
    taskflow_engine_stop(engine);
    taskflow_engine_destroy(engine);
    return 0;
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
