# Heapstore — 运行时数据存储

`agentos/heapstore/` 是 AgentOS 的运行时数据存储模块，提供系统运行过程中的日志、注册信息和链路追踪数据的持久化存储能力。

## 设计目标

- **高性能写入**：针对高频写入场景优化，最小化写入延迟
- **混合存储**：结合 LMDB、SQLite 和 Redis 满足不同场景需求
- **数据可观测**：提供数据查看和查询能力（heap_viewer）
- **低开销**：最小化对主业务逻辑的性能影响

## 存储架构

| 存储组件 | 用途 | 特点 |
|----------|------|------|
| **LMDB** | 高性能键值存储 | 内存映射文件，读写极快 |
| **SQLite** | 结构化数据存储 | 支持 SQL 查询，事务支持 |
| **Redis** | 缓存和实时数据 | 内存存储，支持 TTL |

## 数据模型

### 日志存储 (heap_log)

系统运行日志的持久化存储，支持按时间范围检索。

### 注册信息 (heap_registry)

组件注册信息存储，包括守护进程注册、工具注册和服务发现数据。

### 链路追踪 (heap_trace)

分布式链路追踪数据存储，记录请求在各服务间的传播路径和耗时。

## 数据查看器

heap_viewer 提供命令行数据查看能力：

```bash
# 查看最新日志
heap_viewer logs --tail 100

# 查询注册信息
heap_viewer registry --service llm_d

# 查看链路追踪
heap_viewer trace --trace-id trace-001
```

## 使用示例

```c
#include "heapstore/heap_store.h"

// 初始化存储
heap_store_t* store = heap_store_create("data/heapstore");

// 写入日志
heap_store_write(store, HEAP_TYPE_LOG, log_entry);

// 注册服务
heap_store_write(store, HEAP_TYPE_REGISTRY, service_info);

// 记录追踪
heap_store_write(store, HEAP_TYPE_TRACE, trace_span);

// 查询数据
heap_query_t query = {
    .type = HEAP_TYPE_LOG,
    .start_time = start_ts,
    .end_time = end_ts
};
heap_result_t* result = heap_store_query(store, &query);
```

## 配置选项

```json
{
    "heapstore": {
        "data_dir": "/var/lib/agentos/heapstore",
        "lmdb": {
            "map_size": "10GB",
            "max_readers": 128
        },
        "sqlite": {
            "journal_mode": "WAL",
            "cache_size": 64000
        },
        "redis": {
            "host": "localhost",
            "port": 6379
        }
    }
}
```

---

*AgentOS Heapstore*
