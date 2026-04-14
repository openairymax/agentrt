Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
"From data intelligence emerges."

# AgentOS 会话管理 API

**版本**: Doc V1.8  
**最后更新**: 2026-04-09  
**状态**: 🟢 生产就绪

---

## 🎯 概述

会话管理 API 提供 AgentOS 会话生命周期的完整控制。会话是 Agent 与用户或系统交互的上下文容器，包含对话历史、记忆关联、状态信息和执行上下文。每个会话遵循**双系统路径**：System 1 快速响应，System 2 深度处理。

### 🧩 五维正交原则体现

会话管理 API 深度体现了 AgentOS 的五维正交设计原则：

| 维度 | 原则体现 | 具体实现 |
|------|----------|---------|
| **系统观** | 会话状态机的完整反馈闭环 | 创建→更新→关闭的完整生命周期管理 |
| **内核观** | 会话资源的契约化管理 | 统一的会话标识符、明确的资源所有权 |
| **认知观** | 双系统交互上下文管理 | System 1/System 2 切换支持，上下文保持机制 |
| **工程观** | 会话数据的安全与可观测性 | 访问控制、审计日志、性能监控 |
| **设计美学** | 优雅的会话组织 | 自然的多轮对话支持，灵活的上下文管理 |

---

## 📋 API 索引

| 函数 | 描述 | 复杂度 |
|------|------|--------|
| `agentos_session_create()` | 创建新会话 | O(1) |
| `agentos_session_get()` | 获取会话信息 | O(1) |
| `agentos_session_update()` | 更新会话状态 | O(1) |
| `agentos_session_close()` | 关闭会话 | O(1) |
| `agentos_session_list()` | 列出所有会话 | O(n) |
| `agentos_session_message_add()` | 添加消息到会话 | O(1) |
| `agentos_session_message_list()` | 列出会话消息 | O(n) |
| `agentos_session_context_get()` | 获取会话上下文 | O(1) |
| `agentos_session_context_set()` | 设置会话上下文 | O(1) |
| `agentos_session_stats()` | 获取会话统计信息 | O(1) |

---

## 🔧 核心数据结构

### 会话描述符

```c
/**
 * @brief 会话描述符结构体
 * 
 * 描述一个交互会话，包含会话元数据、状态和上下文信息。
 */
typedef struct agentos_session_descriptor {
    /** 会话唯一标识符（UUID 格式） */
    char id[37];  // 36字符UUID + 终止符
    
    /** 会话标题（可选） */
    char title[256];
    
    /** 会话创建时间戳（Unix 毫秒） */
    uint64_t created_at;
    
    /** 最后活动时间戳（Unix 毫秒） */
    uint64_t last_activity_at;
    
    /** 会话状态 */
    enum {
        SESSION_STATE_ACTIVE = 0,     /* 活跃状态 */
        SESSION_STATE_PAUSED = 1,     /* 暂停状态 */
        SESSION_STATE_CLOSED = 2,     /* 已关闭 */
        SESSION_STATE_ARCHIVED = 3    /* 已归档 */
    } state;
    
    /** 会话类型 */
    enum {
        SESSION_TYPE_CHAT = 0,        /* 聊天会话 */
        SESSION_TYPE_TASK = 1,        /* 任务执行会话 */
        SESSION_TYPE_ANALYSIS = 2,    /* 分析会话 */
        SESSION_TYPE_DEBUG = 3        /* 调试会话 */
    } type;
    
    /** 所属 Agent ID */
    char agent_id[64];
    
    /** 用户标识（可选） */
    char user_id[128];
    
    /** 会话标签 */
    char** tags;
    
    /** 标签数量 */
    uint32_t tag_count;
    
    /** 会话元数据（JSON 格式） */
    char* metadata;
    
    /** 会话上下文（JSON 格式） */
    char* context;
    
    /** 消息数量 */
    uint32_t message_count;
    
    /** 总交互次数 */
    uint32_t total_interactions;
    
    /** 平均响应时间（毫秒） */
    uint32_t avg_response_time_ms;
    
    /** 会话 trace_id（用于全链路追踪） */
    char trace_id[128];
} agentos_session_descriptor_t;
```

### 会话消息

```c
/**
 * @brief 会话消息结构体
 * 
 * 描述会话中的一条消息，包含内容、角色和元数据。
 */
typedef struct agentos_session_message {
    /** 消息唯一标识符 */
    char id[37];  // UUID
    
    /** 消息角色 */
    enum {
        MESSAGE_ROLE_USER = 0,      /* 用户消息 */
        MESSAGE_ROLE_ASSISTANT = 1, /* 助手消息 */
        MESSAGE_ROLE_SYSTEM = 2,    /* 系统消息 */
        MESSAGE_ROLE_TOOL = 3       /* 工具调用消息 */
    } role;
    
    /** 消息内容（UTF-8） */
    char* content;
    
    /** 内容长度（字节） */
    size_t content_len;
    
    /** 消息时间戳（Unix 毫秒） */
    uint64_t timestamp;
    
    /** 消息元数据（JSON 格式） */
    char* metadata;
    
    /** 关联的记忆记录 ID（可选） */
    char* memory_record_id;
    
    /** 关联的任务 ID（可选） */
    uint64_t task_id;
    
    /** 处理路径 */
    enum {
        PROCESSING_PATH_SYSTEM1 = 0,  /* System 1 快速路径 */
        PROCESSING_PATH_SYSTEM2 = 1   /* System 2 深度路径 */
    } processing_path;
    
    /** 处理耗时（毫秒） */
    uint32_t processing_time_ms;
    
    /** 消息 trace_id */
    char trace_id[128];
    
    /** 下一消息指针（链表结构） */
    struct agentos_session_message* next;
} agentos_session_message_t;
```

### 会话上下文

```c
/**
 * @brief 会话上下文结构体
 * 
 * 描述会话的运行时上下文，用于维护对话状态和记忆关联。
 */
typedef struct agentos_session_context {
    /** 对话历史摘要 */
    char* conversation_summary;
    
    /** 当前对话主题 */
    char* current_topic;
    
    /** 用户意图（JSON 格式） */
    char* user_intent;
    
    /** 对话状态（JSON 格式） */
    char* dialog_state;
    
    /** 相关记忆记录 ID 列表 */
    char** related_memory_ids;
    
    /** 相关记忆记录数量 */
    uint32_t related_memory_count;
    
    /** 活跃任务 ID 列表 */
    uint64_t* active_task_ids;
    
    /** 活跃任务数量 */
    uint32_t active_task_count;
    
    /** 会话变量（JSON 格式） */
    char* variables;
    
    /** 上下文版本号 */
    uint32_t version;
    
    /** 最后更新时间戳 */
    uint64_t last_updated_at;
} agentos_session_context_t;
```

### 会话统计信息

```c
/**
 * @brief 会话统计信息结构体
 */
typedef struct agentos_session_statistics {
    /** 总会话数 */
    uint64_t total_sessions;
    
    /** 活跃会话数 */
    uint64_t active_sessions;
    
    /** 平均会话时长（分钟） */
    float avg_session_duration_minutes;
    
    /** 平均消息数/会话 */
    float avg_messages_per_session;
    
    /** 总消息数 */
    uint64_t total_messages;
    
    /** System 1 路径消息占比（%） */
    float system1_message_ratio_percent;
    
    /** System 2 路径消息占比（%） */
    float system2_message_ratio_percent;
    
    /** 平均响应时间（毫秒） */
    uint32_t avg_response_time_ms;
    
    /** 会话创建频率（会话/小时） */
    float session_creation_rate_per_hour;
    
    /** 按类型分布的会话数 */
    struct {
        uint64_t chat;
        uint64_t task;
        uint64_t analysis;
        uint64_t debug;
    } sessions_by_type;
} agentos_session_statistics_t;
```

---

## 📖 API 详细说明

### `agentos_session_create()`

```c
/**
 * @brief 创建新会话
 * 
 * 创建一个新的交互会话，并返回会话 ID。
 * 
 * @param agent_id 所属 Agent ID
 * @param type 会话类型
 * @param title 会话标题（可选）
 * @param metadata 会话元数据（JSON 格式，可选）
 * @param out_session_id 输出参数，返回会话 ID
 * @return AGENTOS_SUCCESS 成功；其他值表示错误
 * 
 * @ownership out_session_id 由调用者负责释放
 * @threadsafe 是
 * @reentrant 否
 * @see agentos_session_close(), agentos_session_get()
 */
AGENTOS_API agentos_error_t
agentos_session_create(const char* agent_id,
                       agentos_session_type_t type,
                       const char* title,
                       const char* metadata,
                       char** out_session_id);
```

**参数说明**：
- `agent_id`: 创建会话的 Agent ID
- `type`: 会话类型（聊天、任务、分析、调试）
- `title`: 会话标题，用于标识会话内容
- `metadata`: JSON 格式元数据，如 `{"priority": "high", "domain": "customer_service"}`
- `out_session_id`: 输出参数，返回生成的会话 ID

**返回值**：
- `AGENTOS_SUCCESS` (0x0000): 创建成功
- `AGENTOS_ERROR_INVALID_ARGUMENT` (0x1001): 参数无效
- `AGENTOS_ERROR_NO_MEMORY` (0x1002): 内存不足
- `AGENTOS_ERROR_AGENT_NOT_FOUND` (0x3006): Agent 不存在

**使用示例**：
```c
char* session_id = NULL;
agentos_error_t err = agentos_session_create(
    "agent_customer_service",
    SESSION_TYPE_CHAT,
    "用户技术支持会话",
    "{\"priority\": \"high\", \"user_tier\": \"premium\"}",
    &session_id
);

if (err == AGENTOS_SUCCESS) {
    printf("会话创建成功，ID: %s\n", session_id);
    free(session_id);
}
```

### `agentos_session_get()`

```c
/**
 * @brief 获取会话信息
 * 
 * 根据会话 ID 获取会话的详细信息。
 * 
 * @param session_id 会话 ID
 * @param out_descriptor 输出参数，返回会话描述符
 * @return AGENTOS_SUCCESS 成功；其他值表示错误
 * 
 * @ownership out_descriptor 由调用者负责释放
 * @threadsafe 是
 * @reentrant 是
 */
AGENTOS_API agentos_error_t
agentos_session_get(const char* session_id,
                    agentos_session_descriptor_t** out_descriptor);
```

### `agentos_session_message_add()`

```c
/**
 * @brief 添加消息到会话
 * 
 * 向指定会话添加一条消息，并触发相应的处理流程。
 * 
 * @param session_id 会话 ID
 * @param role 消息角色
 * @param content 消息内容
 * @param content_len 内容长度
 * @param metadata 消息元数据（JSON 格式，可选）
 * @param out_message_id 输出参数，返回消息 ID
 * @return AGENTOS_SUCCESS 成功；其他值表示错误
 * 
 * @ownership out_message_id 由调用者负责释放
 * @threadsafe 是
 * @reentrant 否
 */
AGENTOS_API agentos_error_t
agentos_session_message_add(const char* session_id,
                            agentos_message_role_t role,
                            const char* content,
                            size_t content_len,
                            const char* metadata,
                            char** out_message_id);
```

**使用示例**：
```c
// 添加用户消息
char* user_msg_id = NULL;
err = agentos_session_message_add(
    session_id,
    MESSAGE_ROLE_USER,
    "我的账户无法登录，显示密码错误",
    38,  // 内容长度（字节）
    "{\"emotion\": \"frustrated\", \"urgency\": \"high\"}",
    &user_msg_id
);

// 添加助手回复
char* assistant_msg_id = NULL;
err = agentos_session_message_add(
    session_id,
    MESSAGE_ROLE_ASSISTANT,
    "很抱歉听到您遇到登录问题。让我帮您解决：\n1. 请确认密码是否正确\n2. 可以尝试重置密码\n3. 检查网络连接是否正常",
    120,
    "{\"processing_path\": \"system1\", \"response_time_ms\": 150}",
    &assistant_msg_id
);
```

### `agentos_session_context_get()`

```c
/**
 * @brief 获取会话上下文
 * 
 * 获取会话的当前上下文信息，包括对话状态、记忆关联等。
 * 
 * @param session_id 会话 ID
 * @param out_context 输出参数，返回会话上下文
 * @return AGENTOS_SUCCESS 成功；其他值表示错误
 * 
 * @ownership out_context 由调用者负责释放
 * @threadsafe 是
 * @reentrant 是
 */
AGENTOS_API agentos_error_t
agentos_session_context_get(const char* session_id,
                            agentos_session_context_t** out_context);
```

### `agentos_session_close()`

```c
/**
 * @brief 关闭会话
 * 
 * 关闭指定会话，释放相关资源，并可选归档会话历史。
 * 
 * @param session_id 会话 ID
 * @param archive 是否归档会话
 * @param summary 会话摘要（可选）
 * @return AGENTOS_SUCCESS 成功；其他值表示错误
 * 
 * @threadsafe 是
 * @reentrant 否
 */
AGENTOS_API agentos_error_t
agentos_session_close(const char* session_id,
                      bool archive,
                      const char* summary);
```

### `agentos_session_update()`

```c
/**
 * @brief 更新会话信息
 * 
 * 动态更新会话的元数据。仅允许更新部分字段，
 * 会话 ID 和创建时间不可更改。
 * 
 * @param session_id 会话 ID
 * @param updates 更新内容（JSON 格式，仅包含需更新的字段）
 * @return AGENTOS_SUCCESS 成功；其他值表示错误
 * 
 * @threadsafe 是
 * @reentrant 否
 */
AGENTOS_API agentos_error_t
agentos_session_update(const char* session_id,
                       const char* updates);
```

**可更新字段**（JSON 格式）：
```json
{
    "title": "新会话标题",
    "tags": ["updated", "important"],
    "metadata": {"key": "value"}
}
```

**返回值**：
- `AGENTOS_SUCCESS` (0x0000): 更新成功
- `AGENTOS_ERROR_SESSION_NOT_FOUND` (0x3001): 会话不存在
- `AGENTOS_ERROR_INVALID_ARGUMENT` (0x1001): 参数无效

### `agentos_session_list()`

```c
/**
 * @brief 列出所有会话
 * 
 * 根据过滤条件列出会话列表。
 * 
 * @param filter 过滤条件（JSON 格式，NULL 表示列出所有）
 * @param offset 分页偏移量
 * @param limit 每页数量限制
 * @param out_sessions [out] 返回会话描述符数组
 * @param out_count [out] 返回实际数量
 * @return AGENTOS_SUCCESS 成功；其他值表示错误
 * 
 * @ownership 调用者拥有返回数组，需逐个释放后释放数组本身
 * @threadsafe 是
 * @reentrant 否
 */
AGENTOS_API agentos_error_t
agentos_session_list(const char* filter,
                     uint32_t offset,
                     uint32_t limit,
                     agentos_session_descriptor_t*** out_sessions,
                     uint32_t* out_count);
```

**返回值**：
- `AGENTOS_SUCCESS` (0x0000): 获取成功
- `AGENTOS_ERROR_INVALID_ARGUMENT` (0x1001): 参数无效
- `AGENTOS_ERROR_NO_MEMORY` (0x1002): 内存不足

### `agentos_session_message_list()`

```c
/**
 * @brief 列出会话消息
 * 
 * 获取指定会话的消息列表，支持分页和过滤。
 * 
 * @param session_id 会话 ID
 * @param offset 消息偏移量（0 为最早消息）
 * @param limit 最大返回条数
 * @param out_messages [out] 返回消息数组
 * @param out_count [out] 返回实际数量
 * @return AGENTOS_SUCCESS 成功；其他值表示错误
 * 
 * @ownership 调用者拥有返回数组，需通过 agentos_session_message_list_free() 释放
 * @threadsafe 是
 * @reentrant 否
 */
AGENTOS_API agentos_error_t
agentos_session_message_list(const char* session_id,
                             uint32_t offset,
                             uint32_t limit,
                             agentos_session_message_t*** out_messages,
                             uint32_t* out_count);
```

**辅助释放函数**：
```c
AGENTOS_API void agentos_session_message_list_free(
    agentos_session_message_t** messages, uint32_t count);
```

**返回值**：
- `AGENTOS_SUCCESS` (0x0000): 获取成功
- `AGENTOS_ERROR_SESSION_NOT_FOUND` (0x3001): 会话不存在
- `AGENTOS_ERROR_INVALID_ARGUMENT` (0x1001): 参数无效

### `agentos_session_context_set()`

```c
/**
 * @brief 设置会话上下文
 * 
 * 手动设置或更新会话的上下文信息。通常上下文由系统自动维护，
 * 此接口用于高级场景的手动干预。
 * 
 * @param session_id 会话 ID
 * @param context 上下文数据（JSON 格式）
 * @return AGENTOS_SUCCESS 成功；其他值表示错误
 * 
 * @threadsafe 是
 * @reentrant 否
 * @see agentos_session_context_get()
 */
AGENTOS_API agentos_error_t
agentos_session_context_set(const char* session_id,
                            const char* context);
```

**上下文 JSON 示例**：
```json
{
    "conversation_summary": "用户正在讨论微内核架构设计",
    "current_topic": "IPC 机制优化",
    "user_intent": "寻求技术方案",
    "related_memory_ids": ["mem_001", "mem_042"],
    "active_task_ids": ["task_007"]
}
```

**返回值**：
- `AGENTOS_SUCCESS` (0x0000): 设置成功
- `AGENTOS_ERROR_SESSION_NOT_FOUND` (0x3001): 会话不存在
- `AGENTOS_ERROR_INVALID_ARGUMENT` (0x1001): 上下文格式无效

### `agentos_session_stats()`

```c
/**
 * @brief 获取会话统计信息
 * 
 * 获取全局会话统计信息，包括会话数量、消息数量等。
 * 
 * @param out_stats [out] 返回统计信息结构体
 * @return AGENTOS_SUCCESS 成功；其他值表示错误
 * 
 * @ownership 无堆内存分配，结构体为值类型
 * @threadsafe 是
 * @reentrant 是
 */
AGENTOS_API agentos_error_t
agentos_session_stats(agentos_session_statistics_t* out_stats);
```

**返回值**：
- `AGENTOS_SUCCESS` (0x0000): 获取成功
- `AGENTOS_ERROR_INVALID_ARGUMENT` (0x1001): 参数无效

---

## 🚀 使用示例

### 完整会话管理流程

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "agentos/session.h"

int main(void)
{
    agentos_error_t err;
    char* session_id = NULL;
    
    // 1. 创建新会话
    err = agentos_session_create(
        "agent_technical_support",
        SESSION_TYPE_CHAT,
        "用户技术支持：登录问题",
        "{\"priority\": \"high\", \"category\": \"authentication\"}",
        &session_id
    );
    
    if (err != AGENTOS_SUCCESS) {
        fprintf(stderr, "会话创建失败: 0x%04X\n", err);
        return 1;
    }
    
    printf("会话创建成功: %s\n", session_id);
    
    // 2. 添加对话消息
    const char* conversation[] = {
        "用户: 我无法登录我的账户，一直显示密码错误",
        "助手: 很抱歉听到您遇到登录问题。首先，请确认：\n1. 密码是否正确（注意大小写）\n2. 是否开启了Caps Lock键",
        "用户: 密码是正确的，我刚刚重置过",
        "助手: 明白了。可能是账户被锁定或存在异常登录尝试。\n请尝试：\n1. 使用\"忘记密码\"功能再次重置\n2. 检查邮箱是否有安全通知\n3. 或者我帮您转接人工客服",
        "用户: 好的，我试试重置密码"
    };
    
    agentos_message_role_t roles[] = {
        MESSAGE_ROLE_USER,
        MESSAGE_ROLE_ASSISTANT,
        MESSAGE_ROLE_USER,
        MESSAGE_ROLE_ASSISTANT,
        MESSAGE_ROLE_USER
    };
    
    for (int i = 0; i < 5; i++) {
        char* msg_id = NULL;
        err = agentos_session_message_add(
            session_id,
            roles[i],
            conversation[i],
            strlen(conversation[i]),
            i % 2 == 0 ? "{\"user_initiated\": true}" : "{\"assistant_response\": true}",
            &msg_id
        );
        
        if (err == AGENTOS_SUCCESS) {
            printf("消息 %d 添加成功: %s\n", i, msg_id);
            free(msg_id);
        }
    }
    
    // 3. 获取会话信息
    agentos_session_descriptor_t* desc = NULL;
    err = agentos_session_get(session_id, &desc);
    
    if (err == AGENTOS_SUCCESS) {
        printf("\n会话信息:\n");
        printf("  标题: %s\n", desc->title);
        printf("  状态: %d\n", desc->state);
        printf("  消息数: %u\n", desc->message_count);
        printf("  最后活动: %llu\n", desc->last_activity_at);
        
        free(desc->metadata);
        free(desc);
    }
    
    // 4. 获取会话上下文
    agentos_session_context_t* context = NULL;
    err = agentos_session_context_get(session_id, &context);
    
    if (err == AGENTOS_SUCCESS) {
        printf("\n会话上下文:\n");
        printf("  当前主题: %s\n", context->current_topic ? context->current_topic : "N/A");
        printf("  相关记忆: %u 条\n", context->related_memory_count);
        printf("  活跃任务: %u 个\n", context->active_task_count);
        
        free(context->conversation_summary);
        free(context->current_topic);
        free(context->user_intent);
        free(context->dialog_state);
        free(context->variables);
        free(context);
    }
    
    // 5. 列出会话消息
    agentos_session_message_t* messages = NULL;
    uint32_t message_count = 0;
    err = agentos_session_message_list(session_id, 0, 10, &messages, &message_count);
    
    if (err == AGENTOS_SUCCESS) {
        printf("\n最近 %u 条消息:\n", message_count);
        
        agentos_session_message_t* current = messages;
        while (current != NULL) {
            const char* role_str = "未知";
            switch (current->role) {
                case MESSAGE_ROLE_USER: role_str = "用户"; break;
                case MESSAGE_ROLE_ASSISTANT: role_str = "助手"; break;
                case MESSAGE_ROLE_SYSTEM: role_str = "系统"; break;
                case MESSAGE_ROLE_TOOL: role_str = "工具"; break;
            }
            
            printf("[%s] %.*s\n", role_str, 
                   (int)(current->content_len > 50 ? 50 : current->content_len),
                   current->content);
            
            current = current->next;
        }
        
        // 释放消息链表
        agentos_session_message_list_free(messages);
    }
    
    // 6. 关闭会话
    err = agentos_session_close(session_id, true, "用户登录问题解决，建议密码重置");
    if (err == AGENTOS_SUCCESS) {
        printf("\n会话已关闭并归档\n");
    }
    
    // 7. 获取会话统计
    agentos_session_statistics_t stats;
    err = agentos_session_stats(&stats);
    
    if (err == AGENTOS_SUCCESS) {
        printf("\n会话统计:\n");
        printf("  总会话数: %llu\n", stats.total_sessions);
        printf("  活跃会话: %llu\n", stats.active_sessions);
        printf("  平均消息数: %.1f\n", stats.avg_messages_per_session);
        printf("  System 1 消息: %.1f%%\n", stats.system1_message_ratio_percent);
        printf("  System 2 消息: %.1f%%\n", stats.system2_message_ratio_percent);
    }
    
    // 清理资源
    free(session_id);
    
    return 0;
}
```

### 高级会话操作

```c
// 批量会话管理
void batch_session_management(void)
{
    // 创建多个会话
    const char* agent_ids[] = {"agent_sales", "agent_support", "agent_technical"};
    char* session_ids[3];
    
    for (int i = 0; i < 3; i++) {
        agentos_session_create(
            agent_ids[i],
            SESSION_TYPE_CHAT,
            "批量测试会话",
            "{\"batch_test\": true}",
            &session_ids[i]
        );
    }
    
    // 批量添加消息
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            char* msg_id = NULL;
            agentos_session_message_add(
                session_ids[i],
                j % 2 == 0 ? MESSAGE_ROLE_USER : MESSAGE_ROLE_ASSISTANT,
                "测试消息内容",
                20,
                NULL,
                &msg_id
            );
            free(msg_id);
        }
    }
    
    // 批量关闭会话
    for (int i = 0; i < 3; i++) {
        agentos_session_close(session_ids[i], false, NULL);
        free(session_ids[i]);
    }
}

// 会话搜索和过滤
void session_search_example(void)
{
    // 构建过滤条件
    const char* filter = 
        "{\n"
        "  \"state\": \"active\",\n"
        "  \"type\": \"chat\",\n"
        "  \"created_after\": 1672531200000,\n"
        "  \"min_messages\": 5,\n"
        "  \"tags\": [\"urgent\", \"customer\"]\n"
        "}";
    
    agentos_session_descriptor_t** sessions = NULL;
    uint32_t session_count = 0;
    
    agentos_error_t err = agentos_session_list(filter, 0, 20, &sessions, &session_count);
    
    if (err == AGENTOS_SUCCESS) {
        printf("找到 %u 个匹配会话:\n", session_count);
        
        for (uint32_t i = 0; i < session_count; i++) {
            printf("%u. %s (消息: %u)\n", 
                   i + 1, 
                   sessions[i]->title,
                   sessions[i]->message_count);
            
            // 释放单个会话描述符
            free(sessions[i]->metadata);
            free(sessions[i]);
        }
        
        // 释放会话数组
        free(sessions);
    }
}
```

---

## ⚠️ 注意事项

### 1. 会话生命周期
- 会话创建后默认处于活跃状态
- 长时间无活动的会话可能被系统自动暂停或关闭
- 关闭的会话可以归档，归档后无法添加新消息但可以查询历史

### 2. 消息管理
- 消息按添加顺序存储在会话中
- 消息内容最大支持 64KB
- 消息添加是异步操作，返回成功表示已加入处理队列
- 使用 `agentos_session_message_list()` 分页获取消息，避免内存溢出

### 3. 上下文一致性
- 会话上下文在每次消息交互后自动更新
- 可以手动调用 `agentos_session_context_set()` 覆盖上下文
- 上下文版本号用于检测并发修改冲突

### 4. 性能考虑
- 会话创建和关闭是 O(1) 操作
- 消息列表查询是 O(n) 操作，建议使用分页
- 大量会话时使用过滤条件减少返回数据量

### 5. 错误处理
- 始终检查 API 返回值
- 使用 `agentos_strerror()` 获取错误描述
- 记录 `trace_id` 便于问题排查

---

## 🔍 错误码参考

| 错误码 | 值 | 描述 |
|--------|-----|------|
| `AGENTOS_SUCCESS` | 0x0000 | 操作成功 |
| `AGENTOS_ERROR_INVALID_ARGUMENT` | 0x1001 | 参数无效 |
| `AGENTOS_ERROR_NO_MEMORY` | 0x1002 | 内存不足 |
| `AGENTOS_ERROR_SESSION_NOT_FOUND` | 0x3001 | 会话不存在 |
| `AGENTOS_ERROR_SESSION_CLOSED` | 0x3002 | 会话已关闭 |
| `AGENTOS_ERROR_SESSION_ARCHIVED` | 0x3003 | 会话已归档 |
| `AGENTOS_ERROR_MESSAGE_TOO_LARGE` | 0x3004 | 消息内容过大 |
| `AGENTOS_ERROR_CONTEXT_VERSION_MISMATCH` | 0x3005 | 上下文版本不匹配 |
| `AGENTOS_ERROR_AGENT_NOT_FOUND` | 0x3006 | Agent 不存在 |
| `AGENTOS_ERROR_PERMISSION_DENIED` | 0x1009 | 权限不足 |

---

## 📚 相关文档

- [系统调用总览](../README.md) - 所有系统调用 API 概览
- [任务管理 API](task.md) - 任务生命周期管理 API
- [记忆管理 API](memory.md) - 记忆读写和查询 API
- [可观测性 API](telemetry.md) - 指标、日志和追踪 API
- [Agent 管理 API](agent.md) - Agent 生命周期管理 API
- [架构设计：三层运行时](../../architecture/folder/coreloopthree.md) - CoreLoopThree 架构详解

---

© 2026 SPHARX Ltd. All Rights Reserved.
*"From data intelligence emerges."*