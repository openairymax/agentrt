/*
 * AgentOS Unified Protocol - 统一协议接口
 * 
 * 本文件定义AgentOS统一协议系统的核心接口，提供对多种
 * 通信协议（JSON-RPC、MCP、A2A、OpenAI、OpenJiuwen）的
 * 统一抽象层。
 *
 * 原位置: agentos/include/agentos/unified_protocol.h
 * 迁移至: agentos/protocols/include/ (2026-04-19 include/整合重构)
 */

#ifndef AGENTOS_UNIFIED_PROTOCOL_H
#define AGENTOS_UNIFIED_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 支持的协议类型
 */
typedef enum {
    AGENTOS_PROTOCOL_JSON_RPC = 0,
    AGENTOS_PROTOCOL_MCP,
    AGENTOS_PROTOCOL_A2A,
    AGENTOS_PROTOCOL_OPENAI,
    AGENTOS_PROTOCOL_OPENJIUWEN,
    AGENTOS_PROTOCOL_COUNT
} agentos_protocol_type_t;

/**
 * @brief 协议适配器结构体（所有适配器共享的接口定义）
 */
typedef struct protocol_adapter_s {
    agentos_protocol_type_t type;
    const char* name;
    const char* version;
    const char* description;
    int (*init)(void* context);
    int (*destroy)(void* context);
    int (*encode)(void* context, const void* msg, void** out_data, size_t* out_size);
    int (*decode)(void* context, const void* data, size_t size, void* out_msg);
    int (*connect)(void* context, const char* endpoint);
    int (*disconnect)(void* context);
    int (*is_connected)(void* context);
    int (*send)(void* context, const void* data, size_t size);
    int (*receive)(void* context, void** data, size_t* size, uint32_t timeout_ms);
    int (*handle_request)(void* context, const void* req, void** resp);
    int (*get_version)(void* context, char* version_buf, size_t max_size);
    uint32_t (*capabilities)(void* context);
    int (*get_stats)(void* context, char* stats_json, size_t max_size);
    void* context;
    void* user_data;
} protocol_adapter_t;

typedef protocol_adapter_t proto_adapter_t;

/**
 * @brief 消息结构
 */
typedef struct {
    const void* data;
    size_t len;
    agentos_protocol_type_t source_protocol;
} agentos_message_t;

typedef agentos_protocol_type_t protocol_type_t;
typedef agentos_protocol_type_t proto_type_t;

typedef enum {
    DIRECTION_REQUEST = 0,
    DIRECTION_RESPONSE,
    DIRECTION_NOTIFICATION,
    DIRECTION_ERROR
} message_direction_t;

#define MSG_TYPE_REQUEST     DIRECTION_REQUEST
#define MSG_TYPE_RESPONSE    DIRECTION_RESPONSE
#define MSG_TYPE_ERROR       DIRECTION_ERROR

#define PROTOCOL_CUSTOM      AGENTOS_PROTOCOL_COUNT
#define PROTOCOL_HTTP        AGENTOS_PROTOCOL_JSON_RPC

#define ENCODING_UTF8_JSON   0

#define PROTO_JSONRPC        AGENTOS_PROTOCOL_JSON_RPC
#define PROTO_MCP            AGENTOS_PROTOCOL_MCP
#define PROTO_A2A            AGENTOS_PROTOCOL_A2A
#define PROTO_OPENAI         AGENTOS_PROTOCOL_OPENAI
#define PROTO_OPENJIUWEN     AGENTOS_PROTOCOL_OPENJIUWEN
#define PROTO_OPENCLAW       (AGENTOS_PROTOCOL_COUNT + 1)
#define PROTO_CLAUDE         (AGENTOS_PROTOCOL_COUNT + 2)

typedef struct {
    char* data;
    size_t size;
    int encoding;
} payload_wrapper_t;

typedef struct {
    agentos_protocol_type_t protocol;
    agentos_protocol_type_t protocol_type;  /* alias for protocol */
    char protocol_name[64];
    const char* endpoint;
    const char* method;
    message_direction_t direction;
    uint64_t message_id;
    const void* payload;
    size_t payload_size;
    uint64_t timestamp;
    bool is_error;
    int error_code;
    int status;  /* alias for compatibility */
    char error_msg[256];
    const void* body;  /* alias for payload */
    size_t body_length;  /* alias for payload_size */
    size_t payload_length;  /* alias for payload_size */
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
int protocol_adapter_create(agentos_protocol_type_t type, protocol_adapter_t* adapter);

/**
 * @brief 销毁协议适配器
 */
void protocol_adapter_destroy(protocol_adapter_t adapter);

/**
 * @brief 通过适配器发送消息
 */
int protocol_adapter_send(protocol_adapter_t adapter, const agentos_message_t* msg);

/**
 * @brief 通过适配器接收消息
 */
int protocol_adapter_recv(protocol_adapter_t adapter, agentos_message_t* msg, size_t max_len);

/**
 * @brief 获取协议类型名称
 */
const char* protocol_type_name(agentos_protocol_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_UNIFIED_PROTOCOL_H */
