// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: Apache-2.0
/**
 * @file agentos_protocol_interface.h
 * @brief AgentOS Protocol System Unified Interface Definition
 *
 * 定义 AgentOS 协议系统的公共接口契约，作为所有协议适配器、网关、SDK 的统一抽象层。
 * 符合 ARCHITECTURAL_PRINCIPLES.md 中 K-2 接口契约化原则。
 *
 * 接口层次:
 *   I-L1 ProtocolAdapter — 单协议适配器基础接口
 *   I-L2 ProtocolRouter — 多协议路由与转换接口
 *   I-L3 ProtocolGateway — 网关协议集成接口
 *   I-L4 ProtocolExtension — 自定义协议扩展接口
 *
 * @since 2.0.0
 * @see unified_protocol.h
 * @see protocol_router.h
 * @see protocol_extension_framework.h
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

/* ============================================================================
 * I-L1: Protocol Adapter Interface (协议适配器基础接口)
 * ============================================================================ */

/**
 * @brief 协议适配器能力标志位
 */
typedef enum {
    PROTO_CAP_SYNC_REQUEST     = 0x0001,  /**< 同步请求-响应 */
    PROTO_CAP_STREAMING         = 0x0002,  /**< 流式传输 */
    PROTO_CAP_BIDIRECTIONAL     = 0x0004,  /**< 双向通信 */
    PROTO_CAP_TOOL_DISCOVERY    = 0x0008,  /**< 工具发现 (MCP) */
    PROTO_CAP_RESOURCE_ACCESS   = 0x0010,  /**< 资源访问 (MCP) */
    PROTO_CAP_AGENT_DISCOVERY   = 0x0020,  /**< 智能体发现 (A2A) */
    PROTO_CAP_TASK_LIFECYCLE    = 0x0040,  /**< 任务生命周期 (A2A) */
    PROTO_CAP_FUNCTION_CALLING  = 0x0080,  /**< 函数调用 (OpenAI) */
    PROTO_CAP_EMBEDDINGS        = 0x0100,  /**< 向量嵌入 (OpenAI) */
    PROTO_CAP_AUTHENTICATION    = 0x0200,  /**< 认证支持 */
    PROTO_CAP_RATE_LIMITING     = 0x0400,  /**< 速率限制 */
    PROTO_CAP_NEGOTIATION       = 0x0800,  /**< 协商机制 (A2A) */
    PROTO_CAP_NOTIFICATIONS     = 0x1000,  /**< 推送通知 */
    PROTO_CAP_CUSTOM            = 0x8000   /**< 自定义能力 */
} proto_capability_flags_t;

/**
 * @brief 协议连接状态
 */
typedef enum {
    PROTO_CONN_DISCONNECTED = 0,
    PROTO_CONN_CONNECTING,
    PROTO_CONN_CONNECTED,
    PROTO_CONN_RECONNECTING,
    PROTO_CONN_ERROR,
    PROTO_CONN_CLOSED
} proto_connection_state_t;

/**
 * @brief 协议统计信息
 */
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

/**
 * @brief 协议适配器接口 vtable（函数指针表）
 *
 * 所有协议适配器必须实现此接口，确保多态调用一致性。
 */
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

/**
 * @brief 协议适配器注册信息
 */
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

/**
 * @brief 路由决策结果
 */
typedef struct {
    const char* adapter_name;
    protocol_type_t target_protocol;
    int confidence;              /**< 0-100 匹配置信度 */
    bool needs_transformation;
    const char* transformer_name;
} route_decision_t;

/**
 * @brief 协议路由器接口
 */
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

/**
 * @brief 创建标准协议路由器实例
 */
proto_router_iface_t* proto_router_standard_create(void);

/**
 * @brief 销毁协议路由器
 */
void proto_router_standard_destroy(proto_router_iface_t* router);

/* ============================================================================
 * I-L3: Protocol Gateway Interface (网关协议集成接口)
 * ============================================================================ */

/**
 * @brief 网关协议处理回调
 *
 * 当网关收到外部请求时，通过此回调将请求路由到正确的协议处理器。
 */
typedef int (*proto_gateway_request_cb)(
    const char* protocol_name,
    const char* method,
    const char* params_json,
    char** response_json,
    void* user_data
);

/**
 * @brief 网关协议事件类型
 */
typedef enum {
    PROTO_GW_EVENT_CONNECTED = 0,
    PROTO_GW_EVENT_DISCONNECTED,
    PROTO_GW_EVENT_MESSAGE_RECEIVED,
    PROTO_GW_EVENT_ERROR,
    PROTO_GW_EVENT_RATE_LIMITED
} proto_gw_event_type_t;

/**
 * @brief 网关协议事件回调
 */
typedef void (*proto_gateway_event_cb)(
    proto_gw_event_type_t event,
    const char* protocol_name,
    const char* detail_json,
    void* user_data
);

/**
 * @brief 网关协议集成接口
 */
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

/**
 * @brief 创建标准网关协议集成实例
 */
proto_gateway_iface_t* proto_gateway_standard_create(void);

/**
 * @brief 销毁网关协议集成实例
 */
void proto_gateway_standard_destroy(proto_gateway_iface_t* gw);

/* ============================================================================
 * I-L4: Protocol Extension Interface (协议扩展接口)
 * ============================================================================ */

/**
 * @brief 扩展协议注册元数据
 */
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

/**
 * @brief 协议扩展管理接口
 */
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

/**
 * @brief 注册内置协议适配器到全局注册表
 *
 * AgentOS 启动时自动注册所有内置协议适配器：
 * - JSON-RPC 2.0 (PROTOCOL_HTTP)
 * - WebSocket (PROTOCOL_WEBSOCKET)
 * - MCP v1.0 (PROTOCOL_CUSTOM -> mcp)
 * - A2A v0.3 (PROTOCOL_CUSTOM -> a2a)
 * - OpenAI API (PROTOCOL_CUSTOM -> openai)
 * - OpenJiuwen (PROTOCOL_CUSTOM -> openjiuwen)
 */
int proto_interface_register_builtins(void);

/**
 * @brief 从全局注册表查找协议适配器
 */
const proto_adapter_entry_t* proto_interface_find(const char* name);

/**
 * @brief 列出所有已注册的协议适配器
 */
int proto_interface_list_all(char** json_output);

/**
 * @brief 获取协议类型的人类可读名称
 */
const char* proto_interface_type_name(protocol_type_t type);

/**
 * @brief 根据名称解析协议类型
 */
protocol_type_t proto_interface_parse_type(const char* name);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_PROTOCOL_INTERFACE_H */
