/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file gateway_forward.c
 * @brief C-L11: gateway_d → gateway 协议路由转发放实现
 *
 * 实现：
 *   P1.10.1: 外部 HTTP → gateway → gateway_d → 协议路由 → 目标 daemon
 *   P1.10.2: A2A 协议转发路径 → agent_d
 *   P1.10.3: MCP 协议转发路径 → tool_d / llm_d
 *   P1.10.4: OpenAI 兼容转发路径 → llm_d
 */

#include "gateway_forward.h"

#include "ipc_bus_helper.h"
#include "memory_compat.h"
#include "platform.h"
#include "safe_string_utils.h"
#include "svc_logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "error.h"

/* ==================== 内部结构 ==================== */

struct gw_forward_s {
    gw_forward_config_t config;
    ipc_bus_helper_t *ipc_helper;
    gw_forward_stats_t stats;
    bool initialized;
    bool healthy;
};

/* ==================== 辅助：协议类型转字符串 ==================== */

static const char *proto_to_string(gw_fwd_proto_t proto)
{
    static const char *names[] = {"A2A", "MCP", "OpenAI", "JSON-RPC"};
    if (proto < GW_FWD_PROTO_COUNT)
        return names[proto];
    return "Unknown";
}

static const char *proto_to_target_daemon(gw_forward_t *fw, gw_fwd_proto_t proto)
{
    switch (proto) {
    case GW_FWD_PROTO_A2A:
        return fw->config.a2a_target_daemon;
    case GW_FWD_PROTO_MCP:
        return fw->config.mcp_target_daemon;
    case GW_FWD_PROTO_OPENAI:
        return fw->config.openai_target_daemon;
    case GW_FWD_PROTO_JSONRPC:
        return fw->config.jsonrpc_target_daemon;
    default:
        return "unknown";
    }
}

static const char *proto_to_channel(gw_forward_t *fw, gw_fwd_proto_t proto)
{
    switch (proto) {
    case GW_FWD_PROTO_A2A:
        return fw->config.a2a_channel;
    case GW_FWD_PROTO_MCP:
        return fw->config.mcp_channel;
    case GW_FWD_PROTO_OPENAI:
        return fw->config.openai_channel;
    case GW_FWD_PROTO_JSONRPC:
        return fw->config.jsonrpc_channel;
    default:
        return "default";
    }
}

/* ==================== 生命周期 ==================== */

gw_forward_t *gw_forward_create(const gw_forward_config_t *config)
{
    gw_forward_t *fw = (gw_forward_t *)AGENTOS_CALLOC(1, sizeof(gw_forward_t));
    if (!fw) {
        SVC_LOG_ERROR("C-L11: gw_forward_create OOM");
        return NULL;
    }

    if (config) {
        fw->config = *config;
    } else {
        fw->config = (gw_forward_config_t)GW_FORWARD_CONFIG_DEFAULTS;
    }

    /* 初始化 IPC Bus helper */
    fw->ipc_helper = ipc_bus_helper_init("gateway_d", NULL);
    if (!fw->ipc_helper) {
        SVC_LOG_ERROR("C-L11: Failed to init IPC Bus helper for gateway forwarding");
        AGENTOS_FREE(fw);
        return NULL;
    }

    __builtin_memset(&fw->stats, 0, sizeof(fw->stats));
    fw->initialized = true;
    fw->healthy = true;

    SVC_LOG_INFO("C-L11: Gateway forwarder created "
                 "(A2A→%s/%s, MCP→%s/%s, OpenAI→%s/%s, JSONRPC→%s/%s)",
                 fw->config.a2a_target_daemon, fw->config.a2a_channel,
                 fw->config.mcp_target_daemon, fw->config.mcp_channel,
                 fw->config.openai_target_daemon, fw->config.openai_channel,
                 fw->config.jsonrpc_target_daemon, fw->config.jsonrpc_channel);
    return fw;
}

void gw_forward_destroy(gw_forward_t *fw)
{
    if (!fw)
        return;
    if (fw->ipc_helper) {
        ipc_bus_helper_shutdown(fw->ipc_helper);
    }
    fw->initialized = false;
    fw->healthy = false;
    AGENTOS_FREE(fw);
    SVC_LOG_INFO("C-L11: Gateway forwarder destroyed");
}

/* ==================== 协议检测 ==================== */

gw_fwd_proto_t gw_forward_detect_proto(const char *content_type, const char *path,
                                       const char *body, size_t body_len)
{
    (void)body_len;

    /* 1. Path-based detection (highest priority) */
    if (path) {
        if (strncmp(path, "/a2a", 4) == 0 || strncmp(path, "/agent/", 7) == 0)
            return GW_FWD_PROTO_A2A;
        if (strncmp(path, "/mcp", 4) == 0 || strncmp(path, "/tools/", 7) == 0)
            return GW_FWD_PROTO_MCP;
        if (strncmp(path, "/v1/chat", 8) == 0 || strncmp(path, "/v1/completions", 15) == 0 ||
            strncmp(path, "/openai/", 8) == 0)
            return GW_FWD_PROTO_OPENAI;
        if (strncmp(path, "/rpc", 4) == 0 || strncmp(path, "/api/", 5) == 0)
            return GW_FWD_PROTO_JSONRPC;
    }

    /* 2. Content-Type hint */
    if (content_type) {
        if (strstr(content_type, "application/json")) {
            /* JSON body detection */
            if (body) {
                if (strstr(body, "\"jsonrpc\"") && strstr(body, "\"2.0\"")) {
                    if (strstr(body, "\"tools/") || strstr(body, "\"resources/"))
                        return GW_FWD_PROTO_MCP;
                    return GW_FWD_PROTO_JSONRPC;
                }
                if ((strstr(body, "\"model\"") && strstr(body, "\"messages\"")) ||
                    strstr(body, "\"chat/completions\""))
                    return GW_FWD_PROTO_OPENAI;
                if (strstr(body, "\"a2a\"") || strstr(body, "\"agentCard\"") ||
                    strstr(body, "\"task/delegate\""))
                    return GW_FWD_PROTO_A2A;
            }
            return GW_FWD_PROTO_JSONRPC;
        }
        if (strstr(content_type, "text/event-stream"))
            return GW_FWD_PROTO_OPENAI;
    }

    /* 3. Body-only detection */
    if (body) {
        if (strstr(body, "\"a2a\"") || strstr(body, "\"agentCard\""))
            return GW_FWD_PROTO_A2A;
        if (strstr(body, "\"tools/") || strstr(body, "\"resources/"))
            return GW_FWD_PROTO_MCP;
        if (strstr(body, "\"model\"") && strstr(body, "\"messages\""))
            return GW_FWD_PROTO_OPENAI;
        if (strstr(body, "\"jsonrpc\"") && strstr(body, "\"2.0\""))
            return GW_FWD_PROTO_JSONRPC;
    }

    return GW_FWD_PROTO_JSONRPC; /* default */
}

/* ==================== 构建 JSON-RPC 转发消息 ==================== */

static char *build_jsonrpc_forward(const char *method, const char *path, const char *body,
                                   size_t body_len)
{
    (void)body_len;
    size_t buf_size = 4096 + (body ? strlen(body) : 0);
    char *msg = (char *)AGENTOS_MALLOC(buf_size);
    if (!msg)
        return NULL;

    int written;
    if (body && body[0] == '{') {
        /* 如果 body 已是 JSON，将其包装为 JSON-RPC params */
        written = snprintf(msg, buf_size,
                           "{\"jsonrpc\":\"2.0\",\"method\":\"%s\","
                           "\"params\":{\"path\":\"%s\",\"body\":%s},\"id\":1}",
                           "gateway.forward", path ? path : "/", body);
    } else {
        written = snprintf(msg, buf_size,
                           "{\"jsonrpc\":\"2.0\",\"method\":\"%s\","
                           "\"params\":{\"path\":\"%s\",\"body\":\"%s\"},\"id\":1}",
                           "gateway.forward", path ? path : "/", body ? body : "");
    }

    if (written < 0 || (size_t)written >= buf_size) {
        AGENTOS_FREE(msg);
        return NULL;
    }

    return msg;
}

/* ==================== 通用转发实现 ==================== */

static int do_forward(gw_forward_t *fw, gw_fwd_proto_t proto, const char *target_daemon,
                      const char *channel, const char *method, const char *path,
                      const char *body, size_t body_len, char **out_response,
                      size_t *out_response_len)
{
    if (!fw || !out_response || !out_response_len) {
        SVC_LOG_ERROR("C-L11: do_forward invalid params");
        return -1;
    }

    uint64_t start_us = agentos_time_ns() / 1000;

    /* 构建 JSON-RPC 转发消息 */
    char *jsonrpc_msg = build_jsonrpc_forward(method, path, body, body_len);
    if (!jsonrpc_msg) {
        SVC_LOG_ERROR("C-L11: Failed to build JSON-RPC forward message for %s",
                      proto_to_string(proto));
        fw->stats.forward_errors++;
        fw->healthy = false;
        return -1;
    }

    SVC_LOG_DEBUG("C-L11: Forwarding %s request to '%s' via channel '%s' path=%s",
                  proto_to_string(proto), target_daemon, channel, path ? path : "/");

    /* 通过 IPC Bus 发送请求到目标 daemon */
    ipc_bus_message_t *req = ipc_bus_message_create(IPC_BUS_MSG_REQUEST,
                                                     IPC_BUS_PROTO_JSON_RPC,
                                                     jsonrpc_msg,
                                                     strlen(jsonrpc_msg));
    AGENTOS_FREE(jsonrpc_msg);

    if (!req) {
        SVC_LOG_ERROR("C-L11: Failed to create IPC request message for %s",
                      proto_to_string(proto));
        fw->stats.forward_errors++;
        fw->healthy = false;
        return -1;
    }

    safe_strcpy(req->header.target, target_daemon, sizeof(req->header.target));

    ipc_bus_message_t resp;
    __builtin_memset(&resp, 0, sizeof(resp));

    int ret = ipc_bus_helper_request(fw->ipc_helper, target_daemon, req, &resp,
                                     fw->config.request_timeout_ms);
    ipc_bus_message_free(req);

    if (ret != 0 || !resp.payload) {
        SVC_LOG_WARN("C-L11: Forward to '%s' failed (ret=%d, proto=%s)",
                     target_daemon, ret, proto_to_string(proto));
        fw->stats.forward_errors++;
        if (ret == -2) { /* timeout */
            fw->stats.timeout_errors++;
        }

        /* 返回错误响应 */
        const char *err_fmt = "{\"error\":{\"code\":%d,\"message\":\"Forward to %s failed\"}}";
        size_t err_len = snprintf(NULL, 0, err_fmt, ret, target_daemon) + 1;
        char *err_resp = (char *)AGENTOS_MALLOC(err_len);
        if (err_resp) {
            snprintf(err_resp, err_len, err_fmt, ret, target_daemon);
            *out_response = err_resp;
            *out_response_len = strlen(err_resp);
        }
        return -1;
    }

    /* 成功：返回目标 daemon 的响应 */
    *out_response = (char *)resp.payload;
    *out_response_len = resp.payload_size;

    uint64_t latency_us = agentos_time_ns() / 1000 - start_us;

    fw->stats.total_forwarded++;
    if (proto < GW_FWD_PROTO_COUNT)
        fw->stats.by_proto[proto]++;

    /* 更新平均延迟 */
    if (fw->stats.total_forwarded > 1) {
        fw->stats.avg_latency_us =
            (fw->stats.avg_latency_us + latency_us) / 2;
    } else {
        fw->stats.avg_latency_us = latency_us;
    }

    SVC_LOG_DEBUG("C-L11: Forward to '%s' succeeded (proto=%s, latency=%lluus)",
                  target_daemon, proto_to_string(proto), (unsigned long long)latency_us);

    return 0;
}

/* ==================== 公共转发 API ==================== */

int gw_forward_request(gw_forward_t *fw, gw_fwd_proto_t proto, const char *method,
                       const char *path, const char *body, size_t body_len,
                       char **out_response, size_t *out_response_len)
{
    if (!fw || !fw->initialized) {
        SVC_LOG_ERROR("C-L11: gw_forward_request: forwarder not initialized");
        return -1;
    }

    const char *target = proto_to_target_daemon(fw, proto);
    const char *channel = proto_to_channel(fw, proto);

    SVC_LOG_INFO("C-L11: %s request routed → %s daemon (channel=%s, path=%s)",
                 proto_to_string(proto), target, channel, path ? path : "/");

    return do_forward(fw, proto, target, channel, method, path, body, body_len,
                      out_response, out_response_len);
}

int gw_forward_a2a(gw_forward_t *fw, const char *method, const char *path, const char *body,
                   size_t body_len, char **out_response, size_t *out_response_len)
{
    return gw_forward_request(fw, GW_FWD_PROTO_A2A, method, path, body, body_len,
                              out_response, out_response_len);
}

int gw_forward_mcp(gw_forward_t *fw, const char *method, const char *path, const char *body,
                   size_t body_len, char **out_response, size_t *out_response_len)
{
    return gw_forward_request(fw, GW_FWD_PROTO_MCP, method, path, body, body_len,
                              out_response, out_response_len);
}

int gw_forward_openai(gw_forward_t *fw, const char *method, const char *path, const char *body,
                      size_t body_len, char **out_response, size_t *out_response_len)
{
    return gw_forward_request(fw, GW_FWD_PROTO_OPENAI, method, path, body, body_len,
                              out_response, out_response_len);
}

/* ==================== 统计 ==================== */

int gw_forward_get_stats(gw_forward_t *fw, gw_forward_stats_t *stats)
{
    if (!fw || !stats)
        return -1;
    *stats = fw->stats;
    return 0;
}

void gw_forward_reset_stats(gw_forward_t *fw)
{
    if (fw)
        __builtin_memset(&fw->stats, 0, sizeof(fw->stats));
}

bool gw_forward_is_healthy(gw_forward_t *fw)
{
    return fw && fw->initialized && fw->healthy;
}