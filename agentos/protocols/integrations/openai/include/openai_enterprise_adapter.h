// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file openai_enterprise_adapter.h
 * @brief OpenAI API Enterprise Adapter for AgentOS
 *
 * OpenAI API 企业级特性适配器，实现完整的Chat Completions、
 * Embeddings、Function Calling、Streaming、Rate Limiting等企业级能力。
 *
 * 企业级特性:
 * 1. Chat Completions (含Function Calling/Tool Use)
 * 2. Embeddings API
 * 3. Streaming SSE响应
 * 4. 速率限制与配额管理
 * 5. 多模型路由与回退
 * 6. Token预算控制
 * 7. 请求重试与超时管理
 * 8. 审计日志与合规
 *
 * @since 2.0.0
 * @see unified_protocol.h
 */

#ifndef AGENTOS_OPENAI_ENTERPRISE_ADAPTER_H
#define AGENTOS_OPENAI_ENTERPRISE_ADAPTER_H

#include "unified_protocol.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OPENAI_ADAPTER_VERSION       "1.0.0"
#define OPENAI_MAX_MODELS            32
#define OPENAI_MAX_FUNCTIONS         128
#define OPENAI_MAX_MESSAGES          256
#define OPENAI_MAX_TOKENS_DEFAULT    4096
#define OPENAI_RATE_LIMIT_RPM        60
#define OPENAI_RATE_LIMIT_TPM        100000
#define OPENAI_MAX_RETRIES           3
#define OPENAI_RETRY_BASE_MS         1000

typedef enum {
    OPENAI_ROLE_SYSTEM = 0,
    OPENAI_ROLE_USER,
    OPENAI_ROLE_ASSISTANT,
    OPENAI_ROLE_TOOL,
    OPENAI_ROLE_FUNCTION
} openai_role_t;

typedef enum {
    OPENAI_FINISH_STOP = 0,
    OPENAI_FINISH_LENGTH,
    OPENAI_FINISH_TOOL_CALLS,
    OPENAI_FINISH_CONTENT_FILTER,
    OPENAI_FINISH_RATE_LIMITED
} openai_finish_reason_t;

typedef enum {
    OPENAI_MODEL_CHAT = 0x01,
    OPENAI_MODEL_EMBEDDING = 0x02,
    OPENAI_MODEL_VISION = 0x04,
    OPENAI_MODEL_FUNCTION = 0x08,
    OPENAI_MODEL_STREAMING = 0x10
} openai_model_capability_t;

typedef struct {
    char* id;
    char* name;
    char* owned_by;
    openai_model_capability_t capabilities;
    int max_context_tokens;
    int max_output_tokens;
    double cost_per_1k_input;
    double cost_per_1k_output;
    bool is_default;
    bool is_available;
} openai_model_t;

typedef struct {
    openai_role_t role;
    char* content;
    char* name;
    char* tool_call_id;
    char* function_name;
    char* function_arguments_json;
} openai_message_t;

typedef struct {
    char* id;
    char* type;
    char* function_name;
    char* function_arguments_json;
} openai_tool_call_t;

typedef struct {
    char* name;
    char* description;
    char* parameters_schema_json;
    bool strict;
} openai_function_def_t;

typedef struct {
    char* type;
    openai_function_def_t function;
} openai_tool_def_t;

typedef struct {
    int prompt_tokens;
    int completion_tokens;
    int total_tokens;
} openai_usage_t;

typedef struct {
    char* id;
    char* object;
    uint64_t created;
    char* model;
    openai_message_t* choices;
    openai_finish_reason_t* finish_reasons;
    size_t choice_count;
    openai_usage_t usage;
    openai_tool_call_t* tool_calls;
    size_t tool_call_count;
} openai_chat_response_t;

typedef struct {
    char* id;
    char* object;
    char* model;
    double* embeddings;
    size_t embedding_dim;
    openai_usage_t usage;
} openai_embedding_response_t;

typedef struct {
    double rpm_limit;
    double tpm_limit;
    double current_rpm;
    double current_tpm;
    uint64_t window_start_ms;
    uint64_t window_duration_ms;
} openai_rate_limit_t;

typedef struct {
    char* api_key;
    char* base_url;
    char* default_model;
    char* organization;
    int max_retries;
    uint32_t retry_base_ms;
    uint32_t request_timeout_ms;
    bool enable_streaming;
    bool enable_function_calling;
    bool enable_rate_limiting;
    bool enable_audit_logging;
    double rpm_limit;
    double tpm_limit;
    int max_tokens_default;
    double temperature_default;
    double top_p_default;
    bool strict_schema_validation;
} openai_enterprise_config_t;

typedef struct openai_enterprise_context_s openai_enterprise_context_t;

typedef int (*openai_chat_handler_t)(const char* model,
                                      const openai_message_t* messages,
                                      size_t message_count,
                                      const openai_tool_def_t* tools,
                                      size_t tool_count,
                                      double temperature,
                                      double top_p,
                                      int max_tokens,
                                      bool stream,
                                      openai_chat_response_t* response,
                                      void* user_data);

typedef int (*openai_embedding_handler_t)(const char* model,
                                           const char** inputs,
                                           size_t input_count,
                                           openai_embedding_response_t* response,
                                           void* user_data);

typedef void (*openai_streaming_handler_t)(const char* chunk_content,
                                            const char* model,
                                            openai_finish_reason_t finish_reason,
                                            void* user_data);

typedef void (*openai_audit_handler_t)(const char* method,
                                        const char* model,
                                        const openai_usage_t* usage,
                                        uint64_t latency_ms,
                                        int status_code,
                                        void* user_data);

openai_enterprise_config_t openai_enterprise_config_default(void);

openai_enterprise_context_t* openai_enterprise_context_create(const openai_enterprise_config_t* config);
void openai_enterprise_context_destroy(openai_enterprise_context_t* ctx);

int openai_enterprise_register_model(openai_enterprise_context_t* ctx,
                                       const openai_model_t* model);

int openai_enterprise_chat_completion(openai_enterprise_context_t* ctx,
                                       const char* model,
                                       const openai_message_t* messages,
                                       size_t message_count,
                                       const openai_tool_def_t* tools,
                                       size_t tool_count,
                                       double temperature,
                                       double top_p,
                                       int max_tokens,
                                       openai_chat_response_t* response);

int openai_enterprise_chat_streaming(openai_enterprise_context_t* ctx,
                                       const char* model,
                                       const openai_message_t* messages,
                                       size_t message_count,
                                       openai_streaming_handler_t handler,
                                       void* user_data);

int openai_enterprise_embeddings(openai_enterprise_context_t* ctx,
                                   const char* model,
                                   const char** inputs,
                                   size_t input_count,
                                   openai_embedding_response_t* response);

int openai_enterprise_list_models(openai_enterprise_context_t* ctx,
                                    openai_model_t** models,
                                    size_t* model_count);

bool openai_enterprise_check_rate_limit(openai_enterprise_context_t* ctx,
                                          int estimated_tokens);

int openai_enterprise_set_chat_handler(openai_enterprise_context_t* ctx,
                                         openai_chat_handler_t handler,
                                         void* user_data);

int openai_enterprise_set_embedding_handler(openai_enterprise_context_t* ctx,
                                              openai_embedding_handler_t handler,
                                              void* user_data);

int openai_enterprise_set_audit_handler(openai_enterprise_context_t* ctx,
                                          openai_audit_handler_t handler,
                                          void* user_data);

int openai_enterprise_route_request(openai_enterprise_context_t* ctx,
                                      const char* path,
                                      const char* method,
                                      const char* body_json,
                                      char** response_json);

const protocol_adapter_t* openai_enterprise_get_adapter(void);

void openai_chat_response_destroy(openai_chat_response_t* resp);
void openai_embedding_response_destroy(openai_embedding_response_t* resp);
void openai_message_destroy(openai_message_t* msg);
void openai_model_destroy(openai_model_t* model);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_OPENAI_ENTERPRISE_ADAPTER_H */
