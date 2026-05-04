// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: Apache-2.0
/**
 * @file agntcy_acp_adapter.h
 * @brief AGNTCY Agent Communication Protocol Adapter for AgentOS
 *
 * AGNTCY ACP 是面向智能体间通信的开放标准协议，定义：
 * 1. agent/discover — 智能体注册与发现（capability card）
 * 2. channel/open — 智能体间安全通道建立（mutual TLS + token）
 * 3. message/exchange — 结构化消息交换（同步/异步/广播）
 * 4. task/orchestrate — 跨智能体任务编排（工作流定义）
 * 5. ack/agreement — 服务等级确认与资源承诺
 *
 * @since 3.0.0
 * @see unified_protocol.h
 */

#ifndef AGENTOS_AGNTCY_ACP_ADAPTER_H
#define AGENTOS_AGNTCY_ACP_ADAPTER_H

#include "unified_protocol.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AGNTCY_ACP_VERSION              "0.1.0"
#define AGNTCY_ACP_PROTOCOL_NAME        "agntcy"
#define AGNTCY_ACP_MAX_AGENTS           512
#define AGNTCY_ACP_MAX_CAPABILITIES     128
#define AGNTCY_ACP_MAX_TASKS            2048
#define AGNTCY_ACP_MAX_CHANNELS         256
#define AGNTCY_ACP_MAX_MESSAGE_SIZE     (8 * 1024 * 1024)
#define AGNTCY_ACP_DEFAULT_TIMEOUT_MS   30000
#define AGNTCY_ACP_TOKEN_SIZE           64
#define AGNTCY_ACP_CHANNEL_ID_SIZE      48

typedef enum {
    AGNTCY_CAP_DISCOVERY    = 0x01,
    AGNTCY_CAP_CHANNEL      = 0x02,
    AGNTCY_CAP_MESSAGING    = 0x04,
    AGNTCY_CAP_ORCHESTRATE  = 0x08,
    AGNTCY_CAP_BROADCAST    = 0x10,
    AGNTCY_CAP_ACK          = 0x20
} agntcy_capability_t;

typedef enum {
    AGNTCY_MSG_SYNC   = 0,
    AGNTCY_MSG_ASYNC  = 1,
    AGNTCY_MSG_BROADCAST = 2,
    AGNTCY_MSG_STREAM = 3
} agntcy_message_mode_t;

typedef enum {
    AGNTCY_TASK_PENDING    = 0,
    AGNTCY_TASK_DISPATCHED = 1,
    AGNTCY_TASK_RUNNING    = 2,
    AGNTCY_TASK_COMPLETED  = 3,
    AGNTCY_TASK_FAILED     = 4,
    AGNTCY_TASK_CANCELLED  = 5
} agntcy_task_state_t;

typedef struct {
    char agent_id[64];
    char name[128];
    uint32_t capabilities_mask;
    char endpoint_url[512];
    char public_key_pem[2048];
    int protocol_version;
    uint64_t registered_at;
    bool online;
} agntcy_agent_card_t;

typedef struct {
    char channel_id[AGNTCY_ACP_CHANNEL_ID_SIZE];
    char initiator_id[64];
    char responder_id[64];
    char session_token[AGNTCY_ACP_TOKEN_SIZE];
    uint64_t established_at;
    uint64_t expires_at;
    bool encrypted;
} agntcy_channel_t;

typedef struct {
    char message_id[64];
    agntcy_message_mode_t mode;
    char sender_id[64];
    char receiver_id[64];
    char channel_id[AGNTCY_ACP_CHANNEL_ID_SIZE];
    char* payload;
    size_t payload_size;
    char content_type[64];
    int priority;
    uint64_t timestamp;
} agntcy_message_t;

typedef struct {
    char task_id[64];
    char name[128];
    char* workflow_json;
    char assigned_agent_ids[AGNTCY_ACP_MAX_AGENTS][64];
    size_t assigned_count;
    agntcy_task_state_t state;
    int priority;
    uint64_t created_at;
    uint64_t deadline_at;
} agntcy_task_t;

typedef struct {
    char resource_type[64];
    size_t requested_amount;
    size_t guaranteed_amount;
    int cpu_cores;
    size_t memory_kb;
    uint64_t valid_until;
    bool committed;
} agntcy_ack_t;

typedef struct {
    agntcy_agent_card_t agents[AGNTCY_ACP_MAX_AGENTS];
    size_t agent_count;
    agntcy_channel_t channels[AGNTCY_ACP_MAX_CHANNELS];
    size_t channel_count;
    agntcy_task_t* tasks[AGNTCY_ACP_MAX_TASKS];
    size_t task_count;
    uint64_t message_counter;
    uint64_t task_counter;
    bool initialized;
} agntcy_handle_t;

int agntcy_acp_create(agntcy_handle_t** handle);
void agntcy_acp_destroy(agntcy_handle_t* handle);

int agntcy_agent_register(agntcy_handle_t* h, const agntcy_agent_card_t* card);
int agntcy_agent_unregister(agntcy_handle_t* h, const char* agent_id);
int agntcy_agent_discover(agntcy_handle_t* h, uint32_t cap_mask,
                          agntcy_agent_card_t* results, size_t* count);

int agntcy_channel_open(agntcy_handle_t* h, const char* initiator_id,
                         const char* responder_id, agntcy_channel_t* channel);
int agntcy_channel_close(agntcy_handle_t* h, const char* channel_id);

int agntcy_message_send(agntcy_handle_t* h, const agntcy_message_t* msg,
                         char* response, size_t* resp_size);
int agntcy_message_broadcast(agntcy_handle_t* h, const agntcy_message_t* msg);

int agntcy_task_orchestrate(agntcy_handle_t* h, const char* task_id,
                             const char* workflow_json);
int agntcy_task_get_state(agntcy_handle_t* h, const char* task_id,
                          agntcy_task_state_t* state);

int agntcy_ack_negotiate(agntcy_handle_t* h, const char* agent_id,
                          const agntcy_ack_t* ack_request,
                          agntcy_ack_t* ack_response);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_AGNTCY_ACP_ADAPTER_H */
