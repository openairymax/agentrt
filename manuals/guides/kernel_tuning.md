Copyright (c) 2026 SPHARX. All Rights Reserved.
"From data intelligence emerges."

# AgentOS 内核调优指南

**版本**: Doc V1.7  
**最后更新**: 2026-03-31  
**难度**: ⭐⭐⭐ 高级  
**预计时间**: 40 分钟  
**作者**: LirenWang

---

## 1. 概述

AgentOS 内核由多个精密协作的子系统组成，每个子系统都提供了丰富的可调参数。本指南基于工程控制论的反馈调优方法论，帮助你根据工作负载特征对内核进行系统性调优。

### 1.1 调优方法论

AgentOS 内核调优遵循**测量→分析→调优→验证**的反馈闭环：

```
    ┌──────────┐
    │  测量     │ 基线性能指标采集
    └────┬─────┘
         │
    ┌────▼─────┐
    │  分析     │ 识别瓶颈和优化点
    └────┬─────┘
         │
    ┌────▼─────┐
    │  调优     │ 调整内核参数
    └────┬─────┘
         │
    ┌────▼─────┐
    │  验证     │ 对比调优前后指标
    └────┬─────┘
         │
         └──────► 回到「测量」（反馈闭环）
```

### 1.2 调优维度

| 维度 | 子系统 | 关键指标 |
|------|--------|---------|
| **吞吐量** | corekern/task, corekern/ipc | 任务完成率、IPC 消息/秒 |
| **延迟** | corekern/ipc, coreloopthree | P50/P95/P99 响应时间 |
| **内存** | corekern/mem, memoryrovol | 内存使用量、分配成功率 |
| **可靠性** | cupolas, coreloopthree | 任务成功率、故障恢复时间 |
| **可观测性** | telemetry | 日志开销、指标精度 |

---

## 2. 基线测量

### 2.1 性能基线工具

```bash
# 运行标准性能基线测试
agentos-cli benchmark baseline --duration 60

# 输出示例
┌─────────────────────────────────────────────┐
│           AgentOS Performance Baseline       │
├──────────────────┬──────────┬───────────────┤
│ Metric           │ Value    │ Unit          │
├──────────────────┼──────────┼───────────────┤
│ tasks_total      │ 12,456   │ ops           │
│ tasks_success    │ 12,401   │ ops           │
│ tasks_failed     │ 55       │ ops           │
│ success_rate     │ 99.56%   │               │
│ latency_p50      │ 2.3      │ ms            │
│ latency_p95      │ 8.7      │ ms            │
│ latency_p99      │ 23.1     │ ms            │
│ ipc_throughput   │ 45,678   │ msgs/sec      │
│ ipc_latency_p99  │ 0.012    │ ms            │
│ memory_usage     │ 256      │ MB            │
│ memory_alloc_ps  │ 89,234   │ allocs/sec    │
│ cpu_usage        │ 67.3%    │               │
└──────────────────┴──────────┴───────────────┘
```

### 2.2 热点分析

```bash
# 生成火焰图
agentos-cli profile flamegraph --duration 30 --output flamegraph.svg

# 生成调用图
agentos-cli profile callgraph --output callgraph.dot

# 内存分配分析
agentos-cli profile memory --duration 30
```

---

## 3. corekern 调优

### 3.1 IPC 调优

IPC（进程间通信）是 AgentOS 内核性能的关键路径。

```yaml
# agentos.yaml → corekern.ipc
corekern:
  ipc:
    # IPC 通道最大数量
    # 增大：支持更多并发连接
    # 减小：降低内存占用
    max_channels: 4096            # 默认 2048，推荐 4096+

    # 单条消息最大字节数
    # 增大：支持大数据传输
    # 减小：降低延迟，减少内存碎片
    max_message_size: 1048576     # 默认 512KB，推荐 1MB

    # IPC 超时时间
    # 增大：容忍慢消费者
    # 减小：快速失败，避免阻塞
    timeout_ms: 5000              # 默认 3000ms

    # 消息批处理大小
    # 增大：提高吞吐量
    # 减小：降低延迟
    batch_size: 32                # 默认 16

    # 零拷贝传输阈值
    # 大于此值的消息使用零拷贝
    zero_copy_threshold: 4096     # 默认 8192
```

**调优场景**:

| 场景 | max_channels | batch_size | zero_copy_threshold |
|------|-------------|------------|---------------------|
| **高并发低延迟** | 8192 | 8 | 2048 |
| **高吞吐批量** | 4096 | 64 | 16384 |
| **低资源嵌入式** | 256 | 4 | 1024 |

### 3.2 任务调度调优

```yaml
corekern:
  task:
    # 最大并发任务数
    max_concurrent: 256           # 默认 128

    # 调度策略
    # priority_fifo: 优先级 FIFO
    # priority_fair: 优先级公平调度
    # round_robin: 轮询
    scheduler: "priority_fifo"

    # 任务队列大小
    queue_size: 4096              # 默认 1024

    # 任务超时（秒）
    default_timeout: 300          # 默认 120

    # 任务窃取阈值
    # 当某工作线程空闲超过此阈值时，尝试窃取任务
    steal_threshold_ms: 10        # 默认 50
```

### 3.3 内存管理调优

```yaml
corekern:
  memory:
    # 内存池大小（MB）
    # 每个内存池对应一个大小等级
    pool_size_mb: 256             # 默认 128

    # 最大同时分配数
    max_allocations: 100000       # 默认 50000

    # 内存分配策略
    # pool: 内存池分配（快速，固定大小）
    # arena: 区域分配（灵活，稍慢）
    # system: 直接调用系统 malloc
    allocator: "pool"

    # 内存碎片整理间隔（秒）
    # 0 表示禁用
    defrag_interval: 3600         # 默认 0

    # OOM 保护阈值（内存使用百分比）
    oom_threshold: 90             # 默认 95
```

---

## 4. coreloopthree 调优

### 4.1 认知层调优

认知层负责任务理解和深度规划，是 System 2 的核心。

```yaml
coreloopthree:
  cognitive:
    # 主模型 — 用于深度规划
    model_primary: "default"

    # 辅模型 — 用于快速分类（System 1）
    model_auxiliary: "fast"

    # 最大重试次数
    max_retries: 3                # 默认 5

    # 单次认知超时
    timeout_ms: 30000             # 默认 60000

    # 上下文窗口大小
    context_window: 4096          # 默认 2048

    # 并行认知请求数
    max_parallel: 4               # 默认 2

    # 缓存命中重用
    cache_enabled: true           # 默认 true
    cache_ttl_seconds: 600        # 默认 300
```

### 4.2 规划层调优

规划层将任务分解为可执行的 DAG。

```yaml
coreloopthree:
  planning:
    # 规划策略
    # incremental: 增量规划（轮次内反馈）
    # full: 全量规划
    # adaptive: 自适应（根据任务复杂度选择）
    strategy: "incremental"       # 推荐 adaptive

    # 最大规划阶段数
    max_stages: 64                # 默认 32

    # 重规划间隔
    replan_interval_ms: 5000      # 默认 10000

    # 并行度 — 同时执行的最大阶段数
    parallelism: 4                # 默认 2

    # 补偿事务启用
    compensation_enabled: true    # 默认 true
```

### 4.3 执行层调优

```yaml
coreloopthree:
  execution:
    # 执行策略
    # parallel: 并行执行
    # sequential: 顺序执行
    # adaptive: 自适应
    strategy: "parallel"

    # 工作线程数
    # 0 = 自动检测（CPU 核数）
    max_workers: 8                # 默认 0

    # 执行超时（毫秒）
    stage_timeout_ms: 30000       # 默认 60000

    # 结果缓存
    result_cache_size: 1024       # 默认 256
```

---

## 5. memoryrovol 调优

### 5.1 L1 原始卷调优

L1 存储原始输入输出数据，访问最频繁。

```yaml
memoryrovol:
  layers:
    L1:
      enabled: true
      backend: "mmap"             # mmap / malloc

      # 最大容量
      max_size_mb: 512            # 默认 256

      # TTL — 自动过期时间
      ttl_seconds: 3600           # 默认 1800

      # 淘汰策略
      # lru: 最近最少使用
      # lfu: 最不经常使用
      # fifo: 先进先出
      eviction: "lru"

      # 预分配比例
      preallocate: 0.8            # 默认 0.5
```

### 5.2 L2 特征层调优

L2 存储提取的特征和索引，支持快速检索。

```yaml
memoryrovol:
  layers:
    L2:
      enabled: true
      backend: "lmdb"             # lmdb / rocksdb

      max_size_mb: 2048           # 默认 1024

      # 向量索引类型
      # flat: 暴力搜索（小数据集）
      # hnsw: 层次导航小世界图（大数据集）
      # ivf: 倒排文件（中等数据集）
      index_type: "hnsw"

      # HNSW 参数
      hnsw_m: 16                  # 连接数，默认 16
      hnsw_ef_construction: 200   # 构建时搜索宽度，默认 200
      hnsw_ef_search: 50          # 查询时搜索宽度，默认 50

      # 写入批处理
      write_batch_size: 100       # 默认 50
      write_flush_interval_ms: 100 # 默认 500
```

### 5.3 L3 结构层调优

L3 存储结构化的知识和关系。

```yaml
memoryrovol:
  layers:
    L3:
      enabled: true
      backend: "sqlite"           # sqlite / postgresql

      max_size_mb: 10240          # 默认 5120

      # 连接池大小
      pool_size: 10               # 默认 5

      # WAL 模式
      wal_enabled: true           # 默认 true

      # 同步模式
      # full: 完全同步（最安全）
      # normal: 正常
      # off: 关闭（最快）
      sync_mode: "normal"

      # 缓存页数
      cache_pages: 2000           # 默认 1000
```

### 5.4 L4 模式层调优

L4 是最高层，存储持久化的行为模式。

```yaml
memoryrovol:
  layers:
    L4:
      enabled: true
      backend: "custom"           # custom / sqlite

      # 持久化
      persistence: true

      # 模式挖掘间隔
      mining_interval_seconds: 3600  # 默认 7200

      # 最小支持度（模式出现频率阈值）
      min_support: 0.05           # 默认 0.1

      # 最大模式数
      max_patterns: 10000         # 默认 5000

      # 遗忘策略
      # exponential: 指数衰减
      # linear: 线性衰减
      # none: 不遗忘
      forgetting: "exponential"

      # 遗忘半衰期（秒）
      forgetting_half_life: 2592000  # 默认 30 天
```

---

## 6. cupolas 安全穹顶调优

### 6.1 虚拟工位调优

```yaml
cupolas:
  workbench:
    enabled: true

    # 隔离级别
    # none: 无隔离（仅开发环境）
    # process: 进程级隔离
    # namespace: 命名空间隔离
    # container: 容器级隔离
    isolation_level: "process"

    # 工位池大小
    pool_size: 32                 # 默认 16

    # 工位预热
    prewarm: true                 # 默认 false

    # 工位空闲回收时间（秒）
    idle_timeout: 300             # 默认 600
```

### 6.2 权限引擎调优

```yaml
cupolas:
  permission:
    # 权限引擎
    # rbac: 基于角色的访问控制
    # abac: 基于属性的访问控制
    # hybrid: 混合模式
    engine: "rbac"

    # 权限缓存 TTL
    cache_ttl_seconds: 300        # 默认 60

    # 审计日志缓冲区大小
    audit_buffer_size: 1024       # 默认 256
```

### 6.3 输入净化调优

```yaml
cupolas:
  sanitizer:
    # 净化级别
    # none: 不净化
    # relaxed: 宽松
    # standard: 标准
    # strict: 严格
    level: "strict"

    # 最大输入长度
    max_input_length: 1048576     # 默认 65536

    # 净化规则缓存
    rules_cache_enabled: true     # 默认 true
    rules_cache_size: 256         # 默认 64
```

---

## 7. telemetry 调优

### 7.1 日志调优

```yaml
telemetry:
  logging:
    level: "INFO"                 # DEBUG/INFO/WARN/ERROR

    # 日志格式
    # json: 结构化 JSON（推荐生产环境）
    # text: 纯文本（推荐开发环境）
    format: "json"

    # 异步日志
    async_enabled: true           # 默认 true
    async_queue_size: 8192        # 默认 4096
    async_flush_interval_ms: 100  # 默认 250

    # 日志轮转
    rotation:
      max_size_mb: 100            # 默认 50
      max_files: 10               # 默认 5
      compress: true              # 默认 false
```

### 7.2 指标调优

```yaml
telemetry:
  metrics:
    enabled: true
    interval_seconds: 15          # 默认 30

    # 指标聚合
    aggregation_window_seconds: 60 # 默认 120

    # 直方图桶数
    histogram_buckets: 20         # 默认 10

    # 热路径低开销采样
    sampling_rate: 1.0            # 默认 1.0
```

### 7.3 链路追踪调优

```yaml
telemetry:
  tracing:
    enabled: true

    # 采样策略
    # always: 全量采样
    # probabilistic: 概率采样
    # adaptive: 自适应采样（基于 QPS）
    # rate_limiting: 速率限制采样
    sampler: "adaptive"

    # 采样率（概率采样时有效）
    rate: 0.1                     # 默认 0.01

    # 最大 Span 数
    max_spans_per_trace: 256      # 默认 128

    # Span 存储时长
    span_ttl_seconds: 86400       # 默认 3600
```

---

## 8. 场景化调优配置

### 8.1 开发环境

```yaml
# manager/profiles/development.yaml
corekern:
  ipc:
    max_channels: 512
    max_message_size: 262144
  task:
    max_concurrent: 32
  memory:
    pool_size_mb: 64
    allocator: "system"

coreloopthree:
  cognitive:
    timeout_ms: 120000
  planning:
    strategy: "full"
  execution:
    max_workers: 2

memoryrovol:
  layers:
    L1: { max_size_mb: 64, ttl_seconds: 300 }
    L2: { max_size_mb: 128 }
    L3: { max_size_mb: 256 }
    L4: { enabled: false }

cupolas:
  workbench:
    isolation_level: "none"
  sanitizer:
    level: "relaxed"

telemetry:
  logging:
    level: "DEBUG"
    format: "text"
  metrics:
    interval_seconds: 5
  tracing:
    sampler: "always"
```

### 8.2 生产环境（高吞吐）

```yaml
# manager/profiles/production_high_throughput.yaml
corekern:
  ipc:
    max_channels: 8192
    batch_size: 64
    zero_copy_threshold: 16384
  task:
    max_concurrent: 512
    queue_size: 8192
    scheduler: "priority_fair"
  memory:
    pool_size_mb: 512
    allocator: "pool"

coreloopthree:
  cognitive:
    max_parallel: 8
    cache_enabled: true
  planning:
    strategy: "adaptive"
    parallelism: 8
  execution:
    max_workers: 16

memoryrovol:
  layers:
    L1: { max_size_mb: 1024, eviction: "lfu" }
    L2: { max_size_mb: 4096, write_batch_size: 200 }
    L3: { pool_size: 20, sync_mode: "normal" }
    L4: { mining_interval_seconds: 1800 }

telemetry:
  logging:
    async_queue_size: 16384
  tracing:
    sampler: "adaptive"
    rate: 0.05
```

### 8.3 生产环境（低延迟）

```yaml
# manager/profiles/production_low_latency.yaml
corekern:
  ipc:
    max_channels: 4096
    batch_size: 8
    zero_copy_threshold: 1024
    timeout_ms: 1000
  task:
    max_concurrent: 128
    steal_threshold_ms: 1
  memory:
    pool_size_mb: 128
    allocator: "pool"

coreloopthree:
  cognitive:
    model_auxiliary: "fastest"
    timeout_ms: 5000
  planning:
    strategy: "incremental"
    replan_interval_ms: 1000
  execution:
    max_workers: 4
    stage_timeout_ms: 5000

memoryrovol:
  layers:
    L1: { max_size_mb: 256, preallocate: 1.0 }
    L2: { index_type: "flat", hnsw_ef_search: 20 }

telemetry:
  logging:
    level: "WARN"
  metrics:
    interval_seconds: 5
  tracing:
    sampler: "rate_limiting"
    rate: 0.01
```

### 8.4 嵌入式 / 边缘设备

```yaml
# manager/profiles/embedded.yaml
corekern:
  ipc:
    max_channels: 64
    max_message_size: 65536
    batch_size: 4
  task:
    max_concurrent: 8
    queue_size: 64
  memory:
    pool_size_mb: 16
    allocator: "pool"
    oom_threshold: 80

coreloopthree:
  cognitive:
    max_parallel: 1
    cache_enabled: true
    cache_ttl_seconds: 1800
  planning:
    strategy: "incremental"
    max_stages: 16
    parallelism: 1
  execution:
    max_workers: 1

memoryrovol:
  layers:
    L1: { max_size_mb: 32, backend: "malloc" }
    L2: { max_size_mb: 64, index_type: "flat" }
    L3: { max_size_mb: 128, pool_size: 2, sync_mode: "off" }
    L4: { enabled: false }

cupolas:
  sanitizer:
    max_input_length: 65536
  permission:
    cache_ttl_seconds: 1800

telemetry:
  logging:
    level: "WARN"
    format: "text"
    async_enabled: false
  metrics:
    enabled: false
  tracing:
    enabled: false
```

---

## 9. 运行时调优

### 9.1 热重载配置

```bash
# 验证配置变更
agentos-cli manager validate --profile production_high_throughput

# 预览变更影响
agentos-cli manager diff --current /etc/agentos/agentos.yaml \
                        --new manager/profiles/production_high_throughput.yaml

# 热重载（无需重启）
agentos-cli manager reload --profile production_high_throughput
```

### 9.2 运行时参数调整

```bash
# 动态调整日志级别
agentos-cli manager set telemetry.logging.level DEBUG

# 动态调整并发任务数
agentos-cli manager set corekern.task.max_concurrent 512

# 查看当前运行时参数
agentos-cli manager get corekern.ipc.batch_size
```

---

## 10. 相关文档

- [架构设计原则](../architecture/ARCHITECTURAL_PRINCIPLES.md)
- [部署指南](deployment.md)
- [故障排查](troubleshooting.md)
- [微内核架构](../architecture/folder/microkernel.md)
- [记忆卷载系统](../architecture/folder/memoryrovol.md)

---

**最后更新**: 2026-03-23  
**维护者**: AgentOS 性能工程团队

---

© 2026 SPHARX Ltd. All Rights Reserved.  
*"From data intelligence emerges."*