// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file a2a_v03_adapter.h
 * @brief A2A v0.3.0 Protocol Adapter for AgentOS
 *
 * Agent-to-Agent Protocol v0.3.0 深度支持适配器。
 * 实现智能体发现、任务委派、协商与协作的完整A2A协议能力。
 *
 * A2A v0.3.0 核心能力:
 * 1. Agent Card — 智能体能力描述与发现
 * 2. Task Lifecycle — 任务创建/更新/取消/完成
 * 3. Message Exchange — 智能体间结构化消息传递
 * 4. Negotiation — 任务协商与条件匹配
 * 5. Streaming — 流式任务执行与进度推送
 * 6. Push Notifications — 事件驱动的通知机制
 *
 * @since 2.0.0
 * @see unified_protocol.h
 */

#ifndef AGENTOS_A2A_V03_ADAPTER_H
#define AGENTOS_A2A_V03_ADAPTER_H

#include "unified_protocol.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define A2A_V03_VERSION             "0.3.0"
#define A2A_V03_PROTOCOL_NAME       "a2a"
#define A2A_V03_MAX_AGENTS          256
#define A2A_V03_MAX_TASKS           4096
#define A2A_V03_MAX_CAPABILITIES    64
#define A2A_V03_MAX_MESSAGE_SIZE    (16 * 1024 * 1024)
#define A2A_V03_DEFAULT_TIMEOUT_MS  60000

typedef enum {
    A2A_CAP_TASK_EXECUTION = 0x01,
    A2A_CAP_STREAMING = 0x02,
    A2A_CAP_PUSH_NOTIFICATIONS = 0x04,
    A2A_CAP_NEGOTIATION = 0x08,
    A2A_CAP_MULTI_TURN = 0x10,
    A2A_CAP_STATE_TRANSITION = 0x20
} a2a_capability_t;

typedef enum {
    A2A_TASK_SUBMITTED = 0,
    A2A_TASK_WORKING,
    A2A_TASK_INPUT_REQUIRED,
    A2A_TASK_COMPLETED,
    A2A_TASK_CANCELED,
    A2A_TASK_FAILED,
    A2A_TASK_REJECTED
} a2a_task_state_t;

typedef enum {
    A2A_MSG_TEXT = 0,
    A2A_MSG_FILE,
    A2A_MSG_STRUCTURED,
    A2A_MSG_ERROR
} a2a_message_type_t;

typedef enum {
    A2A_NEGOTIATE_PROPOSE = 0,
    A2A_NEGOTIATE_ACCEPT,
    A2A_NEGOTIATE_REJECT,
    A2A_NEGOTIATE_COUNTER
} a2a_negotiation_action_t;

typedef struct {
    char* name;
    char* description;
    char* schema_json;
} a2a_skill_t;

typedef struct {
    char* id;
    char* name;
    char* description;
    char* url;
    char* version;
    a2a_capability_t capabilities;
    a2a_skill_t* skills;
    size_t skill_count;
    char* provider_name;
    char* provider_url;
    char* documentation_url;
    char* authentication_schemes_json;
} a2a_agent_card_t;

typedef struct {
    char* id;
    char* session_id;
    char* agent_id;
    a2a_task_state_t state;
    char* description;
    char* input_json;
    char* output_json;
    uint64_t created_at;
    uint64_t updated_at;
    double progress;
    char* error_message;
    a2a_agent_card_t* assigned_agent;
} a2a_task_t;

typedef struct {
    char* role;
    a2a_message_type_t type;
    char* content_json;
    char* mime_type;
    char* file_name;
    uint8_t* file_data;
    size_t file_data_size;
} a2a_message_t;

typedef struct {
    a2a_negotiation_action_t action;
    char* task_id;
    char* agent_id;
    char* terms_json;
    char* counter_proposal_json;
    char* reason;
} a2a_negotiation_t;

typedef struct {
    char* event_type;
    char* task_id;
    char* agent_id;
    char* data_json;
    uint64_t timestamp;
} a2a_notification_t;

typedef struct {
    uint32_t capabilities;
    uint32_t default_timeout_ms;
    size_t max_agents;
    size_t max_tasks;
    size_t max_message_size;
    bool enable_negotiation;
    bool enable_streaming;
    bool enable_push_notifications;
    bool require_authentication;
    char* default_authentication;
} a2a_v03_config_t;

typedef struct a2a_v03_context_s a2a_v03_context_t;

typedef int (*a2a_task_handler_t)(a2a_v03_context_t* ctx,
                                   const a2a_task_t* task,
                                   a2a_task_state_t* new_state,
                                   char** output_json,
                                   void* user_data);

typedef int (*a2a_message_handler_t)(a2a_v03_context_t* ctx,
                                     const char* target_agent_id,
                                     const a2a_message_t* message,
                                     a2a_message_t** response,
                                     size_t* response_count,
                                     void* user_data);

typedef int (*a2a_negotiation_handler_t)(a2a_v03_context_t* ctx,
                                          const a2a_negotiation_t* negotiation,
                                          a2a_negotiation_action_t* response_action,
                                          char** response_terms,
                                          void* user_data);

typedef void (*a2a_notification_handler_t)(a2a_v03_context_t* ctx,
                                            const a2a_notification_t* notification,
                                            void* user_data);

typedef void (*a2a_streaming_handler_t)(a2a_v03_context_t* ctx,
                                         const char* task_id,
                                         double progress,
                                         const char* chunk_json,
                                         bool is_final,
                                         void* user_data);

a2a_v03_config_t a2a_v03_config_default(void);

a2a_v03_context_t* a2a_v03_context_create(const a2a_v03_config_t* config);
void a2a_v03_context_destroy(a2a_v03_context_t* ctx);

int a2a_v03_register_agent(a2a_v03_context_t* ctx,
                             const a2a_agent_card_t* card);

int a2a_v03_unregister_agent(a2a_v03_context_t* ctx,
                               const char* agent_id);

const a2a_agent_card_t* a2a_v03_get_agent_card(a2a_v03_context_t* ctx,
                                                 const char* agent_id);

int a2a_v03_discover_agents(a2a_v03_context_t* ctx,
                              const char* capability,
                              const char* skill_name,
                              a2a_agent_card_t*** results,
                              size_t* result_count);

int a2a_v03_create_task(a2a_v03_context_t* ctx,
                          const char* agent_id,
                          const char* description,
                          const char* input_json,
                          a2a_task_t** task);

int a2a_v03_update_task(a2a_v03_context_t* ctx,
                          const char* task_id,
                          a2a_task_state_t new_state,
                          const char* output_json,
                          double progress);

int a2a_v03_cancel_task(a2a_v03_context_t* ctx,
                          const char* task_id,
                          const char* reason);

int a2a_v03_get_task(a2a_v03_context_t* ctx,
                       const char* task_id,
                       a2a_task_t** task);

int a2a_v03_send_message(a2a_v03_context_t* ctx,
                           const char* target_agent_id,
                           const a2a_message_t* message,
                           a2a_message_t** response,
                           size_t* response_count);

int a2a_v03_negotiate(a2a_v03_context_t* ctx,
                        const a2a_negotiation_t* proposal,
                        a2a_negotiation_action_t* response_action,
                        char** response_terms);

int a2a_v03_subscribe_notifications(a2a_v03_context_t* ctx,
                                      a2a_notification_handler_t handler,
                                      void* user_data);

int a2a_v03_unsubscribe_notifications(a2a_v03_context_t* ctx);

int a2a_v03_send_notification(a2a_v03_context_t* ctx,
                                const a2a_notification_t* notification);

int a2a_v03_stream_task_update(a2a_v03_context_t* ctx,
                                 const char* task_id,
                                 double progress,
                                 const char* chunk_json,
                                 bool is_final);

int a2a_v03_set_task_handler(a2a_v03_context_t* ctx,
                               a2a_task_handler_t handler,
                               void* user_data);

int a2a_v03_set_message_handler(a2a_v03_context_t* ctx,
                                  a2a_message_handler_t handler,
                                  void* user_data);

int a2a_v03_set_negotiation_handler(a2a_v03_context_t* ctx,
                                      a2a_negotiation_handler_t handler,
                                      void* user_data);

int a2a_v03_set_streaming_handler(a2a_v03_context_t* ctx,
                                    a2a_streaming_handler_t handler,
                                    void* user_data);

int a2a_v03_route_request(a2a_v03_context_t* ctx,
                            const char* method,
                            const char* params_json,
                            char** response_json);

const protocol_adapter_t* a2a_v03_get_adapter(void);

size_t a2a_v03_get_agent_count(a2a_v03_context_t* ctx);
size_t a2a_v03_get_task_count(a2a_v03_context_t* ctx);
uint32_t a2a_v03_get_capabilities(a2a_v03_context_t* ctx);

void a2a_agent_card_destroy(a2a_agent_card_t* card);
void a2a_task_destroy(a2a_task_t* task);
void a2a_message_destroy(a2a_message_t* msg);
void a2a_negotiation_destroy(a2a_negotiation_t* neg);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_A2A_V03_ADAPTER_H */
