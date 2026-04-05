/**
 * @file provider.c
 * @brief 提供商通用工具实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 改进说明：
 * 1. 提取公共代码到通用函数
 * 2. 提供 OpenAI 兼容的请求构建和响应解析
 * 3. 统一的 HTTP 请求处理
 */

#include "provider.h"
#include "svc_logger.h"
#include "error.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <curl/curl.h>
#include <errno.h>
#include <cjson/cJSON.h>

/* ---------- 通用上下文初始化 ---------- */

void provider_base_init(provider_base_ctx_t* base_ctx,
                        const char* api_key,
                        const char* api_base,
                        const char* organization,
                        double timeout_sec,
                        int max_retries,
                        const char* default_base) {
    if (!base_ctx) return;

    memset(base_ctx, 0, sizeof(provider_base_ctx_t));

    if (api_key) {
        size_t key_len = strlen(api_key);
        if (key_len < sizeof(base_ctx->api_key)) {
            memcpy(base_ctx->api_key, api_key, key_len + 1);
        }
    }

    if (api_base) {
        size_t base_len = strlen(api_base);
        if (base_len < sizeof(base_ctx->api_base)) {
            memcpy(base_ctx->api_base, api_base, base_len + 1);
        }
    } else if (default_base) {
        size_t default_len = strlen(default_base);
        if (default_len < sizeof(base_ctx->api_base)) {
            memcpy(base_ctx->api_base, default_base, default_len + 1);
        }
    }

    if (organization) {
        size_t org_len = strlen(organization);
        if (org_len < sizeof(base_ctx->organization)) {
            memcpy(base_ctx->organization, organization, org_len + 1);
        }
    }

    base_ctx->timeout_sec = timeout_sec > 0 ? timeout_sec : 30.0;
    base_ctx->max_retries = max_retries > 0 ? max_retries : 3;
}

/* ---------- HTTP 响应管理 ---------- */

void provider_http_resp_free(provider_http_resp_t* resp) {
    if (resp) {
        free(resp->data);
        free(resp);
    }
}

/* ---------- HTTP 回调 ---------- */

static size_t http_write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    provider_http_resp_t* mem = (provider_http_resp_t*)userp;

    size_t new_size = mem->size + realsize + 1;
    if (new_size > mem->capacity) {
        size_t new_cap = mem->capacity * 2;
        if (new_cap < new_size) new_cap = new_size;

        char* ptr = (char*)realloc(mem->data, new_cap);
        if (!ptr) return 0;

        mem->data = ptr;
        mem->capacity = new_cap;
    }

    memcpy(&(mem->data[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->data[mem->size] = '\0';
    return realsize;
}

/* ---------- HTTP POST 实现 ---------- */

int provider_http_post(const char* url,
                       struct curl_slist* headers,
                       const char* body,
                       double timeout_sec,
                       int max_retries,
                       provider_http_resp_t** out_response,
                       long* out_http_code) {
    if (!url || !body || !out_response || !out_http_code) {
        errno = EINVAL;
        return AGENTOS_ERR_INVALID_PARAM;
    }

    provider_http_resp_t* resp = (provider_http_resp_t*)calloc(1, sizeof(provider_http_resp_t));
    if (!resp) return AGENTOS_ERR_OUT_OF_MEMORY;

    CURL* curl = NULL;
    int retry = 0;
    int success = -1;
    CURLcode res;
    long http_code = 0;

    while (retry <= max_retries) {
        curl = curl_easy_init();
        if (!curl) {
            SVC_LOG_ERROR("provider_http_post: curl_easy_init failed");
            provider_http_resp_free(resp);
            return AGENTOS_ERR_UNKNOWN;
        }

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, http_write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, resp);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long)timeout_sec);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

        res = curl_easy_perform(curl);
        if (res == CURLE_OK) {
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
            success = 0;
            curl_easy_cleanup(curl);
            break;
        }

        SVC_LOG_WARN("provider_http_post: attempt %d failed: %s",
                         retry + 1, curl_easy_strerror(res));
        retry++;
        curl_easy_cleanup(curl);
        if (retry <= max_retries) {
            free(resp->data);
            resp->data = NULL;
            resp->size = 0;
            resp->capacity = 0;
        }
    }

    if (success != 0) {
        provider_http_resp_free(resp);
        return AGENTOS_ERR_IO;
    }

    *out_response = resp;
    *out_http_code = http_code;
    return AGENTOS_OK;
}

/* ---------- 通用请求构建 ---------- */

char* provider_build_openai_request(const llm_request_config_t* manager,
                                     const char* default_model) {
    if (!manager) return NULL;

    cJSON* root = cJSON_CreateObject();
    if (!root) return NULL;

    const char* model = manager->model && manager->model[0] ? manager->model : default_model;
    cJSON_AddStringToObject(root, "model", model ? model : "gpt-3.5-turbo");
    cJSON_AddNumberToObject(root, "temperature",
                          manager->temperature > 0 ? manager->temperature : 0.7);

    if (manager->top_p > 0) {
        cJSON_AddNumberToObject(root, "top_p", manager->top_p);
    }

    if (manager->max_tokens > 0) {
        cJSON_AddNumberToObject(root, "max_tokens", manager->max_tokens);
    }

    if (manager->stream) {
        cJSON_AddBoolToObject(root, "stream", 1);
    }

    if (manager->presence_penalty != 0) {
        cJSON_AddNumberToObject(root, "presence_penalty", manager->presence_penalty);
    }

    if (manager->frequency_penalty != 0) {
        cJSON_AddNumberToObject(root, "frequency_penalty", manager->frequency_penalty);
    }

    if (manager->stop_count > 0 && manager->stop) {
        cJSON* stop = cJSON_CreateArray();
        for (size_t i = 0; i < manager->stop_count; ++i) {
            cJSON_AddItemToArray(stop, cJSON_CreateString(manager->stop[i]));
        }
        cJSON_AddItemToObject(root, "stop", stop);
    }

    cJSON* msgs = cJSON_CreateArray();
    for (size_t i = 0; i < manager->message_count; ++i) {
        cJSON* msg = cJSON_CreateObject();
        const char* role = manager->messages[i].role ? manager->messages[i].role : "user";
        const char* content = manager->messages[i].content ? manager->messages[i].content : "";
        cJSON_AddStringToObject(msg, "role", role);
        cJSON_AddStringToObject(msg, "content", content);
        cJSON_AddItemToArray(msgs, msg);
    }
    cJSON_AddItemToObject(root, "messages", msgs);

    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}

/* ---------- 通用响应解析 ---------- */

int provider_parse_openai_response(const char* body, llm_response_t** out) {
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
        resp->choices = (llm_message_t*)calloc((size_t)size, sizeof(llm_message_t));

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
