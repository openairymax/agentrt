// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file mcp_v1_adapter.h
 * @brief MCP v1.0 Protocol Adapter for AgentOS
 *
 * Model Context Protocol v1.0 完整协议适配器。
 * 实现MCP规范定义的工具发现、调用、资源访问、采样等核心能力。
 *
 * MCP v1.0 核心能力:
 * 1. tools/list + tools/call — 工具发现与调用
 * 2. resources/list + resources/read + resources/templates — 资源管理
 * 3. prompts/list + prompts/get — 提示模板管理
 * 4. completion/complete — 自动补全
 * 5. sampling/createMessage — LLM采样请求
 * 6. logging/setLogLevel — 日志级别控制
 * 7. notifications — 进度/取消/消息通知
 *
 * @since 2.0.0
 * @see unified_protocol.h
 */

#ifndef AGENTOS_MCP_V1_ADAPTER_H
#define AGENTOS_MCP_V1_ADAPTER_H

#include "unified_protocol.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MCP_V1_VERSION              "1.0.0"
#define MCP_V1_PROTOCOL_NAME        "mcp"
#define MCP_V1_MAX_TOOLS            1024
#define MCP_V1_MAX_RESOURCES        512
#define MCP_V1_MAX_PROMPTS          256
#define MCP_V1_MAX_TOOL_ARGS        32
#define MCP_V1_MAX_SCHEMA_DEPTH     16
#define MCP_V1_DEFAULT_TIMEOUT_MS   30000
#define MCP_V1_MAX_MESSAGE_SIZE     (10 * 1024 * 1024)

typedef enum {
    MCP_CAP_TOOLS = 0x01,
    MCP_CAP_RESOURCES = 0x02,
    MCP_CAP_PROMPTS = 0x04,
    MCP_CAP_SAMPLING = 0x08,
    MCP_CAP_LOGGING = 0x10,
    MCP_CAP_COMPLETION = 0x20
} mcp_capability_t;

typedef enum {
    MCP_CONTENT_TEXT = 0,
    MCP_CONTENT_IMAGE,
    MCP_CONTENT_RESOURCE,
    MCP_CONTENT_EMBEDDED
} mcp_content_type_t;

typedef enum {
    MCP_LOG_DEBUG = 0,
    MCP_LOG_INFO,
    MCP_LOG_NOTICE,
    MCP_LOG_WARNING,
    MCP_LOG_ERROR,
    MCP_LOG_CRITICAL
} mcp_log_level_t;

typedef struct {
    mcp_content_type_t type;
    char* text;
    char* mime_type;
    char* uri;
    char* data;
    size_t data_size;
} mcp_content_t;

typedef struct {
    char* name;
    char* description;
    char* input_schema_json;
    mcp_capability_t required_caps;
} mcp_tool_t;

typedef struct {
    char* uri;
    char* name;
    char* description;
    char* mime_type;
} mcp_resource_t;

typedef struct {
    char* uri_template;
    char* name;
    char* description;
    char* mime_type;
} mcp_resource_template_t;

typedef struct {
    char* name;
    char* description;
    char* arguments_schema_json;
} mcp_prompt_t;

typedef struct {
    char* role;
    mcp_content_t* content;
    size_t content_count;
} mcp_sampling_message_t;

typedef struct {
    mcp_sampling_message_t* messages;
    size_t message_count;
    char* model;
    int max_tokens;
    double temperature;
    double top_p;
    int n;
    bool stream;
    char* stop_sequences_json;
} mcp_sampling_params_t;

typedef struct {
    char* model;
    mcp_content_t* content;
    size_t content_count;
    mcp_sampling_message_t* messages;
    size_t message_count;
    bool stopped_early;
    char* stop_reason;
} mcp_sampling_result_t;

typedef struct {
    char* reference_type;
    char* ref_uri;
    char* ref_name;
} mcp_completion_ref_t;

typedef struct {
    mcp_completion_ref_t ref;
    char* argument_name;
    char* prefix;
} mcp_completion_request_t;

typedef struct {
    char** values;
    size_t value_count;
    bool has_more;
    int total;
} mcp_completion_result_t;

typedef struct {
    uint32_t capabilities;
    uint32_t default_timeout_ms;
    size_t max_tools;
    size_t max_resources;
    size_t max_message_size;
    bool enable_progress_notifications;
    bool enable_cancellation;
    bool enable_sampling;
    char* server_name;
    char* server_version;
} mcp_v1_config_t;

typedef struct mcp_v1_context_s mcp_v1_context_t;

typedef void (*mcp_tool_handler_t)(const char* tool_name,
                                    const char* arguments_json,
                                    mcp_content_t** results,
                                    size_t* result_count,
                                    bool* is_error,
                                    void* user_data);

typedef void (*mcp_resource_handler_t)(const char* uri,
                                        char** content,
                                        char** mime_type,
                                        void* user_data);

typedef void (*mcp_prompt_handler_t)(const char* name,
                                      const char* arguments_json,
                                      mcp_sampling_message_t** messages,
                                      size_t* message_count,
                                      void* user_data);

typedef void (*mcp_sampling_handler_t)(const mcp_sampling_params_t* params,
                                        mcp_sampling_result_t* result,
                                        void* user_data);

typedef void (*mcp_completion_handler_t)(const mcp_completion_request_t* request,
                                          mcp_completion_result_t* result,
                                          void* user_data);

typedef void (*mcp_progress_callback_t)(const char* progress_token,
                                         double progress,
                                         double total,
                                         void* user_data);

typedef void (*mcp_log_callback_t)(mcp_log_level_t level,
                                    const char* logger,
                                    const char* message,
                                    void* user_data);

/* ========== Streaming Support (PROTO-001) ========== */

typedef enum {
    MCP_STREAM_EVENT_CONTENT = 0,
    MCP_STREAM_EVENT_ERROR,
    MCP_STREAM_EVENT_DONE,
    MCP_STREAM_EVENT_PROGRESS,
    MCP_STREAM_EVENT_CANCELLED
} mcp_stream_event_type_t;

typedef struct {
    mcp_stream_event_type_t type;
    char* event_data;
    size_t data_size;
    double progress;
    double total;
} mcp_stream_event_t;

typedef void (*mcp_stream_callback_t)(const mcp_stream_event_t* event,
                                      void* user_data);

typedef struct {
    bool enabled;
    int chunk_size;
    int max_buffer_size;
    int flush_interval_ms;
} mcp_stream_config_t;

mcp_v1_config_t mcp_v1_config_default(void);

mcp_v1_context_t* mcp_v1_context_create(const mcp_v1_config_t* config);
void mcp_v1_context_destroy(mcp_v1_context_t* ctx);

int mcp_v1_register_tool(mcp_v1_context_t* ctx,
                          const mcp_tool_t* tool,
                          mcp_tool_handler_t handler,
                          void* user_data);

int mcp_v1_register_resource(mcp_v1_context_t* ctx,
                              const mcp_resource_t* resource,
                              mcp_resource_handler_t handler,
                              void* user_data);

int mcp_v1_register_resource_template(mcp_v1_context_t* ctx,
                                       const mcp_resource_template_t* template);

int mcp_v1_register_prompt(mcp_v1_context_t* ctx,
                            const mcp_prompt_t* prompt,
                            mcp_prompt_handler_t handler,
                            void* user_data);

int mcp_v1_set_sampling_handler(mcp_v1_context_t* ctx,
                                 mcp_sampling_handler_t handler,
                                 void* user_data);

int mcp_v1_set_completion_handler(mcp_v1_context_t* ctx,
                                   mcp_completion_handler_t handler,
                                   void* user_data);

int mcp_v1_set_progress_callback(mcp_v1_context_t* ctx,
                                  mcp_progress_callback_t callback,
                                  void* user_data);

int mcp_v1_set_log_callback(mcp_v1_context_t* ctx,
                             mcp_log_callback_t callback,
                             void* user_data);

int mcp_v1_set_log_level(mcp_v1_context_t* ctx, mcp_log_level_t level);

int mcp_v1_handle_tools_list(mcp_v1_context_t* ctx,
                              char** response_json);

int mcp_v1_handle_tools_call(mcp_v1_context_t* ctx,
                              const char* name,
                              const char* arguments_json,
                              char** response_json);

int mcp_v1_handle_resources_list(mcp_v1_context_t* ctx,
                                  char** response_json);

int mcp_v1_handle_resources_read(mcp_v1_context_t* ctx,
                                  const char* uri,
                                  char** response_json);

int mcp_v1_handle_resources_templates(mcp_v1_context_t* ctx,
                                       char** response_json);

int mcp_v1_handle_prompts_list(mcp_v1_context_t* ctx,
                                char** response_json);

int mcp_v1_handle_prompts_get(mcp_v1_context_t* ctx,
                               const char* name,
                               const char* arguments_json,
                               char** response_json);

int mcp_v1_handle_sampling(mcp_v1_context_t* ctx,
                            const mcp_sampling_params_t* params,
                            mcp_sampling_result_t* result);

int mcp_v1_handle_completion(mcp_v1_context_t* ctx,
                              const mcp_completion_request_t* request,
                              mcp_completion_result_t* result);

int mcp_v1_send_progress(mcp_v1_context_t* ctx,
                          const char* progress_token,
                          double progress,
                          double total);

int mcp_v1_notify_cancelled(mcp_v1_context_t* ctx,
                             const char* request_id,
                             const char* reason);

/* Streaming API (PROTO-001) */
int mcp_v1_stream_config(mcp_v1_context_t* ctx, const mcp_stream_config_t* config);
int mcp_v1_handle_tools_call_streaming(mcp_v1_context_t* ctx,
                                        const char* name,
                                        const char* arguments_json,
                                        mcp_stream_callback_t callback,
                                        void* user_data);
int mcp_v1_handle_sampling_streaming(mcp_v1_context_t* ctx,
                                      const mcp_sampling_params_t* params,
                                      mcp_stream_callback_t callback,
                                      void* user_data);
const char* mcp_stream_event_type_string(mcp_stream_event_type_t type);
void mcp_stream_event_init(mcp_stream_event_t* event,
                            mcp_stream_event_type_t type,
                            const char* data);

int mcp_v1_route_request(mcp_v1_context_t* ctx,
                          const char* method,
                          const char* params_json,
                          char** response_json);

const protocol_adapter_t* mcp_v1_get_adapter(void);

size_t mcp_v1_get_tool_count(mcp_v1_context_t* ctx);
size_t mcp_v1_get_resource_count(mcp_v1_context_t* ctx);
size_t mcp_v1_get_prompt_count(mcp_v1_context_t* ctx);
uint32_t mcp_v1_get_capabilities(mcp_v1_context_t* ctx);

void mcp_content_destroy(mcp_content_t* content, size_t count);
void mcp_sampling_result_destroy(mcp_sampling_result_t* result);
void mcp_completion_result_destroy(mcp_completion_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_MCP_V1_ADAPTER_H */
