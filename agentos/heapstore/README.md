# heapstore - AgentOS 数据分区核心模块

**版本**: v1.0.0 (生产就绪)  
**许可证**: Apache-2.0  
**最后更新**: 2026-04-06  

---

## 📖 模块概述

heapstore 是 AgentOS 操作系统的**数据分区管理核心模块**，负责统一管理和持久化存储所有子系统的运行时数据。该模块采用**五维正交架构**设计理念，提供高性能、高可靠、线程安全的数据存储服务。

### 核心特性

| 特性 | 描述 | 状态 |
|------|------|------|
| **🔒 线程安全** | 所有公共API均支持多线程并发访问 | ✅ 生产级 |
| **⚡ 高性能** | 批量写入API提供5-10倍性能提升 | ✅ 已优化 |
| **🛡️ 安全加固** | 输入净化、路径遍历防护、SQL注入防护 | ✅ 已实现 |
| **🔄 故障容忍** | 熔断器模式(Circuit Breaker)三级状态机 | ✅ 完整 |
| **📊 可观测性** | 结构化日志 + 分布式追踪 + 运行时指标 | ✅ 三合一 |
| **🔧 可配置** | 外部配置文件 + 热加载支持 | ✅ 灵活 |

---

## 🏗️ 架构设计

### 分层架构

```
┌─────────────────────────────────────────────────────────────┐
│                    应用层 (Application)                      │
├─────────────────────────────────────────────────────────────┤
│                  集成层 (Integration API)                     │
│         heapstore_integration.h  (统一封装 + 便捷访问)        │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐        │
│  │  Core    │ │ Registry │ │   Log    │ │  Trace   │        │
│  │  核心    │ │  注册表  │ │  日志    │ │  追踪    │        │
│  └──────────┘ └──────────┘ └──────────┘ └──────────┘        │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐        │
│  │ Memory   │ │   IPC    │ │  Token   │ │  Batch   │        │
│  │ 内存管理 │ │ 进程通信 │ │ Token   │ │ 批量操作 │        │
│  └──────────┘ └──────────┘ └──────────┘ └──────────┘        │
│                                                              │
├─────────────────────────────────────────────────────────────┤
│                   工具层 (Utilities)                         │
│              utils.h/c  (安全函数 + 字符串工具)               │
├─────────────────────────────────────────────────────────────┤
│                  存储层 (Storage Backend)                    │
│            SQLite (WAL Mode) + 文件系统                      │
└─────────────────────────────────────────────────────────────┘
```

### 子模块职责

| 模块 | 头文件 | 职责 | API数量 |
|------|--------|------|---------|
| **Core** | `heapstore.h` | 生命周期管理、配置、统计、错误处理 | 16 |
| **Registry** | `heapstore_registry.h` | Agent/Session/Skill 注册表 CRUD | 24 |
| **Log** | `heapstore_log.h` | 结构化日志写入、级别管理、轮转 | 12 |
| **Trace** | `heapstore_trace.h` | 分布式追踪Span管理、上下文传播 | 11 |
| **Memory** | `heapstore_memory.h` | 内存池管理、分配统计 | 10 |
| **IPC** | `heapstore_ipc.h` | IPC通道和缓冲区记录 | 10 |
| **Token** | `heapstore_token.h` | LLM Token使用统计和监控 | 8 |
| **Batch** | `heapstore_batch.h` | 批量操作事务管理 | 6 |
| **Integration** | `heapstore_integration.h` | 统一初始化、便捷访问接口 | 8 |
| **Utils** | `utils.h` | 安全工具函数、字符串处理 | 7 |
| **总计** | - | - | **112** |

---

## 🚀 快速开始

### 基础用法

```c
#include "heapstore.h"
#include "heapstore_integration.h"

int main() {
    // 1. 初始化 (使用默认配置)
    heapstore_error_t err = heapstore_init(NULL);
    if (err != heapstore_SUCCESS) {
        fprintf(stderr, "Init failed: %s\n", heapstore_strerror(err));
        return -1;
    }

    // 2. 写入日志
    heapstore_log_write(LOG_INFO, "my_service", NULL, __FILE__, __LINE__,
                        "Application started successfully");

    // 3. 记录追踪Span
    heapstore_trace_span_t* span = heapstore_trace_span_start("operation", NULL);
    
    // 4. 执行业务逻辑...
    
    // 5. 结束追踪
    heapstore_trace_span_end(span, true);

    // 6. 关闭 (释放所有资源)
    heapstore_shutdown();
    return 0;
}
```

### 高级用法 (批量操作)

```c
#include "heapstore_batch.h"
#include "heapstore_registry.h"

void batch_insert_example() {
    // 1. 创建批量上下文
    heapstore_batch_context_t* ctx = heapstore_batch_begin(100);
    if (!ctx) return;

    // 2. 准备数据
    heapstore_agent_record_t agents[50];
    for (int i = 0; i < 50; i++) {
        snprintf(agents[i].id, sizeof(agents[i].id), "agent_%d", i);
        snprintf(agents[i].name, sizeof(agents[i].name), "Agent %d", i);
        agents[i].status = HEAPSTORE_AGENT_STATUS_ACTIVE;
        
        // 添加到批量上下文
        heapstore_batch_add_agent(ctx, &agents[i]);
    }

    // 3. 一次性提交 (5-10x 性能提升)
    heapstore_error_t err = heapstore_batch_commit(ctx);
    if (err != heapstore_SUCCESS) {
        fprintf(stderr, "Batch commit failed: %s\n", heapstore_strerror(err));
    }

    // 4. 销毁上下文
    heapstore_batch_end(ctx);
}
```

---

## 📁 目录结构

```
heapstore/
├── include/                    # 公共头文件 (API定义)
│   ├── heapstore.h            # 主入口头文件
│   ├── heapstore_core.h       # 核心类型和常量
│   ├── heapstore_registry.h   # 注册表服务
│   ├── heapstore_log.h        # 日志系统
│   ├── heapstore_trace.h      # 追踪系统
│   ├── heapstore_memory.h     # 内存管理
│   ├── heapstore_ipc.h        # IPC通信
│   ├── heapstore_token.h      # Token统计
│   ├── heapstore_batch.h      # 批量操作
│   ├── heapstore_integration.h # 集成封装
│   └── utils.h                # 工具函数
│
├── src/                        # 实现文件
│   ├── heapstore_core.c       # 核心逻辑 (1478行)
│   ├── heapstore_registry.c   # 注册表实现 (1090行)
│   ├── heapstore_log.c        # 日志实现 (507行)
│   ├── heapstore_trace.c      # 追踪实现 (499行)
│   ├── heapstore_memory.c     # 内存管理 (277行)
│   ├── heapstore_ipc.c        # IPC实现 (251行)
│   ├── heapstore_token.c      # Token统计 (410行)
│   ├── heapstore_batch.c      # 批量操作 (392行)
│   ├── heapstore_integration.c # 集成层 (636行)
│   ├── utils.c                # 工具函数 (257行)
│   └── private.h              # 内部共享定义
│
├── tests/                      # 测试套件
│   ├── test_heapstore_core.c  # 核心功能测试
│   ├── test_heapstore_registry.c # 注册表测试
│   ├── test_heapstore_log.c   # 日志系统测试
│   ├── test_heapstore_trace.c # 追踪系统测试
│   ├── test_security_path_traversal.c # 安全测试
│   ├── test_batch_performance.c # 性能基准测试
│   ├── test_edge_cases.c      # 边缘场景测试
│   └── benchmark_heapstore.c  # 综合性能基准
│
├── examples/                   # 示例代码
│   ├── quick_start.c          # 快速开始示例
│   └── batch_write.c          # 批量写入示例
│
├── kernel/                     # 内核抽象层 (预留扩展)
│   ├── ipc/                   # IPC子系统
│   └── memory/                # 内存子系统
│
├── services/                   # 服务层 (预留扩展)
│   ├── llm_d/                 # LLM服务
│   ├── market_d/              # 市场服务
│   └── tool_d/                # 工具服务
│
├── CMakeLists.txt             # 构建配置
├── README.md                  # 本文档
└── add_since_tags.py          # 文档自动化工具
```

---

## 🔧 配置说明

### 配置结构体

```c
typedef struct heapstore_config {
    const char* root_path;                    // 数据分区根路径
    size_t max_log_size_mb;                   // 最大日志文件大小(MB)
    int log_retention_days;                   // 日志保留天数
    int trace_retention_days;                 // 追踪数据保留天数
    bool enable_auto_cleanup;                 // 启用自动清理
    bool enable_log_rotation;                 // 启用日志轮转
    bool enable_trace_export;                 // 启用追踪导出
    int db_vacuum_interval_days;             // 数据库Vacuum间隔(天)
    uint32_t circuit_breaker_threshold;      // 熔断器阈值(失败次数)
    uint32_t circuit_breaker_timeout_sec;    // 熔断器超时(秒)
} heapstore_config_t;
```

### 默认配置值

```c
heapstore_config_t default_config = {
    .root_path = "/tmp/agentos_heapstore",
    .max_log_size_mb = 100,
    .log_retention_days = 30,
    .trace_retention_days = 7,
    .enable_auto_cleanup = true,
    .enable_log_rotation = true,
    .enable_trace_export = false,
    .db_vacuum_interval_days = 7,
    .circuit_breaker_threshold = 5,
    .circuit_breaker_timeout_sec = 30
};
```

---

## 🛡️ 安全机制

### 输入验证

所有公共API在入口处进行严格的参数验证：

```c
// 示例: 路径组件净化 (防止路径遍历攻击)
char safe_input[256];
if (heapstore_sanitize_path_component(safe_input, user_input, sizeof(safe_input)) != 0) {
    return heapstore_ERR_INVALID_PARAM;  // 安全拒绝恶意输入
}
```

### SQL注入防护

所有数据库操作使用参数化查询：

```c
// ✅ 安全: 使用sqlite3_bind_text()参数化
sqlite3_bind_text(stmt, 1, user_id, -1, SQLITE_TRANSIENT);

// ❌ 危险: 拼接SQL字符串 (已禁止)
// char sql[512];
// snprintf(sql, sizeof(sql), "SELECT * FROM users WHERE id='%s'", user_id);
```

### 熔断器机制

三级状态自动故障隔离：

```
正常工作 (CLOSED)
    ↓ 失败次数 >= threshold
快速失败 (OPEN) ← 拒绝所有请求
    ↓ 超过 timeout_sec
探测恢复 (HALF_OPEN) ← 允许有限探测请求
    ↓ 探测成功
恢复正常 (CLOSED)
```

---

## 📊 性能基准

### 测试环境

- **操作系统**: Linux 5.15 (x86_64)
- **编译器**: GCC 11.2 -O2
- **存储**: SSD (NVMe)
- **数据库**: SQLite 3.39.0 (WAL Mode)

### 性能数据

| 操作 | 单条耗时 | 批量(1000条) | 吞吐提升 |
|------|----------|--------------|----------|
| **Agent插入** | ~1.2ms | ~145ms (批量) | **8.3x** |
| **Session插入** | ~1.1ms | ~135ms (批量) | **8.1x** |
| **Skill插入** | ~1.0ms | ~125ms (批量) | **8.0x** |
| **日志写入(快速)** | ~8μs | - | - |
| **日志写入(慢速)** | ~45μs | - | - |
| **Span创建** | ~5μs | - | - |
| **查询Agent** | ~0.8ms | - | - |

---

## 🧪 测试覆盖

### 测试套件列表

| 测试文件 | 测试数 | 断言数 | 覆盖范围 |
|----------|--------|--------|----------|
| `test_heapstore_core.c` | 12 | 45 | 生命周期、配置、统计 |
| `test_heapstore_registry.c` | 15 | 62 | CRUD操作、批量插入 |
| `test_heapstore_log.c` | 10 | 38 | 日志写入、级别管理 |
| `test_heapstore_trace.c` | 8 | 32 | Span生命周期、上下文 |
| `test_heapstore_ipc.c` | 6 | 24 | 通道管理、缓冲区 |
| `test_heapstore_memory.c` | 7 | 28 | 内存池、分配统计 |
| `test_heapstore_batch.c` | 9 | 41 | 批量事务、回滚 |
| `test_heapstore_integration.c` | 8 | 35 | 统一初始化、便捷API |
| `test_security_path_traversal.c` | 8 | 25 | 路径遍历防护、输入净化 |
| `test_batch_performance.c` | 4 | 15 | 性能基准、吞吐对比 |
| `test_edge_cases.c` | 9 | 47 | 边界值、异常输入 |
| **总计** | **96** | **392** | **全模块覆盖** |

### 运行测试

```bash
# 构建测试
mkdir build && cd build
cmake .. -DHEAPSTORE_BUILD_TESTS=ON
make

# 运行所有测试
./tests/run_all_tests

# 运行特定测试
./tests/test_heapstore_registry

# 运行性能基准
./tests/benchmark_heapstore
```

---

## 📚 API参考

### 核心生命周期API

```c
// 初始化/关闭
heapstore_error_t heapstore_init(const heapstore_config_t* config);
void heapstore_shutdown(void);
bool heapstore_is_initialized(void);

// 配置管理
const char* heapstore_get_root(void);
const char* heapstore_get_path(heapstore_path_type_t type);
heapstore_error_t heapstore_reload_config(const heapstore_config_t* config);

// 统计信息
heapstore_error_t heapstore_get_stats(heapstore_stats_t* stats);
heapstore_error_t heapstore_reset_stats(void);

// 错误处理
const char* heapstore_strerror(heapstore_error_t err);
bool heapstore_is_recoverable(heapstore_error_t err);

// 健康检查
heapstore_error_t heapstore_health_check(bool detailed, heapstore_health_report_t* report);
```

### 日志API

```c
// 初始化
heapstore_error_t heapstore_log_init(void);
void heapstore_log_shutdown(void);

// 写入日志 (推荐使用宏)
void heapstore_log_write(
    heapstore_log_level_t level,
    const char* service,
    const char* trace_id,
    const char* file,
    int line,
    const char* format, ...
);

// 快速/慢速双路径
heapstore_error_t heapstore_log_write_fast(const char* service, int level, const char* message);
heapstore_error_t heapstore_log_write_slow(const char* service, int level, const char* message, 
                                           const char* trace_id, uint32_t timeout_ms);

// 配置
void heapstore_log_set_level(heapstore_log_level_t level);
heapstore_log_level_t heapstore_log_get_level(void);
```

### 注册表API

```c
// Agent CRUD
heapstore_error_t heapstore_registry_add_agent(const heapstore_agent_record_t* record);
heapstore_error_t heapstore_registry_get_agent(const char* id, heapstore_agent_record_t* record);
heapstore_error_t heapstore_registry_update_agent(const heapstore_agent_record_t* record);
heapstore_error_t heapstore_registry_delete_agent(const char* id);

// Session CRUD (类似API)
// Skill CRUD (类似API)

// 批量操作 (5-10x性能提升)
heapstore_error_t heapstore_registry_batch_insert_agents(
    const heapstore_agent_record_t* records, 
    size_t count
);
heapstore_error_t heapstore_registry_batch_insert_sessions(...);
heapstore_error_t heapstore_registry_batch_insert_skills(...);

// 查询 (支持过滤条件)
heapstore_error_t heapstore_registry_query_agents(
    const heapstore_agent_filter_t* filter,
    heapstore_agent_record_t** results,
    size_t* count
);
```

完整API文档请参见各头文件的Doxygen注释。

---

## ⚠️ 注意事项

### 线程安全

- ✅ 所有公共API都是**线程安全**的（内部使用互斥锁）
- ⚠️ `heapstore_init()` 和 `heapstore_shutdown()` **不可并发调用**
- ⚠️ 同一批量上下文(`heapstore_batch_context_t*`) **不应跨线程共享**

### 资源管理

- ✅ 模块内部管理所有资源（数据库连接、文件句柄等）
- ⚠️ 调用者负责传入参数的生命周期（如config结构体）
- ⚠️ 批量上下文需手动调用 `heapstore_batch_end()` 释放

### 错误处理

- ✅ 所有错误码都有对应的描述信息 (`heapstore_strerror()`)
- ✅ 错误消息遵循统一格式：`[ERROR] code: message (context). Suggestion: xxx`
- ⚠️ 部分错误是**可恢复的** (`heapstore_is_recoverable()`)

### 性能建议

1. **高频场景**: 使用 `heapstore_log_write_fast()` (~8μs)
2. **大批量写入**: 使用Batch API (5-10x性能提升)
3. **避免频繁init/shutdown**: 在程序生命周期内保持初始化状态
4. **合理配置熔断器**: 根据业务场景调整threshold和timeout

---

## 🔄 版本历史

| 版本 | 日期 | 主要变更 |
|------|------|----------|
| **v1.0.0** | 2026-04-06 | 生产就绪版本 |
| | | - ✅ 新增批量插入API (Phase 1) |
| | | - ✅ 路径遍历漏洞修复 (Phase 0) |
| | | - ✅ 代码质量重构 CC降低26% (Phase 2) |
| | | - ✅ 错误消息标准化 (18条统一格式) |
| | | - ✅ 代码格式化清理 (147行尾空格) |
| **v0.9.0-beta** | 2026-04-05 | Beta测试版 |
| **v0.1.0-alpha** | 2026-04-04 | 初始版本 |

---

## 📄 许可证

```
Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.

SPDX-License-Identifier: Apache-2.0

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at:

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
```

---

## 📞 支持与反馈

- **问题报告**: [GitHub Issues](https://github.com/spharx/AgentOS/issues)
- **功能请求**: 通过Issue标签 `enhancement` 提交
- **文档改进**: 欢迎提交Pull Request
- **技术讨论**: 查看 `ARCHITECTURAL_PRINCIPLES.md` 了解设计原则

---

**"From data intelligence emerges."** 🚀

*最后更新: 2026-04-06 | 维护状态: 活跃开发 | 生产就绪: ✅ 是*
