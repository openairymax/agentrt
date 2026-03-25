/**
 * @file local.c
 * @brief 本地模型适配器（兼容 OpenAI 格式）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 改进说明：
 * 1. 统一错误码为 AGENTOS_ERR_*
 * 2. 使用通用 HTTP 接口 (libcurl)
 * 3. 移除不存在的头文件依赖
 * 4. 自行实现请求构建和响应解析
 */

#include "provider.h"
#include "error.h"
#include "platform.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>

typedef struct {
    char api_base[512];
    double timeout_sec;
    int max_retries;
} local_ctx_t;

/* ---------- 构建请求体 ---------- */
static char* build_request(const llm_request_config_t* config) {
    if (!config) return NULL;

    cJSON* root = cJSON_CreateObject();
    if (!root) return NULL;

    cJSON_AddStringToObject(root, "model", config->model ? config->model : "gpt-3.5-turbo");
    cJSON_AddNumberToObject(root, "temperature", config->temperature > 0 ? config->temperature : 0.7);

    if (config->top_p > 0) {
        cJSON_AddNumberToObject(root, "top_p", config->top_p);
    }

    if (config->max_tokens > 0) {
        cJSON_AddNumberToObject(root, "max_tokens", config->max_tokens);
    }

    if (config->stream) {
        cJSON_AddBoolToObject(root, "stream", 1);
    }

    if (config->presence_penalty != 0) {
        cJSON_AddNumberToObject(root, "presence_penalty", config->presence_penalty);
    }

    if (config->frequency_penalty != 0) {
        cJSON_AddNumberToObject(root, "frequency_penalty", config->frequency_penalty);
    }

    if (config->stop_count > 0 && config->stop) {
        cJSON* stop = cJSON_CreateArray();
        for (size_t i = 0; i < config->stop_count; ++i) {
            cJSON_AddItemToArray(stop, cJSON_CreateString(config->stop[i]));
        }
        cJSON_AddItemToObject(root, "stop", stop);
    }

    cJSON* msgs = cJSON_CreateArray();
    for (size_t i = 0; i < config->message_count; ++i) {
        cJSON* msg = cJSON_CreateObject();
        cJSON_AddStringToObject(msg, "role", config->messages[i].role ? config->messages[i].role : "user");
        cJSON_AddStringToObject(msg, "content", config->messages[i].content ? config->messages[i].content : "");
        cJSON_AddItemToArray(msgs, msg);
    }
    cJSON_AddItemToObject(root, "messages", msgs);

    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}

/* ---------- 解析响应 ---------- */
static int parse_response(const char* body, llm_response_t** out) {
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

    cJSON* created = cJSON_GetObjectItem(root, "created");
    if (cJSON_IsNumber(created)) {
        resp->created = (uint64_t)created->valuedouble;
    }

    cJSON* choices = cJSON_GetObjectItem(root, "choices");
    if (cJSON_IsArray(choices)) {
        int size = cJSON_GetArraySize(choices);
        resp->choice_count = (size_t)size;
        resp->choices = (llm_message_t*)calloc(size, sizeof(llm_message_t));

        if (resp->choices) {
            for (int i = 0; i < size; ++i) {
                cJSON* choice = cJSON_GetArrayItem(choices, i);
                cJSON* message = cJSON_GetObjectItem(choice, "message");
                if (message) {
                    cJSON* role = cJSON_GetObjectItem(message, "role");
                    cJSON* content = cJSON_GetObjectItem(message, "content");
                    if (cJSON_IsString(role) && role->valuestring) {
                        resp->choices[i].role = strdup(role->valuestring);
                    }
                    if (cJSON_IsString(content) && content->valuestring) {
                        resp->choices[i].content = strdup(content->valuestring);
                    }
                }
                cJSON* finish = cJSON_GetObjectItem(choice, "finish_reason");
                if (cJSON_IsString(finish) && finish->valuestring && !resp->finish_reason) {
                    resp->finish_reason = strdup(finish->valuestring);
                }
            }
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
    return AGENTOS_OK;
}

/* ---------- HTTP 请求 ---------- */
static int http_request(const char* url,
                        const char* body,
                        char** response_body,
                        long* status_code,
                        double timeout_sec) {
    if (!url || !response_body) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    *response_body = NULL;
    if (status_code) *status_code = 0;

    CURL* curl = curl_easy_init();
    if (!curl) {
        return AGENTOS_ERR_UNKNOWN;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long)timeout_sec);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    if (body) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
    }

    struct StringBuffer {
        char* data;
        size_t size;
        size_t capacity;
    } sb;
    memset(&sb, 0, sizeof(sb));

    auto size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
        size_t realsize = size * nmemb;
        struct StringBuffer* buf = (struct StringBuffer*)userp;

        size_t new_size = buf->size + realsize + 1;
        if (new_size > buf->capacity) {
            size_t new_cap = buf->capacity * 2;
            if (new_cap < new_size) new_cap = new_size;

            char* new_data = (char*)realloc(buf->data, new_cap);
            if (!new_data) return 0;

            buf->data = new_data;
            buf->capacity = new_cap;
        }

        memcpy(buf->data + buf->size, contents, realsize);
        buf->size += realsize;
        buf->data[buf->size] = '\0';

        return realsize;
    }

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &sb);

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        if (sb.data) free(sb.data);
        return AGENTOS_ERR_IO;
    }

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    if (status_code) *status_code = http_code;

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    *response_body = sb.data;
    return (http_code >= 200 && http_code < 300) ? AGENTOS_OK : AGENTOS_ERR_IO;
}

/* ---------- 提供商操作 ---------- */
static provider_ctx_t* local_init(const char* name,
                                   const char* api_key,
                                   const char* api_base,
                                   const char* organization,
                                   double timeout_sec,
                                   int max_retries) {
    (void)name; (void)api_key; (void)organization;

    local_ctx_t* ctx = (local_ctx_t*)calloc(1, sizeof(local_ctx_t));
    if (!ctx) return NULL;

    if (api_base) {
        strncpy(ctx->api_base, api_base, sizeof(ctx->api_base) - 1);
    } else {
        strncpy(ctx->api_base, "http://localhost:8080/v1", sizeof(ctx->api_base) - 1);
    }

    ctx->timeout_sec = timeout_sec > 0 ? timeout_sec : 60.0;
    ctx->max_retries = max_retries;

    return (provider_ctx_t*)ctx;
}

static void local_destroy(provider_ctx_t* ctx_ptr) {
    if (ctx_ptr) {
        free(ctx_ptr);
    }
}

static int local_complete(provider_ctx_t* ctx_ptr,
                          const llm_request_config_t* config,
                          llm_response_t** out_response) {
    if (!ctx_ptr || !config || !out_response) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    local_ctx_t* ctx = (local_ctx_t*)ctx_ptr;

    char* req_body = build_request(config);
    if (!req_body) {
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }

    char url[1024];
    snprintf(url, sizeof(url), "%s/chat/completions", ctx->api_base);

    char* resp_body = NULL;
    long status_code = 0;

    int ret = http_request(url, req_body, &resp_body, &status_code, ctx->timeout_sec);
    free(req_body);

    if (ret != AGENTOS_OK) {
        return ret;
    }

    if (status_code != 200) {
        free(resp_body);
        return AGENTOS_ERR_IO;
    }

    ret = parse_response(resp_body, out_response);
    free(resp_body);
    return ret;
}

static int local_complete_stream(provider_ctx_t* ctx_ptr,
                                 const llm_request_config_t* config,
                                 llm_stream_callback_t callback,
                                 void* callback_data,
                                 llm_response_t** out_response) {
    if (!ctx_ptr || !config || !callback) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    local_ctx_t* ctx = (local_ctx_t*)ctx_ptr;
    llm_request_config_t stream_cfg = *config;
    stream_cfg.stream = 1;

    char* req_body = build_request(&stream_cfg);
    if (!req_body) {
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }

    char url[1024];
    snprintf(url, sizeof(url), "%s/chat/completions", ctx->api_base);

    char* resp_body = NULL;
    long status_code = 0;

    int ret = http_request(url, req_body, &resp_body, &status_code, ctx->timeout_sec);
    free(req_body);

    if (ret != AGENTOS_OK) {
        return ret;
    }

    if (status_code != 200) {
        free(resp_body);
        return AGENTOS_ERR_IO;
    }

    if (callback && resp_body) {
        callback(resp_body, callback_data);
    }

    if (out_response) {
        *out_response = NULL;
    }
    free(resp_body);
    return AGENTOS_OK;
}

const provider_ops_t local_ops = {
    .init = local_init,
    .destroy = local_destroy,
    .complete = local_complete,
    .complete_stream = local_complete_stream
};