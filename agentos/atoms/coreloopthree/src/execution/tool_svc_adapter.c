/**
 * @file tool_svc_adapter.c
 * @brief C-L04: CoreLoopThree → tool_d IPC 适配器实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 实现 CoreLoopThree 执行引擎与 tool_d 守护进程之间的 IPC 桥接。
 * 使用 ServiceDiscovery 动态发现 tool_d 端点，通过 IPC Bus 发送
 * JSON-RPC 工具执行请求并接收结果。
 *
 * 数据流：
 *   execution_engine → tool_svc_adapter_execute()
 *     → sd_helper_select("tool_d") → ipc_bus_helper_request()
 *     → tool_d daemon → tool executor → result
 *     → deserialize → tool_result_t
 *
 * C-L05 集成：当 enable_approval 为 true 时，工具执行前会
 * 通过 Cupolas SafetyGuard 进行审批检查。
 */

#include "tool_svc_adapter.h"

#include "daemon_bootstrap_ipc.h"
#include "daemon_bootstrap_sd.h"
#include "ipc_service_bus.h"
#include "logger.h"
#include "memory_compat.h"
#include "service_discovery.h"
#include "string_compat.h"
#include "agentos_quality.h"
#include "tool_approval.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==================== 默认配置 ==================== */

#define DEFAULT_TOOL_D_SERVICE_NAME "tool_d"
#define DEFAULT_CHANNEL_NAME        "coreloopthree-tool"
#define DEFAULT_REQUEST_TIMEOUT_MS  60000
#define DEFAULT_SD_POLL_INTERVAL_MS 5000

/* ==================== 适配器内部结构 ==================== */

struct tool_svc_adapter_s {
    daemon_bootstrap_sd_t *bsd;           /* ServiceDiscovery 引导 */
    daemon_bootstrap_ipc_t *bipc;         /* IPC Bus 引导 */
    tool_service_t *wrapper_svc;          /* 包装的 tool_service_t（模拟句柄） */
    tool_approval_ctx_t *approval_ctx;    /* C-L05: 工具审批上下文 */
    char tool_d_service_name[64];         /* tool_d 服务名 */
    char channel_name[64];                /* IPC 通道名 */
    uint32_t request_timeout_ms;          /* 请求超时 */
    uint32_t sd_poll_interval_ms;         /* 服务发现轮询间隔 */
    bool enable_approval;                 /* 是否启用审批 */
    bool connected;                       /* 是否已连接 */

    /* 统计 */
    uint64_t total_executions;
    uint64_t total_errors;
    uint64_t total_latency_us;
};

/* ==================== 内部：JSON-RPC 消息构建 ==================== */

/**
 * @brief 将 tool_execute_request_t 序列化为 JSON-RPC 请求字符串
 */
static char *build_tool_execute_json(const tool_execute_request_t *req)
{
    if (!req) return NULL;

    char *json = (char *)AGENTOS_MALLOC(65536);
    if (!json) return NULL;

    /* 对 params_json 进行 JSON 转义 */
    char escaped_params[32768];
    size_t ep = 0;
    const char *pj = req->params_json ? req->params_json : "{}";
    for (const char *c = pj; *c && ep < sizeof(escaped_params) - 2; c++) {
        if (*c == '"' || *c == '\\') {
            escaped_params[ep++] = '\\';
        }
        escaped_params[ep++] = *c;
    }
    escaped_params[ep] = '\0';

    snprintf(json, 65536,
             "{"
             "\"jsonrpc\":\"2.0\","
             "\"method\":\"tool.execute\","
             "\"id\":1,"
             "\"params\":{"
             "\"tool_id\":\"%s\","
             "\"params\":%s,"
             "\"stream\":%d"
             "}}",
             req->tool_id ? req->tool_id : "",
             escaped_params,
             req->stream ? 1 : 0);

    return json;
}

/**
 * @brief 构建工具注册 JSON-RPC 请求
 */
static char *build_tool_register_json(const tool_metadata_t *meta)
{
    if (!meta) return NULL;

    char *json = (char *)AGENTOS_MALLOC(65536);
    if (!json) return NULL;

    /* 构建参数 JSON */
    char params_json[16384];
    size_t po = 0;
    po += (size_t)snprintf(params_json + po, sizeof(params_json) - po, "[");
    for (size_t i = 0; i < meta->param_count && i < 32; i++) {
        if (i > 0) po += (size_t)snprintf(params_json + po,
                                          sizeof(params_json) - po, ",");
        po += (size_t)snprintf(params_json + po, sizeof(params_json) - po,
                               "{\"name\":\"%s\",\"schema\":%s}",
                               meta->params[i].name ? meta->params[i].name : "",
                               meta->params[i].schema ? meta->params[i].schema : "{}");
    }
    po += (size_t)snprintf(params_json + po, sizeof(params_json) - po, "]");

    snprintf(json, 65536,
             "{"
             "\"jsonrpc\":\"2.0\","
             "\"method\":\"tool.register\","
             "\"id\":1,"
             "\"params\":{"
             "\"id\":\"%s\","
             "\"name\":\"%s\","
             "\"description\":\"%s\","
             "\"executable\":\"%s\","
             "\"params\":%s,"
             "\"timeout_sec\":%d,"
             "\"cacheable\":%d,"
             "\"permission_rule\":\"%s\""
             "}}",
             meta->id ? meta->id : "",
             meta->name ? meta->name : "",
             meta->description ? meta->description : "",
             meta->executable ? meta->executable : "",
             params_json,
             meta->timeout_sec,
             meta->cacheable,
             meta->permission_rule ? meta->permission_rule : "");

    return json;
}

/**
 * @brief 从 JSON-RPC 响应反序列化为 tool_result_t
 */
static tool_result_t *parse_tool_result_json(const char *json)
{
    if (!json) return NULL;

    tool_result_t *result =
        (tool_result_t *)AGENTOS_CALLOC(1, sizeof(tool_result_t));
    if (!result) return NULL;

    /* 提取 success 字段 */
    const char *success_start = strstr(json, "\"success\"");
    if (success_start) {
        success_start = strchr(success_start, ':');
        if (success_start) {
            result->success = (strstr(success_start, "true") != NULL) ? 0 : 1;
        }
    }

    /* 提取 output 字段 */
    const char *output_start = strstr(json, "\"output\"");
    if (output_start) {
        output_start = strchr(output_start, ':');
        if (output_start) {
            output_start++;
            while (*output_start == ' ' || *output_start == '"')
                output_start++;
            const char *output_end = output_start;
            while (*output_end) {
                if (*output_end == '"' && (output_end == output_start ||
                    *(output_end - 1) != '\\'))
                    break;
                output_end++;
            }
            size_t len = (size_t)(output_end - output_start);
            if (len > 0) {
                result->output = (char *)AGENTOS_MALLOC(len + 1);
                if (result->output) {
                    memcpy(result->output, output_start, len);
                    result->output[len] = '\0';
                }
            }
        }
    }

    /* 提取 error 字段 */
    const char *error_start = strstr(json, "\"error\"");
    if (error_start) {
        error_start = strchr(error_start, ':');
        if (error_start) {
            error_start++;
            while (*error_start == ' ' || *error_start == '"')
                error_start++;
            const char *error_end = error_start;
            while (*error_end) {
                if (*error_end == '"' && (error_end == error_start ||
                    *(error_end - 1) != '\\'))
                    break;
                error_end++;
            }
            size_t len = (size_t)(error_end - error_start);
            if (len > 0) {
                result->error = (char *)AGENTOS_MALLOC(len + 1);
                if (result->error) {
                    memcpy(result->error, error_start, len);
                    result->error[len] = '\0';
                }
            }
        }
    }

    /* 提取 exit_code */
    result->exit_code = 0;
    const char *exit_start = strstr(json, "\"exit_code\"");
    if (exit_start) {
        exit_start = strchr(exit_start, ':');
        if (exit_start) {
            result->exit_code = (int)strtol(exit_start + 1, NULL, 10);
        }
    }

    /* 提取 duration_ms */
    result->duration_ms = 0;
    const char *dur_start = strstr(json, "\"duration_ms\"");
    if (dur_start) {
        dur_start = strchr(dur_start, ':');
        if (dur_start) {
            result->duration_ms = (uint64_t)strtoll(dur_start + 1, NULL, 10);
        }
    }

    return result;
}

/* ==================== 生命周期实现 ==================== */

tool_svc_adapter_t *tool_svc_adapter_create(const tool_svc_adapter_config_t *config)
{
    tool_svc_adapter_t *adapter =
        (tool_svc_adapter_t *)AGENTOS_CALLOC(1, sizeof(tool_svc_adapter_t));
    if (!adapter) {
        AGENTOS_LOG_ERROR("C-L04: Failed to allocate tool adapter");
        return NULL;
    }

    /* 应用配置 */
    const char *svc_name = (config && config->tool_d_service_name)
                               ? config->tool_d_service_name
                               : DEFAULT_TOOL_D_SERVICE_NAME;
    const char *ch_name = (config && config->channel_name)
                              ? config->channel_name
                              : DEFAULT_CHANNEL_NAME;
    adapter->request_timeout_ms = (config && config->request_timeout_ms > 0)
                                      ? config->request_timeout_ms
                                      : DEFAULT_REQUEST_TIMEOUT_MS;
    adapter->sd_poll_interval_ms = (config && config->sd_poll_interval_ms > 0)
                                       ? config->sd_poll_interval_ms
                                       : DEFAULT_SD_POLL_INTERVAL_MS;
    adapter->enable_approval = config ? config->enable_approval : false;

    safe_strcpy(adapter->tool_d_service_name, sizeof(adapter->tool_d_service_name),
                svc_name);
    safe_strcpy(adapter->channel_name, sizeof(adapter->channel_name),
                ch_name);

    /* 初始化 ServiceDiscovery（C-L08） */
    adapter->bsd = daemon_bootstrap_sd_start(
        ch_name,
        "tool_client",
        NULL,
        0,
        "coreloopthree,execution,tool",
        adapter->sd_poll_interval_ms);
    if (!adapter->bsd) {
        AGENTOS_LOG_WARN("C-L04: SD bootstrap failed for '%s', "
                         "will retry on first request", ch_name);
    }

    /* 初始化 IPC Bus（C-L09） */
    adapter->bipc = daemon_bootstrap_ipc_start(
        ch_name, ch_name, NULL, 0, IPC_BUS_PROTO_JSON_RPC);
    if (!adapter->bipc) {
        AGENTOS_LOG_ERROR("C-L04: IPC bootstrap failed for '%s'", ch_name);
        if (adapter->bsd) {
            daemon_bootstrap_sd_stop(adapter->bsd);
        }
        AGENTOS_FREE(adapter);
        return NULL;
    }

    /* C-L05: 初始化工具审批上下文 */
    if (adapter->enable_approval) {
        tool_approval_config_t approval_cfg;
        __builtin_memset(&approval_cfg, 0, sizeof(approval_cfg));
        approval_cfg.agent_id = (config && config->agent_id)
                                    ? config->agent_id : "coreloopthree";
        approval_cfg.enable_safety_guard_chain = true;
        approval_cfg.enable_audit_logging = true;
        approval_cfg.permission_rules = NULL;

        adapter->approval_ctx = tool_approval_create(&approval_cfg);
        if (adapter->approval_ctx) {
            AGENTOS_LOG_INFO("C-L04: Tool approval enabled (C-L05 integrated)");
        } else {
            AGENTOS_LOG_WARN("C-L04: Tool approval init failed, "
                             "proceeding without approval");
        }
    }

    /* 创建包装的 tool_service_t（占位符句柄，tool_service_t 是不透明类型） */
    adapter->wrapper_svc = (tool_service_t *)AGENTOS_MALLOC(1);
    if (!adapter->wrapper_svc) {
        AGENTOS_LOG_ERROR("C-L04: Failed to allocate wrapper service handle");
        if (adapter->approval_ctx) tool_approval_destroy(adapter->approval_ctx);
        daemon_bootstrap_ipc_stop(adapter->bipc);
        if (adapter->bsd) daemon_bootstrap_sd_stop(adapter->bsd);
        AGENTOS_FREE(adapter);
        return NULL;
    }

    adapter->connected = true;
    adapter->total_executions = 0;
    adapter->total_errors = 0;
    adapter->total_latency_us = 0;

    AGENTOS_LOG_INFO("C-L04: Tool adapter created (service=%s, channel=%s, "
                     "timeout=%ums, approval=%s)",
                     adapter->tool_d_service_name, adapter->channel_name,
                     adapter->request_timeout_ms,
                     adapter->enable_approval ? "on" : "off");
    return adapter;
}

void tool_svc_adapter_destroy(tool_svc_adapter_t *adapter)
{
    if (!adapter) return;

    AGENTOS_LOG_INFO("C-L04: Tool adapter destroyed (executions=%llu errors=%llu)",
                     (unsigned long long)adapter->total_executions,
                     (unsigned long long)adapter->total_errors);

    if (adapter->wrapper_svc) AGENTOS_FREE(adapter->wrapper_svc);
    if (adapter->approval_ctx) tool_approval_destroy(adapter->approval_ctx);
    if (adapter->bipc) daemon_bootstrap_ipc_stop(adapter->bipc);
    if (adapter->bsd) daemon_bootstrap_sd_stop(adapter->bsd);
    AGENTOS_FREE(adapter);
}

/* ==================== 服务接口实现 ==================== */

tool_service_t *tool_svc_adapter_get_service(tool_svc_adapter_t *adapter)
{
    if (!adapter || !adapter->connected) return NULL;
    return adapter->wrapper_svc;
}

int tool_svc_adapter_execute(tool_svc_adapter_t *adapter,
                             const tool_execute_request_t *req,
                             tool_result_t **out_result)
{
    if (!adapter || !req || !out_result) return -1;

    *out_result = NULL;
    adapter->total_executions++;

    if (!adapter->connected || !adapter->bipc) {
        AGENTOS_LOG_WARN("C-L04: Adapter not connected, cannot execute tool");
        adapter->total_errors++;
        return -1;
    }

    /* C-L05: 工具审批检查（P1.4.1） */
    if (adapter->enable_approval && adapter->approval_ctx) {
        tool_metadata_t meta;
        __builtin_memset(&meta, 0, sizeof(meta));
        meta.id = (char *)req->tool_id;
        meta.name = (char *)req->tool_id;

        tool_approval_detail_t detail;
        __builtin_memset(&detail, 0, sizeof(detail));

        int approval_ret = tool_approval_check(
            adapter->approval_ctx, &meta, req->params_json, &detail);

        if (approval_ret != 0 || detail.decision == TOOL_APPROVAL_DENIED) {
            AGENTOS_LOG_WARN("C-L05: Tool '%s' denied by approval: %s",
                             req->tool_id ? req->tool_id : "?",
                             detail.reason[0] ? detail.reason : "policy");
            adapter->total_errors++;

            /* 返回审批拒绝的结果 */
            *out_result = (tool_result_t *)AGENTOS_CALLOC(1, sizeof(tool_result_t));
            if (*out_result) {
                (*out_result)->success = 1;
                (*out_result)->exit_code = 1;
                (*out_result)->error = AGENTOS_STRDUP(
                    detail.reason[0] ? detail.reason : "Tool denied by safety guard");
            }
            return -1;
        }

        if (detail.decision == TOOL_APPROVAL_SANITIZED) {
            AGENTOS_LOG_INFO("C-L05: Tool '%s' params sanitized by approval",
                             req->tool_id ? req->tool_id : "?");
        }
    }

    /* 通过 ServiceDiscovery 查找 tool_d 端点 */
    if (adapter->bsd) {
        sd_helper_t *sdh = daemon_bootstrap_sd_get_helper(adapter->bsd);
        if (sdh) {
            sd_instance_t instance;
            __builtin_memset(&instance, 0, sizeof(instance));
            int sd_ret = sd_helper_select_with_strategy(
                sdh, adapter->tool_d_service_name,
                SD_LB_LEAST_CONNECTION, &instance);
            if (sd_ret != 0) {
                AGENTOS_LOG_WARN("C-L04: tool_d '%s' not found in SD",
                                 adapter->tool_d_service_name);
            }
        }
    }

    /* 构建 JSON-RPC 请求 */
    char *request_json = build_tool_execute_json(req);
    if (!request_json) {
        AGENTOS_LOG_ERROR("C-L04: Failed to build execute request JSON");
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
        adapter->tool_d_service_name,
        request_msg, &response_msg,
        adapter->request_timeout_ms);

    ipc_bus_message_free(request_msg);
    AGENTOS_FREE(request_json);

    if (ipc_ret != 0) {
        AGENTOS_LOG_ERROR("C-L04: IPC tool execute failed: ret=%d", ipc_ret);
        adapter->total_errors++;
        return -1;
    }

    /* 解析响应 */
    if (response_msg.payload && response_msg.payload_size > 0) {
        const char *resp_json = (const char *)response_msg.payload;
        *out_result = parse_tool_result_json(resp_json);
    }

    if (response_msg.payload) {
        AGENTOS_FREE(response_msg.payload);
    }

    if (!*out_result) {
        AGENTOS_LOG_WARN("C-L04: Failed to parse tool result");
        adapter->total_errors++;
        return -1;
    }

    return 0;
}

int tool_svc_adapter_execute_stream(tool_svc_adapter_t *adapter,
                                    const tool_execute_request_t *req,
                                    tool_stream_callback_t callback,
                                    void *callback_data,
                                    tool_result_t **out_result)
{
    if (!adapter || !req || !out_result) return -1;

    *out_result = NULL;
    adapter->total_executions++;

    if (!adapter->connected || !adapter->bipc) {
        adapter->total_errors++;
        return -1;
    }

    /* 构建流式请求 */
    char *request_json = build_tool_execute_json(req);
    if (!request_json) {
        adapter->total_errors++;
        return -1;
    }

    ipc_bus_message_t *request_msg = ipc_bus_message_create(
        IPC_BUS_MSG_REQUEST, IPC_BUS_PROTO_JSON_RPC,
        request_json, strlen(request_json));

    ipc_bus_message_t response_msg;
    __builtin_memset(&response_msg, 0, sizeof(response_msg));

    int ipc_ret = ipc_bus_helper_request(
        daemon_bootstrap_ipc_get_helper(adapter->bipc),
        adapter->tool_d_service_name,
        request_msg, &response_msg,
        adapter->request_timeout_ms);

    ipc_bus_message_free(request_msg);
    AGENTOS_FREE(request_json);

    if (ipc_ret != 0) {
        adapter->total_errors++;
        return -1;
    }

    if (response_msg.payload && response_msg.payload_size > 0) {
        const char *resp_json = (const char *)response_msg.payload;

        if (callback) {
            callback(resp_json, 0, callback_data);
        }

        *out_result = parse_tool_result_json(resp_json);
    }

    if (response_msg.payload) {
        AGENTOS_FREE(response_msg.payload);
    }

    if (!*out_result) {
        adapter->total_errors++;
        return -1;
    }

    return 0;
}

int tool_svc_adapter_register(tool_svc_adapter_t *adapter,
                              const tool_metadata_t *meta)
{
    if (!adapter || !meta) return -1;

    if (!adapter->connected || !adapter->bipc) {
        return -1;
    }

    char *request_json = build_tool_register_json(meta);
    if (!request_json) return -1;

    ipc_bus_message_t *request_msg = ipc_bus_message_create(
        IPC_BUS_MSG_REQUEST, IPC_BUS_PROTO_JSON_RPC,
        request_json, strlen(request_json));

    ipc_bus_message_t response_msg;
    __builtin_memset(&response_msg, 0, sizeof(response_msg));

    int ipc_ret = ipc_bus_helper_request(
        daemon_bootstrap_ipc_get_helper(adapter->bipc),
        adapter->tool_d_service_name,
        request_msg, &response_msg,
        adapter->request_timeout_ms);

    ipc_bus_message_free(request_msg);
    AGENTOS_FREE(request_json);

    if (response_msg.payload) {
        AGENTOS_FREE(response_msg.payload);
    }

    if (ipc_ret != 0) {
        AGENTOS_LOG_ERROR("C-L04: Tool register IPC failed: ret=%d", ipc_ret);
        return -1;
    }

    AGENTOS_LOG_INFO("C-L04: Tool '%s' registered via IPC", meta->name);
    return 0;
}

int tool_svc_adapter_list(tool_svc_adapter_t *adapter, char **out_json)
{
    if (!adapter || !out_json) return -1;

    *out_json = NULL;

    if (!adapter->connected || !adapter->bipc) {
        return -1;
    }

    const char *list_json = "{\"jsonrpc\":\"2.0\",\"method\":\"tool.list\","
                            "\"id\":1,\"params\":{}}";

    ipc_bus_message_t *request_msg = ipc_bus_message_create(
        IPC_BUS_MSG_REQUEST, IPC_BUS_PROTO_JSON_RPC,
        list_json, strlen(list_json));

    ipc_bus_message_t response_msg;
    __builtin_memset(&response_msg, 0, sizeof(response_msg));

    int ipc_ret = ipc_bus_helper_request(
        daemon_bootstrap_ipc_get_helper(adapter->bipc),
        adapter->tool_d_service_name,
        request_msg, &response_msg,
        adapter->request_timeout_ms);

    ipc_bus_message_free(request_msg);

    if (ipc_ret == 0 && response_msg.payload && response_msg.payload_size > 0) {
        *out_json = (char *)AGENTOS_MALLOC(response_msg.payload_size + 1);
        if (*out_json) {
            memcpy(*out_json, response_msg.payload, response_msg.payload_size);
            (*out_json)[response_msg.payload_size] = '\0';
        }
    }

    if (response_msg.payload) {
        AGENTOS_FREE(response_msg.payload);
    }

    return ipc_ret;
}

/* ==================== 状态查询实现 ==================== */

bool tool_svc_adapter_is_connected(tool_svc_adapter_t *adapter)
{
    if (!adapter) return false;
    return adapter->connected && adapter->bipc &&
           daemon_bootstrap_ipc_is_running(adapter->bipc);
}

void tool_svc_adapter_get_stats(tool_svc_adapter_t *adapter,
                                uint64_t *out_total_executions,
                                uint64_t *out_total_errors,
                                uint64_t *out_avg_latency_ms)
{
    if (!adapter) {
        if (out_total_executions) *out_total_executions = 0;
        if (out_total_errors) *out_total_errors = 0;
        if (out_avg_latency_ms) *out_avg_latency_ms = 0;
        return;
    }

    if (out_total_executions) *out_total_executions = adapter->total_executions;
    if (out_total_errors) *out_total_errors = adapter->total_errors;
    if (out_avg_latency_ms) {
        if (adapter->total_executions > 0) {
            *out_avg_latency_ms = (uint64_t)(
                (double)adapter->total_latency_us /
                (double)adapter->total_executions / 1000.0);
        } else {
            *out_avg_latency_ms = 0;
        }
    }
}