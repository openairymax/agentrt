/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
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
    gw_fwd_proto_t detected = GW_FWD_PROTO_JSONRPC;
    const char *detection_method = "default";

    /* 1. Path-based detection (highest priority) */
    if (path) {
        if (strncmp(path, "/a2a", 4) == 0 || strncmp(path, "/agent/", 7) == 0) {
            detected = GW_FWD_PROTO_A2A;
            detection_method = "path";
            goto log_detection;
        }
        if (strncmp(path, "/mcp", 4) == 0 || strncmp(path, "/tools/", 7) == 0) {
            detected = GW_FWD_PROTO_MCP;
            detection_method = "path";
            goto log_detection;
        }
        if (strncmp(path, "/v1/chat", 8) == 0 || strncmp(path, "/v1/completions", 15) == 0 ||
            strncmp(path, "/openai/", 8) == 0) {
            detected = GW_FWD_PROTO_OPENAI;
            detection_method = "path";
            goto log_detection;
        }
        if (strncmp(path, "/rpc", 4) == 0 || strncmp(path, "/api/", 5) == 0) {
            detected = GW_FWD_PROTO_JSONRPC;
            detection_method = "path";
            goto log_detection;
        }
    }

    /* 2. Content-Type hint */
    if (content_type) {
        if (strstr(content_type, "application/json")) {
            /* JSON body detection */
            if (body) {
                if (strstr(body, "\"jsonrpc\"") && strstr(body, "\"2.0\"")) {
                    if (strstr(body, "\"tools/") || strstr(body, "\"resources/")) {
                        detected = GW_FWD_PROTO_MCP;
                        detection_method = "content-type+body(mcp)";
                        goto log_detection;
                    }
                    detected = GW_FWD_PROTO_JSONRPC;
                    detection_method = "content-type+body(jsonrpc)";
                    goto log_detection;
                }
                if ((strstr(body, "\"model\"") && strstr(body, "\"messages\"")) ||
                    strstr(body, "\"chat/completions\"")) {
                    detected = GW_FWD_PROTO_OPENAI;
                    detection_method = "content-type+body(openai)";
                    goto log_detection;
                }
                if (strstr(body, "\"a2a\"") || strstr(body, "\"agentCard\"") ||
                    strstr(body, "\"task/delegate\"")) {
                    detected = GW_FWD_PROTO_A2A;
                    detection_method = "content-type+body(a2a)";
                    goto log_detection;
                }
            }
            detected = GW_FWD_PROTO_JSONRPC;
            detection_method = "content-type(fallback)";
            goto log_detection;
        }
        if (strstr(content_type, "text/event-stream")) {
            detected = GW_FWD_PROTO_OPENAI;
            detection_method = "content-type(sse)";
            goto log_detection;
        }
    }

    /* 3. Body-only detection */
    if (body) {
        if (strstr(body, "\"a2a\"") || strstr(body, "\"agentCard\"")) {
            detected = GW_FWD_PROTO_A2A;
            detection_method = "body(a2a)";
        } else if (strstr(body, "\"tools/") || strstr(body, "\"resources/")) {
            detected = GW_FWD_PROTO_MCP;
            detection_method = "body(mcp)";
        } else if (strstr(body, "\"model\"") && strstr(body, "\"messages\"")) {
            detected = GW_FWD_PROTO_OPENAI;
            detection_method = "body(openai)";
        } else if (strstr(body, "\"jsonrpc\"") && strstr(body, "\"2.0\"")) {
            detected = GW_FWD_PROTO_JSONRPC;
            detection_method = "body(jsonrpc)";
        } else {
            detection_method = "default";
        }
    }

log_detection:
    SVC_LOG_DEBUG("C-L11: PROTO-DETECT method=%s result=%s path=%s ct=%s body_len=%zu",
                  detection_method, proto_to_string(detected),
                  path ? path : "(none)",
                  content_type ? content_type : "(none)",
                  body_len);
    return detected;
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
        SVC_LOG_ERROR("C-L11: do_forward invalid params (fw=%p out_resp=%p out_len=%p)",
                      (void *)fw, (void *)out_response, (void *)out_response_len);
        return -1;
    }

    uint64_t start_us = agentos_time_ns() / 1000;

    /* 构建 JSON-RPC 转发消息 */
    char *jsonrpc_msg = build_jsonrpc_forward(method, path, body, body_len);
    if (!jsonrpc_msg) {
        SVC_LOG_ERROR("C-L11: Failed to build JSON-RPC forward message for %s (path=%s body_len=%zu)",
                      proto_to_string(proto), path ? path : "/", body_len);
        fw->stats.forward_errors++;
        fw->healthy = false;
        return -1;
    }

    size_t msg_len = strlen(jsonrpc_msg);

    SVC_LOG_DEBUG("C-L11: FORWARD [%s] → target=%s channel=%s method=%s path=%s "
                  "body_len=%zu msg_len=%zu",
                  proto_to_string(proto), target_daemon, channel,
                  method ? method : "POST", path ? path : "/",
                  body_len, msg_len);

    /* 通过 IPC Bus 发送请求到目标 daemon */
    ipc_bus_message_t *req = ipc_bus_message_create(IPC_BUS_MSG_REQUEST,
                                                     IPC_BUS_PROTO_JSON_RPC,
                                                     jsonrpc_msg,
                                                     msg_len);
    AGENTOS_FREE(jsonrpc_msg);

    if (!req) {
        SVC_LOG_ERROR("C-L11: Failed to create IPC request message for %s → %s",
                      proto_to_string(proto), target_daemon);
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
        const char *err_reason;
        if (ret == -2) {
            err_reason = "timeout";
            fw->stats.timeout_errors++;
        } else if (ret == -3) {
            err_reason = "backpressure";
        } else if (ret == -4) {
            err_reason = "service_unavailable";
        } else {
            err_reason = "unknown";
        }
        SVC_LOG_WARN("C-L11: FORWARD-FAIL [%s] → %s ret=%d reason=%s timeout=%ums",
                     proto_to_string(proto), target_daemon, ret, err_reason,
                     fw->config.request_timeout_ms);
        fw->stats.forward_errors++;

        /* 返回错误响应 */
        const char *err_fmt = "{\"error\":{\"code\":%d,\"message\":\"Forward to %s failed: %s\"}}";
        size_t err_len = snprintf(NULL, 0, err_fmt, ret, target_daemon, err_reason) + 1;
        char *err_resp = (char *)AGENTOS_MALLOC(err_len);
        if (err_resp) {
            snprintf(err_resp, err_len, err_fmt, ret, target_daemon, err_reason);
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

    /* C-L11: 更新吞吐量统计 */
    fw->stats.body_size_total += body_len;
    fw->stats.response_size_total += resp.payload_size;
    fw->stats.last_latency_us = latency_us;
    fw->stats.last_forward_time = (uint64_t)time(NULL);

    /* 更新平均延迟 */
    if (fw->stats.total_forwarded > 1) {
        fw->stats.avg_latency_us =
            (fw->stats.avg_latency_us * (fw->stats.total_forwarded - 1) + latency_us)
            / fw->stats.total_forwarded;
    } else {
        fw->stats.avg_latency_us = latency_us;
    }

    /* 更新最大/最小延迟 */
    if (latency_us > fw->stats.max_latency_us)
        fw->stats.max_latency_us = latency_us;
    if (fw->stats.min_latency_us == 0 || latency_us < fw->stats.min_latency_us)
        fw->stats.min_latency_us = latency_us;

    SVC_LOG_DEBUG("C-L11: FORWARD-OK [%s] → %s latency=%lluus resp_len=%zu "
                  "(total=%llu avg=%lluus min=%lluus max=%lluus)",
                  proto_to_string(proto), target_daemon,
                  (unsigned long long)latency_us, resp.payload_size,
                  (unsigned long long)fw->stats.total_forwarded,
                  (unsigned long long)fw->stats.avg_latency_us,
                  (unsigned long long)fw->stats.min_latency_us,
                  (unsigned long long)fw->stats.max_latency_us);

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
    if (!fw || !fw->initialized)
        return false;

    if (!fw->healthy) {
        SVC_LOG_DEBUG("C-L11: HEALTH-CHECK unhealthy — total=%llu errors=%llu timeouts=%llu",
                      (unsigned long long)fw->stats.total_forwarded,
                      (unsigned long long)fw->stats.forward_errors,
                      (unsigned long long)fw->stats.timeout_errors);
        return false;
    }
    return true;
}

void gw_forward_dump_stats(gw_forward_t *fw, uint32_t interval_sec)
{
    if (!fw || !fw->initialized) {
        SVC_LOG_WARN("C-L11: STATS unavailable (forwarder not initialized)");
        return;
    }

    gw_forward_stats_t *s = &fw->stats;
    uint64_t throughput = interval_sec > 0 ? s->total_forwarded / interval_sec : 0;

    SVC_LOG_INFO("C-L11: STATS total=%llu "
                 "(A2A=%llu MCP=%llu OpenAI=%llu JSONRPC=%llu) "
                 "errors=%llu timeouts=%llu "
                 "latency=avg=%llu/max=%llu/min=%llu us "
                 "throughput=%llu req/s "
                 "body=%llu resp=%llu bytes",
                 (unsigned long long)s->total_forwarded,
                 (unsigned long long)s->by_proto[GW_FWD_PROTO_A2A],
                 (unsigned long long)s->by_proto[GW_FWD_PROTO_MCP],
                 (unsigned long long)s->by_proto[GW_FWD_PROTO_OPENAI],
                 (unsigned long long)s->by_proto[GW_FWD_PROTO_JSONRPC],
                 (unsigned long long)s->forward_errors,
                 (unsigned long long)s->timeout_errors,
                 (unsigned long long)s->avg_latency_us,
                 (unsigned long long)s->max_latency_us,
                 (unsigned long long)s->min_latency_us,
                 (unsigned long long)throughput,
                 (unsigned long long)s->body_size_total,
                 (unsigned long long)s->response_size_total);
}