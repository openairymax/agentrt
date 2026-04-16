/**
 * @file unified_protocol.h
 * @brief Standard unified protocol interface for AgentOS
 * @copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 */

#ifndef AGENTOS_UNIFIED_PROTOCOL_STANDARD_H
#define AGENTOS_UNIFIED_PROTOCOL_STANDARD_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PROTOCOL_HTTP = 0,
    PROTOCOL_WEBSOCKET,
    PROTOCOL_GRPC,
    PROTOCOL_MQTT,
    PROTOCOL_AMQP,
    PROTOCOL_RAW_TCP,
    PROTOCOL_RAW_UDP,
    PROTOCOL_CUSTOM
} protocol_type_t;

typedef enum {
    DIRECTION_REQUEST = 0,
    DIRECTION_RESPONSE,
    DIRECTION_NOTIFICATION
} message_direction_t;

typedef enum {
    UNIFIED_PROTOCOL_AUTO = 0,
    UNIFIED_PROTOCOL_JSONRPC,
    UNIFIED_PROTOCOL_MCP,
    UNIFIED_PROTOCOL_A2A,
    UNIFIED_PROTOCOL_OPENAI_API,
    UNIFIED_PROTOCOL_UNKNOWN = -1
} unified_protocol_type_t;

typedef struct {
    uint64_t message_id;
    protocol_type_t protocol;
    message_direction_t direction;
    const char* endpoint;
    const void* payload;
    size_t payload_size;
    uint32_t flags;
    uint64_t timestamp;
    void* user_data;
} unified_message_t;

typedef struct {
    protocol_type_t type;
    int (*init)(void*);
    void (*destroy)(void*);
    int (*encode)(void*, const unified_message_t*, void**, size_t*);
    int (*decode)(void*, const void*, size_t, unified_message_t*);
    int (*connect)(void*, const char*);
    int (*disconnect)(void*);
    bool (*is_connected)(void*);
    void (*get_stats)(void*, void*);
} protocol_adapter_t;

typedef struct {
    const char* name;
    protocol_type_t default_protocol;
    size_t max_message_size;
    uint32_t timeout_ms;
    bool enable_compression;
    bool enable_encryption;
    void* custom_config;
} protocol_stack_config_t;

typedef struct protocol_stack_s* protocol_stack_handle_t;

protocol_stack_handle_t protocol_stack_create(const protocol_stack_config_t* config);
void protocol_stack_destroy(protocol_stack_handle_t handle);
int protocol_stack_register_adapter(protocol_stack_handle_t handle, const protocol_adapter_t* adapter);
int protocol_stack_send(protocol_stack_handle_t handle, const unified_message_t* message);
int protocol_stack_receive(protocol_stack_handle_t handle, unified_message_t* message, uint32_t timeout_ms);
int protocol_stack_set_callback(protocol_stack_handle_t handle,
                               void (*callback)(const unified_message_t*, void*), void* user_data);
int protocol_stack_get_stats(protocol_stack_handle_t handle, void* stats);

unified_message_t unified_message_create(protocol_type_t protocol, message_direction_t direction,
                                        const char* endpoint, const void* payload, size_t payload_size);
void unified_message_destroy(unified_message_t message);
const char* protocol_type_to_string(protocol_type_t type);
protocol_type_t protocol_type_from_string(const char* str);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_UNIFIED_PROTOCOL_STANDARD_H */
