/**
 * @file anthropic.c
 * @brief Anthropic 适配器实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "provider.h"
#include "svc_http.h"
#include "svc_logger.h"
#include "svc_error.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    char api_key[512];
    char api_base[512];
    double timeout_sec;
    int max_retries;
} anthropic_ctx_t;

/* ---------- 生命周期 ---------- */
static provider_ctx_t* anthropic_init(const char* name, const char* api_key,
                                       const char* api_base, const char* organization,
                                       double timeout_sec, int max_retries) {
                                       // From data intelligence emerges. by spharx
    (void)name; (void)organization;
    anthropic_ctx_t* ctx = calloc(1, sizeof(anthropic_ctx_t));
    if (!ctx) return NULL;
    strncpy(ctx->api_key, api_key, sizeof(ctx->api_key) - 1);
    if (api_base)
        strncpy(ctx->api_base, api_base, sizeof(ctx->api_base) - 1);
    else
        strcpy(ctx->api_base, "https://api.anthropic.com/v1");
    ctx->timeout_sec = timeout_sec > 0 ? timeout_sec : 30.0;
    ctx->max_retries = max_retries;
    return (provider_ctx_t*)ctx;
}

static void anthropic_destroy(provider_ctx_t* ctx_ptr) {
    free(ctx_ptr);
}

/* ---------- 构建请求体 ---------- */
static char* build_request(const llm_request_config_t* config) {
    cJSON* root = cJSON_CreateObject();
    if (!root) return NULL;

    cJSON_AddStringToObject(root, "model", config->model);
    cJSON_AddNumberToObject(root, "temperature", config->temperature);
    if (config->max_tokens > 0) cJSON_AddNumberToObject(root, "max_tokens", config->max_tokens);
    if (config->stream) cJSON_AddBoolToObject(root, "stream", config->stream);

    char* system = NULL;
    cJSON* messages = cJSON_CreateArray();
    for (size_t i = 0; i < config->message_count; ++i) {
        if (strcmp(config->messages[i].role, "system") == 0) {
            system = strdup(config->messages[i].content);
        } else {
            cJSON* msg = cJSON_CreateObject();
            cJSON_AddStringToObject(msg, "role", config->messages[i].role);
            cJSON_AddStringToObject(msg, "content", config->messages[i].content);
            cJSON_AddItemToArray(messages, msg);
        }
    }
    if (system) {
        cJSON_AddStringToObject(root, "system", system);
        free(system);
    }
    cJSON_AddItemToObject(root, "messages", messages);

    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}

/* ---------- 解析非流式响应 ---------- */
static int parse_response(const char* body, llm_response_t** out) {
    cJSON* root = cJSON_Parse(body);
    if (!root) return ERR_HTTP;

    llm_response_t* resp = calloc(1, sizeof(llm_response_t));
    if (!resp) {
        cJSON_Delete(root);
        return ERR_NOMEM;
    }

    cJSON* id = cJSON_GetObjectItem(root, "id");
    if (cJSON_IsString(id)) resp->id = strdup(id->valuestring);

    cJSON* model = cJSON_GetObjectItem(root, "model");
    if (cJSON_IsString(model)) resp->model = strdup(model->valuestring);

    cJSON* content = cJSON_GetObjectItem(root, "content");
    if (cJSON_IsArray(content) && cJSON_GetArraySize(content) > 0) {
        cJSON* first = cJSON_GetArrayItem(content, 0);
        cJSON* text = cJSON_GetObjectItem(first, "text");
        if (cJSON_IsString(text)) {
            resp->choice_count = 1;
            resp->choices = calloc(1, sizeof(llm_message_t));
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
    return 0;
}

/* ---------- 流式写回调 ---------- */
typedef struct {
    llm_stream_callback_t user_cb;
    void* user_data;
} stream_adapter_t;

static size_t stream_write(void* contents, size_t size, size_t nmemb, void* userp) {
    stream_adapter_t* ad = (stream_adapter_t*)userp;
    size_t realsize = size * nmemb;
    char* data = (char*)contents;
    char* line = data;
    char* end;

    while ((end = memchr(line, '\n', realsize - (line - data))) != NULL) {
        size_t len = end - line;
        char saved = line[len];
        line[len] = '\0';

        if (strncmp(line, "data: ", 6) == 0) {
            char* json_str = line + 6;
            cJSON* root = cJSON_Parse(json_str);
            if (root) {
                const char* type = NULL;
                cJSON* type_item = cJSON_GetObjectItem(root, "type");
                if (cJSON_IsString(type_item)) type = type_item->valuestring;
                if (type && strcmp(type, "content_block_delta") == 0) {
                    cJSON* delta = cJSON_GetObjectItem(root, "delta");
                    if (delta) {
                        cJSON* text = cJSON_GetObjectItem(delta, "text");
                        if (cJSON_IsString(text) && text->valuestring) {
                            ad->user_cb(text->valuestring, ad->user_data);
                        }
                    }
                }
                cJSON_Delete(root);
            }
        }
        line[len] = saved;
        line = end + 1;
    }
    return realsize;
}

/* ---------- 同步完成 ---------- */
static int anthropic_complete(provider_ctx_t* ctx_ptr,
                              const llm_request_config_t* config,
                              llm_response_t** out_response) {
    anthropic_ctx_t* ctx = (anthropic_ctx_t*)ctx_ptr;
    char* req_body = build_request(config);
    if (!req_body) return ERR_NOMEM;

    char url[1024];
    snprintf(url, sizeof(url), "%s/messages", ctx->api_base);

    http_headers_t* headers = http_headers_new();
    if (!headers) {
        free(req_body);
        return ERR_NOMEM;
    }
    http_headers_add(headers, "Content-Type: application/json");
    http_headers_add(headers, "anthropic-version: 2023-06-01");
    char auth[512];
    snprintf(auth, sizeof(auth), "x-api-key: %s", ctx->api_key);
    http_headers_add(headers, auth);

    http_response_t* resp = http_post(url, headers, req_body, ctx->timeout_sec);
    http_headers_free(headers);
    free(req_body);

    if (!resp) return ERR_HTTP;
    if (resp->status_code != 200) {
        http_response_free(resp);
        return ERR_HTTP;
    }
    int ret = parse_response(resp->body, out_response);
    http_response_free(resp);
    return ret;
}

/* ---------- 流式完成 ---------- */
static int anthropic_complete_stream(provider_ctx_t* ctx_ptr,
                                     const llm_request_config_t* config,
                                     llm_stream_callback_t callback,
                                     llm_response_t** out_response) {
    anthropic_ctx_t* ctx = (anthropic_ctx_t*)ctx_ptr;
    llm_request_config_t stream_cfg = *config;
    stream_cfg.stream = 1;
    char* req_body = build_request(&stream_cfg);
    if (!req_body) return ERR_NOMEM;

    char url[1024];
    snprintf(url, sizeof(url), "%s/messages", ctx->api_base);

    http_headers_t* headers = http_headers_new();
    if (!headers) {
        free(req_body);
        return ERR_NOMEM;
    }
    http_headers_add(headers, "Content-Type: application/json");
    http_headers_add(headers, "anthropic-version: 2023-06-01");
    char auth[512];
    snprintf(auth, sizeof(auth), "x-api-key: %s", ctx->api_key);
    http_headers_add(headers, auth);

    stream_adapter_t ad = {callback, config->user_data};
    http_response_t* resp = http_post_stream(url, headers, req_body, ctx->timeout_sec,
                                              stream_write, &ad);
    http_headers_free(headers);
    free(req_body);

    if (!resp) return ERR_HTTP;
    if (resp->status_code != 200) {
        http_response_free(resp);
        return ERR_HTTP;
    }
    if (out_response) *out_response = NULL;
    http_response_free(resp);
    return 0;
}

/* ---------- 操作表 ---------- */
const provider_ops_t anthropic_ops = {
    .init = anthropic_init,
    .destroy = anthropic_destroy,
    .complete = anthropic_complete,
    .complete_stream = anthropic_complete_stream
};