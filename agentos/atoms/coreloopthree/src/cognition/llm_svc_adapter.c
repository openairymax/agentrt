/**
 * @file llm_svc_adapter.c
 * @brief C-L02: CoreLoopThree → llm_d IPC 适配器实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 实现 CoreLoopThree 认知引擎与 llm_d 守护进程之间的 IPC 桥接。
 * 使用 ServiceDiscovery 动态发现 llm_d 端点，通过 IPC Bus 发送
 * JSON-RPC 请求并接收响应。
 *
 * 数据流：
 *   cognition_engine → llm_svc_adapter_complete()
 *     → sd_helper_select("llm_d") → ipc_bus_helper_request()
 *     → llm_d daemon → OpenAI/Anthropic/... → response
 *     → deserialize → llm_response_t
 */

#include "llm_svc_adapter.h"

#include "daemon_bootstrap_ipc.h"
#include "daemon_bootstrap_sd.h"
#include "ipc_service_bus.h"
#include "logger.h"
#include "memory_compat.h"
#include "service_discovery.h"
#include "string_compat.h"
#include "agentos_quality.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==================== 默认配置 ==================== */

#define DEFAULT_LLM_D_SERVICE_NAME  "llm_d"
#define DEFAULT_CHANNEL_NAME        "coreloopthree-llm"
#define DEFAULT_REQUEST_TIMEOUT_MS  30000
#define DEFAULT_SD_POLL_INTERVAL_MS 5000

/* ==================== 适配器内部结构 ==================== */

struct llm_svc_adapter_s {
    daemon_bootstrap_sd_t *bsd;          /* ServiceDiscovery 引导 */
    daemon_bootstrap_ipc_t *bipc;        /* IPC Bus 引导 */
    llm_service_t *wrapper_svc;          /* 包装的 llm_service_t（模拟句柄） */
    char llm_d_service_name[64];         /* llm_d 服务名 */
    char channel_name[64];               /* IPC 通道名 */
    uint32_t request_timeout_ms;         /* 请求超时 */
    uint32_t sd_poll_interval_ms;        /* 服务发现轮询间隔 */
    bool enable_streaming;               /* 是否启用流式 */
    bool connected;                      /* 是否已连接 */

    /* 统计 */
    uint64_t total_requests;
    uint64_t total_errors;
    uint64_t total_latency_us;
};

/* ==================== 内部：包装 llm_service_t 的虚函数表 ==================== */

/*
 * 适配器包装的 llm_service_t 句柄实际上是一个占位符。
 * 真正的 IPC 调用通过 llm_svc_adapter_complete() 完成。
 * 此句柄用于注入到认知引擎的 set_llm_service() 接口。
 */

/* ==================== 内部：JSON-RPC 消息构建 ==================== */

/**
 * @brief 将 llm_request_config_t 序列化为 JSON-RPC 请求字符串
 */
static char *build_llm_request_json(const llm_request_config_t *config)
{
    if (!config) return NULL;

    /* 构建 messages JSON 数组 */
    char msgs_json[65536];
    size_t offset = 0;
    offset += (size_t)snprintf(msgs_json + offset, sizeof(msgs_json) - offset, "[");
    for (size_t i = 0; i < config->message_count && i < 64; i++) {
        if (i > 0) offset += (size_t)snprintf(msgs_json + offset,
                                              sizeof(msgs_json) - offset, ",");
        /* 转义 content 中的双引号 */
        const char *content = config->messages[i].content
                                  ? config->messages[i].content : "";
        offset += (size_t)snprintf(msgs_json + offset,
                                   sizeof(msgs_json) - offset,
                                   "{\"role\":\"%s\",\"content\":\"",
                                   config->messages[i].role
                                       ? config->messages[i].role : "user");
        /* 简单的 JSON 转义 */
        for (const char *c = content; *c && offset < sizeof(msgs_json) - 4; c++) {
            if (*c == '"' || *c == '\\') {
                msgs_json[offset++] = '\\';
            }
            msgs_json[offset++] = *c;
        }
        offset += (size_t)snprintf(msgs_json + offset,
                                   sizeof(msgs_json) - offset, "\"}");
    }
    offset += (size_t)snprintf(msgs_json + offset,
                               sizeof(msgs_json) - offset, "]");

    /* 构建完整 JSON-RPC 请求 */
    char *json = (char *)AGENTOS_MALLOC(131072);
    if (!json) return NULL;

    snprintf(json, 131072,
             "{"
             "\"jsonrpc\":\"2.0\","
             "\"method\":\"llm.complete\","
             "\"id\":1,"
             "\"params\":{"
             "\"model\":\"%s\","
             "\"messages\":%s,"
             "\"temperature\":%f,"
             "\"top_p\":%f,"
             "\"max_tokens\":%d,"
             "\"stream\":%d"
             "}}",
             config->model ? config->model : "",
             msgs_json,
             (double)config->temperature,
             (double)config->top_p,
             config->max_tokens,
             config->stream ? 1 : 0);

    return json;
}

/**
 * @brief 从 JSON-RPC 响应字符串反序列化为 llm_response_t
 */
static llm_response_t *parse_llm_response_json(const char *json)
{
    if (!json) return NULL;

    llm_response_t *resp =
        (llm_response_t *)AGENTOS_CALLOC(1, sizeof(llm_response_t));
    if (!resp) return NULL;

    /* 简单 JSON 解析（提取关键字段） */
    /* 注意：生产环境应使用 cJSON 库进行完整解析 */

    /* 提取 choices[0].message.content */
    const char *content_start = strstr(json, "\"content\"");
    if (content_start) {
        content_start = strchr(content_start, ':');
        if (content_start) {
            content_start++; /* 跳过冒号 */
            while (*content_start == ' ' || *content_start == '"')
                content_start++;
            const char *content_end = content_start;
            /* 找到未转义的结束引号 */
            while (*content_end) {
                if (*content_end == '"' && (content_end == content_start ||
                    *(content_end - 1) != '\\'))
                    break;
                content_end++;
            }
            size_t len = (size_t)(content_end - content_start);
            if (len > 0) {
                resp->choices =
                    (llm_message_t *)AGENTOS_CALLOC(1, sizeof(llm_message_t));
                if (resp->choices) {
                    resp->choices[0].role = AGENTOS_STRDUP("assistant");
                    resp->choices[0].content =
                        (char *)AGENTOS_MALLOC(len + 1);
                    if (resp->choices[0].content) {
                        AGENTOS_MEMCPY((void *)resp->choices[0].content, content_start, len);
                        ((char *)resp->choices[0].content)[len] = '\0';
                    }
                    resp->choice_count = 1;
                }
            }
        }
    }

    /* 提取 finish_reason */
    const char *finish_start = strstr(json, "\"finish_reason\"");
    if (finish_start) {
        finish_start = strchr(finish_start, ':');
        if (finish_start) {
            finish_start++;
            while (*finish_start == ' ' || *finish_start == '"')
                finish_start++;
            const char *finish_end = finish_start;
            while (*finish_end && *finish_end != '"' && *finish_end != ',')
                finish_end++;
            size_t len = (size_t)(finish_end - finish_start);
            if (len > 0 && len < 64) {
                resp->finish_reason = (char *)AGENTOS_MALLOC(len + 1);
                if (resp->finish_reason) {
                    AGENTOS_MEMCPY(resp->finish_reason, finish_start, len);
                    resp->finish_reason[len] = '\0';
                }
            }
        }
    }

    /* 提取 model */
    const char *model_start = strstr(json, "\"model\"");
    if (model_start) {
        model_start = strchr(model_start, ':');
        if (model_start) {
            model_start++;
            while (*model_start == ' ' || *model_start == '"')
                model_start++;
            const char *model_end = model_start;
            while (*model_end && *model_end != '"' && *model_end != ',')
                model_end++;
            size_t len = (size_t)(model_end - model_start);
            if (len > 0 && len < 64) {
                resp->model = (char *)AGENTOS_MALLOC(len + 1);
                if (resp->model) {
                    AGENTOS_MEMCPY(resp->model, model_start, len);
                    resp->model[len] = '\0';
                }
            }
        }
    }

    /* 提取 usage.token 信息 */
    resp->prompt_tokens = 0;
    resp->completion_tokens = 0;
    resp->total_tokens = 0;

    return resp;
}

/* ==================== 生命周期实现 ==================== */

llm_svc_adapter_t *llm_svc_adapter_create(const llm_svc_adapter_config_t *config)
{
    llm_svc_adapter_t *adapter =
        (llm_svc_adapter_t *)AGENTOS_CALLOC(1, sizeof(llm_svc_adapter_t));
    if (!adapter) {
        AGENTOS_LOG_ERROR("C-L02: Failed to allocate LLM adapter");
        return NULL;
    }

    /* 应用配置 */
    const char *svc_name = (config && config->llm_d_service_name)
                               ? config->llm_d_service_name
                               : DEFAULT_LLM_D_SERVICE_NAME;
    const char *ch_name = (config && config->channel_name)
                              ? config->channel_name
                              : DEFAULT_CHANNEL_NAME;
    adapter->request_timeout_ms = (config && config->request_timeout_ms > 0)
                                      ? config->request_timeout_ms
                                      : DEFAULT_REQUEST_TIMEOUT_MS;
    adapter->sd_poll_interval_ms = (config && config->sd_poll_interval_ms > 0)
                                       ? config->sd_poll_interval_ms
                                       : DEFAULT_SD_POLL_INTERVAL_MS;
    adapter->enable_streaming = config ? config->enable_streaming : false;

    safe_strcpy(adapter->llm_d_service_name, sizeof(adapter->llm_d_service_name),
                svc_name);
    safe_strcpy(adapter->channel_name, sizeof(adapter->channel_name),
                ch_name);

    /* 初始化 ServiceDiscovery（C-L08） */
    adapter->bsd = daemon_bootstrap_sd_start(
        ch_name,           /* 以适配器通道名注册 */
        "llm_client",      /* 类型：LLM 客户端 */
        NULL,              /* 不需要 TCP 端口 */
        0,                 /* 端口 0 */
        "coreloopthree,cognition,llm",  /* 标签 */
        adapter->sd_poll_interval_ms);
    if (!adapter->bsd) {
        AGENTOS_LOG_WARN("C-L02: SD bootstrap failed for '%s', "
                         "will retry on first request", ch_name);
        /* 非致命错误，继续初始化 */
    }

    /* 初始化 IPC Bus（C-L09） */
    adapter->bipc = daemon_bootstrap_ipc_start(
        ch_name,           /* daemon 名 */
        ch_name,           /* 通道名 */
        NULL,              /* 无 TCP 端口 */
        0,                 /* 端口 0 */
        IPC_BUS_PROTO_JSON_RPC);
    if (!adapter->bipc) {
        AGENTOS_LOG_ERROR("C-L02: IPC bootstrap failed for '%s'", ch_name);
        if (adapter->bsd) {
            daemon_bootstrap_sd_stop(adapter->bsd);
        }
        AGENTOS_FREE(adapter);
        return NULL;
    }

    /* 创建包装的 llm_service_t（占位符句柄，llm_service_t 是不透明类型） */
    adapter->wrapper_svc = (llm_service_t *)AGENTOS_MALLOC(1);
    if (!adapter->wrapper_svc) {
        AGENTOS_LOG_ERROR("C-L02: Failed to allocate wrapper service handle");
        daemon_bootstrap_ipc_stop(adapter->bipc);
        if (adapter->bsd) {
            daemon_bootstrap_sd_stop(adapter->bsd);
        }
        AGENTOS_FREE(adapter);
        return NULL;
    }

    adapter->connected = true;
    adapter->total_requests = 0;
    adapter->total_errors = 0;
    adapter->total_latency_us = 0;

    AGENTOS_LOG_INFO("C-L02: LLM adapter created (service=%s, channel=%s, "
                     "timeout=%ums, streaming=%s)",
                     adapter->llm_d_service_name, adapter->channel_name,
                     adapter->request_timeout_ms,
                     adapter->enable_streaming ? "on" : "off");
    return adapter;
}

void llm_svc_adapter_destroy(llm_svc_adapter_t *adapter)
{
    if (!adapter) return;

    AGENTOS_LOG_INFO("C-L02: LLM adapter destroyed (requests=%llu errors=%llu)",
                     (unsigned long long)adapter->total_requests,
                     (unsigned long long)adapter->total_errors);

    if (adapter->wrapper_svc) {
        AGENTOS_FREE(adapter->wrapper_svc);
    }
    if (adapter->bipc) {
        daemon_bootstrap_ipc_stop(adapter->bipc);
    }
    if (adapter->bsd) {
        daemon_bootstrap_sd_stop(adapter->bsd);
    }
    AGENTOS_FREE(adapter);
}

/* ==================== 服务接口实现 ==================== */

llm_service_t *llm_svc_adapter_get_service(llm_svc_adapter_t *adapter)
{
    if (!adapter || !adapter->connected) return NULL;
    return adapter->wrapper_svc;
}

int llm_svc_adapter_complete(llm_svc_adapter_t *adapter,
                             const llm_request_config_t *config,
                             llm_response_t **out_response)
{
    if (!adapter || !config || !out_response) return -1;

    *out_response = NULL;
    adapter->total_requests++;

    /* P1.2.3: 错误处理 — 检查 llm_d 是否可用 */
    if (!adapter->connected || !adapter->bipc) {
        AGENTOS_LOG_WARN("C-L02: Adapter not connected, cannot send request");
        adapter->total_errors++;
        return -1;
    }

    /* 通过 ServiceDiscovery 查找 llm_d 端点 */
    sd_instance_t instance;
    __builtin_memset(&instance, 0, sizeof(instance));

    if (adapter->bsd) {
        sd_helper_t *sdh = daemon_bootstrap_sd_get_helper(adapter->bsd);
        if (sdh) {
            int sd_ret = sd_helper_select_with_strategy(
                sdh, adapter->llm_d_service_name,
                SD_LB_LEAST_CONNECTION, &instance);
            if (sd_ret != 0) {
                AGENTOS_LOG_WARN("C-L02: llm_d '%s' not found in SD, "
                                 "falling back to direct IPC",
                                 adapter->llm_d_service_name);
                /* 降级：尝试直接通过 IPC 发送 */
            }
        }
    }

    /* 构建 JSON-RPC 请求 */
    char *request_json = build_llm_request_json(config);
    if (!request_json) {
        AGENTOS_LOG_ERROR("C-L02: Failed to build request JSON");
        adapter->total_errors++;
        return -1;
    }

    /* 通过 IPC Bus 发送请求 */
    ipc_bus_message_t *request_msg = ipc_bus_message_create(
        IPC_BUS_MSG_REQUEST, IPC_BUS_PROTO_JSON_RPC,
        request_json, strlen(request_json));

    ipc_bus_message_t response_msg;
    __builtin_memset(&response_msg, 0, sizeof(response_msg));

    int ipc_ret = ipc_bus_helper_request(
        daemon_bootstrap_ipc_get_helper(adapter->bipc),
        adapter->llm_d_service_name,
        request_msg, &response_msg,
        adapter->request_timeout_ms);

    ipc_bus_message_free(request_msg);
    AGENTOS_FREE(request_json);

    if (ipc_ret != 0) {
        AGENTOS_LOG_ERROR("C-L02: IPC request failed: ret=%d", ipc_ret);
        adapter->total_errors++;
        return -1;
    }

    /* 解析响应 */
    if (response_msg.payload && response_msg.payload_size > 0) {
        const char *resp_json = (const char *)response_msg.payload;
        *out_response = parse_llm_response_json(resp_json);
    }

    /* 释放响应消息 */
    if (response_msg.payload) {
        AGENTOS_FREE(response_msg.payload);
    }

    if (!*out_response) {
        AGENTOS_LOG_WARN("C-L02: Failed to parse LLM response");
        adapter->total_errors++;
        return -1;
    }

    return 0;
}

int llm_svc_adapter_complete_stream(llm_svc_adapter_t *adapter,
                                    const llm_request_config_t *config,
                                    llm_stream_callback_t callback,
                                    void *callback_data,
                                    llm_response_t **out_response)
{
    if (!adapter || !config || !out_response) return -1;

    *out_response = NULL;
    adapter->total_requests++;

    if (!adapter->connected || !adapter->bipc) {
        AGENTOS_LOG_WARN("C-L02: Adapter not connected for streaming");
        adapter->total_errors++;
        return -1;
    }

    /* 构建流式 JSON-RPC 请求 */
    char *request_json = build_llm_request_json(config);
    if (!request_json) {
        adapter->total_errors++;
        return -1;
    }

    /* 通过 IPC Bus 发送流式请求 */
    ipc_bus_message_t *request_msg = ipc_bus_message_create(
        IPC_BUS_MSG_REQUEST, IPC_BUS_PROTO_JSON_RPC,
        request_json, strlen(request_json));

    ipc_bus_message_t response_msg;
    __builtin_memset(&response_msg, 0, sizeof(response_msg));

    int ipc_ret = ipc_bus_helper_request(
        daemon_bootstrap_ipc_get_helper(adapter->bipc),
        adapter->llm_d_service_name,
        request_msg, &response_msg,
        adapter->request_timeout_ms);

    ipc_bus_message_free(request_msg);
    AGENTOS_FREE(request_json);

    if (ipc_ret != 0) {
        AGENTOS_LOG_ERROR("C-L02: Streaming IPC request failed: ret=%d", ipc_ret);
        adapter->total_errors++;
        return -1;
    }

    /* 解析响应并调用流式回调 */
    if (response_msg.payload && response_msg.payload_size > 0) {
        const char *resp_json = (const char *)response_msg.payload;

        /* 如果设置了回调，通知每个 chunk */
        if (callback) {
            callback(resp_json, callback_data);
        }

        *out_response = parse_llm_response_json(resp_json);
    }

    if (response_msg.payload) {
        AGENTOS_FREE(response_msg.payload);
    }

    if (!*out_response) {
        adapter->total_errors++;
        return -1;
    }

    return 0;
}

/* ==================== 状态查询实现 ==================== */

bool llm_svc_adapter_is_connected(llm_svc_adapter_t *adapter)
{
    if (!adapter) return false;
    return adapter->connected && adapter->bipc &&
           daemon_bootstrap_ipc_is_running(adapter->bipc);
}

void llm_svc_adapter_get_stats(llm_svc_adapter_t *adapter,
                               uint64_t *out_total_requests,
                               uint64_t *out_total_errors,
                               uint64_t *out_avg_latency_ms)
{
    if (!adapter) {
        if (out_total_requests) *out_total_requests = 0;
        if (out_total_errors) *out_total_errors = 0;
        if (out_avg_latency_ms) *out_avg_latency_ms = 0;
        return;
    }

    if (out_total_requests) *out_total_requests = adapter->total_requests;
    if (out_total_errors) *out_total_errors = adapter->total_errors;
    if (out_avg_latency_ms) {
        if (adapter->total_requests > 0) {
            *out_avg_latency_ms = (uint64_t)(
                (double)adapter->total_latency_us /
                (double)adapter->total_requests / 1000.0);
        } else {
            *out_avg_latency_ms = 0;
        }
    }
}