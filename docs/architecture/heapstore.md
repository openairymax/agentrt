Copyright (c) 2026 SPHARX. All Rights Reserved.
"From data intelligence emerges."

# AgentOS HeapStore 数据分区系统

**版本**: 1.0.0
**最后更新**: 2026-04-06
**状态**: 生产就绪
**核心文档**: [heapstore.h](../../agentos/heapstore/include/heapstore.h)
**源码位置**: [agentos/heapstore/](../../agentos/heapstore/)

---

## 📋 概述

HeapStore 是 AgentOS 的**统一数据分区管理系统**，负责管理所有组件的数据存储、日志、追踪和 IPC 数据。基于**熔断器模式 (Circuit Breaker)** 和**快速路径/慢速路径双轨写入**设计。

### 核心设计原则

| 原则 | 描述 | 实现方式 |
|------|------|----------|
| **数据隔离** | 不同类型数据独立存储 | 分区路径枚举 (PATH_KERNEL, PATH_LOGS, PATH_TRACES 等) |
| **高可用性** | 单点故障不影响全局 | 熔断器 + 自动降级 |
| **高性能** | 高频日志不阻塞主流程 | 快速路径（无锁异步）+ 慢速路径（完整校验） |
| **数据生命周期** | 自动清理过期数据 | 配置驱动的保留策略 + 定时清理 |

---

## 🏗️ 架构设计

```
┌─────────────────────────────────────────────────────────────┐
│                    HeapStore Core API                       │
│   init / shutdown / stats / flush / health_check           │
├─────────────────────────────────────────────────────────────┤
│                  双轨写入引擎                               │
│   ┌─────────────────┐    ┌─────────────────┐              │
│   │  Fast Path      │    │  Slow Path       │              │
│   │  (无锁异步)     │    │  (同步完整检查)  │              │
│   │  · heapstore_   │    │  · heapstore_    │              │
│   │    log_write_   │    │    log_write_    │              │
│   │    fast()       │    │    slow()        │              │
│   └────────┬────────┘    └────────┬────────┘              │
│            └──────────┬─────────┘                         │
│                       ↓                                   │
│            ┌─────────────────────┐                        │
│            │   Circuit Breaker    │                        │
│            │  CLOSED → OPEN →     │                        │
│            │  HALF_OPEN → CLOSED  │                        │
│            └──────────┬──────────┘                        │
│                       ↓                                   │
│  ┌──────────────────────────────────────────────────┐     │
│  │              Data Partitions                      │     │
│  │  kernel/  logs/  registry/  traces/  ipc/  memory/ │     │
│  └──────────────────────────────────────────────────┘     │
└─────────────────────────────────────────────────────────────┘
```

---

## 📂 数据分区结构

### 路径类型定义（来自源码 heapstore.h）

```c
typedef enum {
    heapstore_PATH_KERNEL,         // 内核数据路径
    heapstore_PATH_LOGS,           // 日志文件路径
    heapstore_PATH_REGISTRY,       // 注册表路径
    heapstore_PATH_SERVICES,       // 服务数据路径
    heapstore_PATH_TRACES,         // 追踪数据路径
    heapstore_PATH_KERNEL_IPC,     // 内核 IPC 数据
    heapstore_PATH_KERNEL_MEMORY,  // 内核内存数据
    heapstore_PATH_MAX
} heapstore_path_type_t;
```

### 典型目录布局

```
/var/lib/agentos/data/
├── kernel/                   # 内核运行时数据
│   ├── config.yaml          # 运行时配置快照
│   ├── state.json           # 内部状态机状态
│   └── metrics.json         # 性能指标快照
├── logs/                     # 结构化日志文件
│   ├── kernel/
│   │   ├── 2026-04-06.log
│   │   └── 2026-04-06.log.1 (轮转后)
│   ├── llm_d/
│   ├── market_d/
│   └── ...
├── registry/                 # 服务注册表
│   ├── agents.json          # Agent 实例注册信息
│   ├── skills.json          # 技能元数据
│   └── sessions.json        # 活跃会话列表
├── traces/                   # 分布式追踪数据
│   ├── spans/               # OpenTelemetry Span 数据
│   └── traces/              # Trace 关联数据
├── ipc/                      # IPC 通信缓存
│   ├── channels/            # IPC 通道元数据
│   └── buffers/             # 共享内存缓冲区映射
└── memory/                   # 记忆系统持久化数据
    ├── l1_raw/              # L1 原始卷备份
    ├── l2_index/            # L2 FAISS 索引快照
    ├── l3_graph/            # L3 图数据库导出
    └── l4_patterns/         # L4 模式库归档
```

---

## 🔧 核心 API 参考

### 初始化与清理

```c
#include "heapstore.h"

// 配置结构体
heapstore_config_t config = {
    .root_path = "/var/lib/agentos/data",  // 数据根目录
    .max_log_size_mb = 100,                 // 单个日志文件最大 100MB
    .log_retention_days = 30,                // 日志保留 30 天
    .trace_retention_days = 7,               // 追踪数据保留 7 天
    .enable_auto_cleanup = true,             // 启用自动清理
    .enable_log_rotation = true,             // 启用日志轮转
    .enable_trace_export = true,             // 启用追踪导出
    .db_vacuum_interval_days = 7,             // 数据库 Vacuum 间隔
    .circuit_breaker_threshold = 5,          // 连续失败 5 次触发熔断
    .circuit_breaker_timeout_sec = 30         // 熔断超时 30 秒
};

// 初始化（必须在所有其他 API 之前调用）
heapstore_error_t err = heapstore_init(&config);
if (err != heapstore_SUCCESS) {
    fprintf(stderr, "HeapStore init failed: %s\n", heapstore_strerror(err));
    exit(1);
}

// ... 使用其他 API ...

// 清理资源（程序退出前调用）
heapstore_shutdown();
```

### 双轨日志写入

#### 快速路径（高频场景，无锁异步）

```c
// 适用场景：普通业务日志、调试日志、性能指标
// 特点：无锁、异步写入、高吞吐量
// 风险：极端情况下可能丢失最后几条日志

heapstore_error_t err = heapstore_log_write_fast(
    "llm_d",                          // 服务名称
    LOG_LEVEL_INFO,                   // 日志级别
    "Processing chat completion request"  // 日志消息
);

if (err == heapstore_ERR_CIRCUIT_OPEN) {
    // 熔断器已打开，降级处理（如写入本地缓冲）
    fallback_log_to_local_buffer("llm_d", msg);
}
```

#### 慢速路径（关键场景，同步完整检查）

```c
// 适用场景：审计日志、错误日志、安全事件
// 特点：同步写入、完整参数验证、支持超时和追踪 ID
// 保证：数据完整性，但吞吐量较低

heapstore_error_t err = heapstore_log_write_slow(
    "cupolas",                        // 服务名称
    LOG_LEVEL_ERROR,                   // 日志级别
    "Permission denied for agent-001", // 日志消息
    "trace-abc123",                    // 追踪 ID（可为 NULL）
    5000                               // 超时时间（毫秒）
);
```

### 批量写入（高性能批量操作）

```c
// 创建批量上下文（默认容量 100）
heapstore_batch_context_t* ctx = heapstore_batch_begin(200);  // 自定义容量 200

if (!ctx) {
    fprintf(stderr, "Failed to create batch context\n");
    return -1;
}

// 添加多种类型的数据到批量缓冲区
heapstore_batch_add_log(ctx, "kernel", LOG_LEVEL_INFO, "System started");
heapstore_batch_add_log_with_trace(ctx, "llm_d", LOG_LEVEL_DEBUG, "trace-001", "Model loaded");
heapstore_batch_add_trace(ctx, "trace-001", "span-001", "", "LLM Inference", start_us, end_us, 0, "{}");

// 提交批量写入（原子操作）
err = heapstore_batch_commit(ctx);
if (err != heapstore_SUCCESS) {
    // 回滚（释放缓冲区但不写入）
    heapstore_batch_rollback(ctx);
    printf("Batch commit failed: %s\n", heapstore_strerror(err));
}

// 销毁上下文
heapstore_batch_context_destroy(ctx);
```

### 统计与监控

```c
// 获取磁盘使用统计
heapstore_stats_t stats;
err = heapstore_get_stats(&stats);
if (err == heapstore_SUCCESS) {
    printf("Total disk usage: %lu bytes (%.2f GB)\n",
           stats.total_disk_usage_bytes,
           stats.total_disk_usage_bytes / (1024.0 * 1024 * 1024));
    printf("Log files: %u, Trace files: %u\n",
           stats.log_file_count, stats.trace_file_count);
}

// 获取性能指标
heapstore_metrics_t metrics;
err = heapstore_get_metrics(&metrics);
if (err == heapstore_SUCCESS) {
    printf("Total ops: %lu, Failed: %.2f%%\n",
           metrics.total_operations,
           (double)metrics.failed_operations / metrics.total_operations * 100);
    printf("Avg latency: %.3f ms\n", metrics.avg_operation_time_ns / 1e6);
    printf("Fast path ratio: %.1f%%\n",
           (double)metrics.fast_path_operations / metrics.total_operations * 100);
}

// 获取熔断器状态
heapstore_circuit_info_t circuit;
err = heapstore_get_circuit_state(&circuit);
if (err == heapstore_SUCCESS) {
    const char* state_str[] = {"CLOSED", "OPEN", "HALF_OPEN"};
    printf("Circuit state: %s, Failures: %u/%u\n",
           state_str[circuit.state],
           circuit.failure_count,
           circuit.threshold);
}
```

### 健康检查

```c
bool registry_ok, trace_ok, log_ok, ipc_ok, memory_ok;

err = heapstore_health_check(
    &registry_ok,   // 注册表系统健康状态
    &trace_ok,      // 追踪系统健康状态
    &log_ok,        // 日志系统健康状态
    &ipc_ok,        // IPC 系统健康状态
    &memory_ok      // 内存系统健康状态
);

if (err == heapstore_SUCCESS) {
    if (registry_ok && log_ok && ipc_ok) {
        printf("All critical systems healthy\n");
    } else {
        printf("Warning: Some subsystems unhealthy\n");
        printf("  Registry: %s, Log: %s, IPC: %s\n",
               registry_ok ? "OK" : "FAIL",
               log_ok ? "OK" : "FAIL",
               ipc_ok ? "OK" : "FAIL");
    }
}
```

---

## ⚡ 熔断器机制详解

### 状态转换

```
                    连续失败 >= threshold
    ┌─────────┐                           ┌───────┐
    │ CLOSED  │ ─────────────────────────> │  OPEN │
    │(正常工作)│                           │(熔断中)│
    └────┬────┘                           └───┬───┘
         │                                     │
         │ 成功请求                              │ timeout 过期
         │                                     │
         ▼                                     ▼
    ┌─────────┐                           ┌───────────┐
    │ CLOSED  │ <──────────────────────── │ HALF_OPEN │
    └─────────┘    探测成功                │(半开探测)│
                                          └─────┬─────┘
                                                │
                                                │ 探测失败
                                                ▼
                                          ┌───────┐
                                          │  OPEN │
                                          └───────┘
```

### 配置参数说明

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `circuit_breaker_threshold` | uint32_t | 5 | 触发熔断的连续失败次数 |
| `circuit_breaker_timeout_sec` | uint32_t | 30 | OPEN→HALF_OPEN 的等待时间（秒） |

### 手动重置（故障恢复后）

```c
// 问题修复后手动重置熔断器
err = heapstore_reset_circuit();
if (err == heapstore_SUCCESS) {
    printf("Circuit breaker reset to CLOSED\n");
}
```

---

## 🔨 Python 绑定示例

虽然 HeapStore 核心是 C 实现，但 Python SDK 通过 Syscall 层间接调用：

```python
from agentos.client import Client
from agentos.modules.memory import MemoryManager

client = Client(endpoint="http://localhost:18789")

# 写入记忆（内部通过 HeapStore L1 分区存储）
memory_mgr = MemoryManager(client)
record_id = memory_mgr.store(
    data="Important context information",
    metadata={"source": "user_input", "priority": "high"},
    tags=["context", "important"]
)

print(f"Stored to HeapStore L1 partition: {record_id}")

# 查询记忆（内部通过 HeapStore L2/L3 分区检索）
results = memory_mgr.search(
    query="context information",
    limit=10,
    filters={"tags": ["important"]}
)

for record in results:
    print(f"ID: {record.id}, Score: {record.score:.3f}")
```

---

## 📊 性能优化建议

### 1. 合理选择写入路径

| 场景 | 推荐路径 | 吞吐量 | 可靠性 |
|------|----------|--------|--------|
| 业务日志、Debug 信息 | Fast Path | >100K/s | 最终一致性 |
| 审计日志、安全事件 | Slow Path | ~10K/s | 强一致性 |
| 批量导入、初始化 | Batch Write | >50K/s | 原子性 |

### 2. 调整熔断器参数

```yaml
# 生产环境推荐配置
heapstore:
  circuit_breaker_threshold: 10    # 适当提高阈值避免误触发
  circuit_breaker_timeout_sec: 60  # 给予足够恢复时间
  max_log_size_mb: 256             # 减少文件 I/O 开销
  enable_auto_cleanup: true         # 自动清理防止磁盘满
  db_vacuum_interval_days: 3       # 更频繁的 Vacuum 保持查询性能
```

### 3. 监控关键指标

```bash
# Prometheus 指标（需集成）
agentos_heapstore_total_operations_total
agentos_heapstore_failed_operations_total
agentos_heapstore_circuit_state{state="closed|open|half_open"}
agentos_heapstore_operation_duration_seconds_bucket
agentos_heapstore_disk_usage_bytes
```

---

## 🧪 测试与验证

### C 单元测试

```bash
cd AgentOS/agentos/heapstore
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make heapstore_test
./test_heapstore
```

### 集成测试

```python
# tests/test_heapstore_integration.py
import subprocess
import time

def test_heapstore_lifecycle():
    """测试 HeapStore 完整生命周期"""
    # 通过 SDK 触发内部 HeapStore 操作
    client = Client(endpoint="http://localhost:18789")

    # 写入大量数据触发快速路径
    for i in range(1000):
        client.post("/api/v1/memory/store", body={"data": f"test-{i}"})

    # 验证统计信息
    metrics = client.metrics()
    assert metrics.memories_total >= 1000

    # 触发健康检查
    health = client.health()
    assert health.status == "healthy"
    assert "memory" in health.checks  # HeapStore 子系统检查
```

---

## 📚 相关文档

- **[微内核架构](architecture/microkernel.md)** — 内核层与 HeapStore 的交互
- **[四层记忆系统](architecture/memoryrovol.md)** — L1-L4 分区详细设计
- **[守护进程 API](daemon-api.md)** — Daemon 如何使用 HeapStore 存储数据
- **[备份恢复](operations/backup-recovery.md)** — HeapStore 数据的备份策略

---

## 🔗 源码索引

| 组件 | 文件路径 | 功能描述 |
|------|----------|----------|
| **核心接口** | [heapstore.h](../../agentos/heapstore/include/heapstore.h) | 主 API 定义 |
| **注册表模块** | [heapstore_registry.h](../../agentos/heapstore/include/heapstore_registry.h) | Agent/Skill/Session 注册 |
| **追踪模块** | [heapstore_trace.h](../../agentos/heapstore/include/heapstore_trace.h) | OpenTelemetry 集成 |
| **IPC 存储** | [heapstore_ipc.h](../../agentos/heapstore/include/heapstore_ipc.h) | IPC 通道和缓冲区管理 |
| **内存存储** | [heapstore_memory.h](../../agentos/heapstore/include/heapstore_memory.h) | 记忆系统持久化 |
| **核心实现** | [heapstore_core.c](../../agentos/heapstore/src/heapstore_core.c) | 初始化、配置、统计 |

---

**© 2026 SPHARX Ltd. All Rights Reserved.**

*"From data intelligence emerges."*
