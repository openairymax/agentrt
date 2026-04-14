// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file openclaw_adapter.h
 * @brief OpenClaw Platform Integration Adapter for AgentOS
 *
 * OpenClaw (九问) 是开源AI Agent平台，专注于政务和企业应用。
 * 本适配器实现AgentOS与OpenClaw平台的完整集成。
 *
 * OpenClaw核心特性（v2026.4.11+）:
 * 1. 离线私有化部署 — 完全本地运行，数据不出域
 * 2. 安全管控 — 多级权限控制、审计追踪
 * 3. 多模态能力 — 文本/图像/音频/视频统一处理
 * 4. 生态兼容 — 支持MCP/A2A/OpenAI等主流协议
 * 5. 多智能体原生 — 内置多Agent编排引擎
 *
 * 集成模式:
 * - 双向桥接: AgentOS ↔ OpenClaw 消息互通
 * - 工具共享: AgentOS工具注册到OpenClaw工具链
 * - 能力映射: OpenClaw能力 → AgentOS协议转换
 *
 * @since 2.1.0
 * @see unified_protocol.h
 * @see agentos_protocol_interface.h
 */

#ifndef AGENTOS_OPENCLAW_ADAPTER_H
#define AGENTOS_OPENCLAW_ADAPTER_H

#include "unified_protocol.h"
#include "agentos_protocol_interface.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OPENCLAW_ADAPTER_VERSION    "1.0.0"
#define OPENCLAW_PLATFORM_VERSION   "2026.4.11"
#define OPENCLAW_MAGIC_PREFIX      "OCLW"
#define OPENCLAW_MAX_AGENTS         64
#define OPENCLAW_MAX_TOOLS          256
#define OPENCLAW_MAX_SESSIONS        128
#define OPENCLAW_MAX_CONTEXT_KB     1024
#define OPENCLAW_HEARTBEAT_INTERVAL_SEC 30
#define OPENCLAW_DEFAULT_TIMEOUT_MS 30000

typedef enum {
    OPENCLAW_MODE_STANDALONE = 0,
    OPENCLAW_MODE_CLUSTERED,
    OPENCLAW_MODE_HYBRID,
    OPENCLAW_MODE_EMBEDDED
} openclaw_mode_t;

typedef enum {
    OPENCLAW_SECURITY_LEVEL_PUBLIC = 0,
    OPENCLAW_SECURITY_LEVEL_INTERNAL,
    OPENCLAW_SECURITY_LEVEL_CONFIDENTIAL,
    OPENCLAW_SECURITY_LEVEL_SECRET,
    OPENCLAW_SECURITY_LEVEL_TOP_SECRET
} openclaw_security_level_t;

typedef enum {
    OPENCLAW_MODALITY_TEXT = 0x01,
    OPENCLAW_MODALITY_IMAGE = 0x02,
    OPENCLAW_MODALITY_AUDIO = 0x04,
    OPENCLAW_MODALITY_VIDEO = 0x08,
    OPENCLAW_MODALITY_FILE = 0x10,
    OPENCLAW_MODALITY_CODE = 0x20,
    OPENCLAW_MODALITY_ALL = 0x3F
} openclaw_modality_t;

typedef enum {
    OPENCLAW_AGENT_STATE_IDLE = 0,
    OPENCLAW_AGENT_STATE_THINKING,
    OPENCLAW_AGENT_STATE_EXECUTING,
    OPENCLAW_AGENT_STATE_WAITING,
    OPENCLAW_AGENT_STATE_ERROR,
    OPENCLAW_AGENT_STATE_TERMINATED
} openclaw_agent_state_t;

typedef struct {
    char* agent_id;
    char* name;
    char* description;
    char* version;
    openclaw_modality_t supported_modalities;
    openclaw_security_level_t security_level;
    int max_concurrent_tasks;
    bool is_active;
    uint64_t created_at;
    uint64_t last_heartbeat;
} openclaw_agent_card_t;

typedef struct {
    char* tool_id;
    char* name;
    char* description;
    char* input_schema_json;
    char* output_schema_json;
    bool requires_auth;
    openclaw_security_level_t min_security_level;
    bool is_registered;
} openclaw_tool_info_t;

typedef struct {
    char* session_id;
    char* agent_id;
    char* parent_session_id;
    openclaw_modality_t modality;
    openclaw_security_level_t security_level;
    uint64_t created_at;
    uint64_t last_activity;
    size_t context_size_bytes;
    bool is_active;
} openclaw_session_t;

typedef struct {
    char* message_id;
    char* session_id;
    char* sender_id;
    char* receiver_id;
    openclaw_modality_t modality;
    char* content_type;
    void* payload;
    size_t payload_size;
    uint64_t timestamp;
    uint32_t priority;
    bool requires_ack;
} openclaw_message_t;

typedef struct {
    char* task_id;
    char* session_id;
    char* description;
    char* input_data_json;
    int priority;
    int max_retries;
    uint32_t timeout_ms;
    char* assigned_agent_id;
    openclaw_agent_state_t state;
    double progress;
    char* result_json;
    char* error_message;
    uint64_t created_at;
    uint64_t completed_at;
} openclaw_task_t;

typedef struct {
    char* node_id;
    char* cluster_name;
    int total_nodes;
    int active_nodes;
    uint64_t total_agents;
    uint64_t active_sessions;
    uint64_t messages_processed;
    uint64_t tasks_completed;
    uint64_t uptime_seconds;
    double cpu_usage_pct;
    double memory_usage_mb;
    double disk_usage_pct;
} openclaw_cluster_status_t;

typedef struct {
    char* endpoint_url;
    char* api_key;
    char* organization_id;
    char* cluster_id;
    openclaw_mode_t mode;
    openclaw_security_level_t default_security_level;
    int heartbeat_interval_sec;
    uint32_t request_timeout_ms;
    int max_sessions;
    size_t max_context_kb;
    bool enable_multimodal;
    bool enable_tool_sharing;
    bool enable_audit_log;
    bool enable_metrics;
    char* custom_headers_json;
    int reconnect_max_attempts;
    uint32_t reconnect_delay_ms;
} openclaw_config_t;

typedef struct openclaw_adapter_context_s openclaw_adapter_context_t;

typedef int (*openclaw_message_handler_t)(const openclaw_message_t* msg,
                                           openclaw_message_t* response,
                                           void* user_data);

typedef int (*openclaw_task_handler_t)(const openclaw_task_t* task,
                                        openclaw_task_t* result,
                                        void* user_data);

typedef void (*openclaw_event_callback_t)(const char* event_type,
                                          const char* event_data_json,
                                          void* user_data);

typedef void (*openclaw_status_callback_t)(const openclaw_cluster_status_t* status,
                                            void* user_data);

openclaw_config_t openclaw_config_default(void);

openclaw_adapter_context_t* openclaw_adapter_create(const openclaw_config_t* config);
void openclaw_adapter_destroy(openclaw_adapter_context_t* ctx);

bool openclaw_adapter_is_initialized(const openclaw_adapter_context_t* ctx);
const char* openclaw_adapter_version(void);
const char* openclaw_adapter_platform_version(void);

int openclaw_connect(openclaw_adapter_context_t* ctx);
int openclaw_disconnect(openclaw_adapter_context_t* ctx);
bool openclaw_is_connected(const openclaw_adapter_context_t* ctx);

int openclaw_register_agent(openclaw_adapter_context_t* ctx,
                             const openclaw_agent_card_t* card);

int openclaw_discover_agents(openclaw_adapter_context_t* ctx,
                              const char* capability_filter,
                              openclaw_security_level_t min_level,
                              openclaw_agent_card_t** agents,
                              size_t* count);

int openclaw_unregister_agent(openclaw_adapter_context_t* ctx,
                               const char* agent_id);

int openclaw_register_tool(openclaw_adapter_context_t* ctx,
                            const openclaw_tool_info_t* tool);

int openclaw_list_tools(openclaw_adapter_context_t* ctx,
                        const char* agent_id,
                        openclaw_tool_info_t** tools,
                        size_t* count);

int openclaw_create_session(openclaw_adapter_context_t* ctx,
                             const openclaw_session_t* session_template,
                             openclaw_session_t* out_session);

int openclaw_close_session(openclaw_adapter_context_t* ctx,
                           const char* session_id);

int openclaw_send_message(openclaw_adapter_context_t* ctx,
                          const openclaw_message_t* msg,
                          openclaw_message_t* response);

int openclaw_delegate_task(openclaw_adapter_context_t* ctx,
                            const openclaw_task_t* task,
                            const char* target_agent_id,
                            openclaw_task_t* result);

int openclaw_query_task(openclaw_adapter_context_t* ctx,
                        const char* task_id,
                        openclaw_task_t* result);

int openclaw_cancel_task(openclaw_adapter_context_t* ctx,
                         const char* task_id);

int openclaw_get_cluster_status(openclaw_adapter_context_t* ctx,
                                openclaw_cluster_status_t* status);

int openclaw_set_message_handler(openclaw_adapter_context_t* ctx,
                                 openclaw_message_handler_t handler,
                                 void* user_data);

int openclaw_set_task_handler(openclaw_adapter_context_t* ctx,
                               openclaw_task_handler_t handler,
                               void* user_data);

int openclaw_set_event_callback(openclaw_adapter_context_t* ctx,
                                 openclaw_event_callback_t callback,
                                 void* user_data);

int openclaw_set_status_callback(openclaw_adapter_context_t* ctx,
                                  openclaw_status_callback_t callback,
                                  void* user_data);

int openclaw_send_heartbeat(openclaw_adapter_context_t* ctx);

int openclaw_get_statistics(openclaw_adapter_context_t* ctx,
                            char* stats_json,
                            size_t buffer_size);

const proto_adapter_t* openclaw_get_protocol_adapter(void);

void openclaw_agent_card_destroy(openclaw_agent_card_t* card);
void openclaw_tool_info_destroy(openclaw_tool_info_t* tool);
void openclaw_session_destroy(openclaw_session_t* session);
void openclaw_message_destroy(openclaw_message_t* msg);
void openclaw_task_destroy(openclaw_task_t* task);
void openclaw_cluster_status_destroy(openclaw_cluster_status_t* status);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_OPENCLAW_ADAPTER_H */
