/**
 * @file local.c
 * @brief 本地模型适配器（兼容 OpenAI 格式）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 改进说明：
 * 1. 使用公共 Provider 基础设施
 * 2. 代码量从 370 行减少到约 140 行
 * 3. 消除了与 openai.c/deepseek.c 的重复代码
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

#define LOCAL_DEFAULT_BASE "http://localhost:8080/v1"
#define LOCAL_DEFAULT_MODEL "gpt-3.5-turbo"
#define LOCAL_DEFAULT_TIMEOUT 60.0

/* ---------- 上下文 ---------- */

typedef struct {
    provider_base_ctx_t base;
} local_ctx_t;

/* ---------- 生命周期 ---------- */

static provider_ctx_t* local_init(const char* name,
                                   const char* api_key,
                                   const char* api_base,
                                   const char* organization,
                                   double timeout_sec,
                                   int max_retries) {
    (void)name;
    (void)api_key;
    (void)organization;

    local_ctx_t* ctx = (local_ctx_t*)calloc(1, sizeof(local_ctx_t));
    if (!ctx) {
        return NULL;
    }

    double timeout = timeout_sec > 0 ? timeout_sec : LOCAL_DEFAULT_TIMEOUT;
    provider_base_init(&ctx->base, NULL, api_base, NULL,
                      timeout, max_retries, LOCAL_DEFAULT_BASE);

    return (provider_ctx_t*)ctx;
}

static void local_destroy(provider_ctx_t* ctx_ptr) {
    if (ctx_ptr) {
        free(ctx_ptr);
    }
}

/* ---------- 同步完成 ---------- */

static int local_complete(provider_ctx_t* ctx_ptr,
                          const llm_request_config_t* manager,
                          llm_response_t** out_response) {
    if (!ctx_ptr || !manager || !out_response) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    local_ctx_t* ctx = (local_ctx_t*)ctx_ptr;
    provider_base_ctx_t* base = &ctx->base;

    char* req_body = provider_build_openai_request(manager, LOCAL_DEFAULT_MODEL);
    if (!req_body) {
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }

    char url[1024];
    snprintf(url, sizeof(url), "%s/chat/completions", base->api_base);

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    provider_http_resp_t* http_resp = NULL;
    long http_code = 0;

    int ret = provider_http_post(url, headers, req_body,
                               base->timeout_sec, base->max_retries,
                               &http_resp, &http_code);

    curl_slist_free_all(headers);
    free(req_body);

    if (ret != AGENTOS_OK) {
        SVC_LOG_ERROR("local: HTTP request failed, status=%ld", http_code);
        return ret;
    }

    if (http_code != 200) {
        SVC_LOG_ERROR("local: HTTP error, status=%ld", http_code);
        provider_http_resp_free(http_resp);
        return AGENTOS_ERR_IO;
    }

    ret = provider_parse_openai_response(http_resp->data, out_response);
    provider_http_resp_free(http_resp);

    return ret;
}

/* ---------- 流式完成 ---------- */

static int local_complete_stream(provider_ctx_t* ctx_ptr,
                                 const llm_request_config_t* manager,
                                 llm_stream_callback_t callback,
                                 void* user_data,
                                 llm_response_t** out_response) {
    if (!ctx_ptr || !manager || !callback) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    local_ctx_t* ctx = (local_ctx_t*)ctx_ptr;
    provider_base_ctx_t* base = &ctx->base;

    llm_request_config_t stream_cfg = *manager;
    stream_cfg.stream = 1;

    char* req_body = provider_build_openai_request(&stream_cfg, LOCAL_DEFAULT_MODEL);
    if (!req_body) {
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }

    char url[1024];
    snprintf(url, sizeof(url), "%s/chat/completions", base->api_base);

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    provider_http_resp_t* http_resp = NULL;
    long http_code = 0;

    int ret = provider_http_post(url, headers, req_body,
                               base->timeout_sec, base->max_retries,
                               &http_resp, &http_code);

    curl_slist_free_all(headers);
    free(req_body);

    if (ret != AGENTOS_OK) {
        return ret;
    }

    if (http_code != 200) {
        provider_http_resp_free(http_resp);
        return AGENTOS_ERR_IO;
    }

    if (callback && http_resp->data) {
        callback(http_resp->data, user_data);
    }

    if (out_response) {
        *out_response = NULL;
    }
    provider_http_resp_free(http_resp);
    return AGENTOS_OK;
}

/* ---------- 操作表 ---------- */

const provider_ops_t local_ops = {
    .init = local_init,
    .destroy = local_destroy,
    .complete = local_complete,
    .complete_stream = local_complete_stream,
    .name = "local",
    .default_model = LOCAL_DEFAULT_MODEL,
    .default_base_url = LOCAL_DEFAULT_BASE
};
