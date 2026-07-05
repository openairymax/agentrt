# Logging — 日志系统

**模块路径**: `agentrt/commons/utils/logging/`
**版本**: v0.1.0

## 概述

Logging 模块是 AgentRT 的统一分层日志系统，采用三层架构设计（Core→Atomic→Service），提供高性能、可配置、可观测的日志记录能力。核心层提供基础日志接口，原子层提供无锁队列和批量写入，服务层提供轮转、过滤、传输和监控统计等高级功能。

## 设计目标

- **高性能写入**：无锁环形缓冲区，最小化业务代码的日志写入开销
- **三层分离**：核心格式化、原子写入、服务聚合三层各司其职
- **多模式支持**：同步写入（调试）/ 异步写入（生产）/ 安全审计三种模式
- **结构化输出**：支持文本、JSON、结构化、二进制四种格式
- **动态配置**：运行时可动态调整日志级别、输出目标和格式
- **线程安全**：所有公共接口均为线程安全

## 目录结构

```
logging/
├── include/
│   ├── logging.h                # Core 层：日志核心 API（含文件输出与轮转）
│   ├── logging_compat.h         # 日志兼容层（SVC_LOG_* / AGENTRT_LOG_* 宏）
│   ├── atomic_logging.h         # Atomic 层：无锁队列与批量写入
│   └── service_logging.h        # Service 层：轮转/过滤/传输/监控
├── src/
│   ├── logging.c                # Core 层实现（含文件输出、色彩、时间戳、节流）
│   ├── logging_compat.c         # 兼容层实现
│   ├── atomic_logging.c         # Atomic 层实现
│   └── service_logging.c        # Service 层实现（console_outputter 路由到 log_write）
├── bench_atomic_logging.c       # Atomic 层性能基准测试
├── test/
│   └── test_logging_unified.c   # 统一日志测试
└── README.md                    # 本文档
```

## 三层架构

```
+-------------------------------------------------------------------+
|  Service 层（远程聚合、实时告警、日志查询、监控统计）                   |
|  轮转  |  过滤  |  传输  |  监控  |  查询  |  管理接口              |
+-------------------------------------------------------------------+
|  Atomic 层（原子写入、无锁队列、故障隔离）                            |
|  环形缓冲区  |  线程本地缓冲  |  批量提交  |  内存屏障  |  CAS      |
+-------------------------------------------------------------------+
|  Core 层（日志核心、格式化、级别过滤）                                |
|  log_write  |  LOG_DEBUG/INFO/WARN/ERROR/FATAL  |  trace_id       |
+-------------------------------------------------------------------+
```

### Core 层

日志核心层提供基础日志能力（含文件输出与轮转，原 `logging_common.c` 已合并到 `logging.c`）：

- **5 级日志级别**：DEBUG、INFO、WARN、ERROR、FATAL
- **5 种输出目标**：Console、File、Syslog、Network、Buffer
- **4 种输出格式**：Text、JSON、Structured、Binary
- **追踪 ID**：`log_set_trace_id` / `log_set_span_id`（OpenTelemetry 兼容）
- **模块级别**：`log_set_module_level`，支持通配符匹配
- **ANSI 色彩输出**：终端自动启用（INFO=蓝/WARN=黄/ERROR=红/FATAL=品红/DEBUG=灰），管道/文件自动禁用
- **文件轮转**：基于大小自动轮转，支持 backup 滑动窗口
- **节流（Throttling）**：相同日志按哈希桶限速，避免日志洪水

### Atomic 层

原子写入层确保写入的可靠性和高性能：

- **无锁环形缓冲区**：MPSC（多生产者单消费者）模型，基于 CAS 操作
- **线程本地缓冲**：每个线程独立缓冲，减少全局队列竞争
- **批量提交**：多个日志记录批量提交，减少系统调用开销
- **内存屏障**：`atomic_write_barrier` / `atomic_read_barrier` 确保可见性
- **内存池**：可选的内存池，减少动态内存分配

### Service 层

服务层提供日志聚合和可观测性能力（可选，简单应用可只使用 Core + Atomic 层）：

- **日志轮转**：基于大小/时间自动轮转，支持压缩和归档
- **日志过滤**：精确/前缀/正则/通配符匹配，支持 AND/OR 逻辑
- **日志传输**：TCP / UDP / TLS / WebSocket / Syslog 协议
- **监控统计**：吞吐量（RPS）、延迟分布（P50/P90/P99）、错误率、队列使用率
- **日志查询**：按时间、级别、模块、trace_id 等维度检索

## 日志级别

| 级别 | 枚举值 | 说明 |
|------|--------|------|
| `LOG_LEVEL_DEBUG` | 0 | 调试信息，开发环境使用 |
| `LOG_LEVEL_INFO` | 1 | 普通信息，记录正常运行状态 |
| `LOG_LEVEL_WARN` | 2 | 警告信息，可能的问题 |
| `LOG_LEVEL_ERROR` | 3 | 错误信息，功能错误 |
| `LOG_LEVEL_FATAL` | 4 | 致命错误，系统无法继续运行 |

## 接口说明

### Core 层 API

| 函数 | 说明 |
|------|------|
| `log_init(config)` | 初始化日志系统 |
| `log_cleanup()` | 清理日志系统资源 |
| `log_write(level, module, line, fmt, ...)` | 记录日志（核心写入函数） |
| `log_set_trace_id(trace_id)` | 设置当前线程追踪 ID |
| `log_set_span_id(span_id)` | 设置 OpenTelemetry Span ID |
| `log_set_module_level(pattern, level)` | 设置模块日志级别（支持通配符） |
| `log_reload_config(path)` | 热重载日志配置 |
| `log_flush()` | 刷新日志缓冲 |
| `log_set_throttle(enable, max_per_sec)` | 启用/配置日志节流 |

### 便捷宏

| 宏 | 说明 |
|------|------|
| `LOG_DEBUG(fmt, ...)` | DEBUG 级别日志 |
| `LOG_INFO(fmt, ...)` | INFO 级别日志 |
| `LOG_WARN(fmt, ...)` | WARN 级别日志 |
| `LOG_ERROR(fmt, ...)` | ERROR 级别日志 |
| `LOG_FATAL(fmt, ...)` | FATAL 级别（记录后 abort） |
| `LOG_IF(cond, level, fmt, ...)` | 条件日志 |
| `LOG_SAMPLE(level, fmt, ...)` | 采样日志（按级别采样率） |

### Atomic 层 API

| 函数 | 说明 |
|------|------|
| `atomic_logging_init(config)` | 初始化原子层 |
| `atomic_logging_submit_lockfree(record, non_blocking)` | 无锁提交日志记录 |
| `atomic_logging_submit_mutex(record)` | 互斥锁提交日志记录 |
| `atomic_logging_submit_batch(records, count)` | 批量提交日志记录 |
| `atomic_logging_acquire(record, timeout_ms)` | 获取日志记录 |
| `atomic_logging_get_stats(out_stats)` | 获取原子层统计信息 |
| `atomic_logging_flush()` | 刷新原子层 |
| `atomic_logging_cleanup()` | 清理原子层 |

### Service 层 API

| 函数 | 说明 |
|------|------|
| `service_logging_init(config)` | 初始化服务层 |
| `service_logging_configure_rotation(config)` | 配置日志轮转 |
| `service_logging_configure_filtering(config)` | 配置日志过滤 |
| `service_logging_configure_transport(config)` | 配置日志传输 |
| `service_logging_configure_monitoring(config)` | 配置监控统计 |
| `service_logging_add_filter_rule(rule)` | 动态添加过滤规则 |
| `service_logging_get_monitoring_stats(stats)` | 获取监控统计 |
| `service_logging_query(query, records, max, timeout)` | 查询日志 |
| `service_logging_cleanup()` | 清理服务层 |

## 使用示例

```c
#include "logging.h"

log_config_t config = {
    .level = LOG_LEVEL_INFO,
    .outputs = (1 << LOG_OUTPUT_CONSOLE) | (1 << LOG_OUTPUT_FILE),
    .format = LOG_FORMAT_JSON,
    .file_path = "/var/log/agentos.log",
    .max_file_size = 100 * 1024 * 1024,
    .max_backup_count = 10,
    .async_mode = true,
    .enable_statistics = true
};
log_init(&config);

LOG_INFO("服务启动完成, port: %d", 8080);
LOG_WARN("内存使用率超过阈值: %.1f%%", 85.5);
LOG_ERROR("连接失败: %s", error_message);

log_set_trace_id("req-123456");
LOG_INFO("请求处理开始");

log_set_module_level("network.*", LOG_LEVEL_WARN);

log_set_throttle(true, 100);
log_cleanup();
```

## 三种写入模式

| 模式 | 说明 | 适用场景 |
|------|------|----------|
| **同步** | 直接写入目标，写入完成前阻塞 | 调试、开发环境 |
| **异步** | 写入环形缓冲区后立即返回，后台批量写入 | 生产环境（默认） |
| **安全** | 双重写入（本地 + 远程），HMAC 签名确保完整性 | 审计、合规场景 |

## 环境变量

| 变量 | 取值 | 说明 |
|------|------|------|
| `AGENTRT_LOG_COLOR` | `0` / `1` | 强制禁用/启用 ANSI 色彩输出。未设置时自动检测：终端启用，管道/文件禁用 |

**示例**：

```bash
# 强制启用色彩（即使输出到管道）
AGENTRT_LOG_COLOR=1 ./build/agentrt/daemons/gateway_d/src/gateway_d | cat

# 强制禁用色彩（即使输出到终端）
AGENTRT_LOG_COLOR=0 ./build/agentrt/daemons/gateway_d/src/gateway_d
```

## 时间源

日志时间戳使用 **`CLOCK_REALTIME`**（墙钟时间），与网络北京时间或宿主机系统时间对齐，格式为 `[YYYY-MM-DD HH:MM:SS.sss]`。

> **重要**：项目内**严禁**使用 `agentrt_time_monotonic_ns()` / `agentrt_time_monotonic_ms()`（基于 `CLOCK_MONOTONIC`，系统启动时间）作为日志时间戳。`CLOCK_MONOTONIC` 仅用于超时计算、调度、性能基准等子系统内部场景。

时间源分离原则：

| 用途 | 时钟 | API |
|------|------|-----|
| 日志时间戳 | `CLOCK_REALTIME` | `log_write()` 内部 `clock_gettime(CLOCK_REALTIME, ...)` |
| 超时/调度 | `CLOCK_MONOTONIC` | `agentrt_time_monotonic_ns()` / `agentrt_time_monotonic_ms()` |
| 墙钟时间查询 | `CLOCK_REALTIME` | `agentrt_time_realtime_ns()` |

## 性能特性

| 模式 | 延迟（P99） | 吞吐量 |
|------|------------|--------|
| 同步 | < 1μs | > 1,000,000 msg/s |
| 异步 | < 100ns | > 10,000,000 msg/s |
| 安全 | < 10μs | > 500,000 msg/s |

## 依赖关系

| 依赖 | 说明 |
|------|------|
| `atomic_compat.h` | 跨平台原子操作（环形缓冲区 CAS） |
| `memory_compat.h` | 统一内存管理 |
| `compat.h` | 平台兼容层 |
| `config_unified.h` | 日志配置热重载 |

---

© 2026 SPHARX Ltd. All Rights Reserved.
