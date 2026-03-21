/**
 * @file provider.c
 * @brief 提供商通用工具实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "provider.h"
#include "logger.h"
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <errno.h>

/* ---------- HTTP 回调 ---------- */
static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    http_buffer_t* mem = (http_buffer_t*)userp;

    char* ptr = realloc(mem->data, mem->size + realsize + 1);
    if (!ptr) return 0;  // 让 libcurl 报告错误

    mem->data = ptr;
    memcpy(&(mem->data[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->data[mem->size] = '\0';
    return realsize;
}

/* ---------- HTTP POST（带重试） ---------- */
int provider_http_post(const char* url,
                       struct curl_slist* headers,
                       const char* body,
                       double timeout_sec,
                       int max_retries,
                       http_buffer_t** out_response,
                       long* out_http_code) {
    if (!url || !body || !out_response || !out_http_code) {
        errno = EINVAL;
        return -1;
    }

    http_buffer_t* response = calloc(1, sizeof(http_buffer_t));
    if (!response) return -1;

    CURL* curl = NULL;
    int retry = 0;
    int success = -1;
    CURLcode res;
    long http_code = 0;

    while (retry <= max_retries) {
        curl = curl_easy_init();
        if (!curl) {
            AGENTOS_LOG_ERROR("provider_http_post: curl_easy_init failed");
            free(response->data);
            free(response);
            return -1;
        }

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long)timeout_sec);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
#ifdef SKIP_PEER_VERIFICATION
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
#endif

        res = curl_easy_perform(curl);
        if (res == CURLE_OK) {
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
            success = 0;
            curl_easy_cleanup(curl);
            break;
        }

        AGENTOS_LOG_WARN("provider_http_post: attempt %d failed: %s",
                         retry + 1, curl_easy_strerror(res));
        retry++;
        curl_easy_cleanup(curl);
        if (retry <= max_retries) {
            // 清空响应缓冲区以备下次
            free(response->data);
            response->data = NULL;
            response->size = 0;
        }
    }

    if (success != 0) {
        free(response->data);
        free(response);
        return -1;
    }

    *out_response = response;
    *out_http_code = http_code;
    return 0;
}

void provider_http_buffer_free(http_buffer_t* buf) {
    if (buf) {
        free(buf->data);
        free(buf);
    }
}

/* ---------- 构建 OpenAI 兼容请求 ---------- */
char* provider_build_openai_request(const llm_request_config_t* config) {
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "model", config->model);
    cJSON_AddNumberToObject(root, "temperature", config->temperature);
    if (config->top_p > 0) cJSON_AddNumberToObject(root, "top_p", config->top_p);
    if (config->max_tokens > 0) cJSON_AddNumberToObject(root, "max_tokens", config->max_tokens);
    if (config->stream) cJSON_AddBoolToObject(root, "stream", config->stream);
    if (config->presence_penalty != 0) cJSON_AddNumberToObject(root, "presence_penalty", config->presence_penalty);
    if (config->frequency_penalty != 0) cJSON_AddNumberToObject(root, "frequency_penalty", config->frequency_penalty);

    if (config->stop_count > 0) {
        cJSON* stop_array = cJSON_CreateArray();
        for (size_t i = 0; i < config->stop_count; i++) {
            cJSON_AddItemToArray(stop_array, cJSON_CreateString(config->stop[i]));
        }
        cJSON_AddItemToObject(root, "stop", stop_array);
    }

    cJSON* messages = cJSON_CreateArray();
    for (size_t i = 0; i < config->message_count; i++) {
        cJSON* msg = cJSON_CreateObject();
        cJSON_AddStringToObject(msg, "role", config->messages[i].role);
        cJSON_AddStringToObject(msg, "content", config->messages[i].content);
        cJSON_AddItemToArray(messages, msg);
    }
    cJSON_AddItemToObject(root, "messages", messages);

    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}

/* ---------- 解析 OpenAI 兼容响应 ---------- */
int provider_parse_openai_response(const char* body, llm_response_t** out_response) {
    cJSON* root = cJSON_Parse(body);
    if (!root) {
        errno = EINVAL;
        return -1;
    }

    llm_response_t* resp = calloc(1, sizeof(llm_response_t));
    if (!resp) {
        cJSON_Delete(root);
        return -1;
    }

    // 提取 id
    cJSON* id_item = cJSON_GetObjectItem(root, "id");
    if (cJSON_IsString(id_item)) resp->id = strdup(id_item->valuestring);

    // 提取 model
    cJSON* model_item = cJSON_GetObjectItem(root, "model");
    if (cJSON_IsString(model_item)) resp->model = strdup(model_item->valuestring);

    // 提取 created
    cJSON* created_item = cJSON_GetObjectItem(root, "created");
    if (cJSON_IsNumber(created_item)) resp->created = (uint64_t)created_item->valuedouble;

    // 提取 usage
    cJSON* usage = cJSON_GetObjectItem(root, "usage");
    if (usage) {
        cJSON* prompt = cJSON_GetObjectItem(usage, "prompt_tokens");
        cJSON* completion = cJSON_GetObjectItem(usage, "completion_tokens");
        cJSON* total = cJSON_GetObjectItem(usage, "total_tokens");
        if (cJSON_IsNumber(prompt)) resp->prompt_tokens = (uint32_t)prompt->valuedouble;
        if (cJSON_IsNumber(completion)) resp->completion_tokens = (uint32_t)completion->valuedouble;
        if (cJSON_IsNumber(total)) resp->total_tokens = (uint32_t)total->valuedouble;
    }

    // 提取 choices
    cJSON* choices_item = cJSON_GetObjectItem(root, "choices");
    if (cJSON_IsArray(choices_item)) {
        size_t count = cJSON_GetArraySize(choices_item);
        resp->choice_count = count;
        resp->choices = calloc(count, sizeof(llm_message_t));
        if (!resp->choices) {
            cJSON_Delete(root);
            llm_response_free(resp);
            return -1;
        }

        for (size_t i = 0; i < count; i++) {
            cJSON* choice = cJSON_GetArrayItem(choices_item, i);
            cJSON* message = cJSON_GetObjectItem(choice, "message");
            if (message) {
                cJSON* role = cJSON_GetObjectItem(message, "role");
                cJSON* content = cJSON_GetObjectItem(message, "content");
                if (cJSON_IsString(role)) {
                    resp->choices[i].role = strdup(role->valuestring);
                    // 注意：role 和 content 是 const char*，但这里分配内存，需要后续释放
                    // 我们使用 char* 强制转换，但 llm_message_t 中 role 是 const char*，实际上指向我们分配的动态内存
                    // 为了统一，我们保持 llm_message_t 中的指针为 const，但内部我们知道它可释放。
                    // 在 llm_response_free 中我们强制转换为可释放指针。
                }
                if (cJSON_IsString(content)) {
                    ((char**)&resp->choices[i].content)[0] = strdup(content->valuestring);
                }
            }
            // 提取 finish_reason（取第一个非空）
            cJSON* finish = cJSON_GetObjectItem(choice, "finish_reason");
            if (cJSON_IsString(finish) && !resp->finish_reason) {
                resp->finish_reason = strdup(finish->valuestring);
            }
        }
    }

    cJSON_Delete(root);
    *out_response = resp;
    return 0;
}

/* ---------- 提取 OpenAI 错误信息 ---------- */
char* provider_extract_openai_error(const char* body) {
    cJSON* root = cJSON_Parse(body);
    if (!root) return NULL;
    char* msg = NULL;
    cJSON* error = cJSON_GetObjectItem(root, "error");
    if (error) {
        cJSON* message = cJSON_GetObjectItem(error, "message");
        if (cJSON_IsString(message)) {
            msg = strdup(message->valuestring);
        }
    }
    cJSON_Delete(root);
    return msg;
}

/* ---------- 提供商注册表 ---------- */
static struct {
    const char* name;
    const provider_ops_t* ops;
} registry[] = {
    {"openai", provider_get_openai_ops()},
    {"anthropic", provider_get_anthropic_ops()},
    {"deepseek", provider_get_deepseek_ops()},
    {"local", provider_get_local_ops()},
    {NULL, NULL}
};

void provider_register_all(void) {
    // 注册表已静态初始化，无需操作
    AGENTOS_LOG_INFO("provider: registered %d providers",
                     (int)(sizeof(registry)/sizeof(registry[0])-1));
}

const provider_ops_t* provider_get_ops(const char* name) {
    for (int i = 0; registry[i].name; i++) {
        if (strcmp(registry[i].name, name) == 0) {
            return registry[i].ops;
        }
    }
    return NULL;
}