// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file claude_adapter.h
 * @brief Anthropic Claude API Integration Adapter for AgentOS
 *
 * Claude API 适配器，实现AgentOS与Anthropic Claude模型的完整集成。
 *
 * Claude核心特性（v2026+）:
 * 1. Messages API — 多轮对话、系统提示词
 * 2. Tool Use — 原生工具调用与函数执行
 * 3. Extended Thinking — 深度推理模式
 * 4. Vision — 图像理解能力
 * 5. Streaming — SSE流式响应
 * 6. Token计数与预算控制
 * 7. Prompt Caching — 提示缓存优化
 * 8. 安全过滤 — 内容安全策略
 *
 * @since 2.1.0
 * @see unified_protocol.h
 */

#ifndef AGENTOS_CLAUDE_ADAPTER_H
#define AGENTOS_CLAUDE_ADAPTER_H

#include "unified_protocol.h"
#include "agentos_protocol_interface.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CLAUDE_ADAPTER_VERSION      "1.0.0"
#define CLAUDE_API_VERSION           "2023-06-01"
#define CLAUDE_MAX_MODELS            16
#define CLAUDE_MAX_TOOLS             64
#define CLAUDE_MAX_MESSAGES          128
#define CLAUDE_MAX_CONTEXT_TOKENS     200000
#define CLAUDE_MAX_OUTPUT_TOKENS      8192
#define CLAUDE_DEFAULT_TIMEOUT_MS    60000
#define CLAUDE_MAX_RETRIES            5

typedef enum {
    CLAUDE_ROLE_USER = 0,
    CLAUDE_ROLE_ASSISTANT,
    CLAUDE_ROLE_SYSTEM
} claude_role_t;

typedef enum {
    CLAUDE_MODEL_CLAUDE_3_5_SONNET = 0,
    CLAUDE_MODEL_CLAUDE_3_5_HAIKU,
    CLAUDE_MODEL_CLAUDE_3_OPUS,
    CLAUDE_MODEL_CLAUDE_3_SONNET,
    CLAUDE_MODEL_CLAUDE_3_HAIKU,
    CLAUDE_MODEL_CLAUDE_3_7_SONNET,
    CLAUDE_MODEL_CUSTOM
} claude_model_id_t;

typedef enum {
    CLAUDE_STOP_END_TURN = 0,
    CLAUDE_STOP_MAX_TOKENS,
    CLAUDE_STOP_TOOL_USE,
    CLAUDE_STOP_SEQUENCE
} claude_stop_reason_t;

typedef enum {
    CLAUDE_THINKING_DISABLED = 0,
    CLAUDE_THINKING_ENABLED,
    CLAUDE_THINKING_EXTENDED
} claude_thinking_mode_t;

typedef enum {
    CLAUDE_CACHE_NONE = 0,
    CLAUDE_CACHE_EPHEMERAL,
    CLAUDE_CACHE_PERSISTENT
} claude_cache_control_t;

typedef struct {
    char* id;
    claude_role_t role;
    char* content;
    char** cache_control_breakpoints;
    size_t breakpoint_count;
} claude_message_t;

typedef struct {
    char* name;
    char* description;
    char* input_schema_json;
} claude_tool_def_t;

typedef struct {
    char* id;
    char* name;
    char* input_json;
} claude_tool_use_t;

typedef struct {
    char* tool_use_id;
    char* content;
    bool is_error;
} claude_tool_result_t;

typedef struct {
    char* type;
    union {
        char* text;
        claude_tool_use_t tool_use;
        claude_tool_result_t tool_result;
        claude_thinking_mode_t thinking;
    } content;
} claude_content_block_t;

typedef struct {
    char* id;
    char* model;
    claude_role_t role;
    claude_content_block_t* content_blocks;
    size_t block_count;
    claude_stop_reason_t stop_reason;
    int input_tokens;
    int output_tokens;
    int cache_creation_input_tokens;
    int cache_read_input_tokens;
} claude_response_t;

typedef struct {
    char* text;
    claude_stop_reason_t stop_reason;
    bool is_final;
} claude_stream_event_t;

typedef struct {
    claude_model_id_t id;
    char* api_name;
    char* display_name;
    int max_context_tokens;
    int max_output_tokens;
    bool supports_vision;
    bool supports_extended_thinking;
    bool supports_tool_use;
    bool supports_prompt_caching;
    double cost_per_million_input;
    double cost_per_million_output;
    bool is_available;
} claude_model_info_t;

typedef struct {
    char* api_key;
    char* base_url;
    claude_model_id_t default_model;
    int max_tokens;
    double temperature;
    double top_p;
    double top_k;
    bool enable_streaming;
    bool enable_tool_use;
    bool enable_extended_thinking;
    claude_thinking_mode_t thinking_mode;
    int thinking_budget_tokens;
    claude_cache_control_t cache_control;
    uint32_t timeout_ms;
    int max_retries;
    bool enable_safety_filtering;
    char* system_prompt;
    char* metadata_json;
} claude_config_t;

typedef struct claude_adapter_context_s claude_adapter_context_t;

typedef int (*claude_message_handler_t)(const char* model,
                                        const claude_message_t* messages,
                                        size_t message_count,
                                        const claude_tool_def_t* tools,
                                        size_t tool_count,
                                        const char* system_prompt,
                                        claude_response_t* response,
                                        void* user_data);

typedef void (*claude_stream_handler_t)(const claude_stream_event_t* event,
                                        void* user_data);

typedef void (*claude_tool_use_handler_t)(const char* tool_name,
                                          const char* input_json,
                                          char** result_json,
                                          void* user_data);

claude_config_t claude_config_default(void);

claude_adapter_context_t* claude_adapter_create(const claude_config_t* config);
void claude_adapter_destroy(claude_adapter_context_t* ctx);

bool claude_adapter_is_initialized(const claude_adapter_context_t* ctx);
const char* claude_adapter_version(void);

int claude_messages_create(claude_adapter_context_t* ctx,
                           const claude_message_t* messages,
                           size_t message_count,
                           const claude_tool_def_t* tools,
                           size_t tool_count,
                           const char* system_prompt,
                           claude_response_t* response);

int claude_messages_stream(claude_adapter_context_t* ctx,
                           const claude_message_t* messages,
                           size_t message_count,
                           const claude_tool_def_t* tools,
                           size_t tool_count,
                           const char* system_prompt,
                           claude_stream_handler_t handler,
                           void* user_data);

int claude_count_tokens(claude_adapter_context_t* ctx,
                        const claude_message_t* messages,
                        size_t message_count,
                        const char* system_prompt,
                        int* token_count);

int claude_list_models(claude_adapter_context_t* ctx,
                       claude_model_info_t** models,
                       size_t* count);

int claude_set_message_handler(claude_adapter_context_t* ctx,
                               claude_message_handler_t handler,
                               void* user_data);

int claude_set_stream_handler(claude_adapter_context_t* ctx,
                              claude_stream_handler_t handler,
                              void* user_data);

int claude_set_tool_use_handler(claude_adapter_context_t* ctx,
                                claude_tool_use_handler_t handler,
                                void* user_data);

int claude_get_usage_statistics(claude_adapter_context_t* ctx,
                               char* stats_json,
                               size_t buffer_size);

const proto_adapter_t* claude_get_protocol_adapter(void);

void claude_response_destroy(claude_response_t* resp);
void claude_message_destroy(claude_message_t* msg);
void claude_tool_def_destroy(claude_tool_def_t* tool);
void claude_model_info_destroy(claude_model_info_t* info);
void claude_stream_event_destroy(claude_stream_event_t* event);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_CLAUDE_ADAPTER_H */
