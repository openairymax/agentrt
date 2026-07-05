# Observability — 可观测性模块

**模块路径**: `agentrt/commons/utils/observability/`
**版本**: v0.1.0

## 概述

Observability 模块提供 AgentRT 系统的三大可观测性支柱：日志（Logging）、指标（Metrics）和链路追踪（Tracing）。该模块为所有 AgentRT 组件提供统一的运行时可见性，帮助开发者监控系统行为、诊断问题和优化性能。日志记录系统事件，指标收集量化数据，追踪则还原请求的完整调用链路。

## 设计目标

- **统一日志接口**：分级日志（DEBUG/INFO/WARN/ERROR/FATAL），支持追踪 ID 关联，自动记录文件名和行号
- **指标收集**：支持 Counter（计数器）、Gauge（仪表值）和 Timing（耗时统计）三种指标类型，可导出 JSON 或 Prometheus 格式
- **链路追踪**：符合 OpenTelemetry 规范的 Span 生命周期管理，支持事件注解和 JSON 导出
- **兼容层**：提供 `observability_compat.h` 向后兼容旧版 API
- **零配置启动**：使用 GCC constructor 属性自动初始化日志系统

## 目录结构

```
observability/
├── include/
│   ├── observability.h          # 可观测性统一接口（日志、指标、追踪）
│   ├── observability_compat.h   # 向后兼容层（类型别名和函数映射）
│   ├── logger.h                 # 日志接口定义（含日志级别常量）
│   ├── metrics.h                # 指标收集接口定义
│   └── trace.h                  # 链路追踪接口定义
├── src/
│   ├── logger.c                 # 日志模块实现（适配统一日志系统）
│   ├── metrics.c                # 指标收集实现
│   └── trace.c                  # 链路追踪实现
└── README.md                    # 本文档
```

## 核心数据结构

### 日志级别

| 常量 | 值 | 说明 |
|------|-----|------|
| `AGENTRT_LOG_LEVEL_DEBUG` | 0 | 调试信息 |
| `AGENTRT_LOG_LEVEL_INFO` | 1 | 一般信息 |
| `AGENTRT_LOG_LEVEL_WARN` | 2 | 警告信息 |
| `AGENTRT_LOG_LEVEL_ERROR` | 3 | 错误信息 |
| `AGENTRT_LOG_LEVEL_FATAL` | 4 | 致命错误（记录后调用 abort） |

### 指标类型

| 类型 | 内部结构 | 说明 |
|------|----------|------|
| Counter | `metric_counter_t`（name + value 链表） | 单调递增计数器，如请求总数 |
| Gauge | `metric_gauge_t`（name + value 链表） | 可增可减的瞬时值，如内存使用量 |
| Timing | `metric_timing_t`（name + sum + count 链表） | 耗时统计，记录总和与次数，可计算平均值 |

### agentrt_trace_span_t — 追踪跨度

Span 是不透明句柄，内部包含：

| 字段 | 说明 |
|------|------|
| `trace_id` | 追踪 ID（64 字符，格式 `tr-{timestamp}-{counter}`） |
| `span_id` | Span ID（32 字符，格式 `sp-{timestamp}-{counter}`） |
| `parent_id` | 父 Span ID（可选） |
| `name` | Span 名称（128 字符） |
| `start_time` / `end_time` | 开始/结束时间戳（微秒） |
| `status` | 原子状态：0=运行中，1=完成，2=错误 |
| `events` | 事件链表（最多 64 个事件） |

## 接口说明

### 日志 API

| 函数/宏 | 说明 |
|------|------|
| `agentrt_log_set_trace_id(trace_id)` | 设置当前追踪 ID（NULL 则自动生成） |
| `agentrt_log_get_trace_id()` | 获取当前追踪 ID |
| `agentrt_log_write(level, file, line, fmt, ...)` | 底层日志写入函数 |
| `AGENTRT_LOG_DEBUG(fmt, ...)` | 调试日志宏（Release 模式下编译为空） |
| `AGENTRT_LOG_INFO(fmt, ...)` | 信息日志宏 |
| `AGENTRT_LOG_WARN(fmt, ...)` | 警告日志宏 |
| `AGENTRT_LOG_ERROR(fmt, ...)` | 错误日志宏 |
| `AGENTRT_LOG_FATAL(fmt, ...)` | 致命日志宏（记录后调用 abort） |

### 指标 API

| 函数 | 说明 |
|------|------|
| `agentrt_metrics_create()` | 创建指标收集器，返回句柄 |
| `agentrt_metrics_destroy(metrics)` | 销毁指标收集器，释放所有指标 |
| `agentrt_metrics_increment(metrics, name, value)` | 增加计数器（如不存在则自动创建） |
| `agentrt_metrics_gauge(metrics, name, value)` | 设置仪表值（如不存在则自动创建） |
| `agentrt_metrics_timing(metrics, name, duration_ms)` | 记录耗时（自动累加 sum 和 count） |
| `agentrt_metrics_export(metrics)` | 导出所有指标为 JSON 字符串（需手动释放） |
| `agentrt_metrics_export_prometheus(metrics)` | 导出为 Prometheus 格式字符串 |
| `agentrt_metrics_export_prometheus_filtered(metrics, prefix)` | 按前缀过滤导出 Prometheus 格式 |

### 追踪 API

| 函数 | 说明 |
|------|------|
| `agentrt_trace_begin(name, parent_id)` | 开始一个追踪 Span，返回句柄 |
| `agentrt_trace_end(span)` | 结束一个 Span（记录结束时间，状态设为完成） |
| `agentrt_trace_add_event(span, name, attributes)` | 向 Span 添加事件（最多 64 个） |
| `agentrt_trace_export()` | 导出所有 Span 为 JSON 字符串（需手动释放） |
| `agentrt_trace_cleanup()` | 清理所有追踪数据 |
| `agentrt_trace_get_span_count()` | 获取当前活跃 Span 数量 |

### 兼容层 API（observability_compat.h）

| 函数 | 说明 |
|------|------|
| `agentrt_observability_create()` | 映射到 `agentrt_metrics_create()` |
| `agentrt_observability_destroy(obs)` | 映射到 `agentrt_metrics_destroy()` |
| `agentrt_observability_register_metric(obs, name, type, desc)` | 注册指标（新 API 自动注册，直接忽略） |
| `agentrt_observability_increment_counter(obs, label, value)` | 映射到 `agentrt_metrics_increment()` |
| `agentrt_observability_record_histogram(obs, name, value)` | 映射到 `agentrt_metrics_timing()` |
| `agentrt_get_monotonic_time_ns()` | 获取单调时钟纳秒时间戳 |

## 使用示例

```c
#include "observability.h"
#include "logger.h"
#include "metrics.h"
#include "trace.h"

// ===== 日志 =====
agentrt_log_set_trace_id(NULL);  // 自动生成追踪 ID
AGENTRT_LOG_INFO("Agent initialization started");
AGENTRT_LOG_DEBUG("Config loaded from %s", config_path);
AGENTRT_LOG_WARN("Retry count exceeded threshold: %d", retries);
AGENTRT_LOG_ERROR("Failed to connect to database: %s", db_error);

// ===== 指标 =====
agentrt_metrics_t *metrics = agentrt_metrics_create();

agentrt_metrics_increment(metrics, "requests_total", 1);
agentrt_metrics_gauge(metrics, "memory_usage_mb", 128.5);
agentrt_metrics_timing(metrics, "request_duration_ms", 42.3);

// 导出 JSON
char *json = agentrt_metrics_export(metrics);
printf("Metrics JSON: %s\n", json);
agentrt_free(json);

// 导出 Prometheus 格式
char *prom = agentrt_metrics_export_prometheus(metrics);
printf("Prometheus:\n%s\n", prom);
agentrt_free(prom);

agentrt_metrics_destroy(metrics);

// ===== 追踪 =====
agentrt_trace_span_t *root = agentrt_trace_begin("handle_request", NULL);
agentrt_trace_add_event(root, "request_received", "{\"method\":\"GET\"}");

agentrt_trace_span_t *child = agentrt_trace_begin("db_query", /*parent_id*/ NULL);
agentrt_trace_add_event(child, "query_started", "{\"sql\":\"SELECT ...\"}");
// ... 执行数据库查询 ...
agentrt_trace_add_event(child, "query_completed", "{\"rows\":42}");
agentrt_trace_end(child);

agentrt_trace_add_event(root, "response_sent", "{\"status\":200}");
agentrt_trace_end(root);

// 导出追踪数据
char *trace_json = agentrt_trace_export();
printf("Trace JSON: %s\n", trace_json);
agentrt_free(trace_json);

agentrt_trace_cleanup();
```

## 日志级别过滤

可通过编译时宏 `AGENTRT_LOG_LEVEL` 控制最低日志输出级别：

```c
// 编译时指定：-DAGENTRT_LOG_LEVEL=AGENTRT_LOG_LEVEL_WARN
// 则 DEBUG 和 INFO 级别日志不会输出

// Release 模式下，AGENTRT_LOG_DEBUG 自动编译为空操作
#ifndef AGENTRT_LOG_DEBUG
#ifdef AGENTRT_DEBUG
#define AGENTRT_LOG_DEBUG(fmt, ...) \
    agentrt_log_write(AGENTRT_LOG_LEVEL_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#else
#define AGENTRT_LOG_DEBUG(fmt, ...) ((void)0)
#endif
#endif
```

## Prometheus 导出格式

`agentrt_metrics_export_prometheus()` 输出符合 Prometheus exposition format：

```
# TYPE requests_total counter
requests_total 1523
# TYPE memory_usage_mb gauge
memory_usage_mb 128.5
# TYPE request_duration_ms summary
request_duration_ms_sum 42.3
request_duration_ms_count 1
```

## 追踪系统限制

| 参数 | 值 | 说明 |
|------|-----|------|
| MAX_SPANS | 1024 | 最大 Span 数量 |
| MAX_EVENTS_PER_SPAN | 64 | 每个 Span 最大事件数 |
| MAX_TRACE_ID_LEN | 64 | 追踪 ID 最大长度 |
| MAX_SPAN_ID_LEN | 32 | Span ID 最大长度 |

## 追踪 JSON 导出格式

```json
[
  {
    "trace_id": "tr-67890abc-0000000000000001",
    "span_id": "sp-67890abc-0000000000000001",
    "name": "handle_request",
    "start_time": 1712345678000000,
    "end_time": 1712345678123456,
    "duration_us": 123456,
    "status": "ok",
    "events": [
      {"name": "request_received", "timestamp": 1712345678000000, "attributes": {"method":"GET"}},
      {"name": "response_sent", "timestamp": 1712345678123456, "attributes": {"status":200}}
    ]
  }
]
```

## 依赖关系

| 依赖 | 说明 |
|------|------|
| `logging_compat.h` | 日志系统兼容层，映射到统一日志系统 |
| `memory_compat.h` | 统一内存管理宏 |
| `string_compat.h` | 字符串操作兼容层 |
| `atomic_compat.h` | 跨平台原子操作（追踪模块使用） |
| `platform.h` | 跨平台时间戳获取 |
| `error.h` | 统一错误码定义 |
| `cjson/cJSON.h` | JSON 序列化（可选，通过 `AGENTRT_NO_CJSON` 禁用） |

---

© 2026 SPHARX Ltd. All Rights Reserved.