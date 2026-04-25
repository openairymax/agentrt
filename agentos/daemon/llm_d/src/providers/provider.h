/**
 * @file provider.h
 * @brief 提供商适配器接口定义
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef LLM_PROVIDER_H
#define LLM_PROVIDER_H

#include "llm_service.h"
#include <curl/curl.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 不透明上下文 */
typedef struct provider_ctx provider_ctx_t;

/* 基础上下文（各提供商共享） */
typedef struct {
    char api_key[256];
    char api_base[512];
    char organization[128];
    double timeout_sec;
    int max_retries;
} provider_base_ctx_t;

/* HTTP 响应 */
typedef struct {
    char* data;
    size_t size;
    size_t capacity;
} provider_http_resp_t;

/* 操作表 */
typedef struct {
    const char* name;
    const char* default_model;
    const char* default_base_url;
    provider_ctx_t* (*init)(const char* name, const char* api_key,
                             const char* api_base, const char* organization,
                             double timeout_sec, int max_retries);
    void (*destroy)(provider_ctx_t* ctx);
    int (*complete)(provider_ctx_t* ctx,
                    const llm_request_config_t* manager,
                    llm_response_t** out_response);
    int (*complete_stream)(provider_ctx_t* ctx,
                           const llm_request_config_t* manager,
                           llm_stream_callback_t callback,
                           void* callback_data,
                           llm_response_t** out_response);
} provider_ops_t;

/* 提供商实例（对外可见） */
typedef struct {
    const char* name;
    const provider_ops_t* ops;
    provider_ctx_t* ctx;
    char** models;
} provider_t;

/* 通用工具函数 */
void provider_base_init(provider_base_ctx_t* base_ctx,
                        const char* api_key,
                        const char* api_base,
                        const char* organization,
                        double timeout_sec,
                        int max_retries,
                        const char* default_base);

int provider_http_post(const char* url,
                       struct curl_slist* headers,
                       const char* body,
                       double timeout_sec,
                       int max_retries,
                       provider_http_resp_t** out_response,
                       long* out_http_code);

void provider_http_resp_free(provider_http_resp_t* resp);

char* provider_build_openai_request(const llm_request_config_t* manager,
                                     const char* default_model);

int provider_parse_openai_response(const char* body, llm_response_t** out);

/* ---------- SSE 流式传输 ---------- */

typedef int (*provider_stream_chunk_cb_t)(const char* data_line,
                                          void* user_data);

int provider_http_post_stream(const char* url,
                              struct curl_slist* headers,
                              const char* body,
                              double timeout_sec,
                              provider_stream_chunk_cb_t on_chunk,
                              void* chunk_user_data,
                              long* out_http_code);

#ifdef __cplusplus
}
#endif

#endif /* LLM_PROVIDER_H */