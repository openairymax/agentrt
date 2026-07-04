// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
/**
 * @file daemon_bootstrap_ipc.c
 * @brief P1.8 C-L09: daemon IPC Bus 一键引导实现
 *
 * @see daemon_bootstrap_ipc.h
 * @see P1.8 C-L09 连接线
 */

#include "daemon_bootstrap_ipc.h"

#include "memory_compat.h"
#include "svc_logger.h"

#include <stdio.h>
#include <string.h>

/* ==================== 内部数据结构 ==================== */

struct daemon_bootstrap_ipc_s {
    ipc_bus_helper_t *ibh;        /* 底层 IPC helper */
    bool             running;
    char             daemon_name[64];
    char             channel_name[64];
    ipc_bus_proto_t  protocol;
};

/* ==================== 构建端点地址 ==================== */

static void build_endpoint(char *buf, size_t buf_size,
                           const char *host, uint16_t port)
{
    if (port > 0) {
        snprintf(buf, buf_size, "%s:%u", host ? host : "127.0.0.1", (unsigned int)port);
    } else {
        snprintf(buf, buf_size, "%s", host ? host : "unknown");
    }
}

/* ==================== 实现 ==================== */

daemon_bootstrap_ipc_t *daemon_bootstrap_ipc_start(const char *daemon_name,
                                                    const char *channel_name,
                                                    const char *host, uint16_t port,
                                                    ipc_bus_proto_t protocol)
{
    if (!daemon_name || !channel_name) {
        SVC_LOG_ERROR("daemon_bootstrap_ipc_start: invalid parameters");
        return NULL;
    }

    daemon_bootstrap_ipc_t *bipc =
        (daemon_bootstrap_ipc_t *)AGENTOS_CALLOC(1, sizeof(daemon_bootstrap_ipc_t));
    if (!bipc) return NULL;

    /* 初始化 IPC Bus helper */
    bipc->ibh = ipc_bus_helper_init(daemon_name, NULL);
    if (!bipc->ibh) {
        SVC_LOG_ERROR("daemon_bootstrap_ipc_start: init failed for '%s'", daemon_name);
        AGENTOS_FREE(bipc);
        return NULL;
    }

    /* 注册通道（P1.8.1） */
    if (ipc_bus_helper_register_channel(bipc->ibh, channel_name, protocol) != 0) {
        SVC_LOG_ERROR("daemon_bootstrap_ipc_start: channel register failed for '%s'", daemon_name);
        ipc_bus_helper_shutdown(bipc->ibh);
        AGENTOS_FREE(bipc);
        return NULL;
    }

    /* 注册端点（P1.8.1） — 让其他 daemon 可发现 */
    char endpoint[256];
    build_endpoint(endpoint, sizeof(endpoint), host, port);

    ipc_bus_proto_t protos[] = { protocol };
    if (ipc_bus_helper_register_endpoint(bipc->ibh, daemon_name, endpoint,
                                          protos, 1) != 0) {
        SVC_LOG_WARN("daemon_bootstrap_ipc_start: endpoint register failed for '%s'",
                      daemon_name);
        /* 非致命 — 通道已注册即可接收消息 */
    }

    AGENTOS_STRNCPY_TERM(bipc->daemon_name, daemon_name, sizeof(bipc->daemon_name) - 1);
    AGENTOS_STRNCPY_TERM(bipc->channel_name, channel_name, sizeof(bipc->channel_name) - 1);
    bipc->protocol = protocol;
    bipc->running = true;

    /* P1.8: 启用背压控制（生产环境推荐） */
    {
        ipc_bp_config_t bp_cfg;
        __builtin_memset(&bp_cfg, 0, sizeof(bp_cfg));
        bp_cfg.queue_capacity = 1024;
        bp_cfg.slow_threshold_pct = 80;
        bp_cfg.drop_threshold_pct = 90;
        bp_cfg.reject_threshold_pct = 95;
        bp_cfg.recover_threshold_pct = 60;
        bp_cfg.sample_interval_ms = 5000;

        if (ipc_bus_helper_enable_backpressure(bipc->ibh, &bp_cfg) == 0) {
            SVC_LOG_INFO("C-L09: Backpressure enabled for '%s' (capacity=%zu)",
                         daemon_name, bp_cfg.queue_capacity);
        }
    }

    SVC_LOG_INFO("C-L09: IPC Bus bootstrapped for '%s' (channel=%s, proto=%s, endpoint=%s)",
                 daemon_name, channel_name,
                 ipc_bus_proto_to_string(protocol), endpoint);
    return bipc;
}

daemon_bootstrap_ipc_t *daemon_bootstrap_ipc_start_unix(const char *daemon_name,
                                                         const char *channel_name,
                                                         const char *socket_path,
                                                         ipc_bus_proto_t protocol)
{
    if (!daemon_name || !channel_name || !socket_path) {
        SVC_LOG_ERROR("daemon_bootstrap_ipc_start_unix: invalid parameters");
        return NULL;
    }

    daemon_bootstrap_ipc_t *bipc =
        (daemon_bootstrap_ipc_t *)AGENTOS_CALLOC(1, sizeof(daemon_bootstrap_ipc_t));
    if (!bipc) return NULL;

    bipc->ibh = ipc_bus_helper_init(daemon_name, NULL);
    if (!bipc->ibh) {
        SVC_LOG_ERROR("daemon_bootstrap_ipc_start_unix: init failed for '%s'", daemon_name);
        AGENTOS_FREE(bipc);
        return NULL;
    }

    if (ipc_bus_helper_register_channel(bipc->ibh, channel_name, protocol) != 0) {
        ipc_bus_helper_shutdown(bipc->ibh);
        AGENTOS_FREE(bipc);
        return NULL;
    }

    ipc_bus_proto_t protos[] = { protocol };
    ipc_bus_helper_register_endpoint(bipc->ibh, daemon_name, socket_path,
                                      protos, 1);

    AGENTOS_STRNCPY_TERM(bipc->daemon_name, daemon_name, sizeof(bipc->daemon_name) - 1);
    AGENTOS_STRNCPY_TERM(bipc->channel_name, channel_name, sizeof(bipc->channel_name) - 1);
    bipc->protocol = protocol;
    bipc->running = true;

    /* P1.8: 启用背压控制（Unix socket 版本） */
    {
        ipc_bp_config_t bp_cfg;
        __builtin_memset(&bp_cfg, 0, sizeof(bp_cfg));
        bp_cfg.queue_capacity = 1024;
        bp_cfg.slow_threshold_pct = 80;
        bp_cfg.drop_threshold_pct = 90;
        bp_cfg.reject_threshold_pct = 95;
        bp_cfg.recover_threshold_pct = 60;
        bp_cfg.sample_interval_ms = 5000;

        if (ipc_bus_helper_enable_backpressure(bipc->ibh, &bp_cfg) == 0) {
            SVC_LOG_INFO("C-L09: Backpressure enabled for '%s' (capacity=%zu, unix)",
                         daemon_name, bp_cfg.queue_capacity);
        }
    }

    SVC_LOG_INFO("C-L09: IPC Bus bootstrapped for '%s' (unix:%s, proto=%s)",
                 daemon_name, socket_path, ipc_bus_proto_to_string(protocol));
    return bipc;
}

void daemon_bootstrap_ipc_stop(daemon_bootstrap_ipc_t *bipc)
{
    if (!bipc) return;

    SVC_LOG_INFO("C-L09: IPC Bus shutting down for '%s'", bipc->daemon_name);

    if (bipc->ibh) {
        ipc_bus_helper_shutdown(bipc->ibh);
        bipc->ibh = NULL;
    }

    bipc->running = false;
    AGENTOS_FREE(bipc);
}

int daemon_bootstrap_ipc_register_handler(daemon_bootstrap_ipc_t *bipc,
                                           ipc_bus_message_handler_t handler,
                                           void *user_data)
{
    if (!bipc || !handler) return -1;
    return ipc_bus_helper_register_handler(bipc->ibh, handler, user_data);
}

int daemon_bootstrap_ipc_send(daemon_bootstrap_ipc_t *bipc,
                               const char *target_service,
                               const void *payload, size_t payload_size)
{
    if (!bipc || !target_service || !payload) return -1;

    SVC_LOG_DEBUG("C-L09: daemon_send [%s] → [%s] payload=%zub",
                  bipc->daemon_name, target_service, payload_size);

    return ipc_bus_helper_route_auto(bipc->ibh, target_service,
                                      payload, payload_size);
}

ipc_bus_helper_t *daemon_bootstrap_ipc_get_helper(daemon_bootstrap_ipc_t *bipc)
{
    return bipc ? bipc->ibh : NULL;
}

bool daemon_bootstrap_ipc_is_running(daemon_bootstrap_ipc_t *bipc)
{
    return bipc ? bipc->running : false;
}