Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
"From data intelligence emerges."

# AgentOS Agent 管理 API

**版本**: Doc V1.8  
**最后更新**: 2026-04-09  
**状态**: 🟢 生产就绪

---

## 🎯 概述

Agent 管理 API 提供 AgentOS Agent 实例的完整生命周期管理。每个 Agent 遵循**双系统认知理论**：System 1 快速路径处理简单任务，System 2 深度路径处理复杂任务。Agent 状态机严格定义合法转换，确保系统稳定性。

### 🧩 五维正交原则体现

Agent 管理 API 深度体现了 AgentOS 的五维正交设计原则：

| 维度 | 原则体现 | 具体实现 |
|------|----------|---------|
| **系统观** | Agent 生命周期的系统管理 | 创建→配置→运行→销毁的完整状态机 |
| **内核观** | Agent 资源的标准化契约 | 统一的配置接口，明确的资源隔离边界 |
| **认知观** | 双系统认知能力配置 | System 1/System 2 协同调度，认知策略可配置 |
| **工程观** | Agent 安全与可观测性 | 权限控制、资源限制、性能监控、健康检查 |
| **设计美学** | 优雅的智能体抽象 | 一致的配置模型，灵活的能力组合机制 |

---

## 📋 API 索引

| 函数 | 描述 | 复杂度 |
|------|------|--------|
| `agentos_agent_create()` | 创建 Agent 实例 | O(1) |
| `agentos_agent_get()` | 获取 Agent 信息 | O(1) |
| `agentos_agent_update()` | 更新 Agent 配置 | O(1) |
| `agentos_agent_destroy()` | 销毁 Agent 实例 | O(1) |
| `agentos_agent_list()` | 列出所有 Agent | O(n) |
| `agentos_agent_start()` | 启动 Agent | O(1) |
| `agentos_agent_pause()` | 暂停 Agent | O(1) |
| `agentos_agent_stop()` | 停止 Agent | O(1) |
| `agentos_agent_state()` | 获取 Agent 状态 | O(1) |
| `agentos_agent_stats()` | 获取 Agent 统计信息 | O(1) |
| `agentos_agent_skill_bind()` | 绑定 Skill 到 Agent | O(1) |
| `agentos_agent_skill_unbind()` | 解绑 Skill | O(1) |
| `agentos_agent_history()` | 获取 Agent 状态历史 | O(n) |

---

## 🔧 核心数据结构

### Agent 配置

```c
/**
 * @brief Agent 配置结构体
 * 
 * 定义 Agent 的初始化参数和行为配置。
 */
typedef struct agentos_agent_config {
    /** Agent 名称（唯一标识） */
    char name[64];
    
    /** Agent 描述 */
    char description[256];
    
    /** Agent 类型 */
    enum {
        AGENT_TYPE_GENERIC = 0,     /* 通用 Agent */
        AGENT_TYPE_CHAT = 1,        /* 聊天 Agent */
        AGENT_TYPE_TASK = 2,        /* 任务执行 Agent */
        AGENT_TYPE_ANALYSIS = 3,    /* 分析 Agent */
        AGENT_TYPE_MONITOR = 4      /* 监控 Agent */
    } type;
    
    /** 最大并发任务数 */
    uint32_t max_concurrent_tasks;
    
    /** 任务队列深度 */
    uint32_t task_queue_depth;
    
    /** 默认任务超时（毫秒） */
    uint32_t default_task_timeout_ms;
    
    /** System 1 阈值：任务描述最大长度 */
    uint32_t system1_max_desc_len;
    
    /** System 1 阈值：最大优先级 */
    uint8_t system1_max_priority;
    
    /** 认知引擎配置（JSON 格式） */
    char* cognition_config;
    
    /** 记忆配置（JSON 格式） */
    char* memory_config;
    
    /** 安全策略配置（JSON 格式） */
    char* security_config;
    
    /** 资源限制 */
    struct {
        /** 最大内存使用（字节） */
        uint64_t max_memory_bytes;
        
        /** 最大 CPU 使用率（%） */
        uint8_t max_cpu_percent;
        
        /** 最大执行时间（秒） */
        uint32_t max_execution_time_sec;
    } resource_limits;
    
    /** 重试配置 */
    struct {
        /** 最大重试次数 */
        uint8_t max_retries;
        
        /** 重试间隔（毫秒） */
        uint32_t retry_interval_ms;
        
        /** 指数退避因子 */
        float backoff_factor;
    } retry_config;
} agentos_agent_config_t;
```

### Agent 描述符

```c
/**
 * @brief Agent 描述符结构体
 * 
 * 描述 Agent 实例的完整信息，包括状态、配置和运行时数据。
 */
typedef struct agentos_agent_descriptor {
    /** Agent 唯一标识符 */
    char id[64];
    
    /** Agent 名称 */
    char name[64];
    
    /** Agent 描述 */
    char description[256];
    
    /** Agent 类型 */
    uint8_t type;
    
    /** Agent 状态 */
    enum {
        AGENT_STATE_CREATED = 0,    /* 已创建 */
        AGENT_STATE_INITING = 1,    /* 初始化中 */
        AGENT_STATE_READY = 2,      /* 就绪 */
        AGENT_STATE_RUNNING = 3,    /* 运行中 */
        AGENT_STATE_PAUSED = 4,     /* 已暂停 */
        AGENT_STATE_STOPPING = 5,   /* 停止中 */
        AGENT_STATE_STOPPED = 6,    /* 已停止 */
        AGENT_STATE_ERROR = 7,      /* 错误状态 */
        AGENT_STATE_DESTROYED = 8   /* 已销毁 */
    } state;
    
    /** 创建时间戳（Unix 毫秒） */
    uint64_t created_at;
    
    /** 最后状态变更时间戳 */
    uint64_t last_state_change_at;
    
    /** 绑定的 Skill 列表 */
    char** bound_skills;
    uint32_t bound_skill_count;
    
    /** 当前活跃任务数 */
    uint32_t active_task_count;
    
    /** 累计完成任务数 */
    uint64_t completed_task_count;
    
    /** 累计失败任务数 */
    uint64_t failed_task_count;
    
    /** 平均任务执行时间（毫秒） */
    uint32_t avg_task_time_ms;
    
    /** System 1 路径执行次数 */
    uint64_t system1_exec_count;
    
    /** System 2 路径执行次数 */
    uint64_t system2_exec_count;
    
    /** 当前内存使用（字节） */
    uint64_t memory_usage_bytes;
    
    /** 当前 CPU 使用率（%） */
    float cpu_usage_percent;
    
    /** Agent 元数据（JSON 格式） */
    char* metadata;
    
    /** trace_id（用于追踪） */
    char trace_id[128];
} agentos_agent_descriptor_t;
```

### Agent 统计信息

```c
/**
 * @brief Agent 统计信息结构体
 */
typedef struct agentos_agent_statistics {
    /** 总 Agent 数 */
    uint64_t total_agents;
    
    /** 各状态 Agent 数 */
    uint64_t agents_by_state[9];  /* CREATED 到 DESTROYED */
    
    /** 各类型 Agent 数 */
    uint64_t agents_by_type[5];   /* GENERIC 到 MONITOR */
    
    /** 总活跃任务数 */
    uint64_t total_active_tasks;
    
    /** 总完成任务数 */
    uint64_t total_completed_tasks;
    
    /** 总失败任务数 */
    uint64_t total_failed_tasks;
    
    /** 平均任务执行时间（毫秒） */
    uint32_t avg_task_time_ms;
    
    /** System 1 路径占比（%） */
    float system1_ratio_percent;
    
    /** System 2 路径占比（%） */
    float system2_ratio_percent;
    
    /** 总内存使用（字节） */
    uint64_t total_memory_usage_bytes;
    
    /** 平均 CPU 使用率（%） */
    float avg_cpu_usage_percent;
} agentos_agent_statistics_t;
```

### 状态历史记录

```c
/**
 * @brief Agent 状态历史记录结构体
 */
typedef struct agentos_state_history {
    /** 状态变更时间戳 */
    uint64_t timestamp;
    
    /** 原状态 */
    uint8_t from_state;
    
    /** 新状态 */
    uint8_t to_state;
    
    /** 变更原因 */
    char reason[256];
    
    /** 触发者 */
    char triggered_by[64];
    
    /** trace_id */
    char trace_id[128];
    
    /** 下一记录指针 */
    struct agentos_state_history* next;
} agentos_state_history_t;
```

---

## 📖 API 详细说明

### `agentos_agent_create()`

```c
/**
 * @brief 创建 Agent 实例
 * 
 * 根据配置创建新的 Agent 实例，返回 Agent ID。
 * 创建后 Agent 处于 CREATED 状态，需要调用 start() 启动。
 * 
 * @param manager Agent 配置
 * @param out_agent_id 输出参数，返回 Agent ID
 * @return AGENTOS_SUCCESS 成功；其他值表示错误
 * 
 * @ownership out_agent_id 由调用者负责释放
 * @threadsafe 是
 * @reentrant 否
 * @see agentos_agent_destroy(), agentos_agent_start()
 */
AGENTOS_API agentos_error_t
agentos_agent_create(const agentos_agent_config_t* manager,
                     char** out_agent_id);
```

**参数说明**：
- `manager`: Agent 配置结构体，定义 Agent 的行为和资源限制
- `out_agent_id`: 输出参数，返回分配的 Agent ID

**返回值**：
- `AGENTOS_SUCCESS` (0x0000): 创建成功
- `AGENTOS_ERROR_INVALID_ARGUMENT` (0x1001): 参数无效
- `AGENTOS_ERROR_NO_MEMORY` (0x1002): 内存不足
- `AGENTOS_ERROR_NAME_EXISTS` (0x5001): Agent 名称已存在
- `AGENTOS_ERROR_RESOURCE_LIMIT` (0x5002): 资源限制超出

**使用示例**：
```c
agentos_agent_config_t manager = {
    .name = "customer_service_agent",
    .description = "Customer service chatbot agent",
    .type = AGENT_TYPE_CHAT,
    .max_concurrent_tasks = 16,
    .task_queue_depth = 256,
    .default_task_timeout_ms = 30000,
    .system1_max_desc_len = 128,
    .system1_max_priority = 50,
    .cognition_config = "{\"model\": \"gpt-4\", \"temperature\": 0.7}",
    .memory_config = "{\"max_records\": 10000}",
    .security_config = "{\"sandbox\": true}",
    .resource_limits = {
        .max_memory_bytes = 512 * 1024 * 1024,  // 512MB
        .max_cpu_percent = 80,
        .max_execution_time_sec = 3600
    },
    .retry_config = {
        .max_retries = 3,
        .retry_interval_ms = 1000,
        .backoff_factor = 2.0
    }
};

char* agent_id = NULL;
agentos_error_t err = agentos_agent_create(&manager, &agent_id);

if (err == AGENTOS_SUCCESS) {
    printf("Agent 创建成功，ID: %s\n", agent_id);
    free(agent_id);
}
```

### `agentos_agent_start()`

```c
/**
 * @brief 启动 Agent
 * 
 * 将 Agent 从 READY 状态转换为 RUNNING 状态，开始处理任务。
 * 
 * @param agent_id Agent ID
 * @return AGENTOS_SUCCESS 成功；其他值表示错误
 * 
 * @threadsafe 是
 * @reentrant 否
 */
AGENTOS_API agentos_error_t
agentos_agent_start(const char* agent_id);
```

### `agentos_agent_pause()`

```c
/**
 * @brief 暂停 Agent
 * 
 * 将 Agent 从 RUNNING 状态转换为 PAUSED 状态，暂停任务处理。
 * 正在执行的任务会继续完成，新任务会排队等待。
 * 
 * @param agent_id Agent ID
 * @return AGENTOS_SUCCESS 成功；其他值表示错误
 * 
 * @threadsafe 是
 * @reentrant 否
 */
AGENTOS_API agentos_error_t
agentos_agent_pause(const char* agent_id);
```

### `agentos_agent_stop()`

```c
/**
 * @brief 停止 Agent
 * 
 * 将 Agent 转换为 STOPPED 状态，停止所有任务处理。
 * 正在执行的任务会被取消。
 * 
 * @param agent_id Agent ID
 * @param force 是否强制停止（忽略正在执行的任务）
 * @return AGENTOS_SUCCESS 成功；其他值表示错误
 * 
 * @threadsafe 是
 * @reentrant 否
 */
AGENTOS_API agentos_error_t
agentos_agent_stop(const char* agent_id, bool force);
```

### `agentos_agent_destroy()`

```c
/**
 * @brief 销毁 Agent 实例
 * 
 * 完全销毁 Agent 实例，释放所有资源。
 * Agent 必须处于 STOPPED 状态才能销毁。
 * 
 * @param agent_id Agent ID
 * @return AGENTOS_SUCCESS 成功；其他值表示错误
 * 
 * @threadsafe 是
 * @reentrant 否
 */
AGENTOS_API agentos_error_t
agentos_agent_destroy(const char* agent_id);
```

### `agentos_agent_skill_bind()`

```c
/**
 * @brief 绑定 Skill 到 Agent
 * 
 * 将指定 Skill 绑定到 Agent，使 Agent 获得该能力。
 * 
 * @param agent_id Agent ID
 * @param skill_name Skill 名称
 * @param manager Skill 配置（JSON 格式，可选）
 * @return AGENTOS_SUCCESS 成功；其他值表示错误
 * 
 * @threadsafe 是
 * @reentrant 否
 * @see agentos_agent_skill_unbind()
 */
AGENTOS_API agentos_error_t
agentos_agent_skill_bind(const char* agent_id,
                         const char* skill_name,
                         const char* manager);
```

### `agentos_agent_skill_unbind()`

```c
/**
 * @brief 解绑 Skill
 * 
 * 将指定 Skill 从 Agent 解绑，Agent 将失去该能力。
 * 正在使用该 Skill 的任务会继续完成，但后续任务无法再调用该 Skill。
 * 
 * @param agent_id Agent ID
 * @param skill_name Skill 名称
 * @return AGENTOS_SUCCESS 成功；其他值表示错误
 * 
 * @threadsafe 是
 * @reentrant 否
 * @see agentos_agent_skill_bind()
 */
AGENTOS_API agentos_error_t
agentos_agent_skill_unbind(const char* agent_id,
                           const char* skill_name);
```

**返回值**：
- `AGENTOS_SUCCESS` (0x0000): 解绑成功
- `AGENTOS_ERROR_AGENT_NOT_FOUND` (0x5001): Agent 不存在
- `AGENTOS_ERROR_SKILL_NOT_BOUND` (0x5007): Skill 未绑定到该 Agent
- `AGENTOS_ERROR_INVALID_ARGUMENT` (0x1001): 参数无效

### `agentos_agent_get()`

```c
/**
 * @brief 获取 Agent 信息
 * 
 * 获取指定 Agent 的完整描述信息，包括状态、配置和运行时数据。
 * 
 * @param agent_id Agent ID
 * @param out_descriptor [out] 返回 Agent 描述符（调用者负责释放）
 * @return AGENTOS_SUCCESS 成功；其他值表示错误
 * 
 * @ownership 调用者拥有 *out_descriptor，需释放 metadata 字段后释放结构体本身
 * @threadsafe 是
 * @reentrant 否
 * @see agentos_agent_create(), agentos_agent_state()
 */
AGENTOS_API agentos_error_t
agentos_agent_get(const char* agent_id,
                  agentos_agent_descriptor_t** out_descriptor);
```

**返回值**：
- `AGENTOS_SUCCESS` (0x0000): 获取成功
- `AGENTOS_ERROR_AGENT_NOT_FOUND` (0x5001): Agent 不存在
- `AGENTOS_ERROR_INVALID_ARGUMENT` (0x1001): 参数无效

**使用示例**：
```c
agentos_agent_descriptor_t* desc = NULL;
agentos_error_t err = agentos_agent_get(agent_id, &desc);
if (err == AGENTOS_SUCCESS) {
    printf("Agent: %s, State: %d, Tasks: %u\n",
           desc->name, desc->state, desc->active_task_count);
    if (desc->metadata) free(desc->metadata);
    free(desc);
}
```

### `agentos_agent_update()`

```c
/**
 * @brief 更新 Agent 配置
 * 
 * 动态更新 Agent 的运行时配置。仅允许更新部分配置项，
 * 核心配置（如类型、名称）不可更改。Agent 必须处于 READY 或 PAUSED 状态。
 * 
 * @param agent_id Agent ID
 * @param config 更新配置（JSON 格式，仅包含需更新的字段）
 * @return AGENTOS_SUCCESS 成功；其他值表示错误
 * 
 * @threadsafe 是
 * @reentrant 否
 * @see agentos_agent_create(), agentos_agent_get()
 */
AGENTOS_API agentos_error_t
agentos_agent_update(const char* agent_id,
                     const char* config);
```

**可更新字段**（JSON 格式）：
```json
{
    "max_concurrent_tasks": 16,
    "default_task_timeout_ms": 60000,
    "cognition_config": "{\"model\": \"gpt-4\", \"temperature\": 0.7}",
    "memory_config": "{\"max_records\": 10000}",
    "security_config": "{\"sandbox\": true}",
    "resource_limits": {
        "max_memory_bytes": 536870912,
        "max_cpu_percent": 80,
        "max_execution_time_sec": 3600
    },
    "retry_config": {
        "max_retries": 3,
        "retry_interval_ms": 1000,
        "backoff_factor": 2.0
    }
}
```

**返回值**：
- `AGENTOS_SUCCESS` (0x0000): 更新成功
- `AGENTOS_ERROR_AGENT_NOT_FOUND` (0x5001): Agent 不存在
- `AGENTOS_ERROR_INVALID_STATE` (0x5003): Agent 状态不允许更新
- `AGENTOS_ERROR_RESOURCE_LIMIT` (0x5004): 新资源限制超出系统容量
- `AGENTOS_ERROR_INVALID_ARGUMENT` (0x1001): 配置格式无效

### `agentos_agent_list()`

```c
/**
 * @brief 列出所有 Agent
 * 
 * 返回当前系统中所有 Agent 的 ID 列表。
 * 
 * @param out_agent_ids [out] 返回 Agent ID 数组（调用者负责逐个释放后释放数组）
 * @param out_count [out] 返回 Agent 数量
 * @return AGENTOS_SUCCESS 成功；其他值表示错误
 * 
 * @ownership 调用者拥有 *out_agent_ids 数组及其每个元素
 * @threadsafe 是
 * @reentrant 否
 */
AGENTOS_API agentos_error_t
agentos_agent_list(char*** out_agent_ids,
                   size_t* out_count);
```

**返回值**：
- `AGENTOS_SUCCESS` (0x0000): 获取成功
- `AGENTOS_ERROR_INVALID_ARGUMENT` (0x1001): 参数无效
- `AGENTOS_ERROR_NO_MEMORY` (0x1002): 内存不足

**使用示例**：
```c
char** agent_ids = NULL;
size_t count = 0;
agentos_error_t err = agentos_agent_list(&agent_ids, &count);
if (err == AGENTOS_SUCCESS) {
    printf("Total agents: %zu\n", count);
    for (size_t i = 0; i < count; i++) {
        printf("  [%zu] %s\n", i, agent_ids[i]);
        free(agent_ids[i]);
    }
    free(agent_ids);
}
```

### `agentos_agent_state()`

```c
/**
 * @brief 获取 Agent 状态
 * 
 * 获取指定 Agent 的当前状态。相比 agentos_agent_get() 更轻量，
 * 仅返回状态值，不分配内存。
 * 
 * @param agent_id Agent ID
 * @param out_state [out] 返回 Agent 状态枚举值
 * @return AGENTOS_SUCCESS 成功；其他值表示错误
 * 
 * @ownership 无堆内存分配
 * @threadsafe 是
 * @reentrant 是
 * @see agentos_agent_get()
 */
AGENTOS_API agentos_error_t
agentos_agent_state(const char* agent_id,
                    uint8_t* out_state);
```

**返回值**：
- `AGENTOS_SUCCESS` (0x0000): 获取成功
- `AGENTOS_ERROR_AGENT_NOT_FOUND` (0x5001): Agent 不存在
- `AGENTOS_ERROR_INVALID_ARGUMENT` (0x1001): 参数无效

### `agentos_agent_stats()`

```c
/**
 * @brief 获取 Agent 统计信息
 * 
 * 获取全局 Agent 统计信息，包括各状态/类型的 Agent 数量、
 * 任务执行统计、资源使用情况等。
 * 
 * @param out_stats [out] 返回统计信息结构体
 * @return AGENTOS_SUCCESS 成功；其他值表示错误
 * 
 * @ownership 无堆内存分配，结构体为值类型
 * @threadsafe 是
 * @reentrant 是
 * @see agentos_agent_get()
 */
AGENTOS_API agentos_error_t
agentos_agent_stats(agentos_agent_statistics_t* out_stats);
```

**返回值**：
- `AGENTOS_SUCCESS` (0x0000): 获取成功
- `AGENTOS_ERROR_INVALID_ARGUMENT` (0x1001): 参数无效

### `agentos_agent_history()`

```c
/**
 * @brief 获取 Agent 状态历史
 * 
 * 获取指定 Agent 的状态变更历史记录，支持分页查询。
 * 返回链表结构，按时间倒序排列（最新变更在前）。
 * 
 * @param agent_id Agent ID
 * @param offset 起始偏移量（0 为最新记录）
 * @param limit 最大返回条数
 * @param out_history [out] 返回状态历史链表头
 * @param out_count [out] 返回实际返回条数
 * @return AGENTOS_SUCCESS 成功；其他值表示错误
 * 
 * @ownership 调用者拥有 *out_history，需通过 agentos_state_history_free() 释放
 * @threadsafe 是
 * @reentrant 否
 * @see agentos_agent_state(), agentos_state_history_free()
 */
AGENTOS_API agentos_error_t
agentos_agent_history(const char* agent_id,
                      uint32_t offset,
                      uint32_t limit,
                      agentos_state_history_t** out_history,
                      uint32_t* out_count);
```

**辅助释放函数**：

```c
/**
 * @brief 释放状态历史链表
 * 
 * 释放 agentos_agent_history() 返回的链表及其所有节点。
 * 
 * @param history 链表头指针
 */
AGENTOS_API void agentos_state_history_free(agentos_state_history_t* history);
```

**返回值**：
- `AGENTOS_SUCCESS` (0x0000): 获取成功
- `AGENTOS_ERROR_AGENT_NOT_FOUND` (0x5001): Agent 不存在
- `AGENTOS_ERROR_INVALID_ARGUMENT` (0x1001): 参数无效

---

## 🚀 使用示例

### 完整 Agent 生命周期管理

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "agentos/agent.h"

int main(void)
{
    agentos_error_t err;
    char* agent_id = NULL;
    
    // 1. 创建 Agent
    agentos_agent_config_t manager = {
        .name = "data_analysis_agent",
        .description = "Data analysis and reporting agent",
        .type = AGENT_TYPE_ANALYSIS,
        .max_concurrent_tasks = 8,
        .task_queue_depth = 128,
        .default_task_timeout_ms = 60000,
        .system1_max_desc_len = 64,
        .system1_max_priority = 30,
        .cognition_config = NULL,
        .memory_config = NULL,
        .security_config = NULL,
        .resource_limits = {
            .max_memory_bytes = 1024 * 1024 * 1024,  // 1GB
            .max_cpu_percent = 90,
            .max_execution_time_sec = 7200
        },
        .retry_config = {
            .max_retries = 2,
            .retry_interval_ms = 2000,
            .backoff_factor = 1.5
        }
    };
    
    err = agentos_agent_create(&manager, &agent_id);
    if (err != AGENTOS_SUCCESS) {
        fprintf(stderr, "Agent 创建失败: 0x%04X\n", err);
        return 1;
    }
    
    printf("Agent 创建成功: %s\n", agent_id);
    
    // 2. 绑定 Skills
    err = agentos_agent_skill_bind(agent_id, "file_reader", NULL);
    printf("绑定 file_reader: %s\n", err == AGENTOS_SUCCESS ? "成功" : "失败");
    
    err = agentos_agent_skill_bind(agent_id, "data_processor", 
        "{\"batch_size\": 1000, \"output_format\": \"json\"}");
    printf("绑定 data_processor: %s\n", err == AGENTOS_SUCCESS ? "成功" : "失败");
    
    err = agentos_agent_skill_bind(agent_id, "report_generator", NULL);
    printf("绑定 report_generator: %s\n", err == AGENTOS_SUCCESS ? "成功" : "失败");
    
    // 3. 启动 Agent
    err = agentos_agent_start(agent_id);
    if (err != AGENTOS_SUCCESS) {
        fprintf(stderr, "Agent 启动失败: 0x%04X\n", err);
        agentos_agent_destroy(agent_id);
        free(agent_id);
        return 1;
    }
    
    printf("Agent 已启动\n");
    
    // 4. 检查 Agent 状态
    agentos_agent_descriptor_t* desc = NULL;
    err = agentos_agent_get(agent_id, &desc);
    
    if (err == AGENTOS_SUCCESS) {
        printf("\nAgent 状态:\n");
        printf("  名称: %s\n", desc->name);
        printf("  状态: %d\n", desc->state);
        printf("  绑定 Skills: %u\n", desc->bound_skill_count);
        printf("  活跃任务: %u\n", desc->active_task_count);
        
        free(desc->metadata);
        free(desc);
    }
    
    // 5. 模拟运行一段时间...
    printf("\nAgent 运行中...\n");
    // ... 执行任务 ...
    
    // 6. 暂停 Agent
    err = agentos_agent_pause(agent_id);
    printf("Agent 已暂停\n");
    
    // 7. 恢复运行
    err = agentos_agent_start(agent_id);
    printf("Agent 已恢复\n");
    
    // 8. 获取状态历史
    agentos_state_history_t* history = NULL;
    uint32_t history_count = 0;
    err = agentos_agent_history(agent_id, 0, 10, &history, &history_count);
    
    if (err == AGENTOS_SUCCESS) {
        printf("\n状态历史 (%u 条):\n", history_count);
        
        agentos_state_history_t* current = history;
        while (current != NULL) {
            printf("  [%llu] %d -> %d: %s\n",
                   current->timestamp,
                   current->from_state,
                   current->to_state,
                   current->reason);
            current = current->next;
        }
        
        agentos_state_history_free(history);
    }
    
    // 9. 停止 Agent
    err = agentos_agent_stop(agent_id, false);
    printf("Agent 已停止\n");
    
    // 10. 销毁 Agent
    err = agentos_agent_destroy(agent_id);
    printf("Agent 已销毁\n");
    
    // 11. 获取全局统计
    agentos_agent_statistics_t stats;
    err = agentos_agent_stats(&stats);
    
    if (err == AGENTOS_SUCCESS) {
        printf("\nAgent 全局统计:\n");
        printf("  总 Agent 数: %llu\n", stats.total_agents);
        printf("  运行中: %llu\n", stats.agents_by_state[AGENT_STATE_RUNNING]);
        printf("  总活跃任务: %llu\n", stats.total_active_tasks);
        printf("  System 1 占比: %.1f%%\n", stats.system1_ratio_percent);
        printf("  System 2 占比: %.1f%%\n", stats.system2_ratio_percent);
    }
    
    // 清理资源
    free(agent_id);
    
    return 0;
}
```

### 批量 Agent 管理

```c
// 创建多个 Agent
void create_agent_pool(void)
{
    const char* agent_names[] = {
        "chat_agent_1",
        "chat_agent_2",
        "task_agent_1",
        "analysis_agent_1"
    };
    
    uint8_t agent_types[] = {
        AGENT_TYPE_CHAT,
        AGENT_TYPE_CHAT,
        AGENT_TYPE_TASK,
        AGENT_TYPE_ANALYSIS
    };
    
    char* agent_ids[4];
    
    for (int i = 0; i < 4; i++) {
        agentos_agent_config_t manager = {
            .name = agent_names[i],
            .type = agent_types[i],
            .max_concurrent_tasks = 8,
            .task_queue_depth = 64
        };
        
        agentos_agent_create(&manager, &agent_ids[i]);
    }
    
    // 批量启动
    for (int i = 0; i < 4; i++) {
        agentos_agent_start(agent_ids[i]);
    }
    
    // ... 使用 Agent ...
    
    // 批量停止和销毁
    for (int i = 0; i < 4; i++) {
        agentos_agent_stop(agent_ids[i], true);
        agentos_agent_destroy(agent_ids[i]);
        free(agent_ids[i]);
    }
}
```

---

## 📊 状态机

Agent 状态遵循严格的转换规则：

```
CREATED ──init()──► INITING ──init_done()──► READY
                                              │
                      ◄──stop()───────────────┤
                      │                       │
                      ▼                       ▼
                   STOPPING ◄──stop()───── RUNNING
                      │                       ▲
                      │                       │
                      ▼                       │
                   STOPPED ◄────pause()───────┘
                      │                       ▲
                      │                       │
                      ▼                       │
                   DESTROYED              PAUSED
                                             │
                                             └──start()──► RUNNING
```

**合法状态转换**：

| 当前状态 | 目标状态 | 触发函数 |
|----------|----------|----------|
| CREATED | INITING | 内部初始化 |
| INITING | READY | 初始化完成 |
| READY | RUNNING | `agentos_agent_start()` |
| RUNNING | PAUSED | `agentos_agent_pause()` |
| PAUSED | RUNNING | `agentos_agent_start()` |
| RUNNING | STOPPING | `agentos_agent_stop()` |
| PAUSED | STOPPING | `agentos_agent_stop()` |
| STOPPING | STOPPED | 停止完成 |
| STOPPED | DESTROYED | `agentos_agent_destroy()` |
| * | ERROR | 错误发生 |

---

## ⚠️ 注意事项

### 1. 状态管理
- 必须按状态机规则进行状态转换
- 非法转换会返回 `AGENTOS_ERROR_INVALID_STATE`
- 使用 `agentos_agent_history()` 追踪状态变更

### 2. 资源管理
- Agent 创建后占用系统资源，不使用时应及时销毁
- 设置合理的资源限制防止资源耗尽
- 监控 Agent 的内存和 CPU 使用

### 3. Skill 绑定
- Skill 必须在 Agent 启动前绑定
- 绑定的 Skill 必须已注册到系统
- 解绑 Skill 会影响正在执行的任务

### 4. 并发控制
- `max_concurrent_tasks` 限制并发任务数
- 超出限制的任务会排队等待
- 队列满时新任务会被拒绝

### 5. 错误处理
- Agent 进入 ERROR 状态后需要手动恢复
- 检查错误原因并修复后调用 `agentos_agent_start()`
- 严重错误可能需要销毁并重建 Agent

---

## 🔍 错误码参考

| 错误码 | 值 | 描述 |
|--------|-----|------|
| `AGENTOS_SUCCESS` | 0x0000 | 操作成功 |
| `AGENTOS_ERROR_INVALID_ARGUMENT` | 0x1001 | 参数无效 |
| `AGENTOS_ERROR_NO_MEMORY` | 0x1002 | 内存不足 |
| `AGENTOS_ERROR_AGENT_NOT_FOUND` | 0x5001 | Agent 不存在 |
| `AGENTOS_ERROR_NAME_EXISTS` | 0x5002 | Agent 名称已存在 |
| `AGENTOS_ERROR_INVALID_STATE` | 0x5003 | 状态转换非法 |
| `AGENTOS_ERROR_RESOURCE_LIMIT` | 0x5004 | 资源限制超出 |
| `AGENTOS_ERROR_SKILL_NOT_FOUND` | 0x5005 | Skill 不存在 |
| `AGENTOS_ERROR_SKILL_ALREADY_BOUND` | 0x5006 | Skill 已绑定 |
| `AGENTOS_ERROR_SKILL_NOT_BOUND` | 0x5007 | Skill 未绑定 |
| `AGENTOS_ERROR_PERMISSION_DENIED` | 0x1009 | 权限不足 |

---

## 📚 相关文档

- [系统调用总览](../README.md) - 所有系统调用 API 概览
- [任务管理 API](task.md) - 任务生命周期管理 API
- [记忆管理 API](memory.md) - 记忆读写和查询 API
- [会话管理 API](session.md) - 会话生命周期管理 API
- [架构设计：三层运行时](../../architecture/folder/coreloopthree.md) - CoreLoopThree 架构详解
- [开发指南：创建 Agent](../../guides/folder/create_agent.md) - Agent 开发教程

---

© 2026 SPHARX Ltd. All Rights Reserved.
*"From data intelligence emerges."*