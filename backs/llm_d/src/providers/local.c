/**
 * @file local.c
 * @brief 本地模型适配器（兼容 OpenAI 格式）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "provider.h"
#include "svc_http.h"
#include "svc_logger.h"
#include "svc_error.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    char api_base[512];
    double timeout_sec;
    int max_retries;
} local_ctx_t;

extern char* openai_build_request(const llm_request_config_t* config);
extern int openai_parse_response(const char* body, llm_response_t** out);
extern size_t openai_stream_write(void* contents, size_t size, size_t nmemb, void* userp);

static provider_ctx_t* local_init(const char* name, const char* api_key,
                                   const char* api_base, const char* organization,
                                   double timeout_sec, int max_retries) {
    (void)name; (void)api_key; (void)organization;
    local_ctx_t* ctx = calloc(1, sizeof(local_ctx_t));
    if (!ctx) return NULL;
    if (api_base)
        strncpy(ctx->api_base, api_base, sizeof(ctx->api_base) - 1);
    else
        strncpy(ctx->api_base, "http://localhost:8080/v1", sizeof(ctx->api_base) - 1);
    ctx->timeout_sec = timeout_sec > 0 ? timeout_sec : 60.0;
    ctx->max_retries = max_retries;
    return (provider_ctx_t*)ctx;
}

static void local_destroy(provider_ctx_t* ctx_ptr) {
    free(ctx_ptr);
}

static int local_complete(provider_ctx_t* ctx_ptr,
                          const llm_request_config_t* config,
                          llm_response_t** out_response) {
    local_ctx_t* ctx = (local_ctx_t*)ctx_ptr;
    char* req_body = openai_build_request(config);
    if (!req_body) return ERR_NOMEM;

    char url[1024];
    snprintf(url, sizeof(url), "%s/chat/completions", ctx->api_base);

    http_headers_t* headers = http_headers_new();
    if (!headers) {
        free(req_body);
        return ERR_NOMEM;
    }
    http_headers_add(headers, "Content-Type: application/json");

    http_response_t* resp = http_post(url, headers, req_body, ctx->timeout_sec);
    http_headers_free(headers);
    free(req_body);

    if (!resp) return ERR_HTTP;
    if (resp->status_code != 200) {
        http_response_free(resp);
        return ERR_HTTP;
    }
    int ret = openai_parse_response(resp->body, out_response);
    http_response_free(resp);
    return ret;
}

static int local_complete_stream(provider_ctx_t* ctx_ptr,
                                 const llm_request_config_t* config,
                                 llm_stream_callback_t callback,
                                 llm_response_t** out_response) {
    local_ctx_t* ctx = (local_ctx_t*)ctx_ptr;
    llm_request_config_t stream_cfg = *config;
    stream_cfg.stream = 1;
    char* req_body = openai_build_request(&stream_cfg);
    if (!req_body) return ERR_NOMEM;

    char url[1024];
    snprintf(url, sizeof(url), "%s/chat/completions", ctx->api_base);

    http_headers_t* headers = http_headers_new();
    if (!headers) {
        free(req_body);
        return ERR_NOMEM;
    }
    http_headers_add(headers, "Content-Type: application/json");

    typedef struct {
        llm_stream_callback_t cb;
        void* user_data;
    } adapter_t;
    adapter_t ad = {callback, config->user_data};

    http_response_t* resp = http_post_stream(url, headers, req_body, ctx->timeout_sec,
                                              openai_stream_write, &ad);
    http_headers_free(headers);
    free(req_body);

    if (!resp) return ERR_HTTP;
    if (resp->status_code != 200) {
        http_response_free(resp);
        return ERR_HTTP;
    }
    if (out_response) *out_response = NULL;
    http_response_free(resp);
    return 0;
}

const provider_ops_t local_ops = {
    .init = local_init,
    .destroy = local_destroy,
    .complete = local_complete,
    .complete_stream = local_complete_stream
};