// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file autogen_adapter.h
 * @brief AutoGen Framework Integration Adapter for AgentOS
 *
 * AutoGen 框架适配器，实现AgentOS与Microsoft AutoGen多代理对话框架的集成。
 *
 * AutoGen核心概念映射:
 * - ConversableAgent → AgentOS Agent + Protocol Session
 * - GroupChat → AgentOS A2A multi-agent coordination
 * - UserProxyAgent → AgentOS human-in-the-loop interface
 * - CodeExecutor → AgentOS tool execution sandbox
 * - AssistantAgent → LLM-backed agent via protocol
 * - ChatCompletionClient → Protocol-based LLM client
 *
 * 支持的AutoGen特性:
 * 1. 多代理对话编排
 * 2. 角色定义 (UserProxy/Assistant/Coder/Planner)
 * 3. 群聊管理 (round-robin, speaker selection)
 * 4. 代码执行沙箱
 * 5. 人机协作 (human-in-the-loop)
 * 6. 对话历史持久化
 * 7. 工具调用与函数执行
 * 8. 流式对话输出
 *
 * @since 2.1.0
 */

#ifndef AGENTOS_AUTOGEN_ADAPTER_H
#define AGENTOS_AUTOGEN_ADAPTER_H

#include "unified_protocol.h"
#include "agentos_protocol_interface.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AUTOGEN_ADAPTER_VERSION     "1.0.0"
#define AUTOGEN_MAX_AGENTS          32
#define AUTOGEN_MAX_GROUP_CHATS      16
#define AUTOGEN_MAX_MESSAGES        512
#define AUTOGEN_MAX_TOOLS            64
#define AUTOGEN_DEFAULT_TIMEOUT_MS 120000

typedef enum {
    AGENT_ROLE_USER_PROXY = 0,
    AGENT_ROLE_ASSISTANT,
    AGENT_ROLE_CODER,
    AGENT_ROLE_PLANNER,
    AGENT_ROLE_RESEARCHER,
    AGENT_ROLE_REVIEWER,
    AGENT_ROLE_CUSTOM
} autogen_agent_role_t;

typedef enum {
    GROUP_CHAT_ROUND_ROBIN = 0,
    GROUP_CHAT_SPEAKER_SELECTION,
    GROUP_CHAT_RAG,
    GROUP_CHAT_SEQUENTIAL,
    GROUP_CHAT_PARALLEL,
    GROUP_CHAT_CUSTOM
} autogen_chat_mode_t;

typedef enum {
    MSG_TYPE_TEXT = 0,
    MSG_TYPE_TOOL_CALL,
    MSG_TYPE_TOOL_RESULT,
    MSG_TYPE_CODE_BLOCK,
    MSG_TYPE_TERMINATION,
    MSG_TYPE_HANDOFF,
    MSG_TYPE_SYSTEM
} autogen_message_type_t;

typedef enum {
    HUMAN_MODE_NEVER = 0,
    HUMAN_MODE_TERMINATE,
    HUMAN_MODE_ALWAYS,
    HUMAN_MODE_CODE_EXECUTION
} autogen_human_mode_t;

typedef struct {
    char* id;
    char* name;
    autogen_agent_role_t role;
    char* system_message;
    char* llm_config_json;
    bool is_termination;
    int max_consecutive_auto_reply;
    bool code_execution_enabled;
    human_mode_t human_input_mode;
    char** allowed_transitions;
    size_t transition_count;
} autogen_agent_def_t;

typedef struct {
    char* agent_id;
    char* name;
    autogen_agent_role_t role;
    bool is_active;
    uint64_t messages_sent;
    uint64_t messages_received;
    uint64_t tool_calls_made;
} autogen_agent_instance_t;

typedef struct {
    char* id;
    char* name;
    autogen_chat_mode_t mode;
    char** participant_ids;
    size_t participant_count;
    char* speaker_selection_prompt;
    int max_rounds;
    int current_round;
    bool allow_repeat_speaker;
    bool is_active;
} autogen_group_chat_def_t;

typedef struct {
    char* message_id;
    char* sender_id;
    char* receiver_id;
    autogen_message_type_t type;
    char* content;
    char** tool_calls;
    size_t tool_call_count;
    char* metadata_json;
    uint64_t timestamp;
    bool is_visible;
} autogen_message_t;

typedef struct {
    char* conversation_id;
    autogen_message_t* messages;
    size_t message_count;
    char* summary;
    uint64_t created_at;
    uint64_t last_activity;
    bool is_complete;
    char* termination_reason;
} autogen_conversation_t;

typedef struct {
    char* group_id;
    char* initiator_id;
    char* initial_message;
    autogen_conversation_t* conversation;
    double total_time_ms;
    int total_rounds;
    int total_messages;
    bool success;
    char* final_summary;
    char* error_message;
} autogen_group_chat_result_t;

typedef struct {
    char* base_url;
    char* api_key;
    uint32_t timeout_ms;
    bool enable_code_execution;
    bool enable_human_loop;
    bool enable_streaming;
    int max_agents_per_group;
    int max_history_per_conv;
    int max_code_execution_sec;
    char* default_llm_model;
    char* work_dir;
    char* cache_dir;
} autogen_config_t;

typedef struct autogen_adapter_context_s autogen_adapter_context_t;

typedef int (*autogen_tool_executor_fn)(const char* agent_id,
                                        const char* tool_name,
                                        const char* input_json,
                                        char** output_json,
                                        void* user_data);

typedef int (*autogen_code_executor_fn)(const char* code,
                                       const char* language,
                                       char** output,
                                       void* user_data);

typedef int (*autogen_human_callback_fn)(const char* prompt,
                                         char** response,
                                         void* user_data);

typedef void (*autogen_message_hook_fn)(const autogen_message_t* msg,
                                       void* user_data);

autogen_config_t autogen_config_default(void);

autogen_adapter_context_t* autogen_adapter_create(const autogen_config_t* config);
void autogen_adapter_destroy(autogen_adapter_context_t* ctx);

bool autogen_adapter_is_initialized(const autogen_adapter_context_t* ctx);
const char* autogen_adapter_version(void);

int autogen_create_agent(autogen_adapter_context_t* ctx,
                         const autogen_agent_def_t* definition,
                         char* out_agent_id);

int autogen_destroy_agent(autogen_adapter_context_t* ctx,
                          const char* agent_id);

int autogen_list_agents(autogen_adapter_context_t* ctx,
                        autogen_agent_instance_t** agents,
                        size_t* count);

int autogen_create_group_chat(autogen_adapter_context_t* ctx,
                              const autogen_group_chat_def_t* definition,
                              char* out_group_id);

int autogen_initiate_chat(autogen_adapter_context_t* ctx,
                          const char* group_id,
                          const char* sender_id,
                          const char* message,
                          autogen_group_chat_result_t* result);

int autogen_send_message(autogen_adapter_context_t* ctx,
                        const char* from_agent_id,
                        const char* to_agent_id,
                        const char* content,
                        autogen_message_type_t type,
                        autogen_message_t* reply);

int autogen_register_tool(autogen_adapter_context_t* ctx,
                          const char* name,
                          const char* description,
                          const char* schema_json,
                          autogen_tool_executor_fn executor,
                          void* user_data);

int autogen_get_conversation(autogen_adapter_context_t* ctx,
                             const char* group_id,
                             autogen_conversation_t* conv);

int autogen_set_code_executor(autogen_adapter_context_t* ctx,
                              autogen_code_executor_fn executor,
                              void* user_data);

int autogen_set_human_callback(autogen_adapter_context_t* ctx,
                               autogen_human_callback_fn callback,
                               void* user_data);

int autogen_set_message_hook(autogen_adapter_context_t* ctx,
                             autogen_message_hook_fn hook,
                             void* user_data);

int autogen_get_statistics(autogen_adapter_context_t* ctx,
                          char* stats_json,
                          size_t buffer_size);

const proto_adapter_t* autogen_get_protocol_adapter(void);

void autogen_agent_def_destroy(autogen_agent_def_t* def);
void autogen_agent_instance_destroy(autogen_agent_instance_t* inst);
void autogen_group_chat_def_destroy(autogen_group_chat_def_t* gc);
void autogen_message_destroy(autogen_message_t* msg);
void autogen_conversation_destroy(autogen_conversation_t* conv);
void autogen_group_chat_result_destroy(autogen_group_chat_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_AUTOGEN_ADAPTER_H */
