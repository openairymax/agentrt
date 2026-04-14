Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
"From data intelligence emerges."

# AgentOS 可观测性 API

**版本**: Doc V1.8  
**最后更新**: 2026-04-09  
**状态**: 🟢 生产就绪

---

## 🎯 概述

可观测性 API 提供 AgentOS 三支柱（指标、日志、追踪）的完整访问接口。基于**工程控制论**的反馈闭环原则，支持三时间尺度监控：实时（τ<100ms）、轮次内（任务周期）、跨轮次（会话级）。

### 🧩 五维正交原则体现

可观测性 API 是五维正交原则中**工程观**的直接体现，同时也贯穿其他四个维度：

| 维度 | 原则体现 | 具体实现 |
|------|----------|---------|
| **系统观** | 基于工程控制论的反馈闭环 | 三时间尺度监控，实时反馈调节系统行为 |
| **内核观** | 可观测性数据的标准化契约 | 统一的日志格式、指标定义、追踪规范 |
| **认知观** | 认知过程的透明化监控 | 双系统决策过程追踪，注意力分配可视化 |
| **工程观** | 安全内生与可观测性设计 | 三支柱（日志、指标、追踪）完整覆盖，自动错误追踪 |
| **设计美学** | 优雅的监控数据组织 | 结构化的日志格式，直观的性能指标，完整的调用链 |

---

## 📋 API 索引

| 函数 | 描述 | 复杂度 |
|------|------|--------|
| `agentos_log_write()` | 写入日志记录 | O(1) |
| `agentos_log_query()` | 查询日志记录 | O(n) |
| `agentos_metric_record()` | 记录指标数据 | O(1) |
| `agentos_metric_query()` | 查询指标数据 | O(n) |
| `agentos_trace_start()` | 开始追踪 span | O(1) |
| `agentos_trace_end()` | 结束追踪 span | O(1) |
| `agentos_trace_context_get()` | 获取当前追踪上下文 | O(1) |
| `agentos_trace_inject()` | 注入追踪上下文 | O(1) |
| `agentos_telemetry_export()` | 导出遥测数据 | O(n) |
| `agentos_telemetry_stats()` | 获取遥测统计信息 | O(1) |

---

## 🔧 核心数据结构

### 日志记录

```c
/**
 * @brief 日志记录结构体
 * 
 * 描述一条日志记录，包含级别、消息和结构化字段。
 */
typedef struct agentos_log_record {
    /** 日志级别 */
    enum {
        LOG_LEVEL_TRACE = 0,   /* 追踪级别（调试用） */
        LOG_LEVEL_DEBUG = 1,   /* 调试级别 */
        LOG_LEVEL_INFO = 2,    /* 信息级别 */
        LOG_LEVEL_WARN = 3,    /* 警告级别 */
        LOG_LEVEL_ERROR = 4,   /* 错误级别 */
        LOG_LEVEL_FATAL = 5    /* 致命级别 */
    } level;
    
    /** 日志模块名 */
    char module[64];
    
    /** 日志消息 */
    char* message;
    
    /** 消息长度 */
    size_t message_len;
    
    /** 结构化字段（JSON 格式） */
    char* fields;
    
    /** 日志时间戳（Unix 毫秒） */
    uint64_t timestamp;
    
    /** trace_id（用于关联追踪） */
    char trace_id[128];
    
    /** span_id（当前 span 标识） */
    char span_id[32];
    
    /** 来源 Agent ID */
    char agent_id[64];
    
    /** 来源进程 ID */
    uint32_t process_id;
    
    /** 来源线程 ID */
    uint64_t thread_id;
    
    /** 文件名（可选） */
    char file[256];
    
    /** 行号（可选） */
    uint32_t line;
    
    /** 函数名（可选） */
    char function[128];
} agentos_log_record_t;
```

### 指标数据点

```c
/**
 * @brief 指标数据点结构体
 */
typedef struct agentos_metric_datapoint {
    /** 指标名称 */
    char name[128];
    
    /** 指标类型 */
    enum {
        METRIC_TYPE_COUNTER = 0,    /* 计数器：只增不减 */
        METRIC_TYPE_GAUGE = 1,      /* 仪表：可增可减 */
        METRIC_TYPE_HISTOGRAM = 2,  /* 直方图：分布统计 */
        METRIC_TYPE_SUMMARY = 3     /* 摘要：分位数统计 */
    } type;
    
    /** 指标值 */
    double value;
    
    /** 指标标签（键值对） */
    char** label_keys;
    char** label_values;
    uint32_t label_count;
    
    /** 时间戳（Unix 毫秒） */
    uint64_t timestamp;
    
    /** 采样间隔（毫秒，用于直方图） */
    uint32_t interval_ms;
} agentos_metric_datapoint_t;
```

### 追踪 Span

```c
/**
 * @brief 追踪 Span 结构体
 * 
 * 描述一个追踪 span，包含操作名、时间和关联信息。
 */
typedef struct agentos_trace_span {
    /** Span 唯一标识符 */
    char span_id[32];
    
    /** 所属 Trace ID */
    char trace_id[128];
    
    /** 父 Span ID（根 span 为空） */
    char parent_span_id[32];
    
    /** 操作名称 */
    char operation_name[256];
    
    /** 开始时间戳（Unix 微秒） */
    uint64_t start_time_us;
    
    /** 结束时间戳（Unix 微秒） */
    uint64_t end_time_us;
    
    /** Span 持续时间（微秒） */
    uint64_t duration_us;
    
    /** Span 状态 */
    enum {
        SPAN_STATUS_OK = 0,        /* 成功 */
        SPAN_STATUS_ERROR = 1,     /* 错误 */
        SPAN_STATUS_UNSET = 2      /* 未设置 */
    } status;
    
    /** 状态描述 */
    char status_message[512];
    
    /** Span 标签 */
    char** attribute_keys;
    char** attribute_values;
    uint32_t attribute_count;
    
    /** Span 事件 */
    struct {
        char name[128];
        uint64_t timestamp_us;
        char** attribute_keys;
        char** attribute_values;
        uint32_t attribute_count;
    }* events;
    uint32_t event_count;
    
    /** Span 类型 */
    enum {
        SPAN_KIND_INTERNAL = 0,    /* 内部操作 */
        SPAN_KIND_SERVER = 1,      /* 服务端 */
        SPAN_KIND_CLIENT = 2,      /* 客户端 */
        SPAN_KIND_PRODUCER = 3,    /* 生产者 */
        SPAN_KIND_CONSUMER = 4     /* 消费者 */
    } kind;
    
    /** 来源 Agent ID */
    char agent_id[64];
} agentos_trace_span_t;
```

### 遥测统计信息

```c
/**
 * @brief 遥测统计信息结构体
 */
typedef struct agentos_telemetry_statistics {
    /** 日志统计 */
    struct {
        uint64_t total_records;        /* 总记录数 */
        uint64_t records_by_level[6];  /* 各级别记录数 */
        uint64_t storage_bytes;        /* 存储大小 */
        float avg_write_latency_ms;    /* 平均写入延迟 */
    } log_stats;
    
    /** 指标统计 */
    struct {
        uint64_t total_datapoints;     /* 总数据点数 */
        uint64_t unique_metrics;       /* 唯一指标数 */
        uint64_t storage_bytes;        /* 存储大小 */
        float avg_write_latency_ms;    /* 平均写入延迟 */
    } metric_stats;
    
    /** 追踪统计 */
    struct {
        uint64_t total_traces;         /* 总 trace 数 */
        uint64_t total_spans;          /* 总 span 数 */
        uint64_t avg_spans_per_trace;  /* 平均 span/trace */
        float avg_trace_duration_ms;   /* 平均 trace 时长 */
        uint64_t storage_bytes;        /* 存储大小 */
    } trace_stats;
    
    /** 导出统计 */
    struct {
        uint64_t export_count;         /* 导出次数 */
        uint64_t export_bytes;         /* 导出字节数 */
        uint32_t last_export_time_ms;  /* 最后导出耗时 */
    } export_stats;
} agentos_telemetry_statistics_t;
```

---

## 📖 API 详细说明

### `agentos_log_write()`

```c
/**
 * @brief 写入日志记录
 * 
 * 将日志记录写入日志系统，支持结构化字段和追踪关联。
 * 
 * @param level 日志级别
 * @param module 模块名（用于过滤和分类）
 * @param message 日志消息（格式化字符串）
 * @param fields 结构化字段（JSON 格式，可选）
 * @return AGENTOS_SUCCESS 成功；其他值表示错误
 * 
 * @threadsafe 是
 * @reentrant 是
 * @see agentos_log_query()
 */
AGENTOS_API agentos_error_t
agentos_log_write(agentos_log_level_t level,
                  const char* module,
                  const char* message,
                  const char* fields);
```

**使用示例**：
```c
// 基本日志
agentos_log_write(LOG_LEVEL_INFO, "corekern", "IPC connection established", NULL);

// 带结构化字段的日志
agentos_log_write(
    LOG_LEVEL_ERROR,
    "memoryrovol",
    "Memory write failed: disk quota exceeded",
    "{\"record_id\": \"abc-123\", \"size_bytes\": 1024, \"quota_bytes\": 4096}"
);

// 带追踪关联的日志（自动获取当前 trace_id）
agentos_log_write(
    LOG_LEVEL_WARN,
    "coreloopthree",
    "Task execution timeout, retrying...",
    "{\"task_id\": 12345, \"retry_count\": 2, \"timeout_ms\": 5000}"
);
```

### `agentos_metric_record()`

```c
/**
 * @brief 记录指标数据
 * 
 * 将指标数据点写入指标系统，支持 Counter、Gauge、Histogram 类型。
 * 
 * @param datapoint 指标数据点
 * @return AGENTOS_SUCCESS 成功；其他值表示错误
 * 
 * @threadsafe 是
 * @reentrant 是
 * @see agentos_metric_query()
 */
AGENTOS_API agentos_error_t
agentos_metric_record(const agentos_metric_datapoint_t* datapoint);
```

**使用示例**：
```c
// Counter 类型：任务完成计数
agentos_metric_datapoint_t counter_dp = {
    .name = "agentos_tasks_completed_total",
    .type = METRIC_TYPE_COUNTER,
    .value = 1,
    .label_keys = (char*[]){"agent_id", "task_type"},
    .label_values = (char*[]){"agent_001", "analysis"},
    .label_count = 2,
    .timestamp = agentos_time_now_ms()
};
agentos_metric_record(&counter_dp);

// Gauge 类型：当前内存使用
agentos_metric_datapoint_t gauge_dp = {
    .name = "agentos_memory_usage_bytes",
    .type = METRIC_TYPE_GAUGE,
    .value = 1024 * 1024 * 512,  // 512MB
    .label_keys = (char*[]){"layer"},
    .label_values = (char*[]){"L1"},
    .label_count = 1,
    .timestamp = agentos_time_now_ms()
};
agentos_metric_record(&gauge_dp);

// Histogram 类型：请求延迟
agentos_metric_datapoint_t hist_dp = {
    .name = "agentos_request_duration_ms",
    .type = METRIC_TYPE_HISTOGRAM,
    .value = 45.2,
    .label_keys = (char*[]){"endpoint", "method"},
    .label_values = (char*[]){"memory_query", "semantic"},
    .label_count = 2,
    .timestamp = agentos_time_now_ms()
};
agentos_metric_record(&hist_dp);
```

### `agentos_trace_start()`

```c
/**
 * @brief 开始追踪 Span
 * 
 * 创建并开始一个新的追踪 span，返回 span ID。
 * 如果当前已有活跃 span，新 span 将作为其子 span。
 * 
 * @param operation_name 操作名称
 * @param kind Span 类型
 * @param attributes Span 属性（JSON 格式，可选）
 * @param out_span_id 输出参数，返回 span ID
 * @return AGENTOS_SUCCESS 成功；其他值表示错误
 * 
 * @ownership out_span_id 由调用者负责释放
 * @threadsafe 是
 * @reentrant 否
 * @see agentos_trace_end()
 */
AGENTOS_API agentos_error_t
agentos_trace_start(const char* operation_name,
                    agentos_span_kind_t kind,
                    const char* attributes,
                    char** out_span_id);
```

### `agentos_trace_end()`

```c
/**
 * @brief 结束追踪 Span
 * 
 * 结束指定的追踪 span，计算持续时间并导出。
 * 
 * @param span_id 要结束的 span ID
 * @param status Span 状态
 * @param status_message 状态描述（可选）
 * @return AGENTOS_SUCCESS 成功；其他值表示错误
 * 
 * @threadsafe 是
 * @reentrant 否
 */
AGENTOS_API agentos_error_t
agentos_trace_end(const char* span_id,
                  agentos_span_status_t status,
                  const char* status_message);
```

**使用示例**：
```c
// 开始追踪
char* span_id = NULL;
agentos_trace_start(
    "memory_semantic_query",
    SPAN_KIND_INTERNAL,
    "{\"query\": \"user feedback\", \"limit\": 10}",
    &span_id
);

// 执行操作...
agentos_memory_query_t query = {0};
query.text = "user feedback";
query.limit = 10;
agentos_memory_result_t* results = NULL;
uint32_t count = 0;
agentos_error_t err = agentos_memory_query(&query, &results, &count);

// 结束追踪
agentos_trace_end(
    span_id,
    err == AGENTOS_SUCCESS ? SPAN_STATUS_OK : SPAN_STATUS_ERROR,
    err == AGENTOS_SUCCESS ? NULL : agentos_strerror(err)
);

free(span_id);
```

### `agentos_log_query()`

```c
/**
 * @brief 查询日志记录
 * 
 * 根据过滤条件查询日志记录。
 * 
 * @param filter 查询过滤条件（JSON 格式）
 * @param offset 分页偏移量
 * @param limit 每页数量限制
 * @param out_records [out] 返回日志记录数组（JSON 格式）
 * @param out_count [out] 返回实际数量
 * @return AGENTOS_SUCCESS 成功；其他值表示错误
 * 
 * @ownership 调用者拥有 *out_records，需通过 agentos_sys_free() 释放
 * @threadsafe 是
 * @reentrant 否
 */
AGENTOS_API agentos_error_t
agentos_log_query(const char* filter,
                  uint32_t offset,
                  uint32_t limit,
                  char** out_records,
                  uint32_t* out_count);
```

**过滤条件示例**：
```json
{
    "level_min": "WARN",
    "module": "cupolas",
    "time_range": {"start": 1672531200000, "end": 1672617600000},
    "trace_id": "trace_abc123",
    "search": "permission denied"
}
```

**返回值**：
- `AGENTOS_SUCCESS` (0x0000): 查询成功
- `AGENTOS_ERROR_INVALID_ARGUMENT` (0x1001): 过滤条件无效
- `AGENTOS_ERROR_NO_MEMORY` (0x1002): 内存不足

### `agentos_metric_query()`

```c
/**
 * @brief 查询指标数据
 * 
 * 根据名称和标签查询指标数据点。
 * 
 * @param name 指标名称（支持通配符 *）
 * @param labels 标签过滤（JSON 格式，可选）
 * @param time_range 时间范围（JSON 格式）
 * @param out_datapoints [out] 返回数据点数组（JSON 格式）
 * @return AGENTOS_SUCCESS 成功；其他值表示错误
 * 
 * @ownership 调用者拥有 *out_datapoints，需通过 agentos_sys_free() 释放
 * @threadsafe 是
 * @reentrant 否
 */
AGENTOS_API agentos_error_t
agentos_metric_query(const char* name,
                     const char* labels,
                     const char* time_range,
                     char** out_datapoints);
```

**返回值**：
- `AGENTOS_SUCCESS` (0x0000): 查询成功
- `AGENTOS_ERROR_INVALID_ARGUMENT` (0x1001): 参数无效
- `AGENTOS_ERROR_NO_MEMORY` (0x1002): 内存不足

### `agentos_trace_context_get()`

```c
/**
 * @brief 获取当前追踪上下文
 * 
 * 获取当前线程的追踪上下文信息，包括 trace_id、span_id 等。
 * 用于在服务内部传递追踪信息。
 * 
 * @param out_context [out] 返回追踪上下文（JSON 格式）
 * @return AGENTOS_SUCCESS 成功；其他值表示错误
 * 
 * @ownership 调用者拥有 *out_context，需通过 agentos_sys_free() 释放
 * @threadsafe 是
 * @reentrant 是
 * @see agentos_trace_inject()
 */
AGENTOS_API agentos_error_t
agentos_trace_context_get(char** out_context);
```

**返回 JSON 示例**：
```json
{
    "trace_id": "trace_abc123",
    "span_id": "span_def456",
    "parent_span_id": "span_ghi789",
    "sampled": true
}
```

**返回值**：
- `AGENTOS_SUCCESS` (0x0000): 获取成功
- `AGENTOS_ERROR_NO_MEMORY` (0x1002): 内存不足

### `agentos_trace_inject()`

```c
/**
 * @brief 注入追踪上下文
 * 
 * 将追踪上下文注入到当前线程，用于分布式追踪的跨服务传播。
 * 通常从请求头中提取的追踪信息通过此接口注入。
 * 
 * @param context 追踪上下文（JSON 格式）
 * @return AGENTOS_SUCCESS 成功；其他值表示错误
 * 
 * @threadsafe 是
 * @reentrant 否
 * @see agentos_trace_context_get()
 */
AGENTOS_API agentos_error_t
agentos_trace_inject(const char* context);
```

**使用示例**：
```c
// 从 HTTP 请求头提取追踪信息并注入
const char* traceparent = request_header("traceparent");
const char* tracestate = request_header("tracestate");

char context[512];
snprintf(context, sizeof(context),
    "{\"trace_id\": \"%s\", \"tracestate\": \"%s\"}",
    traceparent, tracestate);

agentos_trace_inject(context);
```

**返回值**：
- `AGENTOS_SUCCESS` (0x0000): 注入成功
- `AGENTOS_ERROR_INVALID_ARGUMENT` (0x1001): 上下文格式无效

### `agentos_telemetry_export()`

```c
/**
 * @brief 导出可观测性数据
 * 
 * 将日志、指标、追踪数据统一导出为 OpenTelemetry 兼容格式。
 * 支持按时间范围和类型筛选导出数据。
 * 
 * @param options 导出选项（JSON 格式）
 * @param out_data [out] 返回导出数据（JSON 格式）
 * @param out_size [out] 返回数据大小
 * @return AGENTOS_SUCCESS 成功；其他值表示错误
 * 
 * @ownership 调用者拥有 *out_data，需通过 agentos_sys_free() 释放
 * @threadsafe 是
 * @reentrant 否
 */
AGENTOS_API agentos_error_t
agentos_telemetry_export(const char* options,
                         char** out_data,
                         size_t* out_size);
```

**导出选项示例**：
```json
{
    "types": ["logs", "metrics", "traces"],
    "time_range": {"start": 1672531200000, "end": 1672617600000},
    "format": "otlp_json",
    "compression": "gzip"
}
```

**返回值**：
- `AGENTOS_SUCCESS` (0x0000): 导出成功
- `AGENTOS_ERROR_INVALID_ARGUMENT` (0x1001): 选项格式无效
- `AGENTOS_ERROR_NO_MEMORY` (0x1002): 内存不足

### `agentos_telemetry_stats()`

```c
/**
 * @brief 获取可观测性统计信息
 * 
 * 获取全局可观测性系统统计信息，包括日志/指标/追踪的数量、
 * 存储使用量、采样率等。
 * 
 * @param out_stats [out] 返回统计信息结构体
 * @return AGENTOS_SUCCESS 成功；其他值表示错误
 * 
 * @ownership 无堆内存分配，结构体为值类型
 * @threadsafe 是
 * @reentrant 是
 */
AGENTOS_API agentos_error_t
agentos_telemetry_stats(agentos_telemetry_statistics_t* out_stats);
```

**返回值**：
- `AGENTOS_SUCCESS` (0x0000): 获取成功
- `AGENTOS_ERROR_INVALID_ARGUMENT` (0x1001): 参数无效

---

## 🚀 使用示例

### 完整可观测性流程

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "agentos/telemetry.h"
#include "agentos/memory.h"

/**
 * @brief 执行记忆查询并记录完整遥测数据
 */
agentos_error_t execute_memory_query_with_telemetry(
    const char* query_text,
    uint32_t limit,
    agentos_memory_result_t** out_results,
    uint32_t* out_count)
{
    char* span_id = NULL;
    agentos_error_t err;
    
    // 1. 开始追踪 span
    char attributes[256];
    snprintf(attributes, sizeof(attributes),
             "{\"query\": \"%s\", \"limit\": %u}", query_text, limit);
    
    agentos_trace_start(
        "memory_semantic_query",
        SPAN_KIND_INTERNAL,
        attributes,
        &span_id
    );
    
    // 2. 记录开始日志
    agentos_log_write(
        LOG_LEVEL_INFO,
        "memoryrovol",
        "Starting memory query",
        attributes
    );
    
    // 3. 记录开始时间
    uint64_t start_time = agentos_time_now_us();
    
    // 4. 执行查询
    agentos_memory_query_t query = {0};
    query.text = (char*)query_text;
    query.limit = limit;
    query.similarity_threshold = 0.5f;
    
    err = agentos_memory_query(&query, out_results, out_count);
    
    // 5. 计算耗时
    uint64_t duration_us = agentos_time_now_us() - start_time;
    double duration_ms = duration_us / 1000.0;
    
    // 6. 记录指标
    agentos_metric_datapoint_t latency_dp = {
        .name = "agentos_memory_query_duration_ms",
        .type = METRIC_TYPE_HISTOGRAM,
        .value = duration_ms,
        .label_keys = (char*[]){"query_type"},
        .label_values = (char*[]){"semantic"},
        .label_count = 1,
        .timestamp = agentos_time_now_ms()
    };
    agentos_metric_record(&latency_dp);
    
    agentos_metric_datapoint_t result_dp = {
        .name = "agentos_memory_query_results",
        .type = METRIC_TYPE_GAUGE,
        .value = (double)*out_count,
        .label_keys = (char*[]){"query_type"},
        .label_values = (char*[]){"semantic"},
        .label_count = 1,
        .timestamp = agentos_time_now_ms()
    };
    agentos_metric_record(&result_dp);
    
    // 7. 记录结束日志
    if (err == AGENTOS_SUCCESS) {
        char log_fields[128];
        snprintf(log_fields, sizeof(log_fields),
                 "{\"duration_ms\": %.2f, \"result_count\": %u}",
                 duration_ms, *out_count);
        
        agentos_log_write(
            LOG_LEVEL_INFO,
            "memoryrovol",
            "Memory query completed successfully",
            log_fields
        );
    } else {
        char log_fields[256];
        snprintf(log_fields, sizeof(log_fields),
                 "{\"error_code\": 0x%04X, \"error_message\": \"%s\"}",
                 err, agentos_strerror(err));
        
        agentos_log_write(
            LOG_LEVEL_ERROR,
            "memoryrovol",
            "Memory query failed",
            log_fields
        );
    }
    
    // 8. 结束追踪 span
    agentos_trace_end(
        span_id,
        err == AGENTOS_SUCCESS ? SPAN_STATUS_OK : SPAN_STATUS_ERROR,
        err == AGENTOS_SUCCESS ? NULL : agentos_strerror(err)
    );
    
    free(span_id);
    return err;
}

int main(void)
{
    // 初始化遥测系统
    agentos_telemetry_config_t manager = {
        .log_level = LOG_LEVEL_INFO,
        .log_async = true,
        .metrics_enabled = true,
        .tracing_enabled = true,
        .export_interval_sec = 15
    };
    agentos_telemetry_init(&manager);
    
    // 执行带完整遥测的查询
    agentos_memory_result_t* results = NULL;
    uint32_t count = 0;
    
    agentos_error_t err = execute_memory_query_with_telemetry(
        "用户反馈",
        10,
        &results,
        &count
    );
    
    if (err == AGENTOS_SUCCESS) {
        printf("查询成功，找到 %u 条记录\n", count);
        // 处理结果...
    }
    
    // 获取遥测统计
    agentos_telemetry_statistics_t stats;
    agentos_telemetry_stats(&stats);
    
    printf("\n遥测统计:\n");
    printf("  日志记录: %llu 条\n", stats.log_stats.total_records);
    printf("  指标数据点: %llu 个\n", stats.metric_stats.total_datapoints);
    printf("  追踪数: %llu 个\n", stats.trace_stats.total_traces);
    printf("  平均 span/trace: %llu\n", stats.trace_stats.avg_spans_per_trace);
    
    return 0;
}
```

### 分布式追踪上下文传播

```c
// 服务端：从请求中提取追踪上下文
void handle_request(const char* trace_context_header)
{
    // 注入追踪上下文
    agentos_trace_context_t ctx;
    agentos_trace_extract(trace_context_header, &ctx);
    
    // 开始服务端 span
    char* span_id = NULL;
    agentos_trace_start(
        "handle_memory_query_request",
        SPAN_KIND_SERVER,
        "{\"http.method\": \"POST\", \"http.url\": \"/api/memory/query\"}",
        &span_id
    );
    
    // 处理请求...
    
    // 结束 span
    agentos_trace_end(span_id, SPAN_STATUS_OK, NULL);
    free(span_id);
}

// 客户端：向请求中注入追踪上下文
void make_request(void)
{
    // 开始客户端 span
    char* span_id = NULL;
    agentos_trace_start(
        "call_memory_service",
        SPAN_KIND_CLIENT,
        "{\"rpc.system\": \"http\", \"rpc.service\": \"memory\"}",
        &span_id
    );
    
    // 获取当前追踪上下文
    char trace_context[256];
    agentos_trace_inject(trace_context, sizeof(trace_context));
    
    // 发送请求（携带 trace_context）...
    // http_request_add_header("Trace-Context", trace_context);
    
    // 结束 span
    agentos_trace_end(span_id, SPAN_STATUS_OK, NULL);
    free(span_id);
}
```

---

## ⚠️ 注意事项

### 1. 日志最佳实践
- 使用适当的日志级别：TRACE < DEBUG < INFO < WARN < ERROR < FATAL
- 生产环境建议使用 INFO 或 WARN 级别
- 结构化字段使用 JSON 格式，便于解析和查询
- 避免在日志中记录敏感信息

### 2. 指标命名规范
- 使用小写字母和下划线：`agentos_memory_usage_bytes`
- Counter 类型使用 `_total` 后缀
- 包含单位：`_bytes`, `_seconds`, `_count`
- 添加适当的标签用于聚合和过滤

### 3. 追踪使用建议
- 每个 span 代表一个有意义的操作单元
- 避免创建过多细粒度的 span（建议 < 100 个/trace）
- 为 span 添加有意义的属性和事件
- 正确设置 span 状态和错误信息

### 4. 性能考虑
- 日志写入是异步的，不影响主流程性能
- 指标记录使用内存缓冲，定期批量导出
- 追踪采样率建议设置为 10%（生产环境）
- 高频操作使用采样或聚合减少开销

### 5. 存储管理
- 日志默认保留 7 天
- 指标数据默认保留 30 天
- 追踪数据默认保留 3 天
- 定期检查存储使用情况，调整保留策略

---

## 🔍 错误码参考

| 错误码 | 值 | 描述 |
|--------|-----|------|
| `AGENTOS_SUCCESS` | 0x0000 | 操作成功 |
| `AGENTOS_ERROR_INVALID_ARGUMENT` | 0x1001 | 参数无效 |
| `AGENTOS_ERROR_NO_MEMORY` | 0x1002 | 内存不足 |
| `AGENTOS_ERROR_STORAGE_FULL` | 0x2001 | 存储空间不足 |
| `AGENTOS_ERROR_TRACE_NOT_FOUND` | 0x4001 | Trace 不存在 |
| `AGENTOS_ERROR_SPAN_NOT_FOUND` | 0x4002 | Span 不存在 |
| `AGENTOS_ERROR_EXPORT_FAILED` | 0x4003 | 导出失败 |
| `AGENTOS_ERROR_INVALID_TRACE_CONTEXT` | 0x4004 | 追踪上下文无效 |

---

## 📚 相关文档

- [系统调用总览](../README.md) - 所有系统调用 API 概览
- [任务管理 API](task.md) - 任务生命周期管理 API
- [记忆管理 API](memory.md) - 记忆读写和查询 API
- [会话管理 API](session.md) - 会话生命周期管理 API
- [架构设计：日志系统](../../architecture/folder/logging_system.md) - 统一日志架构详解

---

© 2026 SPHARX Ltd. All Rights Reserved.
*"From data intelligence emerges."*