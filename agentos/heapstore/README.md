# Heapstore — 运行时数据存储

**模块路径**: `agentos/heapstore/`
**版本**: v0.1.0

## 概述

Heapstore 是 AgentRT 的运行时数据存储模块，提供系统运行过程中的日志、注册信息、链路追踪、内存管理、Token 计数和批量数据的持久化存储能力。模块采用 **混合存储引擎** 架构，结合 SQLite（条件编译）和内存后端满足不同场景需求，并通过 **熔断器** 机制保障写入可靠性，以及 **快速/慢速双路径** 设计优化高频写入性能。

## 设计目标

- **高性能写入**：快速/慢速双路径设计，针对高频写入场景优化，最小化写入延迟
- **混合存储**：结合 SQLite 和内存后端满足不同场景需求，SQLite 不可用时自动回退
- **数据可观测**：提供完整的统计信息、性能指标和健康检查接口
- **低开销**：最小化对主业务逻辑的性能影响，快速路径无锁写入
- **熔断保护**：内置熔断器机制，连续失败时自动切断写入，防止级联故障
- **批量优化**：支持事务性批量写入，减少高频场景下的 I/O 开销

## 目录结构

```
heapstore/
├── include/                          # 公共头文件
│   ├── heapstore.h                   # 核心接口（初始化/关闭/统计/熔断/批量写入）
│   ├── heapstore_types.h             # 共享类型定义（打破循环依赖）
│   ├── heapstore_log.h               # 日志管理接口
│   ├── heapstore_registry.h          # 注册表接口（Agent/Skill/Session）
│   ├── heapstore_trace.h             # 链路追踪存储接口
│   ├── heapstore_memory.h            # 内存管理数据存储接口
│   ├── heapstore_token.h             # Token 计数与预算接口
│   ├── heapstore_batch.h             # 批量写入接口
│   ├── heapstore_ipc.h               # IPC 数据存储接口
│   ├── heapstore_integration.h       # 集成测试接口
│   └── utils.h                       # 内部工具函数
├── src/                              # 源代码实现
│   ├── private.h                     # 内部私有头文件
│   ├── heapstore_core.c              # 核心实现（初始化/路径管理/统计/熔断器）
│   ├── heapstore_log.c               # 日志系统实现
│   ├── heapstore_registry.c          # 注册表实现（SQLite + 内存回退）
│   ├── heapstore_trace.c             # 链路追踪实现
│   ├── heapstore_memory.c            # 内存数据存储实现
│   ├── heapstore_token.c             # Token 计数实现
│   ├── heapstore_batch.c             # 批量写入实现（链表缓冲 + 事务提交）
│   ├── heapstore_ipc.c               # IPC 数据存储实现
│   ├── heapstore_integration.c       # 集成层实现
│   └── utils.c                       # 工具函数实现
├── kernel/                           # 内核级服务
│   ├── services/                     # 内核服务实现
│   │   ├── log_store_service.c       # 日志存储内核服务
│   │   └── trace_store_service.c     # 追踪存储内核服务
│   ├── ipc/                          # IPC 内核数据
│   │   ├── channels/.keep            # IPC 通道数据目录
│   │   └── binder/.keep              # IPC Binder 数据目录
│   └── memory/                       # 内存内核数据
│       ├── patterns/.keep            # 内存模式目录
│       ├── index/.keep               # 内存索引目录
│       ├── meta/.keep                # 内存元数据目录
│       └── raw/.keep                 # 原始内存数据目录
├── services/                         # 服务数据目录
│   ├── market_d/.keep                # market_d 服务数据
│   ├── tool_d/.keep                  # tool_d 服务数据
│   └── llm_d/.keep                   # llm_d 服务数据
├── tests/                            # 测试套件
│   ├── CMakeLists.txt                # 测试构建配置
│   ├── test_heapstore_core.c         # 核心功能测试
│   ├── test_heapstore_log.c          # 日志系统测试
│   ├── test_heapstore_registry.c     # 注册表测试
│   ├── test_heapstore_trace.c        # 追踪系统测试
│   ├── test_heapstore_memory.c       # 内存存储测试
│   ├── test_heapstore_ipc.c          # IPC 存储测试
│   ├── test_heapstore_batch.c        # 批量写入测试
│   ├── test_heapstore_integration.c  # 集成测试
│   ├── test_batch_performance.c      # 批量性能测试
│   ├── test_security_path_traversal.c # 安全路径遍历测试
│   ├── test_fuzzing_concurrency.c    # 并发模糊测试
│   ├── test_edge_cases.c             # 边界情况测试
│   └── benchmark_heapstore.c         # 性能基准测试
├── examples/                         # 示例代码
│   ├── CMakeLists.txt                # 示例构建配置
│   ├── quick_start.c                 # 快速入门示例
│   └── batch_write.c                 # 批量写入示例
├── scripts/                          # 运维脚本
│   ├── performance_regression_detector.py  # 性能回归检测
│   └── add_since_tags.py             # 版本标签添加工具
├── CMakeLists.txt                    # CMake 构建配置
├── .gitignore                        # Git 忽略规则
└── README.md                         # 本文件
```

## 存储架构

### 7 大存储引擎

| 存储引擎 | 源文件 | 后端 | 用途 |
|----------|--------|------|------|
| **heapstore_core** | `heapstore_core.c` | LMDB/SQLite/Redis | 核心初始化、路径管理、统计、熔断器 |
| **heapstore_log** | `heapstore_log.c` | SQLite | 系统运行日志持久化，支持轮转与清理 |
| **heapstore_registry** | `heapstore_registry.c` | SQLite | Agent/Skill/Session 注册信息，支持迭代查询 |
| **heapstore_trace** | `heapstore_trace.c` | SQLite | 分布式链路追踪 Span 存储，支持导出 |
| **heapstore_memory** | `heapstore_memory.c` | LMDB | 内存池与分配记录，读写性能优先 |
| **heapstore_token** | `heapstore_token.c` | SQLite | Token 使用统计与预算管理 |
| **heapstore_batch** | `heapstore_batch.c` | LMDB | 批量数据写入，链表缓冲 + 事务提交 |

### 条件编译

| 依赖 | 条件宏 | 缺失时行为 |
|------|--------|-----------|
| SQLite3 | `AGENTOS_HAS_SQLITE3` | 注册表回退到内存后端（功能完整但数据不持久化） |

> **注意**：SQLite3 为可选依赖。若构建时未检测到 SQLite3 开发库，heapstore 将自动回退到内存后端，功能完整但进程退出后数据丢失。

## 数据模型

### 日志存储 (heapstore_log)

系统运行日志的持久化存储，支持按服务名称分文件、日志轮转和过期清理。

```c
typedef struct heapstore_log_entry {
    int level;              // 日志级别 (DEBUG/INFO/WARN/ERROR/FATAL)
    char service[64];       // 服务名称
    char trace_id[64];      // 追踪 ID
    char message[1024];     // 日志消息
    time_t timestamp;       // 时间戳
} heapstore_log_entry_t;
```

### 注册信息 (heapstore_registry)

组件注册信息存储，包括 Agent、Skill 和 Session 三种注册表类型，支持 CRUD 和迭代查询。

| 注册表类型 | 枚举值 | 记录结构 |
|-----------|--------|---------|
| Agent 注册表 | `heapstore_REG_AGENTS` | `heapstore_agent_record_t` |
| Skill 注册表 | `heapstore_REG_SKILLS` | `heapstore_skill_record_t` |
| Session 注册表 | `heapstore_REG_SESSIONS` | `heapstore_session_record_t` |

### 链路追踪 (heapstore_trace)

分布式链路追踪数据存储，记录请求在各服务间的传播路径和耗时，支持按 trace_id 和时间范围查询。

```c
typedef struct heapstore_span {
    char trace_id[64];          // 追踪 ID
    char span_id[32];           // Span ID
    char parent_span_id[32];    // 父 Span ID
    char name[128];             // Span 名称
    char kind[64];              // Span 类型
    uint64_t start_time_ns;     // 开始时间（纳秒）
    uint64_t end_time_ns;       // 结束时间（纳秒）
    char service_name[64];      // 服务名称
    char status[32];            // 状态
    void *attributes;           // 属性键值对
    size_t attribute_count;     // 属性数量
} heapstore_span_t;
```

### 内存管理 (heapstore_memory)

内存池与分配记录存储，用于运行时内存使用的可观测性。

| 数据类型 | 说明 |
|----------|------|
| `heapstore_memory_pool_t` | 内存池信息（ID/名称/总大小/已用大小/块数） |
| `heapstore_memory_allocation_t` | 分配记录（ID/池ID/大小/地址/状态） |

### Token 管理 (heapstore_token)

Token 生命周期管理，包括使用统计、预算控制和缓存命中追踪。

| Token 类型 | 说明 |
|-----------|------|
| `HEAPSTORE_TOKEN_TYPE_PROMPT` | Prompt Token |
| `HEAPSTORE_TOKEN_TYPE_COMPLETION` | Completion Token |
| `HEAPSTORE_TOKEN_TYPE_SYSTEM` | System Token |
| `HEAPSTORE_TOKEN_TYPE_USER` | User Token |
| `HEAPSTORE_TOKEN_TYPE_CACHE_HIT` | 缓存命中节省的 Token |

### 批量操作 (heapstore_batch)

批量数据写入与查询操作，支持 9 种数据类型的链表缓冲和事务性批量提交。

| 批量项类型 | 说明 |
|-----------|------|
| `HEAPSTORE_BATCH_ITEM_LOG` | 日志条目 |
| `HEAPSTORE_BATCH_ITEM_SPAN` | 追踪 Span |
| `HEAPSTORE_BATCH_ITEM_SESSION` | 会话记录 |
| `HEAPSTORE_BATCH_ITEM_AGENT` | Agent 记录 |
| `HEAPSTORE_BATCH_ITEM_SKILL` | Skill 记录 |
| `HEAPSTORE_BATCH_ITEM_MEMORY_POOL` | 内存池记录 |
| `HEAPSTORE_BATCH_ITEM_MEMORY_ALLOC` | 内存分配记录 |
| `HEAPSTORE_BATCH_ITEM_IPC_CHANNEL` | IPC 通道记录 |
| `HEAPSTORE_BATCH_ITEM_IPC_BUFFER` | IPC 缓冲区记录 |

## 核心机制

### 快速/慢速双路径

| 路径 | 函数 | 特点 |
|------|------|------|
| **快速路径** | `heapstore_log_write_fast()` | 无锁异步写入，适用于高频日志，熔断器打开时返回错误 |
| **慢速路径** | `heapstore_log_write_slow()` | 同步写入，完整参数验证和错误处理，支持超时和 trace_id |

### 熔断器

```c
typedef enum {
    heapstore_CIRCUIT_CLOSED = 0,   // 正常状态
    heapstore_CIRCUIT_OPEN,         // 熔断器打开（拒绝写入）
    heapstore_CIRCUIT_HALF_OPEN     // 半开状态（试探性恢复）
} heapstore_circuit_state_t;
```

- 连续失败次数超过阈值（`circuit_breaker_threshold`）时熔断器打开
- 超时后进入半开状态，允许少量请求试探
- 可通过 `heapstore_reset_circuit()` 手动重置

## 接口说明

### 核心 API

| 函数 | 说明 |
|------|------|
| `heapstore_init(config)` | 初始化数据分区（必须首先调用） |
| `heapstore_shutdown()` | 关闭数据分区并清理资源 |
| `heapstore_is_initialized()` | 检查是否已初始化 |
| `heapstore_get_root()` | 获取数据分区根路径 |
| `heapstore_get_path(type)` | 获取指定类型的相对路径 |
| `heapstore_get_full_path(type, buffer, size)` | 获取完整路径 |
| `heapstore_get_stats(stats)` | 获取磁盘使用统计 |
| `heapstore_get_metrics(metrics)` | 获取性能指标 |
| `heapstore_health_check(...)` | 健康检查（5 个子系统） |
| `heapstore_cleanup(dry_run, freed_bytes)` | 清理过期数据 |
| `heapstore_flush()` | 强制刷新待写入数据 |
| `heapstore_reload_config(config)` | 重新加载配置 |
| `heapstore_strerror(err)` | 错误码描述字符串 |

### 熔断器 API

| 函数 | 说明 |
|------|------|
| `heapstore_get_circuit_state(info)` | 获取熔断器状态信息 |
| `heapstore_reset_circuit()` | 手动重置熔断器 |

### 批量写入 API

| 函数 | 说明 |
|------|------|
| `heapstore_batch_begin(batch_size)` | 创建批量写入上下文 |
| `heapstore_batch_add_log(ctx, ...)` | 添加日志到批量缓冲 |
| `heapstore_batch_add_span(ctx, ...)` | 添加 Span 到批量缓冲 |
| `heapstore_batch_add_session(ctx, ...)` | 添加会话记录 |
| `heapstore_batch_add_agent(ctx, ...)` | 添加 Agent 记录 |
| `heapstore_batch_commit(ctx)` | 提交批量写入（事务性） |
| `heapstore_batch_rollback(ctx)` | 回滚批量写入 |
| `heapstore_batch_context_destroy(ctx)` | 销毁批量写入上下文 |

### 日志 API

| 函数 | 说明 |
|------|------|
| `heapstore_log_init()` | 初始化日志系统 |
| `heapstore_log_write(level, service, trace_id, file, line, fmt, ...)` | 写入日志 |
| `heapstore_log_set_level(level)` | 设置日志级别 |
| `heapstore_log_rotate()` | 执行日志轮转 |
| `heapstore_log_cleanup(days, freed_bytes)` | 清理过期日志 |
| `heapstore_log_is_healthy()` | 健康检查 |

便捷宏：`HEAPSTORE_LOG_ERROR` / `HEAPSTORE_LOG_WARN` / `HEAPSTORE_LOG_INFO` / `HEAPSTORE_LOG_DEBUG`

### 注册表 API

| 函数 | 说明 |
|------|------|
| `heapstore_registry_add_agent(record)` | 添加 Agent 记录 |
| `heapstore_registry_get_agent(id, record)` | 获取 Agent 记录 |
| `heapstore_registry_update_agent(record)` | 更新 Agent 记录 |
| `heapstore_registry_delete_agent(id)` | 删除 Agent 记录 |
| `heapstore_registry_query_agents(type, status, iter)` | 查询 Agent（迭代器） |
| `heapstore_registry_batch_insert_agents(records, count)` | 批量插入 Agent（事务优化） |
| `heapstore_registry_vacuum()` | 执行 VACUUM 操作 |

### Token API

| 函数 | 说明 |
|------|------|
| `heapstore_token_init()` | 初始化 Token 计数器 |
| `heapstore_token_record(type, count, operation)` | 记录 Token 使用 |
| `heapstore_token_get_stats(out_stats)` | 获取 Token 统计 |
| `heapstore_token_set_budget(task_id, budget)` | 设置任务 Token 预算 |
| `heapstore_token_check_budget(task_id, requested, allowed)` | 检查预算是否允许 |

### 错误码

| 枚举值 | 值 | 说明 |
|--------|----|------|
| `heapstore_SUCCESS` | 0 | 成功 |
| `heapstore_ERR_INVALID_PARAM` | -1 | 无效参数 |
| `heapstore_ERR_NOT_INITIALIZED` | -2 | 未初始化 |
| `heapstore_ERR_ALREADY_INITIALIZED` | -3 | 已初始化 |
| `heapstore_ERR_DIR_CREATE_FAILED` | -4 | 目录创建失败 |
| `heapstore_ERR_DB_INIT_FAILED` | -8 | 数据库初始化失败 |
| `heapstore_ERR_CIRCUIT_OPEN` | -15 | 熔断器打开 |
| `heapstore_ERR_TIMEOUT` | -16 | 超时 |
| `heapstore_ERR_INTERNAL` | -99 | 内部错误 |

## 依赖关系

| 组件 | 条件 | 用途 |
|------|------|------|
| SQLite3 | 可选（`AGENTOS_HAS_SQLITE3`） | 注册表/日志/追踪/Token 持久化存储 |
| agentos_common | 必需 | 公共工具库 |
| agentos_compile_defs | 必需 | 编译定义 |
| Threads::Threads | 必需 | 多线程支持 |

## 构建说明

```bash
# 构建 Heapstore 模块
mkdir build && cd build
cmake .. -DBUILD_TESTS=ON
make agentos_heapstore

# 运行测试
ctest --test-dir build -R heapstore

# 运行性能基准
./build/benchmark_heapstore

# 运行示例
./build/quick_start
./build/batch_write
```

## 使用示例

### 基本初始化与日志写入

```c
#include "heapstore/heapstore.h"

heapstore_config_t config = {
    .root_path = "/var/lib/agentos/heapstore",
    .max_log_size_mb = 100,
    .log_retention_days = 30,
    .enable_auto_cleanup = true,
    .enable_log_rotation = true,
    .circuit_breaker_threshold = 10,
    .circuit_breaker_timeout_sec = 30
};

heapstore_init(&config);

heapstore_log_write_fast("gateway", HEAPSTORE_LOG_INFO, "Server started");

heapstore_log_write_slow("gateway", HEAPSTORE_LOG_ERROR, "Connection failed",
                         "trace-001", 5000);

heapstore_shutdown();
```

### 注册表操作

```c
heapstore_agent_record_t agent = {
    .id = "agent-001",
    .name = "ChatAgent",
    .type = "conversational",
    .version = "1.0.0",
    .status = "active"
};
heapstore_registry_add_agent(&agent);

heapstore_agent_record_t result;
heapstore_registry_get_agent("agent-001", &result);
```

### 批量写入

```c
heapstore_batch_context_t *ctx = heapstore_batch_begin(100);

for (int i = 0; i < 50; i++) {
    heapstore_batch_add_log(ctx, "service", HEAPSTORE_LOG_INFO, "batch message");
}

heapstore_error_t err = heapstore_batch_commit(ctx);
if (err != heapstore_SUCCESS) {
    heapstore_batch_rollback(ctx);
}
heapstore_batch_context_destroy(ctx);
```

### 健康检查

```c
bool registry_ok, trace_ok, log_ok, ipc_ok, memory_ok;
heapstore_health_check(&registry_ok, &trace_ok, &log_ok, &ipc_ok, &memory_ok);

heapstore_circuit_info_t circuit;
heapstore_get_circuit_state(&circuit);
```

## 配置选项

```json
{
    "heapstore": {
        "data_dir": "/var/lib/agentos/heapstore",
        "max_log_size_mb": 100,
        "log_retention_days": 30,
        "trace_retention_days": 14,
        "enable_auto_cleanup": true,
        "enable_log_rotation": true,
        "enable_trace_export": true,
        "db_vacuum_interval_days": 7,
        "circuit_breaker_threshold": 10,
        "circuit_breaker_timeout_sec": 30
    }
}
```

---

© 2026 SPHARX Ltd. All Rights Reserved.
