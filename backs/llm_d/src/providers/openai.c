/**
 * @file openai.c
 * @brief OpenAI 适配器实现
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
    char organization[256];
    double timeout_sec;
    int max_retries;
} openai_ctx_t;

/* ---------- 生命周期 ---------- */

static provider_ctx_t* openai_init(const char* name,
                                  const char* api_key,
                                  const char* api_base,
                                  const char* organization,
                                  double timeout_sec,
                                  int max_retries) {
    (void)name;
    
    openai_ctx_t* ctx = (openai_ctx_t*)calloc(1, sizeof(openai_ctx_t));
    if (!ctx) {
        return NULL;
    }

    if (api_key) {
        size_t key_len = strlen(api_key);
        if (key_len >= sizeof(ctx->api_key)) {
            free(ctx);
            return NULL;
        }
        memcpy(ctx->api_key, api_key, key_len + 1);
    }
    
    if (api_base) {
        size_t base_len = strlen(api_base);
        if (base_len >= sizeof(ctx->api_base)) {
            free(ctx);
            return NULL;
        }
        memcpy(ctx->api_base, api_base, base_len + 1);
    } else {
        const char* default_base = "https://api.openai.com/v1";
        memcpy(ctx->api_base, default_base, strlen(default_base) + 1);
    }
    
    if (organization) {
        size_t org_len = strlen(organization);
        if (org_len >= sizeof(ctx->organization)) {
            free(ctx);
            return NULL;
        }
        memcpy(ctx->organization, organization, org_len + 1);
    }
    
    ctx->timeout_sec = timeout_sec > 0 ? timeout_sec : 30.0;
    ctx->max_retries = max_retries > 0 ? max_retries : 3;
    
    return (provider_ctx_t*)ctx;
}

static void openai_destroy(provider_ctx_t* ctx_ptr) {
    if (ctx_ptr) {
        free(ctx_ptr);
    }
}

/* ---------- 构建请求体 ---------- */

static char* build_request(const llm_request_config_t* config) {
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

    cJSON* created = cJSON_GetObjectItem(root, "created");
    if (cJSON_IsNumber(created)) {
        resp->created = (uint64_t)created->valuedouble;
    }

    cJSON* choices = cJSON_GetObjectItem(root, "choices");
    if (cJSON_IsArray(choices)) {
        int size = cJSON_GetArraySize(choices);
        resp->choice_count = (size_t)size;
        resp->choices = (llm_message_t*)calloc(size, sizeof(llm_message_t));
        
        if (!resp->choices) {
            cJSON_Delete(root);
            llm_response_free(resp);
            return AGENTOS_ERR_OUT_OF_MEMORY;
        }
        
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

/* ---------- 辅助函数：发起 HTTP 请求 ---------- */

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
    
    /* 使用 libcurl 发起请求 */
    CURL* curl = curl_easy_init();
    if (!curl) {
        return AGENTOS_ERR_UNKNOWN;
    }
    
    /* 设置 URL */
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method);
    
    /* 设置超时 */
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    
    /* 设置 SSL */
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    
    /* 设置授权头 */
    struct curl_slist* headers = NULL;
    char auth_header[1024];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", api_key ? api_key : "");
    headers = curl_slist_append(headers, auth_header);
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    /* 设置请求体 */
    if (body) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
    }
    
    /* 设置写回调 */
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
    
    /* 执行请求 */
    CURLcode res = curl_easy_perform(curl);
    
    if (res != CURLE_OK) {
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        if (sb.data) free(sb.data);
        return AGENTOS_ERR_IO;
    }
    
    /* 获取状态码 */
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    if (status_code) *status_code = http_code;
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    *response_body = sb.data;
    
    return (http_code >= 200 && http_code < 300) ? AGENTOS_OK : AGENTOS_ERR_IO;
}

/* ---------- 同步完成 ---------- */

static int openai_complete(provider_ctx_t* ctx_ptr,
                           const llm_request_config_t* config,
                           llm_response_t** out_response) {
    if (!ctx_ptr || !config || !out_response) {
        return AGENTOS_ERR_INVALID_PARAM;
    }
    
    openai_ctx_t* ctx = (openai_ctx_t*)ctx_ptr;
    
    char* req_body = build_request(config);
    if (!req_body) {
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }

    char url[1024];
    snprintf(url, sizeof(url), "%s/chat/completions", ctx->api_base);

    char* resp_body = NULL;
    long status_code = 0;
    
    int ret = http_request("POST", url, ctx->api_key, req_body, &resp_body, &status_code);
    
    free(req_body);
    
    if (ret != AGENTOS_OK) {
        SVC_LOG_ERROR("openai: HTTP request failed, status=%ld", status_code);
        return ret;
    }

    if (status_code != 200) {
        SVC_LOG_ERROR("openai: HTTP error, status=%ld", status_code);
        free(resp_body);
        return AGENTOS_ERR_IO;
    }
    
    ret = parse_response(resp_body, out_response);
    free(resp_body);
    
    return ret;
}

/* ---------- 流式完成 ---------- */

static int openai_complete_stream(provider_ctx_t* ctx_ptr,
                                  const llm_request_config_t* config,
                                  llm_stream_callback_t callback,
                                  void* user_data,
                                  llm_response_t** out_response) {
    if (!ctx_ptr || !config || !callback) {
        return AGENTOS_ERR_INVALID_PARAM;
    }
    
    /* 流式暂未实现，使用同步版本 */
    SVC_LOG_WARN("OpenAI streaming not yet implemented, using synchronous mode");
    
    llm_response_t* resp = NULL;
    int ret = openai_complete(ctx_ptr, config, &resp);
    
    if (ret == AGENTOS_OK && resp && resp->choices && resp->choices[0].content) {
        /* 通过回调发送完整内容 */
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

const provider_ops_t openai_ops = {
    .init = openai_init,
    .destroy = openai_destroy,
    .complete = openai_complete,
    .complete_stream = openai_complete_stream
};
