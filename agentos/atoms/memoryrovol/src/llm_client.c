/**
 * @file llm_client.c
 * @brief LLM 客户端实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * @details
 * 实现与大语言模型的交互，支持：
 * - OpenAI API 兼容接口
 * - DeepSeek 等国产模型
 * - 本地模型服务
 */

#include "../include/llm_client.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include <agentos/memory_compat.h>
#include <agentos/string_compat.h>
#include <string.h>
#include <stdio.h>

/* CURL HTTP 客户端（与 CMakeLists.txt 中的 AGENTOS_HAS_CURL 保持一致） */
#ifdef AGENTOS_HAS_CURL
#include <curl/curl.h>
#else
#include "../include/curl_stub.h"
#endif /* AGENTOS_HAS_CURL */

/* JSON 解析 */
#ifdef AGENTOS_HAS_CJSON
#include <cjson/cJSON.h>
#else
/* cJSON stub - 简化实现（仅用于编译通过） */
typedef struct cJSON {
    int type;
    char* valuestring;
    struct cJSON* child;
    struct cJSON* next;
    struct cJSON* prev;
} cJSON;

#define cJSON_NULL 0
#define cJSON_False 1
#define cJSON_True 2
#define cJSON_Number 3
#define cJSON_String 4
#define cJSON_Array 5
#define cJSON_Object 6

static inline cJSON* cJSON_CreateObject(void) { return NULL; }
static inline void cJSON_AddStringToObject(cJSON* object, const char* name, const char* string) { (void)object; (void)name; (void)string; }
static inline char* cJSON_PrintUnformatted(const cJSON* item) { (void)item; return NULL; }
static inline void cJSON_Delete(cJSON* item) { (void)item; }
#endif /* AGENTOS_HAS_CJSON */

/**
 * @brief 内存缓冲区（用于 HTTP 响应）
 */
typedef struct {
    char* data;
    size_t size;
} memory_buffer_t;

/**
 * @brief LLM 服务内部结构
 */
struct agentos_llm_service {
    agentos_llm_config_t config;    /**< 配置 */
    CURL* curl;                     /**< CURL 句柄 */
    int initialized;                /**< 初始化标志 */
};

/**
 * @brief HTTP 响应回调函数
 */
static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    memory_buffer_t* mem = (memory_buffer_t*)userp;

    char* ptr = (char*)AGENTOS_REALLOC(mem->data, mem->size + realsize + 1);
    if (!ptr) {
        return 0;
    }

    mem->data = ptr;
    memcpy(&(mem->data[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->data[mem->size] = 0;

    return realsize;
}

/**
 * @brief 创建 LLM 服务
 */
agentos_error_t agentos_llm_service_create(
    const agentos_llm_config_t* config,
    agentos_llm_service_t** out_service)
{
    if (!config || !out_service) {
        AGENTOS_LOG_ERROR("Invalid parameters to llm_service_create");
        return AGENTOS_EINVAL;
    }

    if (!config->api_key || !config->base_url) {
        AGENTOS_LOG_ERROR("API key and base URL are required");
        return AGENTOS_EINVAL;
    }

    agentos_llm_service_t* service = 
        (agentos_llm_service_t*)AGENTOS_CALLOC(1, sizeof(agentos_llm_service_t));
    if (!service) {
        return AGENTOS_ENOMEM;
    }

    /* 复制配置 */
    service->config.model_name = config->model_name ? AGENTOS_STRDUP(config->model_name) : AGENTOS_STRDUP("gpt-3.5-turbo");
    service->config.api_key = AGENTOS_STRDUP(config->api_key);
    service->config.base_url = AGENTOS_STRDUP(config->base_url);
    service->config.timeout_ms = config->timeout_ms > 0 ? config->timeout_ms : 30000;
    service->config.temperature = config->temperature > 0 ? config->temperature : 0.7f;
    service->config.max_tokens = config->max_tokens > 0 ? config->max_tokens : 2048;

    /* 初始化 CURL（支持降级模式） */
    CURL* curl = curl_easy_init();
    if (!curl) {
        AGENTOS_LOG_WARN("CURL initialization failed - running in degraded mode (no LLM support)");
        /* 降级模式：继续创建服务但标记为无CURL支持 */
        service->curl = NULL;
        service->initialized = 1;  /* 仍然标记为初始化，但功能受限 */
        *out_service = service;
        AGENTOS_LOG_INFO("LLM service created in degraded mode (model: %s)", service->config.model_name);
        return AGENTOS_SUCCESS;
    }

    service->curl = curl;
    service->initialized = 1;
    *out_service = service;

    AGENTOS_LOG_INFO("LLM service created for model: %s", service->config.model_name);
    return AGENTOS_SUCCESS;
}

/**
 * @brief 完整LLM调用接口
 */
agentos_error_t agentos_llm_complete(
    agentos_llm_service_t* service,
    const agentos_llm_request_t* request,
    agentos_llm_response_t** out_response)
{
    if (!service || !request || !out_response) {
        return AGENTOS_EINVAL;
    }

    if (!service->initialized) {
        return AGENTOS_ENOTINIT;
    }

    /* 降级模式检查：如果没有CURL支持，返回降级响应 */
    if (!service->curl) {
        AGENTOS_LOG_WARN("LLM complete attempted in degraded mode (no CURL support)");
        agentos_llm_response_t* resp = (agentos_llm_response_t*)AGENTOS_CALLOC(1, sizeof(agentos_llm_response_t));
        if (!resp) {
            return AGENTOS_ENOMEM;
        }
        resp->text = AGENTOS_STRDUP("LLM service unavailable (degraded mode)");
        if (!resp->text) {
            AGENTOS_FREE(resp);
            return AGENTOS_ENOMEM;
        }
        resp->usage_tokens = 0;
        resp->total_tokens = 0;
        resp->finish_reason = 0;
        *out_response = resp;
        return AGENTOS_SUCCESS;
    }

    /* 使用现有的agentos_llm_service_call实现作为基础 */
    char* response_text = NULL;
    agentos_error_t err = agentos_llm_service_call(service, request->prompt, &response_text);
    if (err != AGENTOS_SUCCESS) {
        return err;
    }

    agentos_llm_response_t* resp = (agentos_llm_response_t*)AGENTOS_CALLOC(1, sizeof(agentos_llm_response_t));
    if (!resp) {
        AGENTOS_FREE(response_text);
        return AGENTOS_ENOMEM;
    }

    resp->text = response_text;
    resp->usage_tokens = strlen(response_text) / 4; /* 近似估算：平均每个token 4个字符 */
    resp->total_tokens = resp->usage_tokens + 100;  /* 粗略估算总token数 */
    resp->finish_reason = 1;  /* 假设正常完成 */

    *out_response = resp;
    return AGENTOS_SUCCESS;
}

/**
 * @brief 释放LLM响应
 */
void agentos_llm_response_free(agentos_llm_response_t* response) {
    if (!response) return;
    
    if (response->text) {
        AGENTOS_FREE(response->text);
    }
    AGENTOS_FREE(response);
}

/**
 * @brief 销毁 LLM 服务
 */
void agentos_llm_service_destroy(agentos_llm_service_t* service) {
    if (!service) return;

    if (service->curl) {
        curl_easy_cleanup(service->curl);
    }

    if (service->config.model_name) AGENTOS_FREE((void*)service->config.model_name);
    if (service->config.api_key) AGENTOS_FREE((void*)service->config.api_key);
    if (service->config.base_url) AGENTOS_FREE((void*)service->config.base_url);

    AGENTOS_FREE(service);
}

/**
 * @brief 调用 LLM 生成响应
 */
agentos_error_t agentos_llm_service_call(
    agentos_llm_service_t* service,
    const char* prompt,
    char** out_response)
{
    if (!service || !prompt || !out_response) {
        return AGENTOS_EINVAL;
    }

    if (!service->initialized) {
        return AGENTOS_ENOTINIT;
    }

    /* 降级模式检查：如果没有CURL支持，返回错误 */
    if (!service->curl) {
        AGENTOS_LOG_WARN("LLM service call attempted in degraded mode (no CURL support)");
        /* 返回一个友好的降级响应而不是错误 */
        *out_response = AGENTOS_STRDUP("LLM service unavailable (running in degraded mode)");
        return *out_response ? AGENTOS_SUCCESS : AGENTOS_ENOMEM;
    }

    /* 构建请求 URL */
    char url[512];
    snprintf(url, sizeof(url), "%s/chat/completions", service->config.base_url);

    /* 构建请求 JSON */
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "model", service->config.model_name);
    cJSON_AddNumberToObject(root, "temperature", service->config.temperature);
    cJSON_AddNumberToObject(root, "max_tokens", service->config.max_tokens);

    /* 构建消息数组 */
    cJSON* messages = cJSON_AddArrayToObject(root, "messages");
    cJSON* message = cJSON_CreateObject();
    cJSON_AddStringToObject(message, "role", "user");
    cJSON_AddStringToObject(message, "content", prompt);
    cJSON_AddItemToArray(messages, message);

    char* request_json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (!request_json) {
        return AGENTOS_ENOMEM;
    }

    /* 准备 HTTP 请求 */
    curl_easy_reset(service->curl);
    curl_easy_setopt(service->curl, CURLOPT_URL, url);
    curl_easy_setopt(service->curl, CURLOPT_POSTFIELDS, request_json);

    /* 设置请求头 */
    struct curl_slist* headers = NULL;
    char auth_header[256];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", service->config.api_key);
    headers = curl_slist_append(headers, auth_header);
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(service->curl, CURLOPT_HTTPHEADER, headers);

    /* 设置超时 */
    curl_easy_setopt(service->curl, CURLOPT_TIMEOUT_MS, (long)service->config.timeout_ms);

    /* 准备响应缓冲区 */
    memory_buffer_t response = {0};
    curl_easy_setopt(service->curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(service->curl, CURLOPT_WRITEDATA, (void*)&response);

    /* 执行请求 */
    CURLcode res = curl_easy_perform(service->curl);
    curl_slist_free_all(headers);
    AGENTOS_FREE(request_json);

    if (res != CURLE_OK) {
        AGENTOS_LOG_ERROR("LLM service call failed: %s", curl_easy_strerror(res));
        if (response.data) AGENTOS_FREE(response.data);
        return AGENTOS_ERROR;
    }

    /* 检查 HTTP 状态码 */
    long response_code = 0;
    curl_easy_getinfo(service->curl, CURLINFO_RESPONSE_CODE, &response_code);
    if (response_code != 200) {
        AGENTOS_LOG_ERROR("LLM service returned error code: %ld", response_code);
        if (response.data) AGENTOS_FREE(response.data);
        return AGENTOS_ERROR;
    }

    /* 解析响应 JSON */
    cJSON* response_json = cJSON_Parse(response.data);
    AGENTOS_FREE(response.data);

    if (!response_json) {
        AGENTOS_LOG_ERROR("Failed to parse LLM response");
        return AGENTOS_ERROR;
    }

    /* 提取响应内容 */
    cJSON* choices = cJSON_GetObjectItemCaseSensitive(response_json, "choices");
    if (!choices || !cJSON_IsArray(choices) || cJSON_GetArraySize(choices) == 0) {
        AGENTOS_LOG_ERROR("No choices in LLM response");
        cJSON_Delete(response_json);
        return AGENTOS_ERROR;
    }

    cJSON* first_choice = cJSON_GetArrayItem(choices, 0);
    cJSON* message_obj = cJSON_GetObjectItemCaseSensitive(first_choice, "message");
    if (!message_obj) {
        AGENTOS_LOG_ERROR("No message in LLM response");
        cJSON_Delete(response_json);
        return AGENTOS_ERROR;
    }

    cJSON* content = cJSON_GetObjectItemCaseSensitive(message_obj, "content");
    if (!content || !cJSON_IsString(content)) {
        AGENTOS_LOG_ERROR("No content in LLM response");
        cJSON_Delete(response_json);
        return AGENTOS_ERROR;
    }

    *out_response = AGENTOS_STRDUP(content->valuestring);
    cJSON_Delete(response_json);

    if (!*out_response) {
        return AGENTOS_ENOMEM;
    }

    return AGENTOS_SUCCESS;
}
