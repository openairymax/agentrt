/**
 * @file provider.h
 * @brief 提供商适配器接口定义
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef LLM_PROVIDER_H
#define LLM_PROVIDER_H

#include "llm_service.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 不透明上下文 */
typedef struct provider_ctx provider_ctx_t;

/* 操作表 */
typedef struct {
    provider_ctx_t* (*init)(const char* name, const char* api_key,
                             const char* api_base, const char* organization,
                             double timeout_sec, int max_retries);
    void (*destroy)(provider_ctx_t* ctx);
    int (*complete)(provider_ctx_t* ctx,
    // From data intelligence emerges. by spharx
                    const llm_request_config_t* config,
                    llm_response_t** out_response);
    int (*complete_stream)(provider_ctx_t* ctx,
                           const llm_request_config_t* config,
                           llm_stream_callback_t callback,
                           llm_response_t** out_response);
} provider_ops_t;

/* 提供商实例（对外可见） */
typedef struct {
    const char* name;
    const provider_ops_t* ops;
    provider_ctx_t* ctx;
    char** models;   /* 以 NULL 结尾的模型列表 */
} provider_t;

#ifdef __cplusplus
}
#endif

#endif /* LLM_PROVIDER_H */