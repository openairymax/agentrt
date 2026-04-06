Copyright (c) 2026 SPHARX. All Rights Reserved.
"From data intelligence emerges."

# AgentOS 性能调优指南

**版本**: 1.0.0
**最后更新**: 2026-04-06
**状态**: 生产就绪

---

## 📋 性能调优方法论

AgentOS 采用**反馈闭环调优法**（基于架构设计原则 S-1）：

```
┌─────────────────────────────────────────────┐
│              监控 (Monitor)                   │
│   收集指标 · 识别瓶颈 · 建立基线              │
└─────────────────┬───────────────────────────┘
                  ↓
┌─────────────────────────────────────────────┐
│              分析 (Analyze)                   │
│   根因分析 · 热点定位 · 影响评估             │
└─────────────────┬───────────────────────────┘
                  ↓
┌─────────────────────────────────────────────┐
│              优化 (Optimize)                  │
│   实施调整 · 参数配置 · 架构改进             │
└─────────────────┬───────────────────────────┘
                  ↓
┌─────────────────────────────────────────────┐
│              验证 (Verify)                    │
│   A/B 测试 · 回归测试 · 持续监控            │
└─────────────────────────────────────────────┘
```

---

## 🎯 性能基线与目标

### 关键性能指标 (KPI)

| 指标 | 基线值 | 目标值 | P99 目标 | 测量方法 |
|------|--------|--------|----------|----------|
| **IPC 延迟** | < 5μs | < 1μs | < 2μs | Prometheus histogram |
| **任务调度延迟** | < 10ms | < 1ms | < 3ms | 内核 metrics |
| **L2 记忆检索** | < 50ms | < 10ms | < 20ms | Memory client timing |
| **LLM 首字延迟 (TTFT)** | < 2000ms | < 500ms | < 1000ms | LLM service logs |
| **API 响应时间 (P95)** | < 500ms | < 100ms | < 200ms | Gateway access logs |
| **系统吞吐量** | 500 QPS | 2000 QPS | - | Load testing |
| **资源利用率** | < 70% | < 60% | < 80% | Prometheus node_exporter |
| **错误率** | < 1% | < 0.1% | < 0.5% | Error tracking |

---

## 🔧 内核层调优

### 1. IPC 通信优化

#### 配置调整

```yaml
# kernel_config.yaml
ipc:
  # 连接池大小
  connection_pool_size: 1000          # 默认: 100

  # 零拷贝缓冲区大小
  zero_copy_buffer_size: "64KB"      # 默认: 4KB

  # 批处理设置
  batch_size: 50                     # 默认: 10
  batch_timeout_ms: 5                # 默认: 10

  # 线程模型
  worker_threads:                    # 基于 CPU 核心数
    min: $(nproc)
    max: $(nproc) * 4

  # 共享内存段
  shm_segment_size: "16MB"           # 默认: 1MB
```

#### 代码级优化

```c
// 启用内联函数减少函数调用开销
static inline int agentos_ipc_fast_call(
    ipc_handle_t *handle,
    const char *method,
    const void *payload,
    size_t payload_len,
    void *response,
    size_t *response_len
) {
    // 使用内存池分配避免 malloc 开销
    ipc_request_t *req = memory_pool_alloc(handle->pool);

    // 序列化到预分配缓冲区
    size_t written = msgpack_serialize(req->buffer, payload, payload_len);

    // 直接写入共享内存（零拷贝）
    shm_write(handle->shm_segment, req->buffer, written);

    // 使用 eventfd 替代 pipe 进行通知
    uint64_t u = 1;
    write(handle->event_fd, &u, sizeof(u));

    // 轮询等待响应（比信号量更快）
    while (!atomic_load(&req->completed)) {
        cpu_pause();  # PAUSE 指令降低功耗
    }

    // 返回结果
    *response_len = req->response_len;
    memcpy(response, req->response_buffer, *response_len);

    memory_pool_free(handle->pool, req);
    return AGENTOS_OK;
}
```

### 2. 内存管理优化

```yaml
memory:
  # 内存池配置
  pools:
    small:
      block_size: "256B"
      count: 10000
    medium:
      block_size: "4KB"
      count: 5000
    large:
      block_size: "64KB"
      count: 1000

  # L1 缓存优化
  l1_cache:
    enabled: true
    size_mb: 512
    ttl_sec: 300
    eviction_policy: lru  # lru | lfu | arc

  # 大页内存 (Huge Pages)
  huge_pages:
    enabled: true
    page_size: "2MB"  # 或 1GB
```

### 3. 任务调度优化

```yaml
scheduler:
  algorithm: "weighted_fair_queueing"  # wfq | cfs | edf

  # 时间片配置
  time_slice_ms: 10                    # 默认: 50

  # 优先级队列数量
  priority_levels: 8

  # 工作窃取 (Work Stealing)
  work_stealing:
    enabled: true
    threshold_ms: 5                   # 窃取阈值

  # CPU 亲和性
  cpu_affinity:
    enabled: true
    strategy: "spread"                # spread | pack | manual
```

---

## 🧠 记忆系统调优

### L2 FAISS 索引优化

```python
# faiss_tuning.py
import faiss
import numpy as np

def optimize_faiss_index(vectors: np.ndarray, index_type: str = "ivf"):
    """FAISS 索引性能优化"""

    d = vectors.shape[1]  # 向量维度
    n = vectors.shape[0]  # 向量数量

    if index_type == "ivf":
        # IVF 索引参数调优
        nlist = max(4096, int(np.sqrt(n)))  # 分区数：√N 或 4K
        nprobe = min(256, nlist // 4)       # 探测数：分区数的 1/4

        quantizer = faiss.IndexFlatIP(d)
        index = faiss.IndexIVFFlat(quantizer, d, nlist, faiss.METRIC_INNER_PRODUCT)

        # 训练索引（使用 K-Means 初始化）
        print(f"Training IVF index with {nlist} lists...")
        index.train(vectors)

        # 添加向量（批量添加更快）
        index.add(vectors)

        # 设置运行时参数
        index.nprobe = nprobe

        return index, {"nlist": nlist, "nprobe": nprobe}

    elif index_type == "hnsw":
        # HNSW 索引参数调优
        M = 48  # 每个节点的最大连接数（默认 16）
        ef_construction = 200  # 构建时搜索宽度（默认 200）
        ef_search = 128  # 查询时搜索宽度（默认 10）

        index = faiss.IndexHNSWFlat(d, M)
        index.hnsw.efConstruction = ef_construction
        index.hnsw.efSearch = ef_search

        index.add(vectors)

        return index, {"M": M, "ef_construction": ef_construction, "ef_search": ef_search}

# 使用示例
vectors = np.random.random((1000000, 768)).astype('float32')
index, params = optimize_faiss_index(vectors)

# 基准测试
import time
query = np.random.random((1, 768)).astype('float32')

start = time.time()
D, I = index.search(query, k=10)
latency_ms = (time.time() - start) * 1000
print(f"Search latency: {latency_ms:.2f}ms")
print(f"Parameters: {params}")
```

### L3 图数据库优化

```sql
-- PostgreSQL 图查询优化

-- 创建适当的索引
CREATE INDEX CONCURRENTLY idx_relations_source ON relations(source_entity);
CREATE INDEX CONCURRENTLY idx_relations_target ON relations(target_entity);
CREATE INDEX CONCURRENTLY idx_relations_type ON relations(relation_type);

-- 使用 BRIN 索引对于时间序列数据
CREATE INDEX idx_records_timestamp USING brin(records(timestamp);

-- 配置 PostgreSQL 参数用于图查询
-- postgresql.conf
shared_buffers = '4GB'                    # 默认 128MB
effective_cache_size = '12GB'
work_mem = '64MB'                         # 排序/哈希操作的内存
maintenance_work_mem = '1GB'              # 维护操作内存
random_page_cost = 1.1                    # SSD 优化
effective_io_concurrency = 200            # SSD 并发 I/O

-- 查询计划分析
EXPLAIN (ANALYZE, BUFFERS, FORMAT TEXT)
SELECT * FROM relations
WHERE source_entity = 'entity-001'
ORDER BY weight DESC
LIMIT 100;
```

### L4 模式挖掘优化

```yaml
l4_mining:
  # 并行化配置
  parallelism:
    workers: 8                           # 默认: 4
    batch_size: 50000                    # 每批处理的记录数

  # 持久同调参数
  persistent_homology:
    max_dimension: 2                     # 最大拓扑维度
    max_edge_length: 0.5                 # Rips 复形最大边长
    use_ripser: true                     # 使用 Ripser 加速算法

  # HDBSCAN 参数
  hdbscan:
    min_cluster_size: 20                 # 最小聚类大小
    min_samples: 10                      # 核心点阈值
    cluster_selection_method: 'eom'      # Excess of Mass
    prediction_data: true                # 允许预测新点归属

  # 缓存中间结果
  cache_intermediate_results: true
  cache_ttl_hours: 24
```

---

## 🤖 LLM 服务调优

### 推理优化

```yaml
llm_service:
  # 批处理推理
  batching:
    enabled: true
    max_batch_size: 32                  # 最大批大小
    max_wait_time_ms: 50               # 最长等待时间

  # KV Cache 优化
  kv_cache:
    enabled: true
    dtype: "fp16"                       # 半精度节省显存
    paged_attention: true               # 分页注意力（PagedAttention）

  # 量化配置
  quantization:
    method: "awq"                        # awq | gptq | bitsandbytes
    bits: 4                             # 4-bit 量化

  # Tokenizer 缓存
  tokenizer_cache:
    enabled: true
    max_entries: 1000
```

### 提示词工程优化

```python
def optimized_system_prompt():
    """优化的系统提示词模板"""
    return """You are AgentOS assistant.
Be concise and direct.
Use structured output format when appropriate.
Prefer JSON for data responses."""

# 使用结构化输出减少 token 消耗
def chat_with_structured_output(agent, query):
    response = agent.chat(
        message=query,
        system_prompt=optimized_system_prompt(),
        max_tokens=1024,  # 限制输出长度
        temperature=0.3,  # 低温度减少随机性
        stop=["\n\n"]     # 早停机制
    )
    return parse_json_response(response.content)
```

---

## 🐳 Docker 与基础设施调优

### 容器资源配置

```yaml
# docker-compose.prod.yml 优化版
services:
  kernel:
    deploy:
      resources:
        limits:
          cpus: '8'
          memory: 8G
        reservations:
          cpus: '4'
          memory: 4G

    # CPU 绑定和隔离
    cpuset: "0-7"

    # 内存限制（防止 OOM）
    memswap_limit: 8G
    mem_reservation: 4G

    # I/O 优先级
    blkio_config:
      weight: 1000
      device_read_bps:
        - path: /dev/sda
          rate: '100mb'

    # PID 限制
    pids_limit: 1000

    # ulimits
    ulimits:
      nofile:
        soft: 65536
        hard: 65536
      nproc:
        soft: 4096
        hard: 8192
```

### 操作系统级别优化

```bash
#!/bin/bash
# os_tuning.sh - 操作系统性能调优脚本

# 1. 文件描述符限制
echo "* soft nofile 65535" >> /etc/security/limits.conf
echo "* hard nofile 65535" >> /etc/security/limits.conf

# 2. TCP 栈优化
sysctl -w net.core.somaxconn=65535
sysctl -w net.ipv4.tcp_max_syn_backlog=65535
sysctl -w net.core.netdev_max_backlog=65535
sysctl -w net.ipv4.tcp_tw_reuse=1
sysctl -w net.ipv4.tcp_fin_timeout=15
sysctl -w net.core.rmem_max=16777216
sysctl -w net.core.wmem_max=16777216
sysctl -w net.ipv4.tcp_rmem="4096 87380 16777216"
sysctl -w net.ipv4.tcp_wmem="4096 65536 16777216"

# 3. VM 子系统优化
sysctl -w vm.swappiness=10            # 减少交换使用
sysctl -w vm.dirty_ratio=10           # 脏页比例
sysctl -w vm.dirty_background_ratio=5
sysctl -w vm.overcommit_memory=1      # 允许内存过度分配

# 4. I/O 调度器（SSD 优化）
echo noop > /sys/block/sda/queue/scheduler  # 或 none/noop

# 5. 透明大页（对 JVM/数据库有利）
echo never > /sys/kernel/mm/transparent_hugepage/enabled

# 6. 持久生效
sysctl -p /etc/sysctl.d/99-agentos-tuning.conf
```

### 数据库优化

```sql
-- PostgreSQL 生产环境优化配置

-- 连接设置
max_connections = 300
superuser_reserved_connections = 5

-- 内存设置
shared_buffers = 4GB                    # RAM 的 25%
effective_cache_size = 12GB            # RAM 的 75%
work_mem = 64MB                         # 每个排序/哈希操作
maintenance_work_mem = 1GB              # VACUUM/CREATE INDEX
huge_pages = try

# WAL 设置
wal_buffers = 64MB
checkpoint_completion_target = 0.9
max_wal_size = 4GB
min_wal_size = 1GB

-- 查询规划器
random_page_cost = 1.1                  # SSD 优化
effective_io_concurrency = 200          # SSD 并发
default_statistics_target = 200         # 统计信息精度

-- 并行查询
max_parallel_workers_per_gather = 4
max_parallel_workers = 8
max_parallel_maintenance_workers = 4

-- 自动清理
autovacuum = on
autovacuum_max_workers = 6
autovacuum_naptime = 1min
```

```bash
# Redis 生产环境优化配置

# redis.conf
maxmemory 4gb
maxmemory-policy allkeys-lru

# 持久化优化
save 900 1
save 300 10
save 60 10000
appendonly yes
appendfsync everysec
auto-aof-rewrite-percentage 100
auto-aof-rewrite-min-size 64mb

# 网络优化
tcp-backlog 65535
timeout 0
tcp-keepalive 300

# 性能优化
hz 10                                  # 后台任务频率
dynamic-zlists yes                     # 压缩列表优化
zset-max-ziplist-entries 128
hash-max-ziplist-entries 512
```

---

## 📊 性能监控与分析工具

### 实时监控仪表板

```bash
# 使用 htop/glances 监控进程资源
htop -p $(pgrep -f agentos-kernel)

# 使用 nethogs 监控网络带宽
nethogs

# 使用 iotop 监控磁盘 I/O
iotop -oP

# 使用 smem 监控实际内存使用（PSS/USS）
smem -k -p agentos
```

### 性能剖析工具链

```bash
# 1. 系统级性能分析
perf record -g -a -- sleep 30
perf report --stdio > perf_report.txt

# 2. 火焰图生成
git clone https://github.com/brendangregg/FlameGraph
perf script | FlameStack/stackcollapse-perf.pl | FlameGraph/flamegraph.pl > flame.svg

# 3. 延迟分析
perf stat -e cycles,instructions,cache-misses,branch-misses ./bin/agentos-kernel

# 4. 锁竞争分析
perf lock contention -ag -p $(pgrep agentos-kernel) -- sleep 30

# 5. eBPF 动态追踪
# 安装 bcc-tools
execsnoop -p $(pgrep agentos-kernel)  # 追踪进程执行
biotop                                # 实时磁盘 I/O
tcptop                                # TCP 连接统计
```

### APM 集成

```yaml
# OpenTelemetry 配置示例
otel:
  exporter:
    endpoint: "http://jaeger:14268/api/traces"
    protocol: grpc

  sampling:
    ratio: 0.1  # 10% 采样率（生产环境）

  resource_attributes:
    service.name: "agentos-kernel"
    deployment.environment: "production"
    service.version: "1.0.0"
```

---

## 🔄 持续性能优化流程

### 每日检查清单

- [ ] 检查 Prometheus 告警
- [ ] 审查 Grafana 仪表板异常指标
- [ ] 检查错误日志趋势
- [ ] 确认备份成功完成
- [ ] 验证关键 SLA 达标情况

### 每周优化任务

- [ ] 分析慢查询 TOP 10
- [ ] 审查资源利用率趋势
- [ ] 更新性能基线数据
- [ ] 测试并应用安全补丁
- [ ] 清理过期数据和缓存

### 每月深度分析

- [ ] 全链路压力测试
- [ ] 容量规划评估
- [ ] 成本效益分析
- [ ] 架构优化提案评审
- [ ] 技术债务梳理

---

## 📚 相关文档

- **[内核调优指南](../../agentos/manuals/guides/kernel_tuning.md)** — 详细调优参数说明
- **[调试指南](../development/debugging.md)** — 性能瓶颈诊断
- **[监控运维](monitoring.md)** — Prometheus/Grafana 配置
- **[Docker 部署](../../docker/README.md)** — 容器性能优化

---

**© 2026 SPHARX Ltd. All Rights Reserved.**

*"From data intelligence emerges."*
