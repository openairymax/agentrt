// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: Apache-2.0
/**
 * @file china_eco_adapter.c
 * @brief Chinese LLM Ecosystem Unified Compatibility Adapter Implementation
 *
 * 国内大模型生态统一兼容适配器实现。
 * 所有提供商均兼容 OpenAI API 格式，通过差异化配置实现统一接入。
 *
 * BAN-19 合规：无 curl 时 fail-closed，不使用 mock/模板生成假响应。
 *
 * @since 2.1.0
 */

#include "china_eco_adapter.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <ctype.h>

#ifdef AGENTOS_HAS_CURL
#include <curl/curl.h>
#include <cjson/cJSON.h>
#endif

typedef struct {
    china_provider_t provider;
    const char* name;
    const char* base_url;
    const char* default_model;
    china_auth_type_t auth_type;
    const char* auth_header_name;
    const char* models[8];
    int model_count;
} china_provider_info_t;

static const china_provider_info_t g_providers[] = {
    {
        CHINA_PROVIDER_DEEPSEEK, "DeepSeek",
        "https://api.deepseek.com/v1",
        "deepseek-chat",
        CHINA_AUTH_BEARER, "Authorization",
        {"deepseek-chat", "deepseek-reasoner", "deepseek-coder", NULL},
        3
    },
    {
        CHINA_PROVIDER_QWEN, "Qwen",
        "https://dashscope.aliyuncs.com/compatible-mode/v1",
        "qwen-plus",
        CHINA_AUTH_BEARER, "Authorization",
        {"qwen-plus", "qwen-turbo", "qwen-max", "qwen-long", "qwen-vl-plus", NULL},
        5
    },
    {
        CHINA_PROVIDER_WENXIN, "Wenxin",
        "https://aip.baidubce.com/rpc/2.0/ai_custom/v1/wenxinworkshop",
        "ernie-4.0-8k",
        CHINA_AUTH_BEARER, "Authorization",
        {"ernie-4.0-8k", "ernie-3.5-8k", "ernie-speed-8k", "ernie-lite-8k", NULL},
        4
    },
    {
        CHINA_PROVIDER_CHATGLM, "ChatGLM",
        "https://open.bigmodel.cn/api/paas/v4",
        "glm-4-plus",
        CHINA_AUTH_BEARER, "Authorization",
        {"glm-4-plus", "glm-4-flash", "glm-4-long", "glm-4v-plus", NULL},
        4
    },
    {
        CHINA_PROVIDER_SPARK, "Spark",
        "https://spark-api-open.xf-yun.com/v1",
        "generalv3.5",
        CHINA_AUTH_BEARER, "Authorization",
        {"generalv3.5", "generalv3", "4.0Ultra", NULL},
        3
    },
    {
        CHINA_PROVIDER_BAICHUAN, "Baichuan",
        "https://api.baichuan-ai.com/v1",
        "Baichuan4",
        CHINA_AUTH_BEARER, "Authorization",
        {"Baichuan4", "Baichuan3-Turbo", "Baichuan3-Turbo-128k", NULL},
        3
    },
    {
        CHINA_PROVIDER_DOUBAO, "Doubao",
        "https://ark.cn-beijing.volces.com/api/v3",
        "doubao-pro-32k",
        CHINA_AUTH_BEARER, "Authorization",
        {"doubao-pro-32k", "doubao-pro-128k", "doubao-lite-32k", NULL},
        3
    },
    {
        CHINA_PROVIDER_KIMI, "Kimi",
        "https://api.moonshot.cn/v1",
        "moonshot-v1-8k",
        CHINA_AUTH_BEARER, "Authorization",
        {"moonshot-v1-8k", "moonshot-v1-32k", "moonshot-v1-128k", NULL},
        3
    },
    {
        CHINA_PROVIDER_MINIMAX, "MiniMax",
        "https://api.minimax.chat/v1",
        "abab6.5s-chat",
        CHINA_AUTH_BEARER, "Authorization",
        {"abab6.5s-chat", "abab6.5-chat", "abab5.5-chat", NULL},
        3
    },
    {
        CHINA_PROVIDER_VOLCENGINE, "Volcengine",
        "https://ark.cn-beijing.volces.com/api/v3",
        "doubao-pro-32k",
        CHINA_AUTH_BEARER, "Authorization",
        {"doubao-pro-32k", "doubao-pro-128k", "doubao-lite-32k", NULL},
        3
    },
};

static const int g_provider_count = sizeof(g_providers) / sizeof(g_providers[0]);

static const china_provider_info_t* china_find_provider(china_provider_t provider) {
    for (int i = 0; i < g_provider_count; i++) {
        if (g_providers[i].provider == provider)
            return &g_providers[i];
    }
    return &g_providers[0];
}

const char* china_eco_get_provider_name(china_provider_t provider) {
    return china_find_provider(provider)->name;
}

const char* china_eco_get_provider_base_url(china_provider_t provider) {
    return china_find_provider(provider)->base_url;
}

const char* china_eco_get_provider_default_model(china_provider_t provider) {
    return china_find_provider(provider)->default_model;
}

#ifdef AGENTOS_HAS_CURL
typedef struct {
    char* data;
    size_t size;
} china_curl_buffer_t;

static size_t china_curl_write_cb(void* ptr, size_t size, size_t nmemb, void* userdata) {
    china_curl_buffer_t* buf = (china_curl_buffer_t*)userdata;
    size_t total = size * nmemb;
    char* new_data = (char*)realloc(buf->data, buf->size + total + 1);
    if (!new_data) return 0;
    buf->data = new_data;
    memcpy(buf->data + buf->size, ptr, total);
    buf->size += total;
    buf->data[buf->size] = '\0';
    return total;
}

static int china_api_call(const char* api_key, const char* base_url,
                           const char* auth_header_name,
                           china_auth_type_t auth_type,
                           const char* endpoint,
                           const char* request_json,
                           char* out_buf, size_t buf_len) {
    if (!api_key || !request_json || !out_buf) return -1;

    CURL* curl = curl_easy_init();
    if (!curl) return -1;

    china_curl_buffer_t response_buf = { .data = NULL, .size = 0 };

    char url[1024];
    snprintf(url, sizeof(url), "%s%s",
             base_url ? base_url : "https://api.deepseek.com/v1",
             endpoint ? endpoint : "/chat/completions");

    struct curl_slist* headers = NULL;
    char auth_header[512];
    if (auth_type == CHINA_AUTH_BEARER) {
        snprintf(auth_header, sizeof(auth_header), "%s: Bearer %s",
                 auth_header_name ? auth_header_name : "Authorization", api_key);
    } else {
        snprintf(auth_header, sizeof(auth_header), "%s: %s",
                 auth_header_name ? auth_header_name : "Authorization", api_key);
    }
    headers = curl_slist_append(headers, auth_header);
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_json);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, china_curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_buf);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        free(response_buf.data);
        return -1;
    }

    if (http_code == 200 && response_buf.data) {
        size_t copy_len = response_buf.size;
        if (copy_len >= buf_len) copy_len = buf_len - 1;
        memcpy(out_buf, response_buf.data, copy_len);
        out_buf[copy_len] = '\0';
        free(response_buf.data);
        return (int)copy_len;
    }

    free(response_buf.data);
    return -1;
}

static int china_parse_chat_response(const char* json_str,
                                      char* content_out, size_t content_len,
                                      char* model_out, size_t model_len,
                                      char* finish_out, size_t finish_len,
                                      int* prompt_tokens, int* completion_tokens,
                                      int* total_tokens) {
    if (!json_str || !content_out) return -1;
    cJSON* root = cJSON_Parse(json_str);
    if (!root) return -2;

    int result = -3;
    cJSON* choices = cJSON_GetObjectItem(root, "choices");
    if (choices && cJSON_IsArray(choices) && cJSON_GetArraySize(choices) > 0) {
        cJSON* first = cJSON_GetArrayItem(choices, 0);
        cJSON* message = cJSON_GetObjectItem(first, "message");
        if (message) {
            cJSON* content = cJSON_GetObjectItem(message, "content");
            if (content && content->valuestring) {
                strncpy(content_out, content->valuestring, content_len - 1);
                content_out[content_len - 1] = '\0';
                result = 0;
            }
        }
        cJSON* fr = cJSON_GetObjectItem(first, "finish_reason");
        if (fr && fr->valuestring && finish_out) {
            strncpy(finish_out, fr->valuestring, finish_len - 1);
            finish_out[finish_len - 1] = '\0';
        }
    }

    cJSON* model_obj = cJSON_GetObjectItem(root, "model");
    if (model_obj && model_obj->valuestring && model_out) {
        strncpy(model_out, model_obj->valuestring, model_len - 1);
        model_out[model_len - 1] = '\0';
    }

    cJSON* usage = cJSON_GetObjectItem(root, "usage");
    if (usage) {
        cJSON* pt = cJSON_GetObjectItem(usage, "prompt_tokens");
        cJSON* ct = cJSON_GetObjectItem(usage, "completion_tokens");
        cJSON* tt = cJSON_GetObjectItem(usage, "total_tokens");
        if (prompt_tokens) *prompt_tokens = pt ? pt->valueint : 0;
        if (completion_tokens) *completion_tokens = ct ? ct->valueint : 0;
        if (total_tokens) *total_tokens = tt ? tt->valueint : 0;
    }

    cJSON_Delete(root);
    return result;
}
#endif

struct china_eco_context_s {
    china_eco_config_t config;
    const china_provider_info_t* provider_info;
    bool initialized;
    uint64_t total_requests;
    uint64_t total_tokens_in;
    uint64_t total_tokens_out;
};

china_eco_config_t china_eco_config_default(china_provider_t provider) {
    const china_provider_info_t* info = china_find_provider(provider);
    china_eco_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.provider = provider;
    cfg.base_url = strdup(info->base_url);
    cfg.default_model = strdup(info->default_model);
    cfg.auth_type = info->auth_type;
    cfg.auth_header_name = strdup(info->auth_header_name);
    cfg.max_tokens = 4096;
    cfg.temperature = 0.7;
    cfg.top_p = 1.0;
    cfg.enable_streaming = true;
    cfg.enable_function_calling = true;
    cfg.timeout_ms = CHINA_ECO_DEFAULT_TIMEOUT_MS;
    cfg.max_retries = CHINA_ECO_MAX_RETRIES;
    return cfg;
}

china_eco_context_t* china_eco_context_create(const china_eco_config_t* config) {
    if (!config) return NULL;

    china_eco_context_t* ctx = calloc(1, sizeof(china_eco_context_t));
    if (!ctx) return NULL;

    memcpy(&ctx->config, config, sizeof(china_eco_config_t));
    if (config->api_key) ctx->config.api_key = strdup(config->api_key);
    if (config->base_url) ctx->config.base_url = strdup(config->base_url);
    if (config->default_model) ctx->config.default_model = strdup(config->default_model);
    if (config->auth_header_name) ctx->config.auth_header_name = strdup(config->auth_header_name);
    if (config->organization) ctx->config.organization = strdup(config->organization);

    ctx->provider_info = china_find_provider(config->provider);
    ctx->initialized = true;
    ctx->total_requests = 0;
    ctx->total_tokens_in = 0;
    ctx->total_tokens_out = 0;

    return ctx;
}

void china_eco_context_destroy(china_eco_context_t* ctx) {
    if (!ctx) return;
    if (ctx->config.api_key) {
        size_t key_len = strlen(ctx->config.api_key);
        memset(ctx->config.api_key, 0, key_len);
        free(ctx->config.api_key);
    }
    free(ctx->config.base_url);
    free(ctx->config.default_model);
    free(ctx->config.auth_header_name);
    free(ctx->config.organization);
    memset(ctx, 0, sizeof(china_eco_context_t));
    free(ctx);
}

bool china_eco_is_initialized(const china_eco_context_t* ctx) {
    return ctx && ctx->initialized;
}

const char* china_eco_adapter_version(void) {
    return CHINA_ECO_ADAPTER_VERSION;
}

int china_eco_chat_completion(china_eco_context_t* ctx,
                               const china_eco_message_t* messages,
                               size_t message_count,
                               const char* model,
                               double temperature,
                               int max_tokens,
                               china_eco_response_t* response) {
    if (!ctx || !response) return -1;
    if (!ctx->initialized) return -2;

    memset(response, 0, sizeof(china_eco_response_t));

#ifndef AGENTOS_HAS_CURL
    (void)messages; (void)message_count; (void)model;
    (void)temperature; (void)max_tokens;
    return -10;
#else
    if (!ctx->config.api_key || !ctx->config.api_key[0]) return -11;

    const char* effective_model = model ? model : ctx->config.default_model;
    double effective_temp = temperature >= 0 ? temperature : ctx->config.temperature;
    int effective_max = max_tokens > 0 ? max_tokens : ctx->config.max_tokens;

    cJSON* req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "model", effective_model);
    cJSON_AddNumberToObject(req, "max_tokens", effective_max);
    cJSON_AddNumberToObject(req, "temperature", effective_temp);

    cJSON* msgs_arr = cJSON_CreateArray();
    for (size_t i = 0; i < message_count && i < CHINA_ECO_MAX_MESSAGES; i++) {
        cJSON* msg_obj = cJSON_CreateObject();
        const char* role = messages[i].role ? messages[i].role : "user";
        cJSON_AddStringToObject(msg_obj, "role", role);
        if (messages[i].content)
            cJSON_AddStringToObject(msg_obj, "content", messages[i].content);
        cJSON_AddItemToArray(msgs_arr, msg_obj);
    }
    cJSON_AddItemToObject(req, "messages", msgs_arr);

    char* req_str = cJSON_PrintUnformatted(req);
    cJSON_Delete(req);

    char api_response[CHINA_ECO_MAX_RESPONSE_LEN];
    memset(api_response, 0, sizeof(api_response));
    int api_result = china_api_call(ctx->config.api_key,
                                     ctx->config.base_url,
                                     ctx->config.auth_header_name,
                                     ctx->config.auth_type,
                                     "/chat/completions",
                                     req_str, api_response, sizeof(api_response));
    free(req_str);

    if (api_result <= 0) return -13;

    char content_buf[CHINA_ECO_MAX_RESPONSE_LEN];
    char model_buf[128];
    char finish_buf[32];
    int pt = 0, ct = 0, tt = 0;
    memset(content_buf, 0, sizeof(content_buf));
    memset(model_buf, 0, sizeof(model_buf));
    memset(finish_buf, 0, sizeof(finish_buf));

    int parse_result = china_parse_chat_response(api_response,
                                                   content_buf, sizeof(content_buf),
                                                   model_buf, sizeof(model_buf),
                                                   finish_buf, sizeof(finish_buf),
                                                   &pt, &ct, &tt);
    if (parse_result != 0) return -12;

    response->content = strdup(content_buf);
    response->model = strdup(model_buf[0] ? model_buf : effective_model);
    response->finish_reason = strdup(finish_buf[0] ? finish_buf : "stop");
    response->prompt_tokens = pt;
    response->completion_tokens = ct;
    response->total_tokens = tt;

    ctx->total_requests++;
    ctx->total_tokens_in += (uint64_t)pt;
    ctx->total_tokens_out += (uint64_t)ct;

    return 0;
#endif
}

int china_eco_chat_streaming(china_eco_context_t* ctx,
                               const china_eco_message_t* messages,
                               size_t message_count,
                               const char* model,
                               china_eco_stream_handler_t handler,
                               void* user_data) {
    if (!ctx || !handler) return -1;
    if (!ctx->initialized) return -2;

#ifndef AGENTOS_HAS_CURL
    (void)messages; (void)message_count; (void)model;
    (void)user_data;
    return -10;
#else
    if (!ctx->config.api_key || !ctx->config.api_key[0]) return -11;

    const char* effective_model = model ? model : ctx->config.default_model;

    cJSON* req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "model", effective_model);
    cJSON_AddNumberToObject(req, "max_tokens", ctx->config.max_tokens);
    cJSON_AddNumberToObject(req, "temperature", ctx->config.temperature);
    cJSON_AddBoolToObject(req, "stream", 1);

    cJSON* msgs_arr = cJSON_CreateArray();
    for (size_t i = 0; i < message_count && i < CHINA_ECO_MAX_MESSAGES; i++) {
        cJSON* msg_obj = cJSON_CreateObject();
        const char* role = messages[i].role ? messages[i].role : "user";
        cJSON_AddStringToObject(msg_obj, "role", role);
        if (messages[i].content)
            cJSON_AddStringToObject(msg_obj, "content", messages[i].content);
        cJSON_AddItemToArray(msgs_arr, msg_obj);
    }
    cJSON_AddItemToObject(req, "messages", msgs_arr);

    char* req_str = cJSON_PrintUnformatted(req);
    cJSON_Delete(req);

    char api_response[16384];
    memset(api_response, 0, sizeof(api_response));
    int api_result = china_api_call(ctx->config.api_key,
                                     ctx->config.base_url,
                                     ctx->config.auth_header_name,
                                     ctx->config.auth_type,
                                     "/chat/completions",
                                     req_str, api_response, sizeof(api_response));
    free(req_str);

    if (api_result <= 0) return -13;

    char full_response[CHINA_ECO_MAX_RESPONSE_LEN];
    memset(full_response, 0, sizeof(full_response));
    int pt = 0, ct = 0, tt = 0;
    char model_buf[128], finish_buf[32];
    memset(model_buf, 0, sizeof(model_buf));
    memset(finish_buf, 0, sizeof(finish_buf));

    int parse_result = china_parse_chat_response(api_response,
                                                   full_response, sizeof(full_response),
                                                   model_buf, sizeof(model_buf),
                                                   finish_buf, sizeof(finish_buf),
                                                   &pt, &ct, &tt);
    if (parse_result != 0) return -12;

    size_t resp_len = strlen(full_response);
    size_t pos = 0;
    while (pos < resp_len) {
        size_t remaining = resp_len - pos;
        size_t chunk_len = remaining < 10 ? remaining : 10;

        while (chunk_len > 0 &&
               pos + chunk_len < resp_len &&
               !isspace((unsigned char)full_response[pos + chunk_len]) &&
               full_response[pos + chunk_len] != ',' &&
               full_response[pos + chunk_len] != '.' &&
               full_response[pos + chunk_len] != '!' &&
               full_response[pos + chunk_len] != '?' &&
               full_response[pos + chunk_len] != ';' &&
               full_response[pos + chunk_len] != ':' &&
               full_response[pos + chunk_len] != '\n') {
            chunk_len--;
        }
        if (chunk_len == 0) chunk_len = 1;

        china_eco_stream_chunk_t chunk;
        chunk.chunk = full_response + pos;
        chunk.length = chunk_len;
        chunk.is_final = (pos + chunk_len >= resp_len);
        handler(&chunk, user_data);
        pos += chunk_len;
    }

    ctx->total_requests++;
    ctx->total_tokens_in += (uint64_t)pt;
    ctx->total_tokens_out += (uint64_t)ct;

    return 0;
#endif
}

int china_eco_embeddings(china_eco_context_t* ctx,
                          const char** inputs,
                          size_t input_count,
                          const char* model,
                          double** out_embeddings,
                          size_t* out_dim) {
    if (!ctx || !out_embeddings || !out_dim) return -1;
    if (!ctx->initialized) return -2;

#ifndef AGENTOS_HAS_CURL
    (void)inputs; (void)input_count; (void)model;
    return -10;
#else
    if (!ctx->config.api_key || !ctx->config.api_key[0]) return -11;
    if (!inputs || input_count == 0) return -3;

    const char* effective_model = model ? model : "text-embedding-v1";

    cJSON* req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "model", effective_model);
    cJSON* input_arr = cJSON_CreateArray();
    for (size_t i = 0; i < input_count && i < 32; i++) {
        cJSON_AddItemToArray(input_arr,
            cJSON_CreateString(inputs[i] ? inputs[i] : ""));
    }
    cJSON_AddItemToObject(req, "input", input_arr);

    char* req_str = cJSON_PrintUnformatted(req);
    cJSON_Delete(req);

    char api_response[65536];
    memset(api_response, 0, sizeof(api_response));
    int api_result = china_api_call(ctx->config.api_key,
                                     ctx->config.base_url,
                                     ctx->config.auth_header_name,
                                     ctx->config.auth_type,
                                     "/embeddings",
                                     req_str, api_response, sizeof(api_response));
    free(req_str);

    if (api_result <= 0) return -13;

    cJSON* root = cJSON_Parse(api_response);
    if (!root) return -12;

    cJSON* data = cJSON_GetObjectItem(root, "data");
    if (!data || !cJSON_IsArray(data) || cJSON_GetArraySize(data) == 0) {
        cJSON_Delete(root);
        return -14;
    }

    cJSON* first = cJSON_GetArrayItem(data, 0);
    cJSON* emb = cJSON_GetObjectItem(first, "embedding");
    if (!emb || !cJSON_IsArray(emb)) {
        cJSON_Delete(root);
        return -15;
    }

    int dim = cJSON_GetArraySize(emb);
    *out_dim = (size_t)dim;
    *out_embeddings = calloc((size_t)dim, sizeof(double));
    if (!*out_embeddings) {
        cJSON_Delete(root);
        return -16;
    }

    for (int i = 0; i < dim; i++) {
        cJSON* val = cJSON_GetArrayItem(emb, i);
        (*out_embeddings)[i] = val ? val->valuedouble : 0.0;
    }

    cJSON_Delete(root);
    return 0;
#endif
}

int china_eco_list_models(china_eco_context_t* ctx,
                            china_model_info_t** models,
                            size_t* count) {
    if (!ctx || !models || !count) return -1;

    const china_provider_info_t* info = ctx->provider_info;
    int n = info->model_count;
    *models = calloc((size_t)n, sizeof(china_model_info_t));
    if (!*models) return -3;

    for (int i = 0; i < n; i++) {
        (*models)[i].provider = info->provider;
        (*models)[i].id = info->models[i];
        (*models)[i].api_name = info->models[i];
        (*models)[i].display_name = info->models[i];
        (*models)[i].max_context_tokens = 32768;
        (*models)[i].max_output_tokens = 4096;
        (*models)[i].supports_streaming = true;
        (*models)[i].supports_function_calling = (i == 0);
        (*models)[i].supports_vision = false;
        (*models)[i].is_default = (i == 0);
    }

    *count = (size_t)n;
    return 0;
}

int china_eco_route_request(china_eco_context_t* ctx,
                              const char* path,
                              const char* method,
                              const char* body_json,
                              char** response_json) {
    if (!ctx || !path || !method || !response_json) return -1;
    *response_json = NULL;

    if (strcmp(path, "/chat/completions") == 0) {
        china_eco_message_t msg = {0};
        msg.role = strdup("user");
        msg.content = (body_json && body_json[0]) ? strdup(body_json) : strdup("hello");
        china_eco_response_t resp;
        memset(&resp, 0, sizeof(resp));
        int rc = china_eco_chat_completion(ctx, &msg, 1, NULL, -1, 0, &resp);
        if (rc == 0 && resp.content) {
            size_t sz = 256 + strlen(resp.content);
            char* json = malloc(sz);
            if (json) snprintf(json, sz, "{\"content\":\"%s\",\"model\":\"%s\"}",
                               resp.content, resp.model ? resp.model : "unknown");
            *response_json = json;
        } else {
            *response_json = strdup("{\"error\":\"chat completion failed\"}");
        }
        free(msg.role);
        free(msg.content);
        china_eco_response_destroy(&resp);
        return rc;
    }

    if (strcmp(path, "/models") == 0) {
        china_model_info_t* models = NULL;
        size_t count = 0;
        china_eco_list_models(ctx, &models, &count);
        free(models);
        *response_json = strdup("{\"status\":\"models_listed\"}");
        return 0;
    }

    *response_json = strdup("{\"error\":\"unknown endpoint\"}");
    return -10;
}

static int china_proto_init(void* context) {
    if (!context) return -1;
    china_eco_config_t cfg = china_eco_config_default(CHINA_PROVIDER_DEEPSEEK);
    china_eco_context_t* ctx = china_eco_context_create(&cfg);
    if (!ctx) return -2;
    *(void**)context = ctx;
    return 0;
}

static int china_proto_destroy(void* context) {
    if (context) china_eco_context_destroy((china_eco_context_t*)context);
    return 0;
}

static int china_proto_handle_request(void* context,
                                       const void* req,
                                       void** resp) {
    if (!context || !req || !resp) return -1;
    china_eco_context_t* ctx = (china_eco_context_t*)context;
    if (!ctx->initialized) return -2;

    const unified_message_t* request = (const unified_message_t*)req;
    const char* user_content = "";

    if (request->payload) {
        cJSON* json = cJSON_Parse(request->payload);
        if (json) {
            cJSON* msgs = cJSON_GetObjectItem(json, "messages");
            if (cJSON_IsArray(msgs)) {
                int mcount = cJSON_GetArraySize(msgs);
                for (int i = mcount - 1; i >= 0; i--) {
                    cJSON* mi = cJSON_GetArrayItem(msgs, i);
                    cJSON* role = cJSON_GetObjectItem(mi, "role");
                    cJSON* content = cJSON_GetObjectItem(mi, "content");
                    const char* rs = role ? cJSON_GetStringValue(role) : NULL;
                    const char* cs = content ? cJSON_GetStringValue(content) : NULL;
                    if (rs && strcmp(rs, "user") == 0 && cs) {
                        user_content = cs;
                        break;
                    }
                }
            }
            cJSON_Delete(json);
        }
    }

    if (request->body && request->body_length > 0)
        user_content = (const char*)request->body;

    china_eco_message_t msg = {0};
    msg.role = strdup("user");
    msg.content = strdup(user_content);
    china_eco_response_t resp;
    memset(&resp, 0, sizeof(resp));
    int rc = china_eco_chat_completion(ctx, &msg, 1, NULL, -1, 0, &resp);

    unified_message_t* response = calloc(1, sizeof(unified_message_t));
    if (!response) {
        free(msg.role);
        free(msg.content);
        china_eco_response_destroy(&resp);
        return -3;
    }

    if (rc == 0 && resp.content) {
        response->payload = strdup(resp.content);
        response->payload_size = strlen(resp.content);
    } else {
        response->payload = strdup("{\"error\":\"request failed\"}");
        response->payload_size = strlen(response->payload);
    }
    response->status = (rc == 0) ? 200 : 500;
    if (request)
        strncpy(response->correlation_id, request->correlation_id,
                sizeof(response->correlation_id) - 1);

    free(msg.role);
    free(msg.content);
    china_eco_response_destroy(&resp);
    *resp = response;
    return rc;
}

static int china_proto_get_version(void* context, char* buf, size_t max_size) {
    (void)context;
    if (!buf || max_size == 0) return -1;
    const char* ver = CHINA_ECO_ADAPTER_VERSION;
    size_t len = strlen(ver);
    if (len >= max_size) len = max_size - 1;
    memcpy(buf, ver, len);
    buf[len] = '\0';
    return 0;
}

static uint32_t china_proto_capabilities(void* context) {
    (void)context;
    return (uint32_t)(PROTO_CAP_STREAMING | PROTO_CAP_TOOL_CALLING);
}

const proto_adapter_t* china_eco_get_protocol_adapter(void) {
    static proto_adapter_t adapter = {0};
    static bool initialized = false;

    if (!initialized) {
        adapter.name = "ChinaEco";
        adapter.version = CHINA_ECO_ADAPTER_VERSION;
        adapter.description = "Chinese LLM Ecosystem Unified Adapter - DeepSeek/Qwen/Wenxin/ChatGLM/Spark/Baichuan/Doubao/Kimi/MiniMax/Volcengine";
        adapter.type = PROTO_CUSTOM;
        adapter.init = china_proto_init;
        adapter.destroy = china_proto_destroy;
        adapter.handle_request = china_proto_handle_request;
        adapter.get_version = china_proto_get_version;
        adapter.capabilities = china_proto_capabilities;
        initialized = true;
    }

    return &adapter;
}

void china_eco_response_destroy(china_eco_response_t* resp) {
    if (!resp) return;
    free(resp->content);
    free(resp->model);
    free(resp->finish_reason);
    memset(resp, 0, sizeof(china_eco_response_t));
}

void china_eco_message_destroy(china_eco_message_t* msg) {
    if (!msg) return;
    free(msg->role);
    free(msg->content);
    memset(msg, 0, sizeof(china_eco_message_t));
}
