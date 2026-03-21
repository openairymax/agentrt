/**
 * @file openai.c
 * @brief OpenAI 适配器实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "provider.h"
#include "svc_http.h"
#include "svc_logger.h"
#include "svc_error.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ---------- 上下文 ---------- */
typedef struct {
    char api_key[512];
    char api_base[512];
    char organization[256];
    double timeout_sec;
    int max_retries;
} openai_ctx_t;

/* ---------- 生命周期 ---------- */
static provider_ctx_t* openai_init(const char* name, const char* api_key,
                                    const char* api_base, const char* organization,
                                    double timeout_sec, int max_retries) {
    (void)name;
    openai_ctx_t* ctx = calloc(1, sizeof(openai_ctx_t));
    if (!ctx) return NULL;

    strncpy(ctx->api_key, api_key, sizeof(ctx->api_key) - 1);
    if (api_base)
        strncpy(ctx->api_base, api_base, sizeof(ctx->api_base) - 1);
    else
        strcpy(ctx->api_base, "https://api.openai.com/v1");
    if (organization)
        strncpy(ctx->organization, organization, sizeof(ctx->organization) - 1);
    ctx->timeout_sec = timeout_sec > 0 ? timeout_sec : 30.0;
    ctx->max_retries = max_retries;
    return (provider_ctx_t*)ctx;
}

static void openai_destroy(provider_ctx_t* ctx_ptr) {
    free(ctx_ptr);
}

/* ---------- 构建请求体 ---------- */
static char* build_request(const llm_request_config_t* config) {
    cJSON* root = cJSON_CreateObject();
    if (!root) return NULL;

    cJSON_AddStringToObject(root, "model", config->model);
    cJSON_AddNumberToObject(root, "temperature", config->temperature);
    if (config->top_p > 0) cJSON_AddNumberToObject(root, "top_p", config->top_p);
    if (config->max_tokens > 0) cJSON_AddNumberToObject(root, "max_tokens", config->max_tokens);
    if (config->stream) cJSON_AddBoolToObject(root, "stream", config->stream);
    if (config->presence_penalty != 0)
        cJSON_AddNumberToObject(root, "presence_penalty", config->presence_penalty);
    if (config->frequency_penalty != 0)
        cJSON_AddNumberToObject(root, "frequency_penalty", config->frequency_penalty);

    if (config->stop_count > 0) {
        cJSON* stop = cJSON_CreateArray();
        for (size_t i = 0; i < config->stop_count; ++i)
            cJSON_AddItemToArray(stop, cJSON_CreateString(config->stop[i]));
        cJSON_AddItemToObject(root, "stop", stop);
    }

    cJSON* msgs = cJSON_CreateArray();
    for (size_t i = 0; i < config->message_count; ++i) {
        cJSON* msg = cJSON_CreateObject();
        cJSON_AddStringToObject(msg, "role", config->messages[i].role);
        cJSON_AddStringToObject(msg, "content", config->messages[i].content);
        cJSON_AddItemToArray(msgs, msg);
    }
    cJSON_AddItemToObject(root, "messages", msgs);

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

    cJSON* created = cJSON_GetObjectItem(root, "created");
    if (cJSON_IsNumber(created)) resp->created = (uint64_t)created->valuedouble;

    cJSON* choices = cJSON_GetObjectItem(root, "choices");
    if (cJSON_IsArray(choices)) {
        resp->choice_count = cJSON_GetArraySize(choices);
        resp->choices = calloc(resp->choice_count, sizeof(llm_message_t));
        for (size_t i = 0; i < resp->choice_count; ++i) {
            cJSON* choice = cJSON_GetArrayItem(choices, i);
            cJSON* message = cJSON_GetObjectItem(choice, "message");
            if (message) {
                cJSON* role = cJSON_GetObjectItem(message, "role");
                cJSON* content = cJSON_GetObjectItem(message, "content");
                if (cJSON_IsString(role))
                    resp->choices[i].role = strdup(role->valuestring);
                if (cJSON_IsString(content))
                    resp->choices[i].content = strdup(content->valuestring);
            }
            cJSON* finish = cJSON_GetObjectItem(choice, "finish_reason");
            if (cJSON_IsString(finish) && !resp->finish_reason)
                resp->finish_reason = strdup(finish->valuestring);
        }
    }

    cJSON* usage = cJSON_GetObjectItem(root, "usage");
    if (usage) {
        cJSON* prompt = cJSON_GetObjectItem(usage, "prompt_tokens");
        cJSON* completion = cJSON_GetObjectItem(usage, "completion_tokens");
        cJSON* total = cJSON_GetObjectItem(usage, "total_tokens");
        if (cJSON_IsNumber(prompt)) resp->prompt_tokens = (uint32_t)prompt->valuedouble;
        if (cJSON_IsNumber(completion)) resp->completion_tokens = (uint32_t)completion->valuedouble;
        if (cJSON_IsNumber(total)) resp->total_tokens = (uint32_t)total->valuedouble;
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
            if (strcmp(json_str, "[DONE]") != 0) {
                cJSON* root = cJSON_Parse(json_str);
                if (root) {
                    cJSON* choices = cJSON_GetObjectItem(root, "choices");
                    if (cJSON_IsArray(choices) && cJSON_GetArraySize(choices) > 0) {
                        cJSON* choice = cJSON_GetArrayItem(choices, 0);
                        cJSON* delta = cJSON_GetObjectItem(choice, "delta");
                        if (delta) {
                            cJSON* content = cJSON_GetObjectItem(delta, "content");
                            if (cJSON_IsString(content) && content->valuestring) {
                                ad->user_cb(content->valuestring, ad->user_data);
                            }
                        }
                    }
                    cJSON_Delete(root);
                }
            }
        }
        line[len] = saved;
        line = end + 1;
    }
    return realsize;
}

/* ---------- 同步完成 ---------- */
static int openai_complete(provider_ctx_t* ctx_ptr,
                           const llm_request_config_t* config,
                           llm_response_t** out_response) {
    openai_ctx_t* ctx = (openai_ctx_t*)ctx_ptr;
    char* req_body = build_request(config);
    if (!req_body) return ERR_NOMEM;

    char url[1024];
    snprintf(url, sizeof(url), "%s/chat/completions", ctx->api_base);

    http_headers_t* headers = http_headers_new();
    if (!headers) {
        free(req_body);
        return ERR_NOMEM;
    }
    http_headers_add(headers, "Content-Type: application/json");
    char auth[512];
    snprintf(auth, sizeof(auth), "Authorization: Bearer %s", ctx->api_key);
    http_headers_add(headers, auth);
    if (ctx->organization[0]) {
        snprintf(auth, sizeof(auth), "OpenAI-Organization: %s", ctx->organization);
        http_headers_add(headers, auth);
    }

    http_response_t* resp = http_post(url, headers, req_body, ctx->timeout_sec);
    http_headers_free(headers);
    free(req_body);

    if (!resp) {
        SVC_LOG_ERROR("openai: HTTP request failed");
        return ERR_HTTP;
    }

    if (resp->status_code != 200) {
        SVC_LOG_ERROR("openai: HTTP %ld", resp->status_code);
        http_response_free(resp);
        return ERR_HTTP;
    }

    int ret = parse_response(resp->body, out_response);
    http_response_free(resp);
    return ret;
}

/* ---------- 流式完成 ---------- */
static int openai_complete_stream(provider_ctx_t* ctx_ptr,
                                  const llm_request_config_t* config,
                                  llm_stream_callback_t callback,
                                  llm_response_t** out_response) {
    openai_ctx_t* ctx = (openai_ctx_t*)ctx_ptr;
    llm_request_config_t stream_cfg = *config;
    stream_cfg.stream = 1;
    char* req_body = build_request(&stream_cfg);
    if (!req_body) return ERR_NOMEM;

    char url[1024];
    snprintf(url, sizeof(url), "%s/chat/completions", ctx->api_base);

    http_headers_t* headers = http_headers_new();
    if (!headers) {
        free(req_body);
        return ERR_NOMEM;
    }
    http_headers_add(headers, "Content-Type: application/json");
    char auth[512];
    snprintf(auth, sizeof(auth), "Authorization: Bearer %s", ctx->api_key);
    http_headers_add(headers, auth);
    if (ctx->organization[0]) {
        snprintf(auth, sizeof(auth), "OpenAI-Organization: %s", ctx->organization);
        http_headers_add(headers, auth);
    }

    stream_adapter_t ad = {callback, config->user_data};
    http_response_t* resp = http_post_stream(url, headers, req_body, ctx->timeout_sec,
                                              stream_write, &ad);
    http_headers_free(headers);
    free(req_body);

    if (!resp) {
        SVC_LOG_ERROR("openai: stream HTTP failed");
        return ERR_HTTP;
    }

    if (resp->status_code != 200) {
        SVC_LOG_ERROR("openai: stream HTTP %ld", resp->status_code);
        http_response_free(resp);
        return ERR_HTTP;
    }

    if (out_response) *out_response = NULL;
    http_response_free(resp);
    return 0;
}

/* ---------- 操作表 ---------- */
const provider_ops_t openai_ops = {
    .init = openai_init,
    .destroy = openai_destroy,
    .complete = openai_complete,
    .complete_stream = openai_complete_stream
};