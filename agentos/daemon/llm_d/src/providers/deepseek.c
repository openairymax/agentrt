#include <cjson/cJSON.h>
/**
 * @file deepseek.c
 * @brief DeepSeek 适配器（兼容 OpenAI 格式）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 改进说明：
 * 1. 使用公共 Provider 基础设施
 * 2. 代码量从 360 行减少到约 150 行
 * 3. 消除了与 openai.c/local.c 的重复代码
 */

#include "provider.h"
#include "error.h"
#include "daemon_errors.h"
#include "svc_logger.h"
#include "platform.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <curl/curl.h>

#define DEEPSEEK_DEFAULT_BASE "https://api.deepseek.com/v1"
#define DEEPSEEK_DEFAULT_MODEL "deepseek-chat"

/* ---------- 上下文 ---------- */

typedef struct {
    provider_base_ctx_t base;
} deepseek_ctx_t;

/* ---------- 生命周期 ---------- */

static provider_ctx_t* deepseek_init(const char* name,
                                    const char* api_key,
                                    const char* api_base,
                                    const char* organization,
                                    double timeout_sec,
                                    int max_retries) {
    (void)name;
    (void)organization;

    deepseek_ctx_t* ctx = (deepseek_ctx_t*)calloc(1, sizeof(deepseek_ctx_t));
    if (!ctx) {
        return NULL;
    }

    provider_base_init(&ctx->base, api_key, api_base, organization,
                      timeout_sec, max_retries, DEEPSEEK_DEFAULT_BASE);

    return (provider_ctx_t*)ctx;
}

static void deepseek_destroy(provider_ctx_t* ctx_ptr) {
    if (ctx_ptr) {
        free(ctx_ptr);
    }
}

/* ---------- 同步完成 ---------- */

static int deepseek_complete(provider_ctx_t* ctx_ptr,
                            const llm_request_config_t* manager,
                            llm_response_t** out_response) {
    if (!ctx_ptr || !manager || !out_response) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    deepseek_ctx_t* ctx = (deepseek_ctx_t*)ctx_ptr;
    provider_base_ctx_t* base = &ctx->base;

    char* req_body = provider_build_openai_request(manager, DEEPSEEK_DEFAULT_MODEL);
    if (!req_body) {
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }

    char url[1024];
    snprintf(url, sizeof(url), "%s/chat/completions", base->api_base);

    struct curl_slist* headers = NULL;
    char auth_header[1024];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s",
             base->api_key[0] ? base->api_key : "");
    headers = curl_slist_append(headers, auth_header);
    headers = curl_slist_append(headers, "Content-Type: application/json");
    explicit_bzero(auth_header, sizeof(auth_header));

    provider_http_resp_t* http_resp = NULL;
    long http_code = 0;

    int ret = provider_http_post(url, headers, req_body,
                                base->timeout_sec, base->max_retries,
                                &http_resp, &http_code);

    curl_slist_free_all(headers);
    free(req_body);

    if (ret != AGENTOS_OK) {
        SVC_LOG_ERROR("deepseek: HTTP request failed, status=%ld", http_code);
        return ret;
    }

    if (http_code != 200) {
        SVC_LOG_ERROR("deepseek: HTTP error, status=%ld", http_code);
        provider_http_resp_free(http_resp);
        return AGENTOS_ERR_IO;
    }

    ret = provider_parse_openai_response(http_resp->data, out_response);
    provider_http_resp_free(http_resp);

    return ret;
}

/* ---------- 流式完成（SSE, OpenAI兼容格式） ---------- */

typedef struct {
    llm_stream_callback_t user_cb;
    void*                 user_data;
    char*                 acc_content;
    size_t                acc_cap;
    size_t                acc_len;
    char*                 resp_id;
    char*                 resp_model;
    uint64_t              resp_created;
    char*                 finish_reason;
} ds_stream_acc_t;

static int ds_stream_on_chunk(const char* json_line, void* userdata) {
    ds_stream_acc_t* acc = (ds_stream_acc_t*)userdata;

    cJSON* root = cJSON_Parse(json_line);
    if (!root) return 0;

    if (!acc->resp_id) {
        cJSON* id = cJSON_GetObjectItem(root, "id");
        if (cJSON_IsString(id) && id->valuestring)
            acc->resp_id = strdup(id->valuestring);
    }

    if (!acc->resp_model) {
        cJSON* model = cJSON_GetObjectItem(root, "model");
        if (cJSON_IsString(model) && model->valuestring)
            acc->resp_model = strdup(model->valuestring);
    }

    cJSON* created = cJSON_GetObjectItem(root, "created");
    if (cJSON_IsNumber(created) && acc->resp_created == 0)
        acc->resp_created = (uint64_t)created->valuedouble;

    cJSON* choices = cJSON_GetObjectItem(root, "choices");
    if (cJSON_IsArray(choices) && cJSON_GetArraySize(choices) > 0) {
        cJSON* choice = cJSON_GetArrayItem(choices, 0);
        cJSON* delta = cJSON_GetObjectItem(choice, "delta");
        if (delta) {
            cJSON* content = cJSON_GetObjectItem(delta, "content");
            if (cJSON_IsString(content) && content->valuestring) {
                const char* text = content->valuestring;
                size_t tlen = strlen(text);

                if (acc->user_cb)
                    acc->user_cb(text, acc->user_data);

                if (tlen > 0) {
                    size_t needed = acc->acc_len + tlen + 1;
                    if (needed > acc->acc_cap) {
                        size_t new_cap = acc->acc_cap * 2;
                        while (new_cap < needed) new_cap *= 2;
                        char* ptr = (char*)realloc(acc->acc_content, new_cap);
                        if (ptr) { acc->acc_content = ptr; acc->acc_cap = new_cap; }
                    }
                    if (acc->acc_content && acc->acc_len + tlen < acc->acc_cap) {
                        memcpy(acc->acc_content + acc->acc_len, text, tlen);
                        acc->acc_len += tlen;
                        acc->acc_content[acc->acc_len] = '\0';
                    }
                }
            }
        }

        cJSON* fr = cJSON_GetObjectItem(choice, "finish_reason");
        if (cJSON_IsString(fr) && fr->valuestring &&
            strcmp(fr->valuestring, "null") != 0) {
            free(acc->finish_reason);
            acc->finish_reason = strdup(fr->valuestring);
        }
    }

    cJSON_Delete(root);
    return 0;
}

static llm_response_t* ds_build_stream_response(ds_stream_acc_t* acc) {
    llm_response_t* resp = (llm_response_t*)calloc(1, sizeof(llm_response_t));
    if (!resp) return NULL;

    resp->id = acc->resp_id ? acc->resp_id : strdup("");
    acc->resp_id = NULL;
    resp->model = acc->resp_model ? acc->resp_model : strdup("unknown");
    acc->resp_model = NULL;
    resp->created = acc->resp_created;
    resp->choice_count = 1;
    resp->choices = (llm_message_t*)calloc(1, sizeof(llm_message_t));
    if (resp->choices) {
        resp->choices[0].role = strdup("assistant");
        resp->choices[0].content = acc->acc_content;
        acc->acc_content = NULL;
    }
    resp->finish_reason = acc->finish_reason ? acc->finish_reason : strdup("stop");
    acc->finish_reason = NULL;
    return resp;
}

static int deepseek_complete_stream(provider_ctx_t* ctx_ptr,
                                   const llm_request_config_t* manager,
                                   llm_stream_callback_t callback,
                                   void* user_data,
                                   llm_response_t** out_response) {
    if (!ctx_ptr || !manager || !callback)
        return AGENTOS_ERR_INVALID_PARAM;

    deepseek_ctx_t* ctx = (deepseek_ctx_t*)ctx_ptr;
    provider_base_ctx_t* base = &ctx->base;

    llm_request_config_t stream_cfg = *manager;
    stream_cfg.stream = 1;

    char* req_body = provider_build_openai_request(&stream_cfg, DEEPSEEK_DEFAULT_MODEL);
    if (!req_body) return AGENTOS_ERR_OUT_OF_MEMORY;

    char url[1024];
    snprintf(url, sizeof(url), "%s/chat/completions", base->api_base);

    struct curl_slist* headers = NULL;
    char auth_header[1024];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s",
             base->api_key[0] ? base->api_key : "");
    headers = curl_slist_append(headers, auth_header);
    headers = curl_slist_append(headers, "Content-Type: application/json");
    explicit_bzero(auth_header, sizeof(auth_header));

    ds_stream_acc_t acc;
    memset(&acc, 0, sizeof(acc));
    acc.user_cb = callback;
    acc.user_data = user_data;
    acc.acc_cap = 4096;
    acc.acc_content = (char*)malloc(acc.acc_cap);

    long http_code = 0;
    int ret = provider_http_post_stream(url, headers, req_body,
                                        base->timeout_sec,
                                        ds_stream_on_chunk, &acc,
                                        &http_code);

    curl_slist_free_all(headers);
    free(req_body);

    if (ret != AGENTOS_OK) {
        SVC_LOG_ERROR("deepseek: Stream HTTP error, status=%ld", http_code);
        free(acc.acc_content); free(acc.resp_id);
        free(acc.resp_model); free(acc.finish_reason);
        return ret;
    }

    llm_response_t* resp = ds_build_stream_response(&acc);
    free(acc.acc_content); free(acc.resp_id);
    free(acc.resp_model); free(acc.finish_reason);

    if (out_response) *out_response = resp;
    else if (resp) llm_response_free(resp);

    return AGENTOS_OK;
}

/* ---------- 操作表 ---------- */

const provider_ops_t deepseek_ops = {
    .init = deepseek_init,
    .destroy = deepseek_destroy,
    .complete = deepseek_complete,
    .complete_stream = deepseek_complete_stream,
    .name = "deepseek",
    .default_model = DEEPSEEK_DEFAULT_MODEL,
    .default_base_url = DEEPSEEK_DEFAULT_BASE
};
