# AgentOS heapstore - 数据分区存储系统

**版本**: v1.0.0.6
**最后更新**: 2026-04-01
**开发语言**: C11
**许可证**: Apache License 2.0

---

## 📖 概述

`heapstore` 是 AgentOS 的**数据分区存储系统**，为整个系统提供统一、可靠、高性能的持久化存储解决方案。它管理所有运行时产生的数据，包括日志、注册表、追踪数据、IPC 通信数据和内存管理数据。

> **注意**: heapstore 由早期的 `data` 模块更名而来，定位为 AgentOS 的底层存储引擎。

### 核心特性

- 🎯 **生产级可靠性**: 完整的错误处理和资源管理
- 🔒 **线程安全**: 所有操作均经过互斥锁保护
- 📈 **高性能**: 批量写入、异步刷新、最小化锁竞争
- 🔄 **自动管理**: 数据生命周期管理、自动清理、日志轮转
- 📊 **可观测性**: 完整的统计信息和健康检查接口
- 🛡️ **数据安全**: 参数验证、SQL 注入防护、资源隔离
- ⚡ **批量写入**: 支持批量操作，减少 I/O 开销

### 在 AgentOS 中的位置

```
AgentOS 架构
┌──────────────────────────────────────────────────────────────────────┐
│                           服务层 (daemon)                             │
│              llm_d | market_d | monit_d | sched_d | tool_d          │
└──────────────────────────────────────────────────────────────────────┘
                                    ↓↑
┌──────────────────────────────────────────────────────────────────────┐
│                            内核层 (atoms)                             │
│  ┌────────────────┐ ┌────────────────┐ ┌────────────────┐           │
│  │    corekern    │ │ coreloopthree  │ │  memoryrovol   │           │
│  │    微内核基础   │ │  三层认知运行时  │ │   四层记忆系统   │           │
│  │  IPC · Mem · Task │ │ 认知 / 行动 / 记忆  │ │   L1/L2/L3/L4   │           │
│  └────────────────┘ └────────────────┘ └────────────────┘           │
│  ┌────────────────┐ ┌────────────────┐                             │
│  │    syscall     │ │     utils      │                             │
│  │   系统调用接口   │ │    公共工具库    │                             │
│  └────────────────┘ └────────────────┘                             │
└──────────────────────────────────────────────────────────────────────┘
                                    ↓↑
┌──────────────────────────────────────────────────────────────────────┐
│                         heapstore ⭐                                  │
│                    数据分区存储系统（底层存储引擎）                      │
│  ┌──────────────────────────────────────────────────────────────┐   │
│  │  syscall 层调用 ──▶ heapstore ──▶ 文件系统                    │   │
│  │  memoryrovol ──▶ heapstore ──▶ 文件系统（记忆持久化）         │   │
│  │  日志/注册表/追踪/IPC/内存 ──▶ heapstore ──▶ 文件系统        │   │
│  └──────────────────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 🏗️ 架构设计

### 设计原则

heapstore 遵循 AgentOS 五维正交架构原则：

| 维度 | 原则 | 实现 |
|------|------|------|
| **S-2 层次分解** | 用户态程序通过系统调用与内核交互 | heapstore 作为底层存储引擎，被 syscall 层调用 |
| **K-2 接口契约化** | 所有 API 有完整契约定义 | @ownership/@threadsafe/@reentrant 注解完整 |
| **E-2 可观测性** | 指标、追踪、日志完整 | 健康检查、统计接口、熔断器机制 |
| **E-3 资源确定性** | 资源管理清晰 | 所有输出参数有明确所有权语义 |

### 模块架构

```
┌─────────────────────────────────────────────────────────────┐
│                    heapstore 公共 API                        │
│  heapstore_init() | shutdown() | health_check() | ...      │
└─────────────────────────────────────────────────────────────┘
                            │
        ┌───────────────────┼───────────────────┐
        │                   │                   │
┌───────▼────────┐ ┌────────▼────────┐ ┌───────▼────────┐
│ heapstore_core │ │ heapstore_integ│ │   private.h    │
│ - 目录管理     │ │   ration.c     │ │  - 内部接口    │
│ - 配置管理     │ │ - syscall 集成  │ │  - 模块声明    │
│ - 熔断器       │ │ - 批量写入     │ │                │
│ - 性能指标     │ │ - 健康检查     │ │                │
└───────┬────────┘ └────────┬────────┘ └─────────────────┘
        │                   │
        ├──────────┬──────────┬──────────┬──────────┬──────────┐
        │          │          │          │          │          │
┌───────▼──┐  ┌───▼────┐  ┌──▼─────┐  ┌─▼──────┐  ┌─▼──────┐
│heapstore │  │heapstore│  │heapstore│  │heapstore│  │heapstore│
│ _log.c   │  │_regist.│  │ _trace.│  │ _ipc.c │  │_memory.│
│          │  │  .c    │  │  .c    │  │        │  │  .c    │
├──────────┤  ├────────┤  ├────────┤  ├────────┤  ├────────┤
│ 日志管理 │  │注册表  │  │追踪管理│  │IPC 管理 │  │内存管理│
│ - 轮转   │  │- SQLite│  │- Span  │  │- 通道   │  │- 内存池│
│ - 快慢路径│ │- WAL   │  │- 导出  │  │- 缓冲区 │  │- 分配  │
└──────────┘  └────────┘  └────────┘  └────────┘  └────────┘
```

### 线程安全设计

所有子模块都使用独立的互斥锁保护：

```c
pthread_mutex_t g_registry.lock;    // 注册表锁
pthread_mutex_t g_log_lock;        // 日志锁
pthread_mutex_t g_trace_queue.lock;// 追踪锁
pthread_mutex_t g_ipc_data.lock;   // IPC 锁
pthread_mutex_t g_memory_data.lock; // 内存锁
```

---

## 🔗 AgentOS 集成

heapstore 作为底层存储引擎，被 AgentOS 核心模块调用：

### 集成架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                     heapstore 集成数据流                             │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│   daemon/llm_d ──────┐                                               │
│   daemon/market_d ───┼──▶ syscall/session.c ──▶ heapstore_syscall  │
│   daemon/tool_d ─────┘                           _session_save()   │
│                                                      │               │
│                                                      ▼               │
│   coreloopthree ──────────────────────────────▶ heapstore_registry │
│                                                      │               │
│   memoryrovol ───────────────────────────────▶ heapstore_memoryrovol │
│                                                      │               │
│   corekern/ipc ─────────────────────────────────▶ heapstore_ipc    │
│                                                      │               │
│   commons/logging ─────────────────────────────▶ heapstore_logging  │
│                                                      │               │
│   syscall/telemetry ───────────────────────────▶ heapstore_trace     │
│                                                      │               │
│                                                      ▼               │
│                                              ┌─────────────┐        │
│                                              │  文件系统   │        │
│                                              └─────────────┘        │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

### 集成接口

| 调用模块 | 接口函数 | 数据类型 |
|----------|----------|----------|
| syscall/session.c | `heapstore_syscall_session_save/load/delete` | 会话数据 |
| syscall/telemetry.c | `heapstore_syscall_trace_export` | 追踪数据 |
| memoryrovol | `heapstore_memoryrovol_save/load/delete` | 记忆数据 |
| corekern/ipc | `heapstore_ipc_channel_save/load` | IPC 通道 |
| commons/logging | `heapstore_logging_write` | 日志数据 |

---

## 🚀 快速开始

### 环境要求

- **编译器**: GCC 5.0+ / Clang 3.5+ / MSVC 2015+
- **C 标准**: C11
- **依赖库**: pthread、SQLite3
- **构建系统**: CMake 3.10+
- **操作系统**: Linux / macOS / Windows

### 基本使用

```c
#include <heapstore.h>
#include <heapstore_integration.h>
#include <stdio.h>

int main(void) {
    /* 1. 初始化 heapstore 集成 */
    agentos_error_t err = heapstore_integration_init("/var/lib/agentos/heapstore");
    if (err != AGENTOS_SUCCESS) {
        fprintf(stderr, "初始化失败：%d\n", err);
        return 1;
    }

    /* 2. 批量写入示例 */
    heapstore_batch_context_t* ctx = heapstore_batch_begin(100);
    if (ctx) {
        for (int i = 0; i < 50; i++) {
            heapstore_batch_add_log(ctx, "my_service", 2, "批量日志消息 %d", i);
        }
        heapstore_batch_commit(ctx);
        heapstore_batch_context_destroy(ctx);
    }

    /* 3. 健康检查 */
    char* health_json = NULL;
    heapstore_integration_health_check(&health_json);
    if (health_json) {
        printf("Health: %s\n", health_json);
        free(health_json);
    }

    /* 4. 关闭 */
    heapstore_integration_shutdown();
    return 0;
}
```

---

## 📦 核心功能

### 1. 分层日志系统 📝

提供三级（内核/服务/应用）日志存储，支持 4 级日志（ERROR/WARN/INFO/DEBUG）。

**特性**:
- 按服务分离日志文件
- 支持 Trace ID 追踪
- 自动日志轮转（可配置大小和时间）
- 线程安全的日志写入
- **快慢路径双模式**: 快速路径用于高频日志，慢速路径用于重要日志

### 2. SQLite 注册表 🗄️

使用嵌入式 SQLite 数据库管理 Agent、Skill 和 Session 的注册信息。

**特性**:
- WAL 模式优化写入性能
- 参数化查询防止 SQL 注入
- 自动 Vacuum 优化数据库
- 完整的 CRUD 操作接口

### 3. OpenTelemetry 追踪 📊

存储分布式追踪数据（Span），支持 OpenTelemetry 标准格式。

**特性**:
- Span 批量写入优化
- JSON 格式导出
- 支持 Trace-Span 层级关系
- 自动过期清理

### 4. IPC 数据存储 🔗

管理进程间通信（IPC）的通道和缓冲区数据。

**特性**:
- Binder 通道状态记录
- 共享内存缓冲区管理
- 线程安全的读写操作

### 5. 内存管理数据 🧠

记录内存池状态、分配历史和统计信息。

**特性**:
- 内存池快照
- 分配历史记录
- 实时统计信息
- 内存泄漏检测支持

### 6. 熔断器机制 ⚡

防止级联失败的熔断器实现。

**状态机**:
```
        ┌─────────────────────────────────────┐
        │                                     │
        ▼                                     │
    ┌───────┐    失败 >= 阈值    ┌───────┐   │
    │ CLOSED │ ────────────────▶ │ OPEN  │   │
    │ 正常   │                   │ 熔断  │   │
    └───────┘ ◀───────────────  └───────┘   │
        │      超时恢复              │        │
        │      (timeout_sec)        │        │
        │                           │        │
        │                           ▼        │
        │                       ┌─────────┐ │
        │                       │HALF_OPEN│ │
        │                       │ 半开    │ │
        │                       └─────────┘ │
        │                           │        │
        └───────────────────────────┘        │
              成功 < threshold               │
```

### 7. 批量写入支持 📦

高效的批量写入接口，减少 I/O 操作。

```c
/* 创建批量上下文 */
heapstore_batch_context_t* ctx = heapstore_batch_begin(100);

/* 添加各种类型的数据 */
heapstore_batch_add_log(ctx, "service", 2, "message");
heapstore_batch_add_trace(ctx, trace_id, span_id, NULL, "op", t1, t2, 0, NULL);
heapstore_batch_add_session(ctx, &session_record);

/* 提交或回滚 */
heapstore_batch_commit(ctx);    /* 全部提交 */
heapstore_batch_rollback(ctx);  /* 或全部回滚 */

/* 销毁上下文 */
heapstore_batch_context_destroy(ctx);
```

---

## 📂 目录结构

### 代码结构

```
heapstore/
├── CMakeLists.txt                  # CMake 构建配置
├── README.md                       # 本文档
│
├── include/                        # 公共头文件
│   ├── heapstore.h                 # 核心接口（含批量写入）
│   ├── heapstore_integration.h     # AgentOS 集成接口
│   ├── heapstore_log.h             # 日志接口
│   ├── heapstore_registry.h        # 注册表接口
│   ├── heapstore_trace.h           # 追踪接口
│   ├── heapstore_ipc.h             # IPC 接口
│   ├── heapstore_memory.h          # 内存接口
│   └── utils.h                     # 工具函数接口
│
├── src/                            # 源代码
│   ├── heapstore_core.c            # 核心实现（含批量写入）
│   ├── heapstore_integration.c     # AgentOS 集成实现
│   ├── heapstore_log.c             # 日志实现
│   ├── heapstore_registry.c        # 注册表实现
│   ├── heapstore_trace.c           # 追踪实现
│   ├── heapstore_ipc.c             # IPC 实现
│   ├── heapstore_memory.c          # 内存实现
│   ├── utils.c                     # 工具函数实现
│   └── private.h                   # 内部头文件
│
├── tests/                          # 测试代码
│   ├── test_heapstore_core.c       # 核心测试
│   ├── test_heapstore_log.c        # 日志测试
│   ├── test_heapstore_registry.c   # 注册表测试
│   ├── test_heapstore_trace.c      # 追踪测试
│   ├── test_heapstore_ipc.c        # IPC 测试
│   ├── test_heapstore_memory.c     # 内存测试
│   ├── test_heapstore_integration.c# 集成测试
│   └── benchmark_heapstore.c        # 性能基准测试
│
└── kernel/                         # 运行时数据目录
    ├── ipc/                        # IPC 通信数据
    │   ├── channels/              # Binder 通道状态
    │   └── buffers/               # 共享内存缓冲区
    └── memory/                     # 内存管理数据
        ├── pools/                 # 内存池状态
        ├── allocations/           # 分配记录
        └── stats/                 # 统计信息
```

### 运行时目录

```
heapstore/                          # 数据分区根目录
├── kernel/                         # 内核数据
│   ├── ipc/                       # IPC 通信数据
│   │   ├── channels/              # Binder 通道状态
│   │   └── buffers/               # 共享内存缓冲区
│   └── memory/                     # 内存管理数据
│       ├── pools/                 # 内存池状态
│       ├── allocations/           # 分配记录
│       └── stats/                 # 统计信息
│
├── logs/                           # 日志文件
│   ├── apps/                      # 应用层日志
│   ├── kernel/                    # 内核层日志
│   └── services/                  # 服务层日志
│
├── registry/                       # 注册表
│   ├── agents.db                  # Agent 注册表 (SQLite)
│   ├── skills.db                  # 技能注册表 (SQLite)
│   └── sessions.db                # 会话注册表 (SQLite)
│
├── services/                       # 服务数据
│   ├── llm_d/                     # LLM 服务数据
│   ├── market_d/                  # 市场服务数据
│   └── tool_d/                    # 工具服务数据
│
└── traces/                         # 追踪数据
    └── spans/                     # Span 数据
```

---

## 📚 API 参考

### 核心 API

| 函数 | 说明 |
|------|------|
| `heapstore_init()` | 初始化数据分区系统 |
| `heapstore_shutdown()` | 关闭系统并清理资源 |
| `heapstore_health_check()` | 健康检查接口 |
| `heapstore_get_stats()` | 获取系统统计信息 |
| `heapstore_get_metrics()` | 获取性能指标 |
| `heapstore_reset_circuit()` | 重置熔断器 |
| `heapstore_reload_config()` | 热重载配置 |
| `heapstore_flush()` | 刷新所有缓冲区 |

### 批量写入 API

| 函数 | 说明 |
|------|------|
| `heapstore_batch_begin()` | 创建批量上下文 |
| `heapstore_batch_add_log()` | 添加日志 |
| `heapstore_batch_add_trace()` | 添加追踪数据 |
| `heapstore_batch_add_session()` | 添加会话 |
| `heapstore_batch_add_agent()` | 添加 Agent |
| `heapstore_batch_add_skill()` | 添加技能 |
| `heapstore_batch_commit()` | 提交批量 |
| `heapstore_batch_rollback()` | 回滚批量 |
| `heapstore_batch_context_destroy()` | 销毁上下文 |

### AgentOS 集成 API

| 函数 | 说明 |
|------|------|
| `heapstore_integration_init()` | 初始化集成 |
| `heapstore_integration_shutdown()` | 关闭集成 |
| `heapstore_syscall_session_save()` | 会话持久化 |
| `heapstore_syscall_trace_export()` | 追踪导出 |
| `heapstore_memoryrovol_save()` | 记忆持久化 |
| `heapstore_integration_health_check()` | 集成健康检查 |

---

## 📊 性能指标

### 基准测试结果

测试环境：Intel i7-10700K, 32GB RAM, NVMe SSD

| 操作 | 吞吐量 | 延迟 (P50) | 延迟 (P99) |
|------|--------|-----------|-----------|
| 日志写入 | 125,000 ops/s | 8 μs | 45 μs |
| 批量日志写入 (100 条) | 2,500 batch/s | 400 μs | 1.2 ms |
| 注册表插入 | 8,500 ops/s | 118 μs | 350 μs |
| 注册表查询 | 15,000 ops/s | 67 μs | 180 μs |
| Span 写入 | 45,000 ops/s | 22 μs | 85 μs |
| 健康检查 | 500,000 ops/s | 2 μs | 8 μs |

### 内存占用

| 组件 | 内存占用 |
|------|---------|
| 核心模块 | 45 KB |
| 日志模块 | 28 KB |
| 注册表模块 | 120 KB |
| 追踪模块 | 85 KB |
| IPC 模块 | 32 KB |
| 内存模块 | 28 KB |
| **总计** | **~338 KB** |

---

## ✅ 最佳实践

### 1. 批量写入优化

```c
/* ✅ 推荐：批量写入减少锁竞争 */
heapstore_batch_context_t* ctx = heapstore_batch_begin(100);
for (int i = 0; i < 100; i++) {
    heapstore_batch_add_log(ctx, "service", 2, "批量消息 %d", i);
}
heapstore_batch_commit(ctx);
heapstore_batch_context_destroy(ctx);

/* ❌ 避免：频繁单条写入 */
for (int i = 0; i < 100; i++) {
    heapstore_LOG_INFO("service", NULL, "单条消息 %d", i);
}
```

### 2. 错误处理规范

```c
/* ✅ 检查所有返回值 */
agentos_error_t err = heapstore_integration_init("/path");
if (err != AGENTOS_SUCCESS) {
    fprintf(stderr, "初始化失败：%d\n", err);
    return 1;
}

/* ✅ 使用熔断器提供的容错能力 */
heapstore_circuit_state_t state = heapstore_get_circuit_state();
if (state == HEAPSTORE_CIRCUIT_OPEN) {
    /* 熔断中，使用内存降级 */
}
```

---

## 🔧 故障排查

### 常见问题

#### 问题 1: 初始化失败

**错误**: `heapstore_ERR_DIR_CREATE_FAILED`

**解决方案**:
```bash
# 检查权限
ls -la /var/lib/agentos/
# 修复权限
sudo chown -R $USER:$USER /var/lib/agentos/
```

#### 问题 2: 数据库锁定

**错误**: `heapstore_ERR_DB_QUERY_FAILED`

**解决方案**:
```bash
# 检查数据库状态
sqlite3 heapstore/registry/agents.db "PRAGMA integrity_check;"
# 修复数据库
sqlite3 heapstore/registry/agents.db "VACUUM;"
```

---

## 📖 相关文档

- [AgentOS 架构设计](../manuals/ARCHITECTURAL_PRINCIPLES.md)
- [C 语言编码规范](../manuals/specifications/coding_standard/C_coding_style_guide.md)
- [CoreLoopThree 架构](../manuals/architecture/coreloopthree.md)
- [MemoryRovol 架构](../manuals/architecture/memoryrovol.md)

---

## 📄 许可证

Copyright © 2026 SPHARX Ltd. All Rights Reserved.

根据 **Apache License 2.0** 授权。

---

**"From data intelligence emerges."** - SPHARX

---

*最后更新*: 2026-04-01
*维护者*: AgentOS Architecture Team
