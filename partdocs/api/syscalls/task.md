Copyright (c) 2026 SPHARX. All Rights Reserved.
"From data intelligence emerges."

# AgentOS 任务管理 API

**版本**: v1.0.0.6  
**最后更新**: 2026-03-23  
**状态**: 🟢 生产就绪

---

## 🎯 概述

任务管理 API 提供 AgentOS 任务生命周期的完整控制，包括任务提交、状态查询、取消、优先级调整等功能。所有任务遵循**双系统路径**：System 1 快速路径处理简单任务，System 2 深度路径处理复杂任务。

---

## 📋 API 索引

| 函数 | 描述 | 复杂度 |
|------|------|--------|
| `agentos_task_submit()` | 提交任务到调度队列 | O(1) |
| `agentos_task_query()` | 查询任务状态 | O(1) |
| `agentos_task_cancel()` | 取消运行中的任务 | O(log n) |
| `agentos_task_list()` | 列出所有任务 | O(n) |
| `agentos_task_priority_set()` | 设置任务优先级 | O(log n) |
| `agentos_task_wait()` | 等待任务完成 | O(1) |
| `agentos_task_stats()` | 获取任务统计信息 | O(1) |

---

## 🔧 核心数据结构

### 任务描述符

```c
/**
 * @brief 任务描述符结构体
 * 
 * 描述一个待执行的任务，包含任务元数据、输入参数和预期输出。
 */
typedef struct agentos_task_descriptor {
    /** 任务唯一标识符（64位） */
    uint64_t id;
    
    /** 任务描述（UTF-8，最大 1024 字节） */
    char description[1024];
    
    /** 任务优先级（0-255，0为最高） */
    uint8_t priority;
    
    /** 任务类型 */
    enum {
        TASK_TYPE_SIMPLE = 0,     /* System 1 快速路径 */
        TASK_TYPE_COMPLEX = 1,    /* System 2 深度路径 */
        TASK_TYPE_CRITICAL = 2,   /* 关键任务（实时性要求高） */
        TASK_TYPE_BACKGROUND = 3  /* 后台任务（低优先级） */
    } type;
    
    /** 任务输入参数（JSON 格式） */
    char input_params[4096];
    
    /** 任务超时时间（毫秒，0表示无超时） */
    uint32_t timeout_ms;
    
    /** 任务创建时间戳（Unix 毫秒） */
    uint64_t created_at;
    
    /** 任务状态 */
    enum {
        TASK_STATE_PENDING = 0,   /* 等待调度 */
        TASK_STATE_RUNNING = 1,   /* 执行中 */
        TASK_STATE_COMPLETED = 2, /* 已完成 */
        TASK_STATE_FAILED = 3,    /* 执行失败 */
        TASK_STATE_CANCELLED = 4, /* 已取消 */
        TASK_STATE_TIMEOUT = 5    /* 超时 */
    } state;
    
    /** 任务结果（JSON 格式） */
    char result[8192];
    
    /** 任务执行耗时（毫秒） */
    uint32_t execution_time_ms;
    
    /** 任务执行单元 ID */
    char execution_unit_id[64];
    
    /** 任务所属 Agent ID */
    char agent_id[64];
    
    /** 任务 trace_id（用于全链路追踪） */
    char trace_id[128];
} agentos_task_descriptor_t;
```

### 任务统计信息

```c
/**
 * @brief 任务统计信息结构体
 */
typedef struct agentos_task_statistics {
    /** 总任务数 */
    uint64_t total_tasks;
    
    /** 等待中任务数 */
    uint64_t pending_tasks;
    
    /** 执行中任务数 */
    uint64_t running_tasks;
    
    /** 已完成任务数 */
    uint64_t completed_tasks;
    
    /** 失败任务数 */
    uint64_t failed_tasks;
    
    /** 平均执行时间（毫秒） */
    uint32_t avg_execution_time_ms;
    
    /** 最大执行时间（毫秒） */
    uint32_t max_execution_time_ms;
    
    /** 最小执行时间（毫秒） */
    uint32_t min_execution_time_ms;
    
    /** 任务吞吐量（任务/秒） */
    float throughput_tasks_per_sec;
    
    /** 任务队列深度 */
    uint32_t queue_depth;
    
    /** System 1 路径任务占比（%） */
    float system1_ratio;
    
    /** System 2 路径任务占比（%） */
    float success_rate_percent;
    
    /** System 1 路径任务占比 */
    float system1_ratio_percent;
    
    /** System 2 路径任务占比 */
    float system2_ratio_percent;
} agentos_task_statistics_t;
```

---

## 📖 API 详细说明

### `agentos_task_submit()`

```c
/**
 * @brief 提交任务到调度队列
 * 
 * 将任务描述转换为任务图，并加入调度队列。
 * 任务 ID 通过 out_id 返回，用于后续查询和取消。
 * 
 * @param description UTF-8 编码的任务描述
 * @param priority 任务优先级（0-255，0为最高）
 * @param input_params JSON 格式的输入参数（可选）
 * @param timeout_ms 任务超时时间（毫秒，0表示无超时）
 * @param out_id 输出参数，返回任务 ID
 * @return AGENTOS_SUCCESS 成功；其他值表示错误
 * 
 * @ownership out_id 由调用者负责释放
 * @threadsafe 是
 * @reentrant 否
 * @see agentos_task_cancel(), agentos_task_query()
 */
AGENTOS_API agentos_error_t
agentos_task_submit(const char* description,
                    uint8_t priority,
                    const char* input_params,
                    uint32_t timeout_ms,
                    uint64_t* out_id);
```

**参数说明**：
- `description`: 任务描述，用于任务分类和路径选择
- `priority`: 任务优先级，影响调度顺序
- `input_params`: JSON 格式的输入参数，如 `{"param1": "value1", "param2": 123}`
- `timeout_ms`: 任务超时时间，超时后任务自动取消
- `out_id`: 输出参数，返回分配的任务 ID

**返回值**：
- `AGENTOS_SUCCESS` (0x0000): 任务提交成功
- `AGENTOS_ERROR_INVALID_ARGUMENT` (0x1001): 参数无效
- `AGENTOS_ERROR_NO_MEMORY` (0x1002): 内存不足
- `AGENTOS_ERROR_QUEUE_FULL` (0x1003): 任务队列已满
- `AGENTOS_ERROR_SYSTEM_BUSY` (0x1004): 系统繁忙

**使用示例**：
```c
uint64_t task_id;
agentos_error_t err = agentos_task_submit(
    "分析用户行为数据并生成报告",
    10,  // 中等优先级
    "{\"user_id\": \"user_123\", \"data_range\": \"last_7_days\"}",
    30000,  // 30秒超时
    &task_id
);

if (err == AGENTOS_SUCCESS) {
    printf("任务提交成功，ID: %llu\n", task_id);
} else {
    printf("任务提交失败: 0x%04X\n", err);
}
```

### `agentos_task_query()`

```c
/**
 * @brief 查询任务状态
 * 
 * 根据任务 ID 查询任务的当前状态和详细信息。
 * 
 * @param task_id 任务 ID
 * @param out_descriptor 输出参数，返回任务描述符
 * @return AGENTOS_SUCCESS 成功；其他值表示错误
 * 
 * @ownership out_descriptor 由调用者负责释放
 * @threadsafe 是
 * @reentrant 是
 */
AGENTOS_API agentos_error_t
agentos_task_query(uint64_t task_id,
                   agentos_task_descriptor_t** out_descriptor);
```

**参数说明**：
- `task_id`: 要查询的任务 ID
- `out_descriptor`: 输出参数，返回任务描述符指针

**返回值**：
- `AGENTOS_SUCCESS` (0x0000): 查询成功
- `AGENTOS_ERROR_NOT_FOUND` (0x1005): 任务不存在
- `AGENTOS_ERROR_INVALID_ARGUMENT` (0x1001): 参数无效

**使用示例**：
```c
agentos_task_descriptor_t* desc = NULL;
agentos_error_t err = agentos_task_query(task_id, &desc);

if (err == AGENTOS_SUCCESS) {
    printf("任务状态: %d\n", desc->state);
    printf("任务描述: %s\n", desc->description);
    printf("执行时间: %u ms\n", desc->execution_time_ms);
    
    // 释放描述符
    free(desc);
}
```

### `agentos_task_cancel()`

```c
/**
 * @brief 取消运行中的任务
 * 
 * 尝试取消指定的任务。如果任务正在执行，会向执行单元发送取消信号。
 * 
 * @param task_id 要取消的任务 ID
 * @param force 是否强制取消（忽略执行单元状态）
 * @return AGENTOS_SUCCESS 成功；其他值表示错误
 * 
 * @threadsafe 是
 * @reentrant 否
 */
AGENTOS_API agentos_error_t
agentos_task_cancel(uint64_t task_id, bool force);
```

**参数说明**：
- `task_id`: 要取消的任务 ID
- `force`: 是否强制取消（true: 立即取消，false: 等待执行单元响应）

**返回值**：
- `AGENTOS_SUCCESS` (0x0000): 取消成功
- `AGENTOS_ERROR_NOT_FOUND` (0x1005): 任务不存在
- `AGENTOS_ERROR_INVALID_STATE` (0x1006): 任务状态不允许取消
- `AGENTOS_ERROR_OPERATION_FAILED` (0x1007): 取消操作失败

### `agentos_task_list()`

```c
/**
 * @brief 列出所有任务
 * 
 * 根据过滤条件列出任务列表。
 * 
 * @param filter 过滤条件（JSON 格式）
 * @param offset 分页偏移量
 * @param limit 每页数量限制
 * @param out_tasks 输出参数，返回任务描述符数组
 * @param out_count 输出参数，返回实际数量
 * @return AGENTOS_SUCCESS 成功；其他值表示错误
 * 
 * @ownership out_tasks 由调用者负责释放
 * @threadsafe 是
 * @reentrant 是
 */
AGENTOS_API agentos_error_t
agentos_task_list(const char* filter,
                  uint32_t offset,
                  uint32_t limit,
                  agentos_task_descriptor_t*** out_tasks,
                  uint32_t* out_count);
```

**过滤条件示例**：
```json
{
    "state": "running",
    "priority_min": 0,
    "priority_max": 50,
    "created_after": 1672531200000,
    "agent_id": "agent_001"
}
```

### `agentos_task_wait()`

```c
/**
 * @brief 等待任务完成
 * 
 * 阻塞等待指定任务完成或超时。
 * 
 * @param task_id 任务 ID
 * @param timeout_ms 等待超时时间（毫秒）
 * @param out_result 输出参数，返回任务结果（JSON）
 * @return AGENTOS_SUCCESS 成功；其他值表示错误
 * 
 * @ownership out_result 由调用者负责释放
 * @threadsafe 是
 * @reentrant 否
 */
AGENTOS_API agentos_error_t
agentos_task_wait(uint64_t task_id,
                  uint32_t timeout_ms,
                  char** out_result);
```

---

## 🚀 使用示例

### 完整任务处理流程

```c
#include <stdio.h>
#include <stdlib.h>
#include "agentos/task.h"

int main(void)
{
    agentos_error_t err;
    uint64_t task_id;
    
    // 1. 提交任务
    err = agentos_task_submit(
        "处理用户上传的图片并生成缩略图",
        20,
        "{\"image_path\": \"/uploads/user_123.jpg\", \"thumbnail_size\": [200, 200]}",
        60000,  // 60秒超时
        &task_id
    );
    
    if (err != AGENTOS_SUCCESS) {
        fprintf(stderr, "任务提交失败: 0x%04X\n", err);
        return 1;
    }
    
    printf("任务已提交，ID: %llu\n", task_id);
    
    // 2. 等待任务完成
    char* result = NULL;
    err = agentos_task_wait(task_id, 65000, &result);
    
    if (err == AGENTOS_SUCCESS) {
        printf("任务完成，结果: %s\n", result);
        free(result);
    } else if (err == AGENTOS_ERROR_TIMEOUT) {
        printf("任务等待超时\n");
        
        // 3. 取消超时任务
        err = agentos_task_cancel(task_id, false);
        if (err == AGENTOS_SUCCESS) {
            printf("任务已取消\n");
        }
    } else {
        printf("任务执行失败: 0x%04X\n", err);
    }
    
    // 4. 查询任务统计
    agentos_task_statistics_t stats;
    err = agentos_task_stats(&stats);
    
    if (err == AGENTOS_SUCCESS) {
        printf("任务统计:\n");
        printf("  总任务数: %llu\n", stats.total_tasks);
        printf("  成功率: %.1f%%\n", stats.success_rate_percent);
        printf("  平均执行时间: %u ms\n", stats.avg_execution_time_ms);
    }
    
    return 0;
}
```

### 批量任务处理

```c
// 批量提交任务
uint64_t task_ids[10];
for (int i = 0; i < 10; i++) {
    char description[256];
    snprintf(description, sizeof(description),
             "处理数据批次 %d", i);
    
    agentos_task_submit(
        description,
        30,
        "{\"batch_id\": i}",
        30000,
        &task_ids[i]
    );
}

// 批量查询状态
for (int i = 0; i < 10; i++) {
    agentos_task_descriptor_t* desc = NULL;
    if (agentos_task_query(task_ids[i], &desc) == AGENTOS_SUCCESS) {
        printf("任务 %llu 状态: %d\n", task_ids[i], desc->state);
        free(desc);
    }
}
```

---

## ⚠️ 注意事项

### 1. 任务 ID 管理
- 任务 ID 在系统重启后不保证唯一性
- 建议在应用层维护任务 ID 到业务 ID 的映射
- 任务 ID 为 64 位无符号整数，范围 1~2^64-1

### 2. 内存管理
- 所有输出参数（`out_*`）都需要调用者负责释放
- 使用 `free()` 释放 `agentos_task_descriptor_t*` 和 `char*` 类型
- 使用 `agentos_task_list()` 返回的数组需要遍历释放每个元素后再释放数组

### 3. 线程安全
- 所有 API 都是线程安全的
- 可以在多个线程中同时调用同一个函数
- 但需要注意任务 ID 的线程间同步

### 4. 错误处理
- 始终检查返回值是否为 `AGENTOS_SUCCESS`
- 使用 `agentos_strerror()` 获取错误描述
- 记录错误码和 trace_id 便于排查

### 5. 性能考虑
- `agentos_task_submit()` 是 O(1) 操作，适合高频调用
- `agentos_task_list()` 是 O(n) 操作，避免在大数据量时频繁调用
- 使用过滤条件减少 `agentos_task_list()` 返回的数据量

---

## 🔍 错误码参考

| 错误码 | 值 | 描述 |
|--------|-----|------|
| `AGENTOS_SUCCESS` | 0x0000 | 操作成功 |
| `AGENTOS_ERROR_INVALID_ARGUMENT` | 0x1001 | 参数无效 |
| `AGENTOS_ERROR_NO_MEMORY` | 0x1002 | 内存不足 |
| `AGENTOS_ERROR_QUEUE_FULL` | 0x1003 | 任务队列已满 |
| `AGENTOS_ERROR_SYSTEM_BUSY` | 0x1004 | 系统繁忙 |
| `AGENTOS_ERROR_NOT_FOUND` | 0x1005 | 任务不存在 |
| `AGENTOS_ERROR_INVALID_STATE` | 0x1006 | 任务状态不允许此操作 |
| `AGENTOS_ERROR_OPERATION_FAILED` | 0x1007 | 操作失败 |
| `AGENTOS_ERROR_TIMEOUT` | 0x1008 | 操作超时 |
| `AGENTOS_ERROR_PERMISSION_DENIED` | 0x1009 | 权限不足 |

---

## 📚 相关文档

- [系统调用总览](../README.md) - 所有系统调用 API 概览
- [记忆管理 API](memory.md) - 记忆读写和查询 API
- [会话管理 API](session.md) - 会话生命周期管理 API
- [可观测性 API](telemetry.md) - 指标、日志和追踪 API
- [Agent 管理 API](agent.md) - Agent 生命周期管理 API

---

© 2026 SPHARX Ltd. All Rights Reserved.
*"From data intelligence emerges."*