// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: Apache-2.0
/**
 * @file agntcy_acp_adapter.h
 * @brief AGNTCY Agent Connect Protocol (ACP) Adapter for AgentOS
 *
 * AGNTCY 是 Linux Foundation 下的开源项目，致力于构建"Agent 互联网"。
 * 本适配器实现 AGNTCY ACP 协议，支持：
 * - OASF Agent 描述与注册
 * - Agent Directory 发现服务
 * - ACP 远程调用与配置
 * - SLIM 安全消息传输
 * - 去中心化身份验证
 *
 * ACP 核心能力:
 * 1. Agent 注册与发现 — OASF schema 描述 + Directory 查询
 * 2. 远程 Agent 调用 — REST API invoke/run
 * 3. 流式输出 — SSE streaming
 * 4. 多轮会话 — 会话管理与上下文保持
 * 5. 安全认证 — OAuth2 / mTLS / API Key
 *
 * @since 2.1.0
 * @see unified_protocol.h
 * @see agentos_protocol_interface.h
 * @see https://docs.agntcy.org/
 * @see https://github.com/agntcy/acp-spec
 */

#ifndef AGENTOS_AGNTCY_ACP_ADAPTER_H
#define AGENTOS_AGNTCY_ACP_ADAPTER_H

#include "unified_protocol.h"
#include "agentos_protocol_interface.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AGNTCY_ACP_VERSION          "0.1.0"
#define AGNTCY_ADAPTER_VERSION      "1.0.0"
#define AGNTCY_MAX_AGENTS           128
#define AGNTCY_MAX_CAPABILITIES     32
#define AGNTCY_MAX_INPUTS           16
#define AGNTCY_MAX_OUTPUTS          16
#define AGNTCY_MAX_SESSIONS         64
#define AGNTCY_MAX_MESSAGE_LEN      16384
#define AGNTCY_DEFAULT_TIMEOUT_MS   30000
#define AGNTCY_HEARTBEAT_INTERVAL   30

typedef enum {
    AGNTCY_AUTH_NONE = 0,
    AGNTCY_AUTH_API_KEY,
    AGNTCY_AUTH_OAUTH2,
    AGNTCY_AUTH_MTLS
} agntcy_auth_type_t;

typedef enum {
    AGNTCY_AGENT_STATE_UNKNOWN = 0,
    AGNTCY_AGENT_STATE_AVAILABLE,
    AGNTCY_AGENT_STATE_BUSY,
    AGNTCY_AGENT_STATE_OFFLINE,
    AGNTCY_AGENT_STATE_ERROR
} agntcy_agent_state_t;

typedef enum {
    AGNTCY_RUN_PENDING = 0,
    AGNTCY_RUN_RUNNING,
    AGNTCY_RUN_COMPLETED,
    AGNTCY_RUN_FAILED,
    AGNTCY_RUN_CANCELLED,
    AGNTCY_RUN_REQUIRES_INPUT
} agntcy_run_status_t;

typedef struct {
    char* name;
    char* description;
    char* type;
    bool required;
} agntcy_io_spec_t;

typedef struct {
    char* name;
    char* description;
    char* version;
    char* framework;
    char** capabilities;
    size_t capability_count;
    agntcy_io_spec_t* inputs;
    size_t input_count;
    agntcy_io_spec_t* outputs;
    size_t output_count;
    char* endpoint_url;
    agntcy_agent_state_t state;
    char* owner_org;
    char* license;
} agntcy_agent_descriptor_t;

typedef struct {
    char* directory_url;
    char* local_agent_endpoint;
    agntcy_auth_type_t auth_type;
    char* api_key;
    char* oauth2_token_endpoint;
    char* oauth2_client_id;
    char* oauth2_client_secret;
    char* mtls_cert_path;
    char* mtls_key_path;
    uint32_t timeout_ms;
    int max_retries;
    bool enable_discovery;
    bool enable_heartbeat;
    uint32_t heartbeat_interval_sec;
} agntcy_acp_config_t;

typedef struct {
    char* run_id;
    char* agent_id;
    agntcy_run_status_t status;
    char* input_json;
    char* output_json;
    char* error_message;
    uint64_t created_at;
    uint64_t completed_at;
} agntcy_run_t;

typedef struct {
    char* session_id;
    char* agent_id;
    char** message_history;
    size_t message_count;
    uint64_t created_at;
    uint64_t last_active;
} agntcy_session_t;

typedef struct {
    char* event_type;
    char* data_json;
    bool is_final;
} agntcy_stream_event_t;

typedef void (*agntcy_stream_handler_t)(const agntcy_stream_event_t* event,
                                         void* user_data);

typedef struct agntcy_acp_context_s agntcy_acp_context_t;

agntcy_acp_config_t agntcy_acp_config_default(void);

agntcy_acp_context_t* agntcy_acp_context_create(const agntcy_acp_config_t* config);
void agntcy_acp_context_destroy(agntcy_acp_context_t* ctx);

bool agntcy_acp_is_initialized(const agntcy_acp_context_t* ctx);
const char* agntcy_acp_adapter_version(void);

int agntcy_register_agent(agntcy_acp_context_t* ctx,
                             const agntcy_agent_descriptor_t* descriptor);

int agntcy_unregister_agent(agntcy_acp_context_t* ctx,
                              const char* agent_id);

int agntcy_discover_agents(agntcy_acp_context_t* ctx,
                              const char* capability_filter,
                              agntcy_agent_descriptor_t** out_agents,
                              size_t* out_count);

int agntcy_get_agent(agntcy_acp_context_t* ctx,
                       const char* agent_id,
                       agntcy_agent_descriptor_t* out_descriptor);

int agntcy_invoke_agent(agntcy_acp_context_t* ctx,
                          const char* agent_id,
                          const char* input_json,
                          agntcy_run_t* out_run);

int agntcy_invoke_agent_streaming(agntcy_acp_context_t* ctx,
                                    const char* agent_id,
                                    const char* input_json,
                                    agntcy_stream_handler_t handler,
                                    void* user_data,
                                    agntcy_run_t* out_run);

int agntcy_get_run_status(agntcy_acp_context_t* ctx,
                            const char* run_id,
                            agntcy_run_t* out_run);

int agntcy_cancel_run(agntcy_acp_context_t* ctx,
                        const char* run_id);

int agntcy_create_session(agntcy_acp_context_t* ctx,
                            const char* agent_id,
                            agntcy_session_t* out_session);

int agntcy_session_send(agntcy_acp_context_t* ctx,
                          const char* session_id,
                          const char* message_json,
                          agntcy_run_t* out_run);

int agntcy_session_close(agntcy_acp_context_t* ctx,
                           const char* session_id);

int agntcy_list_local_agents(agntcy_acp_context_t* ctx,
                                agntcy_agent_descriptor_t** out_agents,
                                size_t* out_count);

const proto_adapter_t* agntcy_acp_get_adapter(void);

void agntcy_agent_descriptor_destroy(agntcy_agent_descriptor_t* desc);
void agntcy_run_destroy(agntcy_run_t* run);
void agntcy_session_destroy(agntcy_session_t* session);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_AGNTCY_ACP_ADAPTER_H */
