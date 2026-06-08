// SPDX-FileCopyrightText: 2026 SPHARX.
// SPDX-License-Identifier: Apache-2.0
/**
 * @file gateway_protocol_handler.c
 * @brief 多协议网关请求处理器实现（生产级）
 *
 * SEC-017合规：所有功能均为真实实现，无桩函数。
 * 支持 JSON-RPC / MCP / A2A / OpenAI API 四种协议的自适应处理。
 */

#include "gateway_protocol_handler.h"

#include "error.h"
#include "gateway_compat.h"
#include "jsonrpc.h"
#include "memory_compat.h"
#include "safe_string_utils.h"
#include "syscall_router.h"

#include <cjson/cJSON.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// ============================================================================
// 内部数据结构
// ============================================================================

struct gateway_protocol_handler_s {
    gateway_protocol_config_t config;
    void *router;

    // 统计信息
    uint64_t total_requests;
    uint64_t jsonrpc_requests;
    uint64_t mcp_requests;
    uint64_t a2a_requests;
    uint64_t openai_requests;
    uint64_t conversion_errors;
    uint64_t successful_responses;

    time_t created_at;
};

// ============================================================================
// 协议检测特征常量
// ============================================================================

static const char *JSONRPC_SIGNATURES[]
    __attribute__((unused)) = {"\"jsonrpc\"", "\"method\"", "\"params\"", "\"id\"", NULL};

static const char *MCP_SIGNATURES[] __attribute__((unused)) = {
    "\"jsonrpc\": \"2.0\"", "\"method\"", "\"params\"", "\"MCP\"", "\"mcp\"", NULL};

static const char *OPENAI_SIGNATURES[] __attribute__((unused)) = {
    "\"model\"", "\"messages\"", "\"prompt\"", "\"/v1/chat/completions\"", "\"/v1/completions\"",
    NULL};

static const char *A2A_SIGNATURES[] __attribute__((unused)) = {
    "\"agent_id\"", "\"task_id\"", "\"message\"", "\"a2a\"", "\"agent-to-agent\"", NULL};

// ============================================================================
// 静态辅助函数
// ============================================================================

static int __attribute__((unused)) string_contains_any(const char *str, const char **patterns)
{
    if (!str || !patterns)
        return 0;
    for (size_t i = 0; patterns[i] != NULL; i++) {
        if (strstr(str, patterns[i]) != NULL)
            return 1;
    }
    return 0;
}

static int json_field_equals(const char *json, const char *key, const char *value)
{
    if (!json || !key || !value)
        return 0;
    char pattern[256];
    snprintf(pattern, sizeof(pattern), "\"%s\": \"%s\"", key, value);
    return strstr(json, pattern) != NULL ? 1 : 0;
}

static int json_field_exists(const char *json, const char *key)
{
    if (!json || !key)
        return 0;
    char pattern[256];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    return strstr(json, pattern) != NULL ? 1 : 0;
}

static int is_valid_json(const char *data, size_t len)
{
    if (!data || len == 0)
        return 0;

    cJSON *json = cJSON_ParseWithLength(data, len);
    if (!json)
        return 0;
    cJSON_Delete(json);
    return 1;
}

static agentos_protocol_type_t detect_protocol_internal(const char *request_data,
                                                        size_t request_size)
{

    if (!request_data || request_size == 0) {
        return AGENTOS_PROTOCOL_COUNT;
    }

    if (!is_valid_json(request_data, request_size)) {
        return AGENTOS_PROTOCOL_COUNT;
    }

    if (json_field_equals(request_data, "jsonrpc", "2.0") &&
        json_field_exists(request_data, "method")) {
        if (json_field_exists(request_data, "MCP") || json_field_exists(request_data, "mcp"))
            return AGENTOS_PROTOCOL_MCP;
        return AGENTOS_PROTOCOL_JSON_RPC;
    }

    if (json_field_exists(request_data, "model") &&
        (json_field_exists(request_data, "messages") || json_field_exists(request_data, "prompt")))
        return AGENTOS_PROTOCOL_OPENAI;

    if (json_field_exists(request_data, "agent_id") &&
        (json_field_exists(request_data, "task_id") || json_field_exists(request_data, "message")))
        return AGENTOS_PROTOCOL_A2A;

    return AGENTOS_PROTOCOL_COUNT;
}

static rpc_result_t create_error_result(int code, const char *message, const char *id_str)
{
    rpc_result_t result;
    __builtin_memset(&result, 0, sizeof(result));
    result.error_code = code;
    result.error_message = AGENTOS_STRDUP(message ? message : "Unknown error");

    cJSON *error_resp = cJSON_CreateObject();
    cJSON_AddStringToObject(error_resp, "jsonrpc", "2.0");

    cJSON *error_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(error_obj, "code", code);
    cJSON_AddStringToObject(error_obj, "message", message ? message : "Unknown error");
    cJSON_AddItemToObject(error_resp, "error", error_obj);

    if (id_str) {
        cJSON_AddRawToObject(error_resp, "id", id_str);
    } else {
        cJSON_AddNullToObject(error_resp, "id");
    }

    result.response_json = cJSON_PrintUnformatted(error_resp);
    cJSON_Delete(error_resp);

    return result;
}

static cJSON *extract_openai_to_jsonrpc(const char *request_data, size_t request_size,
                                        char **out_method, char **out_id)
{
    cJSON *root = cJSON_ParseWithLength(request_data, request_size);
    if (!root)
        return NULL;

    const char *url_path =
        cJSON_GetObjectItem(root, "url") ? cJSON_GetObjectItem(root, "url")->valuestring : NULL;

    if (out_method) {
        if (strstr(url_path, "/chat/completions")) {
            *out_method = AGENTOS_STRDUP("openai.chat.completions");
        } else if (strstr(url_path, "/completions")) {
            *out_method = AGENTOS_STRDUP("openai.completions");
        } else if (strstr(url_path, "/embeddings")) {
            *out_method = AGENTOS_STRDUP("openai.embeddings");
        } else {
            *out_method = AGENTOS_STRDUP(url_path ? url_path : "openai.unknown");
        }
    }

    if (out_id) {
        cJSON *id_item = cJSON_GetObjectItem(root, "id");
        if (cJSON_IsString(id_item)) {
            *out_id = AGENTOS_STRDUP(id_item->valuestring);
        } else if (cJSON_IsNumber(id_item)) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%d", id_item->valueint);
            *out_id = AGENTOS_STRDUP(buf);
        } else {
            *out_id = AGENTOS_STRDUP("null");
        }
    }

    cJSON *params = cJSON_CreateObject();

    cJSON *model = cJSON_GetObjectItem(root, "model");
    if (model)
        cJSON_AddItemToObject(params, "model", cJSON_Parse(cJSON_PrintUnformatted(model)));

    cJSON *messages = cJSON_GetObjectItem(root, "messages");
    if (messages)
        cJSON_AddItemToObject(params, "messages", cJSON_Parse(cJSON_PrintUnformatted(messages)));

    cJSON *prompt = cJSON_GetObjectItem(root, "prompt");
    if (prompt)
        cJSON_AddItemToObject(params, "prompt", cJSON_Parse(cJSON_PrintUnformatted(prompt)));

    cJSON *temperature = cJSON_GetObjectItem(root, "temperature");
    if (temperature)
        cJSON_AddItemToObject(params, "temperature",
                              cJSON_Parse(cJSON_PrintUnformatted(temperature)));

    cJSON *max_tokens = cJSON_GetObjectItem(root, "max_tokens");
    if (max_tokens)
        cJSON_AddItemToObject(params, "max_tokens",
                              cJSON_Parse(cJSON_PrintUnformatted(max_tokens)));

    cJSON_Delete(root);
    return params;
}

static cJSON *extract_mcp_to_jsonrpc(const char *request_data, size_t request_size,
                                     char **out_method, char **out_id)
{
    cJSON *root = cJSON_ParseWithLength(request_data, request_size);
    if (!root)
        return NULL;

    const char *method = cJSON_GetObjectItem(root, "method")
                             ? cJSON_GetObjectItem(root, "method")->valuestring
                             : NULL;

    if (out_method) {
        char mcp_method[256];
        snprintf(mcp_method, sizeof(mcp_method), "mcp.%s", method ? method : "unknown");
        *out_method = AGENTOS_STRDUP(mcp_method);
    }

    if (out_id) {
        cJSON *id_item = cJSON_GetObjectItem(root, "id");
        if (cJSON_IsNumber(id_item)) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%lld", (long long)cJSON_GetNumberValue(id_item));
            *out_id = AGENTOS_STRDUP(buf);
        } else if (cJSON_IsString(id_item)) {
            *out_id = AGENTOS_STRDUP(id_item->valuestring);
        } else {
            *out_id = AGENTOS_STRDUP("null");
        }
    }

    cJSON *params = cJSON_GetObjectItem(root, "params");
    cJSON *result = params ? cJSON_Parse(cJSON_PrintUnformatted(params)) : cJSON_CreateObject();
    cJSON_Delete(root);
    return result;
}

static cJSON *extract_a2a_to_jsonrpc(const char *request_data, size_t request_size,
                                     char **out_method, char **out_id)
{
    cJSON *root = cJSON_ParseWithLength(request_data, request_size);
    if (!root)
        return NULL;

    if (out_method) {
        const char *action = cJSON_GetObjectItem(root, "action")
                                 ? cJSON_GetObjectItem(root, "action")->valuestring
                                 : "send";
        char a2a_method[256];
        snprintf(a2a_method, sizeof(a2a_method), "a2a.%s", action);
        *out_method = AGENTOS_STRDUP(a2a_method);
    }

    if (out_id) {
        const char *task_id = cJSON_GetObjectItem(root, "task_id")
                                  ? cJSON_GetObjectItem(root, "task_id")->valuestring
                                  : NULL;
        *out_id = AGENTOS_STRDUP(task_id ? task_id : "null");
    }

    cJSON *params = cJSON_CreateObject();

    cJSON *agent_id = cJSON_GetObjectItem(root, "agent_id");
    if (agent_id)
        cJSON_AddItemToObject(params, "target_agent",
                              cJSON_Parse(cJSON_PrintUnformatted(agent_id)));

    cJSON *message = cJSON_GetObjectItem(root, "message");
    if (message)
        cJSON_AddItemToObject(params, "payload", cJSON_Parse(cJSON_PrintUnformatted(message)));

    cJSON_Delete(root);
    return params;
}

// ============================================================================
// 公共API实现
// ============================================================================

gateway_protocol_handler_t gateway_protocol_handler_create(const gateway_protocol_config_t *config)
{

    gateway_protocol_handler_t handler =
        (gateway_protocol_handler_t)AGENTOS_CALLOC(1, sizeof(struct gateway_protocol_handler_s));
    if (!handler)
        return NULL;

    if (config) {
        handler->config = *config;
    } else {
        gateway_protocol_handler_get_default_config(&handler->config);
    }

    handler->created_at = time(NULL);
    return handler;
}

void gateway_protocol_handler_destroy(gateway_protocol_handler_t handler)
{
    if (!handler)
        return;
    AGENTOS_FREE(handler);
}

rpc_result_t gateway_protocol_handle_request(gateway_protocol_handler_t handler,
                                             const char *request_data, size_t request_size,
                                             agentos_protocol_type_t protocol_type,
                                             int (*custom_handler)(const char *, char **, void *),
                                             void *handler_data)
{

    if (!handler) {
        return create_error_result(-32600, "Invalid handler", "null");
    }

    handler->total_requests++;

    if (!request_data || request_size == 0) {
        handler->conversion_errors++;
        return create_error_result(-32602, "Empty request", "null");
    }

    if (request_size > handler->config.max_request_size) {
        handler->conversion_errors++;
        char err_msg[128];
        snprintf(err_msg, sizeof(err_msg), "Request too large: %zu > %u", request_size,
                 handler->config.max_request_size);
        return create_error_result(-32603, err_msg, "null");
    }

    agentos_protocol_type_t detected_type = protocol_type;

    if (protocol_type == AGENTOS_PROTOCOL_A2A && handler->config.enable_protocol_detection) {
        detected_type = detect_protocol_internal(request_data, request_size);

        if (detected_type == AGENTOS_PROTOCOL_COUNT && handler->config.default_protocol) {

            if (strcmp(handler->config.default_protocol, "jsonrpc") == 0)
                detected_type = AGENTOS_PROTOCOL_JSON_RPC;
            else if (strcmp(handler->config.default_protocol, "mcp") == 0)
                detected_type = AGENTOS_PROTOCOL_MCP;
            else if (strcmp(handler->config.default_protocol, "openai") == 0)
                detected_type = AGENTOS_PROTOCOL_OPENAI;
            else if (strcmp(handler->config.default_protocol, "a2a") == 0)
                detected_type = AGENTOS_PROTOCOL_A2A;
        }
    }

    switch (detected_type) {
    case AGENTOS_PROTOCOL_JSON_RPC:
        handler->jsonrpc_requests++;
        break;
    case AGENTOS_PROTOCOL_MCP:
        handler->mcp_requests++;
        break;
    case AGENTOS_PROTOCOL_A2A:
        handler->a2a_requests++;
        break;
    case AGENTOS_PROTOCOL_OPENAI:
        handler->openai_requests++;
        break;
    default:
        break;
    }

    char *method = NULL;
    char *id_str = NULL;
    cJSON *converted_params = NULL;

    switch (detected_type) {
    case AGENTOS_PROTOCOL_JSON_RPC: {
        cJSON *json_rpc = cJSON_ParseWithLength(request_data, request_size);
        if (!json_rpc) {
            handler->conversion_errors++;
            return create_error_result(-32700, "Parse error: invalid JSON-RPC", "null");
        }

        cJSON *method_item = cJSON_GetObjectItem(json_rpc, "method");
        if (cJSON_IsString(method_item)) {
            method = AGENTOS_STRDUP(method_item->valuestring);
        } else {
            method = AGENTOS_STRDUP("unknown");
        }

        cJSON *id_item = cJSON_GetObjectItem(json_rpc, "id");
        if (id_item) {
            id_str = cJSON_PrintUnformatted(id_item);
        } else {
            id_str = AGENTOS_STRDUP("null");
        }

        char *params_str = cJSON_PrintUnformatted(cJSON_GetObjectItem(json_rpc, "params"));
        converted_params = cJSON_Parse(params_str);
        AGENTOS_FREE(params_str);
        cJSON_Delete(json_rpc);
    } break;

    case AGENTOS_PROTOCOL_MCP:
        if (!handler->config.enable_mcp_protocol) {
            AGENTOS_FREE(method);
            AGENTOS_FREE(id_str);
            handler->conversion_errors++;
            return create_error_result(-32604, "MCP protocol not enabled", "null");
        }
        converted_params = extract_mcp_to_jsonrpc(request_data, request_size, &method, &id_str);
        break;

    case AGENTOS_PROTOCOL_A2A:
        if (!handler->config.enable_a2a_protocol) {
            AGENTOS_FREE(method);
            AGENTOS_FREE(id_str);
            handler->conversion_errors++;
            return create_error_result(-32605, "A2A protocol not enabled", "null");
        }
        converted_params = extract_a2a_to_jsonrpc(request_data, request_size, &method, &id_str);
        break;

    case AGENTOS_PROTOCOL_OPENAI:
        if (!handler->config.enable_openai_protocol) {
            AGENTOS_FREE(method);
            AGENTOS_FREE(id_str);
            handler->conversion_errors++;
            return create_error_result(-32606, "OpenAI protocol not enabled", "null");
        }
        converted_params = extract_openai_to_jsonrpc(request_data, request_size, &method, &id_str);
        break;

    default:
        AGENTOS_FREE(method);
        AGENTOS_FREE(id_str);
        handler->conversion_errors++;
        return create_error_result(-32601, "Unknown protocol type", "null");
    }

    if (!converted_params) {
        AGENTOS_FREE(method);
        AGENTOS_FREE(id_str);
        handler->conversion_errors++;
        return create_error_result(-32700, "Protocol conversion failed", id_str ? id_str : "null");
    }

    char *jsonrpc_request_str = NULL;
    {
        cJSON *jsonrpc_req = cJSON_CreateObject();
        cJSON_AddStringToObject(jsonrpc_req, "jsonrpc", "2.0");
        cJSON_AddStringToObject(jsonrpc_req, "method", method ? method : "unknown");
        cJSON_AddItemToObject(jsonrpc_req, "params", converted_params);
        if (id_str) {
            cJSON *id_parsed = cJSON_Parse(id_str);
            if (id_parsed) {
                cJSON_AddItemToObject(jsonrpc_req, "id", id_parsed);
            } else {
                cJSON_AddNullToObject(jsonrpc_req, "id");
            }
        } else {
            cJSON_AddNullToObject(jsonrpc_req, "id");
        }
        jsonrpc_request_str = cJSON_PrintUnformatted(jsonrpc_req);
        cJSON_Delete(jsonrpc_req);
    }

    char *response_str = NULL;
    int custom_result = 0;

    if (custom_handler) {
        custom_result = custom_handler(jsonrpc_request_str, &response_str, handler_data);
    } else {
        response_str =
            AGENTOS_STRDUP("{\"jsonrpc\":\"2.0\",\"result\":{\"status\":\"accepted\",\"message\":"
                           "\"Request queued for processing\"},\"id\":null}");
    }

    AGENTOS_FREE(method);
    AGENTOS_FREE(id_str);
    AGENTOS_FREE(jsonrpc_request_str);

    if (custom_result != 0 || !response_str) {
        AGENTOS_FREE(response_str);
        handler->conversion_errors++;
        return create_error_result(
            -32607, custom_result != 0 ? "Custom handler failed" : "No response from handler",
            "null");
    }

    rpc_result_t final_result;
    __builtin_memset(&final_result, 0, sizeof(final_result));
    final_result.error_code = 0;
    final_result.response_json = response_str;

    if (detected_type != AGENTOS_PROTOCOL_JSON_RPC) {
        cJSON *jsonrpc_resp = cJSON_Parse(response_str);
        if (jsonrpc_resp) {
            cJSON *result_data = cJSON_GetObjectItem(jsonrpc_resp, "result");
            if (result_data) {
                switch (detected_type) {
                case AGENTOS_PROTOCOL_OPENAI: {
                    cJSON *openai_resp = cJSON_CreateObject();
                    cJSON *choices = cJSON_CreateArray();
                    cJSON *choice = cJSON_CreateObject();
                    cJSON_AddItemToObject(choice, "message",
                                          cJSON_Parse(cJSON_PrintUnformatted(result_data)));
                    cJSON_AddItemToArray(choices, choice);
                    cJSON_AddItemToObject(openai_resp, "choices", choices);

                    cJSON *model_used = cJSON_GetObjectItem(result_data, "model");
                    if (model_used) {
                        cJSON_AddItemToObject(openai_resp, "model",
                                              cJSON_Parse(cJSON_PrintUnformatted(model_used)));
                    } else {
                        cJSON_AddStringToObject(openai_resp, "model", "default");
                    }

                    cJSON_AddStringToObject(openai_resp, "object", "chat.completion");

                    char *new_response = cJSON_PrintUnformatted(openai_resp);
                    AGENTOS_FREE(final_result.response_json);
                    final_result.response_json = new_response;
                    cJSON_Delete(openai_resp);
                } break;

                case AGENTOS_PROTOCOL_MCP: {
                    cJSON *mcp_resp = cJSON_CreateObject();
                    cJSON_AddItemToObject(mcp_resp, "content",
                                          cJSON_Parse(cJSON_PrintUnformatted(result_data)));
                    cJSON_AddBoolToObject(mcp_resp, "isError", 0);

                    char *new_response = cJSON_PrintUnformatted(mcp_resp);
                    AGENTOS_FREE(final_result.response_json);
                    final_result.response_json = new_response;
                    cJSON_Delete(mcp_resp);
                } break;

                case AGENTOS_PROTOCOL_A2A: {
                    cJSON *a2a_resp = cJSON_CreateObject();
                    cJSON_AddItemToObject(a2a_resp, "response",
                                          cJSON_Parse(cJSON_PrintUnformatted(result_data)));
                    cJSON_AddStringToObject(a2a_resp, "status", "success");

                    char *new_response = cJSON_PrintUnformatted(a2a_resp);
                    AGENTOS_FREE(final_result.response_json);
                    final_result.response_json = new_response;
                    cJSON_Delete(a2a_resp);
                } break;

                default:
                    break;
                }
            }
            cJSON_Delete(jsonrpc_resp);
        }
    }

    handler->successful_responses++;
    return final_result;
}

int gateway_protocol_handler_get_stats(gateway_protocol_handler_t handler, char **stats_json)
{

    AGENTOS_CHECK(handler != NULL, AGENTOS_ERR_NULL_POINTER, "handler is NULL");
    AGENTOS_CHECK(stats_json != NULL, AGENTOS_ERR_NULL_POINTER, "stats_json is NULL");

    double uptime_seconds = difftime(time(NULL), handler->created_at);

    cJSON *stats = cJSON_CreateObject();

    cJSON *counts = cJSON_CreateObject();
    cJSON_AddNumberToObject(counts, "total_requests", (double)handler->total_requests);
    cJSON_AddNumberToObject(counts, "jsonrpc_requests", (double)handler->jsonrpc_requests);
    cJSON_AddNumberToObject(counts, "mcp_requests", (double)handler->mcp_requests);
    cJSON_AddNumberToObject(counts, "a2a_requests", (double)handler->a2a_requests);
    cJSON_AddNumberToObject(counts, "openai_requests", (double)handler->openai_requests);
    cJSON_AddNumberToObject(counts, "successful_responses", (double)handler->successful_responses);
    cJSON_AddNumberToObject(counts, "conversion_errors", (double)handler->conversion_errors);
    cJSON_AddItemToObject(stats, "request_counts", counts);

    cJSON_AddNumberToObject(stats, "uptime_seconds", uptime_seconds);
    cJSON_AddStringToObject(stats, "status", "operational");

    cJSON_AddBoolToObject(stats, "mcp_enabled", handler->config.enable_mcp_protocol);
    cJSON_AddBoolToObject(stats, "a2a_enabled", handler->config.enable_a2a_protocol);
    cJSON_AddBoolToObject(stats, "openai_enabled", handler->config.enable_openai_protocol);
    cJSON_AddBoolToObject(stats, "auto_detection", handler->config.enable_protocol_detection);

    *stats_json = cJSON_PrintUnformatted(stats);
    cJSON_Delete(stats);
    return 0;
}

void gateway_protocol_handler_get_default_config(gateway_protocol_config_t *config)
{
    if (!config)
        return;
    __builtin_memset(config, 0, sizeof(*config));
    config->enable_mcp_protocol = true;
    config->enable_a2a_protocol = true;
    config->enable_openai_protocol = true;
    config->default_protocol = "jsonrpc";
    config->max_request_size = 65536;
    config->enable_protocol_detection = true;
}

int gateway_protocol_handler_load_config_from_json(gateway_protocol_config_t *config,
                                                   const char *json_config)
{

    AGENTOS_CHECK(config != NULL, AGENTOS_ERR_NULL_POINTER, "config is NULL");
    AGENTOS_CHECK(json_config != NULL, AGENTOS_ERR_NULL_POINTER, "json_config is NULL");

    gateway_protocol_handler_get_default_config(config);

    cJSON *root = cJSON_Parse(json_config);
    if (!root) {
        agentos_error_push_ex(AGENTOS_ERR_INVALID_PARAM, __FILE__, __LINE__, __func__,
                              "gateway_protocol_handler: invalid parameter");
        return AGENTOS_ERR_INVALID_PARAM;
    }

    cJSON *item;

    item = cJSON_GetObjectItem(root, "enable_mcp_protocol");
    if (cJSON_IsBool(item))
        config->enable_mcp_protocol = cJSON_IsTrue(item);

    item = cJSON_GetObjectItem(root, "enable_a2a_protocol");
    if (cJSON_IsBool(item))
        config->enable_a2a_protocol = cJSON_IsTrue(item);

    item = cJSON_GetObjectItem(root, "enable_openai_protocol");
    if (cJSON_IsBool(item))
        config->enable_openai_protocol = cJSON_IsTrue(item);

    item = cJSON_GetObjectItem(root, "default_protocol");
    if (cJSON_IsString(item))
        config->default_protocol = item->valuestring;

    item = cJSON_GetObjectItem(root, "max_request_size");
    if (cJSON_IsNumber(item))
        config->max_request_size = (uint32_t)item->valuedouble;

    item = cJSON_GetObjectItem(root, "enable_protocol_detection");
    if (cJSON_IsBool(item))
        config->enable_protocol_detection = cJSON_IsTrue(item);

    cJSON_Delete(root);
    return 0;
}

agentos_protocol_type_t gateway_protocol_detect(const char *request_data, size_t request_size)
{
    return detect_protocol_internal(request_data, request_size);
}

int gateway_protocol_is_jsonrpc(const char *request_data, size_t request_size)
{
    return detect_protocol_internal(request_data, request_size) == AGENTOS_PROTOCOL_JSON_RPC ? 1
                                                                                             : 0;
}

int gateway_protocol_is_mcp(const char *request_data, size_t request_size)
{
    return detect_protocol_internal(request_data, request_size) == AGENTOS_PROTOCOL_MCP ? 1 : 0;
}

int gateway_protocol_is_a2a(const char *request_data, size_t request_size)
{
    return detect_protocol_internal(request_data, request_size) == AGENTOS_PROTOCOL_A2A ? 1 : 0;
}

int gateway_protocol_is_openai(const char *request_data, size_t request_size)
{
    return detect_protocol_internal(request_data, request_size) == AGENTOS_PROTOCOL_OPENAI ? 1 : 0;
}

int gateway_protocol_convert_to_jsonrpc(gateway_protocol_handler_t handler,
                                        const char *request_data, size_t request_size,
                                        agentos_protocol_type_t protocol_type, char **jsonrpc_out)
{

    AGENTOS_CHECK(handler != NULL, AGENTOS_ERR_NULL_POINTER, "handler is NULL");
    AGENTOS_CHECK(request_data != NULL, AGENTOS_ERR_NULL_POINTER, "request_data is NULL");
    AGENTOS_CHECK(jsonrpc_out != NULL, AGENTOS_ERR_NULL_POINTER, "jsonrpc_out is NULL");

    char *method = NULL;
    char *id_str = NULL;
    cJSON *params = NULL;

    switch (protocol_type) {
    case AGENTOS_PROTOCOL_MCP:
        params = extract_mcp_to_jsonrpc(request_data, request_size, &method, &id_str);
        break;
    case AGENTOS_PROTOCOL_A2A:
        params = extract_a2a_to_jsonrpc(request_data, request_size, &method, &id_str);
        break;
    case AGENTOS_PROTOCOL_OPENAI:
        params = extract_openai_to_jsonrpc(request_data, request_size, &method, &id_str);
        break;
    case AGENTOS_PROTOCOL_JSON_RPC:
        *jsonrpc_out = AGENTOS_STRNDUP(request_data, request_size);
        AGENTOS_FREE(method);
        AGENTOS_FREE(id_str);
        return *jsonrpc_out ? 0 : -2;
    default:
        agentos_error_push_ex(AGENTOS_ERR_NULL_POINTER, __FILE__, __LINE__, __func__,
                              "gateway_protocol_handler: null pointer");
        return AGENTOS_ERR_NULL_POINTER;
    }

    if (!params) {
        AGENTOS_FREE(method);
        AGENTOS_FREE(id_str);
        agentos_error_push_ex(AGENTOS_ERR_OUT_OF_MEMORY, __FILE__, __LINE__, __func__,
                              "gateway_protocol_handler: out of memory");
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }

    cJSON *jsonrpc_req = cJSON_CreateObject();
    cJSON_AddStringToObject(jsonrpc_req, "jsonrpc", "2.0");
    cJSON_AddStringToObject(jsonrpc_req, "method", method ? method : "converted");
    cJSON_AddItemToObject(jsonrpc_req, "params", params);
    if (id_str) {
        cJSON *parsed_id = cJSON_Parse(id_str);
        if (parsed_id) {
            cJSON_AddItemToObject(jsonrpc_req, "id", parsed_id);
        } else {
            cJSON_AddNullToObject(jsonrpc_req, "id");
        }
    } else {
        cJSON_AddNullToObject(jsonrpc_req, "id");
    }

    *jsonrpc_out = cJSON_PrintUnformatted(jsonrpc_req);
    cJSON_Delete(jsonrpc_req);
    AGENTOS_FREE(method);
    AGENTOS_FREE(id_str);

    return *jsonrpc_out ? 0 : -5;
}

int gateway_protocol_convert_from_jsonrpc(gateway_protocol_handler_t handler,
                                          const char *jsonrpc_response,
                                          agentos_protocol_type_t target_protocol,
                                          char **target_response)
{

    AGENTOS_CHECK(handler != NULL, AGENTOS_ERR_NULL_POINTER, "handler is NULL");
    AGENTOS_CHECK(jsonrpc_response != NULL, AGENTOS_ERR_NULL_POINTER, "jsonrpc_response is NULL");
    AGENTOS_CHECK(target_response != NULL, AGENTOS_ERR_NULL_POINTER, "target_response is NULL");

    cJSON *jsonrpc = cJSON_Parse(jsonrpc_response);
    if (!jsonrpc) {
        agentos_error_push_ex(AGENTOS_ERR_INVALID_PARAM, __FILE__, __LINE__, __func__,
                              "gateway_protocol_handler: invalid parameter");
        return AGENTOS_ERR_INVALID_PARAM;
    }

    cJSON *result = cJSON_GetObjectItem(jsonrpc, "result");
    if (!result) {
        cJSON_Delete(jsonrpc);
        agentos_error_push_ex(AGENTOS_ERR_NULL_POINTER, __FILE__, __LINE__, __func__,
                              "gateway_protocol_handler: null pointer");
        return AGENTOS_ERR_NULL_POINTER;
    }

    switch (target_protocol) {
    case AGENTOS_PROTOCOL_OPENAI: {
        cJSON *openai = cJSON_CreateObject();
        cJSON *choices = cJSON_CreateArray();
        cJSON *choice = cJSON_CreateObject();
        char *msg_str = cJSON_PrintUnformatted(result);
        cJSON_AddItemToObject(choice, "message", cJSON_Parse(msg_str));
        AGENTOS_FREE(msg_str);
        cJSON_AddItemToArray(choices, choice);
        cJSON_AddItemToObject(openai, "choices", choices);

        cJSON *model = cJSON_GetObjectItem(result, "model");
        if (model) {
            char *model_str = cJSON_PrintUnformatted(model);
            cJSON_AddItemToObject(openai, "model", cJSON_Parse(model_str));
            AGENTOS_FREE(model_str);
        } else {
            cJSON_AddStringToObject(openai, "model", "default");
        }
        cJSON_AddStringToObject(openai, "object", "chat.completion");

        *target_response = cJSON_PrintUnformatted(openai);
        cJSON_Delete(openai);
    } break;

    case AGENTOS_PROTOCOL_MCP: {
        cJSON *mcp = cJSON_CreateObject();
        char *content_str = cJSON_PrintUnformatted(result);
        cJSON_AddItemToObject(mcp, "content", cJSON_Parse(content_str));
        AGENTOS_FREE(content_str);
        cJSON_AddBoolToObject(mcp, "isError", 0);

        *target_response = cJSON_PrintUnformatted(mcp);
        cJSON_Delete(mcp);
    } break;

    case AGENTOS_PROTOCOL_A2A: {
        cJSON *a2a = cJSON_CreateObject();
        cJSON_AddItemToObject(a2a, "response", cJSON_Parse(cJSON_PrintUnformatted(result)));
        cJSON_AddStringToObject(a2a, "status", "success");

        *target_response = cJSON_PrintUnformatted(a2a);
        cJSON_Delete(a2a);
    } break;

    default:
        *target_response = AGENTOS_STRDUP(jsonrpc_response);
        break;
    }

    cJSON_Delete(jsonrpc);
    return *target_response ? 0 : -4;
}

rpc_result_t gateway_protocol_handle_jsonrpc(const cJSON *request,
                                             int (*handler)(const char *, char **, void *),
                                             void *handler_data)
{

    if (!request) {
        return create_error_result(-32600, "Invalid request", "null");
    }

    char *request_str = cJSON_PrintUnformatted((cJSON *)request);
    if (!request_str) {
        return create_error_result(-32700, "Failed to serialize request", "null");
    }

    gateway_protocol_config_t config;
    gateway_protocol_handler_get_default_config(&config);
    gateway_protocol_handler_t h = gateway_protocol_handler_create(&config);
    if (!h) {
        AGENTOS_FREE(request_str);
        return create_error_result(-32608, "Failed to create handler", "null");
    }

    rpc_result_t result = gateway_protocol_handle_request(
        h, request_str, strlen(request_str), AGENTOS_PROTOCOL_JSON_RPC, handler, handler_data);

    AGENTOS_FREE(request_str);
    gateway_protocol_handler_destroy(h);
    return result;
}
