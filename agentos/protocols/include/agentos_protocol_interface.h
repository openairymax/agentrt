// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: Apache-2.0
/**
 * @file agentos_protocol_interface.h
 * @brief AgentOS Protocol System Unified Interface Definition
 *
 * 定义 AgentOS 协议系统的公共接口契约，作为所有协议适配器、网关、SDK 的统一抽象层。
 *
 * 原位置: agentos/interfaces/include/
 * 迁移至: agentos/protocols/include/ (2026-04-19 interfaces删除重构)
 */

#ifndef AGENTOS_PROTOCOL_INTERFACE_H
#define AGENTOS_PROTOCOL_INTERFACE_H

#include "unified_protocol.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* unified_message_t and protocol_type_t now defined in unified_protocol.h */

/* ============================================================================
 * I-L1: Protocol Adapter Interface (协议适配器基础接口)
 * ============================================================================ */

typedef enum {
    PROTO_CAP_SYNC_REQUEST     = 0x0001,
    PROTO_CAP_REQUEST_RESPONSE = 0x0001,
    PROTO_CAP_STREAMING         = 0x0002,
    PROTO_CAP_BIDIRECTIONAL     = 0x0004,
    PROTO_CAP_TOOL_DISCOVERY    = 0x0008,
    PROTO_CAP_RESOURCE_ACCESS   = 0x0010,
    PROTO_CAP_AGENT_DISCOVERY   = 0x0020,
    PROTO_CAP_TASK_LIFECYCLE    = 0x0040,
    PROTO_CAP_FUNCTION_CALLING  = 0x0080,
    PROTO_CAP_EMBEDDINGS        = 0x0100,
    PROTO_CAP_AUTHENTICATION    = 0x0200,
    PROTO_CAP_RATE_LIMITING     = 0x0400,
    PROTO_CAP_NEGOTIATION       = 0x0800,
    PROTO_CAP_NOTIFICATIONS     = 0x1000,
    PROTO_CAP_CUSTOM            = 0x8000,
    PROTO_CAP_BATCH             = 0x00010000,
    PROTO_CAP_TOOL_CALLING      = 0x00020000,
    PROTO_CAP_CONSENSUS         = 0x00040000,
    PROTO_CAP_BINARY            = 0x00080000,
    PROTO_CAP_LOW_LATENCY       = 0x00100000,
    PROTO_CAP_CRC_CHECKSUM      = 0x00200000,
    PROTO_CAP_MULTIMODAL        = 0x00400000,
    PROTO_CAP_VISION            = 0x00800000,
    PROTO_CAP_EXTENDED_THINKING = 0x01000000,
    PROTO_CAP_CODE_EXECUTION    = 0x02000000,
    PROTO_CAP_HUMAN_LOOP        = 0x04000000
} proto_capability_flags_t;

typedef enum {
    PROTO_CONN_DISCONNECTED = 0,
    PROTO_CONN_CONNECTING,
    PROTO_CONN_CONNECTED,
    PROTO_CONN_RECONNECTING,
    PROTO_CONN_ERROR,
    PROTO_CONN_CLOSED
} proto_connection_state_t;

typedef struct {
    uint64_t messages_sent;
    uint64_t messages_received;
    uint64_t bytes_sent;
    uint64_t bytes_received;
    uint64_t errors_total;
    uint64_t errors_timeout;
    uint64_t errors_protocol;
    double avg_latency_ms;
    double p50_latency_ms;
    double p99_latency_ms;
    uint32_t active_connections;
    proto_connection_state_t connection_state;
} proto_stats_t;

typedef struct {
    int (*init)(void** context);
    void (*destroy)(void* context);
    int (*encode)(void* context, const unified_message_t* in_msg,
                  void** out_data, size_t* out_size);
    int (*decode)(void* context, const void* in_data, size_t in_size,
                  unified_message_t* out_msg);
    int (*connect)(void* context, const char* address);
    int (*disconnect)(void* context);
    bool (*is_connected)(void* context);
    int (*send)(void* context, const unified_message_t* message);
    int (*receive)(void* context, unified_message_t* message,
                  int timeout_ms);
    int (*get_stats)(void* context, proto_stats_t* stats);
    const char* (*get_name)(void);
    protocol_type_t (*get_type)(void);
    uint32_t (*get_capabilities)(void);
} proto_adapter_vtable_t;

typedef struct proto_adapter_entry_s {
    const char* name;
    const char* version;
    const char* description;
    protocol_type_t type;
    uint32_t capabilities;
    const proto_adapter_vtable_t* vtable;
    bool is_builtin;
    struct proto_adapter_entry_s* next;
} proto_adapter_entry_t;

/* ============================================================================
 * I-L2: Protocol Router Interface (协议路由接口)
 * ============================================================================ */

typedef struct {
    const char* adapter_name;
    protocol_type_t target_protocol;
    int confidence;
    bool needs_transformation;
    const char* transformer_name;
} route_decision_t;

typedef struct proto_router_iface_s {
    int (*add_route)(struct proto_router_iface_s* router,
                     const char* source_pattern,
                     protocol_type_t source_proto,
                     const char* target_endpoint,
                     protocol_type_t target_proto,
                     int priority);
    int (*remove_route)(struct proto_router_iface_s* router,
                        const char* source_pattern);
    int (*route)(struct proto_router_iface_s* router,
                 const unified_message_t* message,
                 route_decision_t* decision);
    int (*transform)(struct proto_router_iface_s* router,
                     const unified_message_t* source,
                     unified_message_t* target,
                     const char* transformer_name);
    int (*batch_route)(struct proto_router_iface_s* router,
                       const unified_message_t* messages,
                       size_t count,
                       route_decision_t* decisions);
    int (*set_default_protocol)(struct proto_router_iface_s* router,
                                protocol_type_t proto);
    int (*list_routes)(struct proto_router_iface_s* router,
                       char** routes_json);
    int (*get_stats)(struct proto_router_iface_s* router,
                     char** stats_json);
} proto_router_iface_t;

proto_router_iface_t* proto_router_standard_create(void);
void proto_router_standard_destroy(proto_router_iface_t* router);

/* ============================================================================
 * I-L3: Protocol Gateway Interface (网关协议集成接口)
 * ============================================================================ */

typedef int (*proto_gateway_request_cb)(
    const char* protocol_name,
    const char* method,
    const char* params_json,
    char** response_json,
    void* user_data
);

typedef enum {
    PROTO_GW_EVENT_CONNECTED = 0,
    PROTO_GW_EVENT_DISCONNECTED,
    PROTO_GW_EVENT_MESSAGE_RECEIVED,
    PROTO_GW_EVENT_ERROR,
    PROTO_GW_EVENT_RATE_LIMITED
} proto_gw_event_type_t;

typedef void (*proto_gateway_event_cb)(
    proto_gw_event_type_t event,
    const char* protocol_name,
    const char* detail_json,
    void* user_data
);

typedef struct proto_gateway_iface_s {
    int (*register_protocol)(struct proto_gateway_iface_s* gw,
                             const char* name,
                             const proto_adapter_vtable_t* adapter);
    int (*unregister_protocol)(struct proto_gateway_iface_s* gw,
                               const char* name);
    int (*handle_request)(struct proto_gateway_iface_s* gw,
                          const char* raw_request,
                          size_t request_size,
                          const char* content_type,
                          char** response,
                          size_t* response_size,
                          char** response_content_type);
    int (*detect_protocol)(struct proto_gateway_iface_s* gw,
                            const char* data,
                            size_t size,
                            char** detected_protocol);
    int (*set_request_handler)(struct proto_gateway_iface_s* gw,
                               proto_gateway_request_cb handler,
                               void* user_data);
    int (*set_event_callback)(struct proto_gateway_iface_s* gw,
                              proto_gateway_event_cb callback,
                              void* user_data);
    int (*list_protocols)(struct proto_gateway_iface_s* gw,
                          char** protocols_json);
    int (*get_protocol_stats)(struct proto_gateway_iface_s* gw,
                              const char* name,
                              proto_stats_t* stats);
} proto_gateway_iface_t;

proto_gateway_iface_t* proto_gateway_standard_create(void);
void proto_gateway_standard_destroy(proto_gateway_iface_t* gw);

/* ============================================================================
 * I-L4: Protocol Extension Interface (协议扩展接口)
 * ============================================================================ */

typedef struct {
    char name[64];
    char version[32];
    char author[128];
    char description[256];
    protocol_type_t type;
    uint32_t capabilities;
    int priority;
    bool hot_reloadable;
    char config_schema_json[1024];
} proto_extension_meta_t;

typedef struct proto_extension_mgr_iface_s {
    int (*register_extension)(struct proto_extension_mgr_iface_s* mgr,
                             const proto_extension_meta_t* meta,
                             const proto_adapter_vtable_t* vtable);
    int (*unregister_extension)(struct proto_extension_mgr_iface_s* mgr,
                               const char* name);
    int (*load_extension)(struct proto_extension_mgr_iface_s* mgr,
                         const char* name,
                         const char* config_json);
    int (*unload_extension)(struct proto_extension_mgr_iface_s* mgr,
                           const char* name);
    int (*auto_detect)(struct proto_extension_mgr_iface_s* mgr,
                      const unified_message_t* msg,
                      char** extension_name);
    int (*list_extensions)(struct proto_extension_mgr_iface_s* mgr,
                          char** extensions_json);
    int (*find_by_capability)(struct proto_extension_mgr_iface_s* mgr,
                             uint32_t capability,
                             char*** names,
                             size_t* count);
} proto_extension_mgr_iface_t;

/* ============================================================================
 * 全局注册与发现 API
 * ============================================================================ */

int proto_interface_register_builtins(void);
const proto_adapter_entry_t* proto_interface_find(const char* name);
int proto_interface_list_all(char** json_output);
const char* proto_interface_type_name(protocol_type_t type);
protocol_type_t proto_interface_parse_type(const char* name);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_PROTOCOL_INTERFACE_H */
