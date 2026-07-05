/*
 * AgentRT Unified Protocol - 统一协议接口
 *
 * 本文件定义AgentRT统一协议系统的核心接口，提供对多种
 * 通信协议（JSON-RPC、MCP、A2A、OpenAI、OpenJiuwen）的
 * 统一抽象层。
 *
 * 原位置: agentos/include/agentrt/unified_protocol.h
 * 迁移至: agentrt/protocols/include/ (2026-04-19 include/整合重构)
 */

// @owner: team-B
#ifndef AGENTRT_UNIFIED_PROTOCOL_H
#define AGENTRT_UNIFIED_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 支持的协议类型
 */
typedef enum {
    AGENTRT_PROTOCOL_JSON_RPC = 0,
    AGENTRT_PROTOCOL_MCP,
    AGENTRT_PROTOCOL_A2A,
    AGENTRT_PROTOCOL_OPENAI,
    AGENTRT_PROTOCOL_OPENJIUWEN,
    AGENTRT_PROTOCOL_CLAUDE,
    AGENTRT_PROTOCOL_CHINA_ECO,
    AGENTRT_PROTOCOL_AGNTCY,
    AGENTRT_PROTOCOL_OPENCLAW,
    AGENTRT_PROTOCOL_COUNT
} agentrt_protocol_type_t;

/**
 * @brief 协议适配器结构体（所有适配器共享的接口定义）
 */
typedef struct protocol_adapter_s {
    agentrt_protocol_type_t type;
    const char *name;
    const char *version;
    const char *description;
    int (*init)(void *context);
    int (*destroy)(void *context);
    int (*encode)(void *context, const void *msg, void **out_data, size_t *out_size);
    int (*decode)(void *context, const void *data, size_t size, void *out_msg);
    int (*connect)(void *context, const char *endpoint);
    int (*disconnect)(void *context);
    int (*is_connected)(void *context);
    int (*send)(void *context, const void *data, size_t size);
    int (*receive)(void *context, void **data, size_t *size, uint32_t timeout_ms);
    int (*handle_request)(void *context, const void *req, void **resp);
    int (*get_version)(void *context, char *version_buf, size_t max_size);
    uint32_t (*capabilities)(void *context);
    int (*get_stats)(void *context, char *stats_json, size_t max_size);
    void *context;
    void *user_data;
} protocol_adapter_t;

typedef protocol_adapter_t proto_adapter_t;

/**
 * @brief 消息结构
 */
typedef struct {
    const void *data;
    size_t len;
    agentrt_protocol_type_t source_protocol;
} agentrt_message_t;

typedef agentrt_protocol_type_t protocol_type_t;
typedef agentrt_protocol_type_t proto_type_t;

typedef enum {
    DIRECTION_REQUEST = 0,
    DIRECTION_RESPONSE,
    DIRECTION_NOTIFICATION,
    DIRECTION_ERROR
} message_direction_t;

#define MSG_TYPE_REQUEST DIRECTION_REQUEST
#define MSG_TYPE_RESPONSE DIRECTION_RESPONSE
#define MSG_TYPE_ERROR DIRECTION_ERROR

#define PROTOCOL_CUSTOM AGENTRT_PROTOCOL_COUNT
#define PROTOCOL_HTTP AGENTRT_PROTOCOL_JSON_RPC

#define ENCODING_UTF8_JSON 0

#define PROTO_JSONRPC AGENTRT_PROTOCOL_JSON_RPC
#define PROTO_MCP AGENTRT_PROTOCOL_MCP
#define PROTO_A2A AGENTRT_PROTOCOL_A2A
#define PROTO_OPENAI AGENTRT_PROTOCOL_OPENAI
#define PROTO_OPENJIUWEN AGENTRT_PROTOCOL_OPENJIUWEN
#define PROTO_OPENCLAW (AGENTRT_PROTOCOL_COUNT + 1)
#define PROTO_CLAUDE (AGENTRT_PROTOCOL_COUNT + 2)
#define PROTO_AGNTCY (AGENTRT_PROTOCOL_COUNT + 3)
#define PROTO_CHINA_ECO (AGENTRT_PROTOCOL_COUNT + 4)
#define PROTOCOL_WEBSOCKET (AGENTRT_PROTOCOL_COUNT + 10)
#define PROTOCOL_GRPC (AGENTRT_PROTOCOL_COUNT + 11)
#define PROTOCOL_MQTT (AGENTRT_PROTOCOL_COUNT + 12)
#define PROTOCOL_AMQP (AGENTRT_PROTOCOL_COUNT + 13)
#define PROTOCOL_RAW_TCP (AGENTRT_PROTOCOL_COUNT + 14)
#define PROTOCOL_RAW_UDP (AGENTRT_PROTOCOL_COUNT + 15)
#define PROTOCOL_STDIO (AGENTRT_PROTOCOL_COUNT + 16)
#define PROTOCOL_IPC (AGENTRT_PROTOCOL_COUNT + 17)
#define ENCODING_BINARY 1

typedef struct {
    char *data;
    size_t size;
    int encoding;
} payload_wrapper_t;

typedef struct {
    agentrt_protocol_type_t protocol;
    agentrt_protocol_type_t protocol_type; /* alias for protocol */
    char protocol_name[64];
    char endpoint[256];
    char method[64];
    message_direction_t direction;
    uint64_t message_id;
    void *payload;
    size_t payload_size;
    uint64_t timestamp;
    bool is_error;
    int error_code;
    int status;
    char error_msg[256];
    void *body;
    size_t body_length;    /* alias for payload_size */
    size_t payload_length; /* alias for payload_size */
    char correlation_id[64];
    char sender_id[64];
    char source_agent[128];
    char target_agent[128];
    struct {
        char trace_id[64];
        char session_id[64];
    } metadata;
} unified_message_t;

/**
 * @brief 创建指定类型的协议适配器
 */
int protocol_adapter_create(agentrt_protocol_type_t type, protocol_adapter_t *adapter);

/**
 * @brief 销毁协议适配器
 */
void protocol_adapter_destroy(protocol_adapter_t adapter);

/**
 * @brief 通过适配器发送消息
 */
int protocol_adapter_send(protocol_adapter_t adapter, const agentrt_message_t *msg);

/**
 * @brief 通过适配器接收消息
 */
int protocol_adapter_recv(protocol_adapter_t adapter, agentrt_message_t *msg, size_t max_len);

typedef struct protocol_stack_s protocol_stack_s;
typedef struct protocol_stack_s *protocol_stack_handle_t;

typedef struct {
    char name[128];
    uint32_t max_adapters;
    bool enable_logging;
    agentrt_protocol_type_t default_protocol;
    uint32_t max_message_size;
    uint32_t timeout_ms;
    bool enable_compression;
    bool enable_encryption;
    void *custom_config;
} protocol_stack_config_t;

protocol_stack_handle_t protocol_stack_create(const protocol_stack_config_t *config);
void protocol_stack_destroy(protocol_stack_handle_t handle);
int protocol_stack_register_adapter(protocol_stack_handle_t handle, protocol_adapter_t adapter);
int protocol_stack_send(protocol_stack_handle_t handle, const unified_message_t *message);
int protocol_stack_receive(protocol_stack_handle_t handle, unified_message_t *message,
                           uint32_t timeout_ms);
int protocol_stack_set_callback(protocol_stack_handle_t handle,
                                void (*callback)(const unified_message_t *message, void *user_data),
                                void *user_data);
int protocol_stack_get_stats(protocol_stack_handle_t handle, void *stats);

unified_message_t unified_message_create(protocol_type_t protocol, message_direction_t direction,
                                         const char *endpoint, const void *payload,
                                         size_t payload_size);
void unified_message_destroy(unified_message_t *message);
const char *protocol_type_to_string(protocol_type_t type);
protocol_type_t protocol_type_from_string(const char *str);

int protocol_auto_transform(const unified_message_t *source, unified_message_t *target,
                            const char *target_protocol_name);

const char *protocol_type_name(agentrt_protocol_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* AGENTRT_UNIFIED_PROTOCOL_H */
