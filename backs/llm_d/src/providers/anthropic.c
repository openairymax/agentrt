/**
 * @file anthropic.c
 * @brief Anthropic 适配器实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 改进说明：
 * 1. 统一错误码为 AGENTOS_ERR_*
 * 2. 使用通用 HTTP 接口
 */

#include "provider.h"
#include "error.h"
#include "platform.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <cjson/cJSON.h>

/* ---------- 上下文 ---------- */
typedef struct {
    char api_key[512];
    char api_base[512];
    double timeout_sec;
    int max_retries;
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

    if (api_key) {
        strncpy(ctx->api_key, api_key, sizeof(ctx->api_key) - 1);
    }
    
    if (api_base) {
        strncpy(ctx->api_base, api_base, sizeof(ctx->api_base) - 1);
    } else {
        strcpy(ctx->api_base, "https://api.anthropic.com/v1");
    }
    
    ctx->timeout_sec = timeout_sec > 0 ? timeout_sec : 30.0;
    ctx->max_retries = max_retries > 0 ? max_retries : 3;
    
    return (provider_ctx_t*)ctx;
}

static void anthropic_destroy(provider_ctx_t* ctx_ptr) {
    if (ctx_ptr) {
        free(ctx_ptr);
    }
}

/* ---------- 构建请求体 ---------- */

static char* build_request(const llm_request_config_t* config) {
    cJSON* root = cJSON_CreateObject();
    if (!root) return NULL;

    cJSON_AddStringToObject(root, "model", config->model ? config->model : "claude-3-sonnet-20240229");
    cJSON_AddNumberToObject(root, "temperature", config->temperature > 0 ? config->temperature : 0.7);
    
    if (config->max_tokens > 0) {
        cJSON_AddNumberToObject(root, "max_tokens", config->max_tokens);
    }
    
    if (config->stream) {
        cJSON_AddBoolToObject(root, "stream", 1);
    }

    char* system_prompt = NULL;
    cJSON* messages = cJSON_CreateArray();
    
    for (size_t i = 0; i < config->message_count; ++i) {
        if (config->messages[i].role && strcmp(config->messages[i].role, "system") == 0) {
            system_prompt = strdup(config->messages[i].content ? config->messages[i].content : "");
        } else {
            cJSON* msg = cJSON_CreateObject();
            const char* role = config->messages[i].role ? config->messages[i].role : "user";
            cJSON_AddStringToObject(msg, "role", role);
            cJSON_AddStringToObject(msg, "content", config->messages[i].content ? config->messages[i].content : "");
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

/* ---------- 解析非流式响应 ---------- */

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

    cJSON* content = cJSON_GetObjectItem(root, "content");
    if (cJSON_IsArray(content) && cJSON_GetArraySize(content) > 0) {
        cJSON* first = cJSON_GetArrayItem(content, 0);
        cJSON* text = cJSON_GetObjectItem(first, "text");
        if (cJSON_IsString(text) && text->valuestring) {
            resp->choice_count = 1;
            resp->choices = (llm_message_t*)calloc(1, sizeof(llm_message_t));
            if (resp->choices) {
                resp->choices[0].role = strdup("assistant");
                resp->choices[0].content = strdup(text->valuestring);
            }
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

/* ---------- HTTP 请求（使用 libcurl） ---------- */

static int http_request(const char* method,
                       const char* url,
                       const char* api_key,
                       const char* body,
                       char** response_body,
                       long* status_code) {
    if (!method || !url || !response_body) {
        return AGENTOS_ERR_INVALID_PARAM;
    }
    
    *response_body = NULL;
    if (status_code) *status_code = 0;
    
    CURL* curl = curl_easy_init();
    if (!curl) {
        return AGENTOS_ERR_UNKNOWN;
    }
    
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    
    struct curl_slist* headers = NULL;
    char auth_header[1024];
    snprintf(auth_header, sizeof(auth_header), "x-api-key: %s", api_key ? api_key : "");
    headers = curl_slist_append(headers, auth_header);
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "anthropic-version: 2023-06-01");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    if (body) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
    }
    
    struct StringBuffer {
        char* data;
        size_t size;
        size_t capacity;
    } sb = {0};
    
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

/* ---------- 同步完成 ---------- */

static int anthropic_complete(provider_ctx_t* ctx_ptr,
                              const llm_request_config_t* config,
                              llm_response_t** out_response) {
    if (!ctx_ptr || !config || !out_response) {
        return AGENTOS_ERR_INVALID_PARAM;
    }
    
    anthropic_ctx_t* ctx = (anthropic_ctx_t*)ctx_ptr;
    
    char* req_body = build_request(config);
    if (!req_body) {
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }

    char url[1024];
    snprintf(url, sizeof(url), "%s/messages", ctx->api_base);

    char* resp_body = NULL;
    long status_code = 0;
    
    int ret = http_request("POST", url, ctx->api_key, req_body, &resp_body, &status_code);
    free(req_body);
    
    if (ret != AGENTOS_OK) {
        return ret;
    }

    ret = parse_response(resp_body, out_response);
    free(resp_body);
    
    return ret;
}

/* ---------- 流式完成 ---------- */

static int anthropic_complete_stream(provider_ctx_t* ctx_ptr,
                                    const llm_request_config_t* config,
                                    llm_stream_callback_t callback,
                                    void* user_data,
                                    llm_response_t** out_response) {
    if (!ctx_ptr || !config || !callback) {
        return AGENTOS_ERR_INVALID_PARAM;
    }
    
    /* 流式暂未实现，使用同步版本 */
    llm_response_t* resp = NULL;
    int ret = anthropic_complete(ctx_ptr, config, &resp);
    
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
    .complete_stream = anthropic_complete_stream
};
