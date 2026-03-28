/**
 * @file anthropic.c
 * @brief Anthropic 适配器实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 改进说明：
 * 1. 使用公共 Provider 基础设施
 * 2. 代码量从 340 行减少到约 200 行
 * 3. 保留 Anthropic 特有的 system prompt 和响应解析逻辑
 */

#include "provider.h"
#include "error.h"
#include "platform.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>

#define ANTHROPIC_DEFAULT_BASE "https://api.anthropic.com/v1"
#define ANTHROPIC_DEFAULT_MODEL "claude-3-sonnet-20240229"

/* ---------- 上下文 ---------- */

typedef struct {
    provider_base_ctx_t base;
} anthropic_ctx_t;

/* ---------- 生命周期 ---------- */

static provider_ctx_t* anthropic_init(const char* name,
                                     const char* api_key,
                                     const char* api_base,
                                     const char* organization,
                                     double timeout_sec,
                                     int max_retries) {
    (void)name;
    (void)organization;

    anthropic_ctx_t* ctx = (anthropic_ctx_t*)calloc(1, sizeof(anthropic_ctx_t));
    if (!ctx) {
        return NULL;
    }

    provider_base_init(&ctx->base, api_key, api_base, organization,
                      timeout_sec, max_retries, ANTHROPIC_DEFAULT_BASE);

    return (provider_ctx_t*)ctx;
}

static void anthropic_destroy(provider_ctx_t* ctx_ptr) {
    if (ctx_ptr) {
        free(ctx_ptr);
    }
}

/* ---------- Anthropic 特有的请求构建 ---------- */

static char* anthropic_build_request(const llm_request_config_t* manager) {
    if (!manager) return NULL;

    cJSON* root = cJSON_CreateObject();
    if (!root) return NULL;

    cJSON_AddStringToObject(root, "model",
        manager->model && manager->model[0] ? manager->model : ANTHROPIC_DEFAULT_MODEL);
    cJSON_AddNumberToObject(root, "temperature",
        manager->temperature > 0 ? manager->temperature : 0.7);

    if (manager->max_tokens > 0) {
        cJSON_AddNumberToObject(root, "max_tokens", manager->max_tokens);
    }

    if (manager->stream) {
        cJSON_AddBoolToObject(root, "stream", 1);
    }

    char* system_prompt = NULL;
    cJSON* messages = cJSON_CreateArray();

    for (size_t i = 0; i < manager->message_count; ++i) {
        if (manager->messages[i].role &&
            strcmp(manager->messages[i].role, "system") == 0) {
            system_prompt = strdup(manager->messages[i].content ?
                                   manager->messages[i].content : "");
        } else {
            cJSON* msg = cJSON_CreateObject();
            const char* role = manager->messages[i].role ? manager->messages[i].role : "user";
            const char* content = manager->messages[i].content ? manager->messages[i].content : "";
            cJSON_AddStringToObject(msg, "role", role);
            cJSON_AddStringToObject(msg, "content", content);
            cJSON_AddItemToArray(messages, msg);
        }
    }

    if (system_prompt) {
        cJSON_AddStringToObject(root, "system", system_prompt);
        free(system_prompt);
    }

    cJSON_AddItemToObject(root, "messages", messages);

    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}

/* ---------- Anthropic 特有的响应解析 ---------- */

static int anthropic_parse_response(const char* body, llm_response_t** out) {
    if (!body || !out) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    cJSON* root = cJSON_Parse(body);
    if (!root) {
        return AGENTOS_ERR_PARSE_ERROR;
    }

    llm_response_t* resp = (llm_response_t*)calloc(1, sizeof(llm_response_t));
    if (!resp) {
        cJSON_Delete(root);
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }

    cJSON* id = cJSON_GetObjectItem(root, "id");
    if (cJSON_IsString(id) && id->valuestring) {
        resp->id = strdup(id->valuestring);
    }

    cJSON* model = cJSON_GetObjectItem(root, "model");
    if (cJSON_IsString(model) && model->valuestring) {
        resp->model = strdup(model->valuestring);
    }

    cJSON* content = cJSON_GetObjectItem(root, "content");
    if (cJSON_IsArray(content) && cJSON_GetArraySize(content) > 0) {
        cJSON* first = cJSON_GetArrayItem(content, 0);
        cJSON* text = cJSON_GetObjectItem(first, "text");
        if (cJSON_IsString(text) && text->valuestring) {
            resp->choice_count = 1;
            resp->choices = (llm_message_t*)calloc(1, sizeof(llm_message_t));
            if (!resp->choices) {
                cJSON_Delete(root);
                llm_response_free(resp);
                return AGENTOS_ERR_OUT_OF_MEMORY;
            }
            resp->choices[0].role = strdup("assistant");
            resp->choices[0].content = strdup(text->valuestring);
        }
    }

    cJSON* usage = cJSON_GetObjectItem(root, "usage");
    if (usage) {
        cJSON* input = cJSON_GetObjectItem(usage, "input_tokens");
        cJSON* output = cJSON_GetObjectItem(usage, "output_tokens");
        if (cJSON_IsNumber(input)) resp->prompt_tokens = (uint32_t)input->valuedouble;
        if (cJSON_IsNumber(output)) resp->completion_tokens = (uint32_t)output->valuedouble;
        resp->total_tokens = resp->prompt_tokens + resp->completion_tokens;
    }

    cJSON_Delete(root);
    *out = resp;
    return AGENTOS_OK;
}

/* ---------- 同步完成 ---------- */

static int anthropic_complete(provider_ctx_t* ctx_ptr,
                              const llm_request_config_t* manager,
                              llm_response_t** out_response) {
    if (!ctx_ptr || !manager || !out_response) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    anthropic_ctx_t* ctx = (anthropic_ctx_t*)ctx_ptr;
    provider_base_ctx_t* base = &ctx->base;

    char* req_body = anthropic_build_request(manager);
    if (!req_body) {
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }

    char url[1024];
    snprintf(url, sizeof(url), "%s/messages", base->api_base);

    struct curl_slist* headers = NULL;
    char auth_header[1024];
    snprintf(auth_header, sizeof(auth_header), "x-api-key: %s",
             base->api_key[0] ? base->api_key : "");
    headers = curl_slist_append(headers, auth_header);
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "anthropic-version: 2023-06-01");

    provider_http_resp_t* http_resp = NULL;
    long http_code = 0;

    int ret = provider_http_post(url, headers, req_body,
                                base->timeout_sec, base->max_retries,
                                &http_resp, &http_code);

    curl_slist_free_all(headers);
    free(req_body);

    if (ret != AGENTOS_OK) {
        SVC_LOG_ERROR("anthropic: HTTP request failed, status=%ld", http_code);
        return ret;
    }

    ret = anthropic_parse_response(http_resp->data, out_response);
    provider_http_resp_free(http_resp);

    return ret;
}

/* ---------- 流式完成 ---------- */

static int anthropic_complete_stream(provider_ctx_t* ctx_ptr,
                                    const llm_request_config_t* manager,
                                    llm_stream_callback_t callback,
                                    void* user_data,
                                    llm_response_t** out_response) {
    if (!ctx_ptr || !manager || !callback) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    SVC_LOG_WARN("Anthropic streaming not yet implemented, using synchronous mode");

    llm_response_t* resp = NULL;
    int ret = anthropic_complete(ctx_ptr, manager, &resp);

    if (ret == AGENTOS_OK && resp && resp->choices && resp->choices[0].content) {
        callback(resp->choices[0].content, user_data);
    }

    if (out_response) {
        *out_response = resp;
    } else if (resp) {
        llm_response_free(resp);
    }

    return ret;
}

/* ---------- 操作表 ---------- */

const provider_ops_t anthropic_ops = {
    .init = anthropic_init,
    .destroy = anthropic_destroy,
    .complete = anthropic_complete,
    .complete_stream = anthropic_complete_stream,
    .name = "anthropic",
    .default_model = ANTHROPIC_DEFAULT_MODEL,
    .default_base_url = ANTHROPIC_DEFAULT_BASE
};
