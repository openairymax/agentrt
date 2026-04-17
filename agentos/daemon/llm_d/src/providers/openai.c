/**
 * @file openai.c
 * @brief OpenAI 适配器实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 改进说明：
 * 1. 使用公共 Provider 基础设施
 * 2. 代码量从 400 行减少到约 150 行
 * 3. 消除了与 deepseek.c/local.c 的重复代码
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

#define OPENAI_DEFAULT_BASE "https://api.openai.com/v1"
#define OPENAI_DEFAULT_MODEL "gpt-3.5-turbo"

/* ---------- 上下文 ---------- */

typedef struct {
    provider_base_ctx_t base;
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

    provider_base_init(&ctx->base, api_key, api_base, organization,
                      timeout_sec, max_retries, OPENAI_DEFAULT_BASE);

    return (provider_ctx_t*)ctx;
}

static void openai_destroy(provider_ctx_t* ctx_ptr) {
    if (ctx_ptr) {
        free(ctx_ptr);
    }
}

/* ---------- 同步完成 ---------- */

static int openai_complete(provider_ctx_t* ctx_ptr,
                           const llm_request_config_t* manager,
                           llm_response_t** out_response) {
    if (!ctx_ptr || !manager || !out_response) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    openai_ctx_t* ctx = (openai_ctx_t*)ctx_ptr;
    provider_base_ctx_t* base = &ctx->base;

    char* req_body = provider_build_openai_request(manager, OPENAI_DEFAULT_MODEL);
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

    provider_http_resp_t* http_resp = NULL;
    long http_code = 0;

    int ret = provider_http_post(url, headers, req_body,
                                 base->timeout_sec, base->max_retries,
                                 &http_resp, &http_code);

    curl_slist_free_all(headers);
    free(req_body);

    if (ret != AGENTOS_OK) {
        SVC_LOG_ERROR("openai: HTTP request failed, status=%ld", http_code);
        return ret;
    }

    if (http_code != 200) {
        SVC_LOG_ERROR("openai: HTTP error, status=%ld", http_code);
        provider_http_resp_free(http_resp);
        return AGENTOS_ERR_IO;
    }

    ret = provider_parse_openai_response(http_resp->data, out_response);
    provider_http_resp_free(http_resp);

    return ret;
}

/* ---------- 流式完成 ---------- */

static int openai_complete_stream(provider_ctx_t* ctx_ptr,
                                  const llm_request_config_t* manager,
                                  llm_stream_callback_t callback,
                                  void* user_data,
                                  llm_response_t** out_response) {
    if (!ctx_ptr || !manager || !callback) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    SVC_LOG_WARN("OpenAI streaming not yet implemented, using synchronous mode");

    llm_response_t* resp = NULL;
    int ret = openai_complete(ctx_ptr, manager, &resp);

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

const provider_ops_t openai_ops = {
    .init = openai_init,
    .destroy = openai_destroy,
    .complete = openai_complete,
    .complete_stream = openai_complete_stream,
    .name = "openai",
    .default_model = OPENAI_DEFAULT_MODEL,
    .default_base_url = OPENAI_DEFAULT_BASE
};
