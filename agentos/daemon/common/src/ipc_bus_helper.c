// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: Apache-2.0
/**
 * @file ipc_bus_helper.c
 * @brief C-L09: IPC Bus → daemon 自动注册便捷层实现
 *
 * 封装 ipc_service_bus 核心 API，提供 daemon 一键注册、消息路由
 * 和协议透明转发的便捷接口。
 *
 * @see ipc_bus_helper.h
 * @see P1.8 C-L09 连接线
 */

#include "ipc_bus_helper.h"

#include "memory_compat.h"
#include "safe_string_utils.h"
#include "svc_logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==================== 内部数据结构 ==================== */

struct ipc_bus_helper_s {
    ipc_service_bus_t bus;           /* 底层 IPC Bus 实例 */
    ipc_bus_channel_t channel;       /* 当前 daemon 的通道 */
    char daemon_name[IPC_BUS_SERVICE_ID_LEN];
    bool channel_registered;
    bool endpoint_registered;
    /* P1.24: 背压控制器 */
    ipc_bp_controller_t *bp_ctrl;    /* 背压控制器（NULL=未启用） */
};

/* ==================== 生命周期 ==================== */

ipc_bus_helper_t *ipc_bus_helper_init(const char *daemon_name,
                                      const ipc_bus_channel_config_t *config) {
    if (!daemon_name) {
        SVC_LOG_ERROR("ipc_bus_helper_init: daemon_name is NULL");
        return NULL;
    }

    ipc_bus_helper_t *ibh = (ipc_bus_helper_t *)AGENTOS_CALLOC(1, sizeof(ipc_bus_helper_t));
    if (!ibh) {
        SVC_LOG_ERROR("ipc_bus_helper_init: failed to allocate");
        return NULL;
    }

    safe_strcpy(ibh->daemon_name, daemon_name, sizeof(ibh->daemon_name));

    /* 创建 IPC Bus */
    char bus_name[IPC_BUS_SERVICE_ID_LEN + 16];
    snprintf(bus_name, sizeof(bus_name), "bus-%s", daemon_name);

    ibh->bus = ipc_service_bus_create(bus_name, config);
    if (!ibh->bus) {
        SVC_LOG_ERROR("Failed to create IPC bus '%s'", bus_name);
        AGENTOS_FREE(ibh);
        return NULL;
    }

    /* 启动 IPC Bus */
    agentos_error_t err = ipc_service_bus_start(ibh->bus);
    if (err != AGENTOS_SUCCESS) {
        SVC_LOG_ERROR("Failed to start IPC bus '%s' (err=%d)", bus_name, err);
        ipc_service_bus_destroy(ibh->bus);
        AGENTOS_FREE(ibh);
        return NULL;
    }

    ibh->channel_registered = false;
    ibh->endpoint_registered = false;

    SVC_LOG_INFO("IPC bus helper initialized for daemon '%s'", daemon_name);
    return ibh;
}

void ipc_bus_helper_shutdown(ipc_bus_helper_t *ibh) {
    if (!ibh) return;

    /* P1.24: 销毁背压控制器 */
    if (ibh->bp_ctrl) {
        ipc_bp_destroy(ibh->bp_ctrl);
        ibh->bp_ctrl = NULL;
    }

    if (ibh->channel) {
        ipc_bus_channel_destroy(ibh->channel);
        ibh->channel = NULL;
    }

    if (ibh->bus) {
        ipc_service_bus_destroy(ibh->bus);
        ibh->bus = NULL;
    }

    SVC_LOG_INFO("IPC bus helper shutdown for daemon '%s'", ibh->daemon_name);
    AGENTOS_FREE(ibh);
}

/* ==================== 通道注册（P1.8.1） ==================== */

int ipc_bus_helper_register_channel(ipc_bus_helper_t *ibh,
                                    const char *channel_name,
                                    ipc_bus_proto_t default_protocol) {
    if (!ibh || !channel_name) return -1;

    if (ibh->channel_registered) {
        SVC_LOG_WARN("Channel already registered for daemon '%s'", ibh->daemon_name);
        return 0;
    }

    ipc_bus_channel_config_t ch_config;
    memset(&ch_config, 0, sizeof(ch_config));
    safe_strcpy(ch_config.name, channel_name, sizeof(ch_config.name));
    ch_config.default_protocol = default_protocol;
    ch_config.timeout_ms = IPC_BUS_DEFAULT_TIMEOUT_MS;
    ch_config.max_retries = IPC_BUS_MAX_RETRIES;
    ch_config.buffer_size = IPC_BUS_MAX_MESSAGE_SIZE;

    ibh->channel = ipc_bus_channel_create(ibh->bus, &ch_config);
    if (!ibh->channel) {
        SVC_LOG_ERROR("Failed to create channel '%s' for daemon '%s'",
                      channel_name, ibh->daemon_name);
        return -1;
    }

    ibh->channel_registered = true;
    SVC_LOG_INFO("IPC channel '%s' registered for daemon '%s' (proto=%s)",
                 channel_name, ibh->daemon_name,
                 ipc_bus_proto_to_string(default_protocol));
    return 0;
}

int ipc_bus_helper_register_endpoint(ipc_bus_helper_t *ibh,
                                     const char *service_name,
                                     const char *endpoint,
                                     const ipc_bus_proto_t *protocols,
                                     uint32_t proto_count) {
    if (!ibh || !service_name || !endpoint || !protocols || proto_count == 0)
        return -1;

    ipc_bus_endpoint_t ep;
    memset(&ep, 0, sizeof(ep));
    safe_strcpy(ep.service_name, service_name, sizeof(ep.service_name));
    safe_strcpy(ep.endpoint, endpoint, sizeof(ep.endpoint));
    ep.healthy = true;
    ep.weight = 100;
    ep.max_connections = 1024;
    ep.active_connections = 0;

    ep.protocol_count = proto_count > IPC_BUS_MAX_PROTOCOLS
                        ? IPC_BUS_MAX_PROTOCOLS : proto_count;
    for (uint32_t i = 0; i < ep.protocol_count; i++) {
        ep.supported_protocols[i] = protocols[i];
    }

    agentos_error_t err = ipc_service_bus_register_endpoint(ibh->bus, &ep);
    if (err != AGENTOS_SUCCESS) {
        SVC_LOG_ERROR("Failed to register endpoint '%s' for daemon '%s' (err=%d)",
                      service_name, ibh->daemon_name, err);
        return -1;
    }

    ibh->endpoint_registered = true;
    SVC_LOG_INFO("Endpoint '%s' registered (addr=%s, protos=%u)",
                 service_name, endpoint, proto_count);
    return 0;
}

/* ==================== 消息处理器（P1.8.2） ==================== */

int ipc_bus_helper_register_handler(ipc_bus_helper_t *ibh,
                                    ipc_bus_message_handler_t handler,
                                    void *user_data) {
    if (!ibh || !handler) return -1;

    agentos_error_t err = ipc_service_bus_register_handler(ibh->bus, handler, user_data);
    if (err != AGENTOS_SUCCESS) {
        SVC_LOG_ERROR("Failed to register message handler for daemon '%s' (err=%d)",
                      ibh->daemon_name, err);
        return -1;
    }

    SVC_LOG_INFO("Message handler registered for daemon '%s'", ibh->daemon_name);
    return 0;
}

int ipc_bus_helper_register_event_handler(ipc_bus_helper_t *ibh,
                                          const char *event_name,
                                          ipc_bus_event_handler_t handler,
                                          void *user_data) {
    if (!ibh || !event_name || !handler) return -1;

    agentos_error_t err = ipc_service_bus_register_event_handler(
        ibh->bus, event_name, handler, user_data);
    if (err != AGENTOS_SUCCESS) {
        SVC_LOG_ERROR("Failed to register event handler '%s' for daemon '%s' (err=%d)",
                      event_name, ibh->daemon_name, err);
        return -1;
    }

    SVC_LOG_INFO("Event handler '%s' registered for daemon '%s'",
                 event_name, ibh->daemon_name);
    return 0;
}

/* ==================== 消息发送（P1.8.3） ==================== */

int ipc_bus_helper_send(ipc_bus_helper_t *ibh, const char *target_service,
                        ipc_bus_msg_type_t msg_type, ipc_bus_proto_t protocol,
                        const void *payload, size_t payload_size) {
    if (!ibh || !target_service || !payload) return -1;

    ipc_bus_message_t *msg = ipc_bus_message_create(msg_type, protocol,
                                                     payload, payload_size);
    if (!msg) return -1;

    /* 设置消息头 */
    safe_strcpy(msg->header.source, ibh->daemon_name,
                sizeof(msg->header.source));
    safe_strcpy(msg->header.target, target_service,
                sizeof(msg->header.target));

    agentos_error_t err = ipc_service_bus_send(ibh->bus, target_service, msg);
    ipc_bus_message_free(msg);

    return (err == AGENTOS_SUCCESS) ? 0 : -1;
}

int ipc_bus_helper_request(ipc_bus_helper_t *ibh, const char *target_service,
                           const ipc_bus_message_t *request,
                           ipc_bus_message_t *response, uint32_t timeout_ms) {
    if (!ibh || !target_service || !request || !response) return -1;

    agentos_error_t err = ipc_service_bus_request(ibh->bus, target_service,
                                                   request, response,
                                                   timeout_ms);
    return (err == AGENTOS_SUCCESS) ? 0 : -1;
}

int ipc_bus_helper_broadcast(ipc_bus_helper_t *ibh,
                             const ipc_bus_message_t *message) {
    if (!ibh || !message) return -1;

    agentos_error_t err = ipc_service_bus_broadcast(ibh->bus, message);
    return (err == AGENTOS_SUCCESS) ? 0 : -1;
}

int ipc_bus_helper_notify(ipc_bus_helper_t *ibh, const char *target_service,
                          const void *payload, size_t payload_size,
                          ipc_bus_proto_t protocol) {
    if (!ibh || !target_service || !payload) return -1;

    agentos_error_t err = ipc_service_bus_notify(ibh->bus, target_service,
                                                  payload, payload_size,
                                                  protocol);
    return (err == AGENTOS_SUCCESS) ? 0 : -1;
}

/* ==================== 协议透明路由（P1.8.4） ==================== */

int ipc_bus_helper_route_auto(ipc_bus_helper_t *ibh,
                              const char *target_service,
                              const void *payload, size_t payload_size) {
    if (!ibh || !target_service || !payload) return -1;

    /* 选择最佳协议：按优先级 JSON-RPC > MCP > A2A > OpenAI */
    static const ipc_bus_proto_t proto_priority[] = {
        IPC_BUS_PROTO_JSON_RPC,
        IPC_BUS_PROTO_MCP,
        IPC_BUS_PROTO_A2A,
        IPC_BUS_PROTO_OPENAI
    };
    static const uint32_t proto_count = 4;

    /* 发现目标服务端点 */
    ipc_bus_endpoint_t endpoints[8];
    uint32_t found = 0;

    agentos_error_t err = ipc_service_bus_discover(ibh->bus, target_service,
                                                    IPC_BUS_PROTO_AUTO,
                                                    endpoints, 8, &found);
    if (err != AGENTOS_SUCCESS || found == 0) {
        SVC_LOG_WARN("No endpoints found for '%s', trying direct send",
                     target_service);
        /* 直接发送，使用默认协议 */
        return ipc_bus_helper_send(ibh, target_service,
                                   IPC_BUS_MSG_REQUEST,
                                   IPC_BUS_PROTO_JSON_RPC,
                                   payload, payload_size);
    }

    /* 按优先级选择协议 */
    for (uint32_t pi = 0; pi < proto_count; pi++) {
        for (uint32_t ei = 0; ei < found; ei++) {
            if (!endpoints[ei].healthy) continue;
            for (uint32_t p = 0; p < endpoints[ei].protocol_count; p++) {
                if (endpoints[ei].supported_protocols[p] == proto_priority[pi]) {
                    SVC_LOG_DEBUG("Auto-routing to '%s' via %s",
                                  target_service,
                                  ipc_bus_proto_to_string(proto_priority[pi]));
                    return ipc_bus_helper_send(ibh, target_service,
                                               IPC_BUS_MSG_REQUEST,
                                               proto_priority[pi],
                                               payload, payload_size);
                }
            }
        }
    }

    /* 降级：使用第一个可用端点 */
    for (uint32_t ei = 0; ei < found; ei++) {
        if (endpoints[ei].healthy && endpoints[ei].protocol_count > 0) {
            return ipc_bus_helper_send(ibh, target_service,
                                       IPC_BUS_MSG_REQUEST,
                                       endpoints[ei].supported_protocols[0],
                                       payload, payload_size);
        }
    }

    return -1;
}

int ipc_bus_helper_discover(ipc_bus_helper_t *ibh,
                            const char *service_name,
                            ipc_bus_proto_t protocol,
                            ipc_bus_endpoint_t *endpoints,
                            uint32_t max_count, uint32_t *found_count) {
    if (!ibh || !endpoints || !found_count) return -1;

    agentos_error_t err = ipc_service_bus_discover(ibh->bus, service_name,
                                                    protocol, endpoints,
                                                    max_count, found_count);
    return (err == AGENTOS_SUCCESS) ? 0 : -1;
}

/* ==================== 状态查询 ==================== */

ipc_service_bus_t ipc_bus_helper_get_bus(ipc_bus_helper_t *ibh) {
    return ibh ? ibh->bus : NULL;
}

bool ipc_bus_helper_is_running(ipc_bus_helper_t *ibh) {
    if (!ibh) return false;
    return ipc_service_bus_is_running(ibh->bus);
}

/* ==================== 背压控制集成（P1.24） ==================== */

int ipc_bus_helper_enable_backpressure(ipc_bus_helper_t *ibh,
                                       const ipc_bp_config_t *config) {
    if (!ibh) return -1;

    /* 已存在则先销毁 */
    if (ibh->bp_ctrl) {
        ipc_bp_destroy(ibh->bp_ctrl);
    }

    ibh->bp_ctrl = ipc_bp_create(config);
    if (!ibh->bp_ctrl) {
        SVC_LOG_ERROR("P1.24: Failed to create backpressure controller for '%s'",
                      ibh->daemon_name);
        return -1;
    }

    SVC_LOG_INFO("P1.24: Backpressure enabled for daemon '%s'", ibh->daemon_name);
    return 0;
}

ipc_bp_level_t ipc_bus_helper_update_backpressure(ipc_bus_helper_t *ibh,
                                                   size_t current_depth) {
    if (!ibh || !ibh->bp_ctrl)
        return IPC_BP_NORMAL;

    return ipc_bp_update(ibh->bp_ctrl, current_depth);
}

int ipc_bus_helper_send_with_bp(ipc_bus_helper_t *ibh, const char *target,
                                ipc_bus_msg_type_t msg_type, ipc_bus_proto_t protocol,
                                const void *payload, size_t payload_size,
                                bool is_droppable) {
    if (!ibh || !target || !payload) return -1;

    /* 如果未启用背压，直接发送 */
    if (!ibh->bp_ctrl) {
        return ipc_bus_helper_send(ibh, target, msg_type, protocol,
                                   payload, payload_size);
    }

    /* 检查背压是否允许发送 */
    if (!ipc_bp_should_send(ibh->bp_ctrl, is_droppable)) {
        SVC_LOG_DEBUG("P1.24: Message to '%s' dropped by backpressure (droppable=%d)",
                      target, is_droppable);
        return 1;  /* 被背压丢弃 */
    }

    /* 正常发送 */
    return ipc_bus_helper_send(ibh, target, msg_type, protocol,
                               payload, payload_size);
}

bool ipc_bus_helper_should_accept_connection(ipc_bus_helper_t *ibh) {
    if (!ibh || !ibh->bp_ctrl)
        return true;

    return ipc_bp_should_accept_connection(ibh->bp_ctrl);
}

int ipc_bus_helper_get_bp_stats(ipc_bus_helper_t *ibh, ipc_bp_stats_t *out_stats) {
    if (!ibh || !out_stats) return -1;
    if (!ibh->bp_ctrl) return -1;

    ipc_bp_get_stats(ibh->bp_ctrl, out_stats);
    return 0;
}

ipc_bp_level_t ipc_bus_helper_get_bp_level(ipc_bus_helper_t *ibh) {
    if (!ibh || !ibh->bp_ctrl)
        return IPC_BP_NORMAL;

    return ipc_bp_get_level(ibh->bp_ctrl);
}