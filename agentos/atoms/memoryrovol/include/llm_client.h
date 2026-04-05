/**
 * @file llm_client.h
 * @brief LLM客户端接口（简化版）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_LLM_CLIENT_H
#define AGENTOS_LLM_CLIENT_H

#include "agentos.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief LLM服务句柄
 */
typedef struct agentos_llm_service agentos_llm_service_t;

/**
 * @brief LLM配置
 */
typedef struct agentos_llm_config {
    const char* model_name;
    const char* api_key;
    const char* base_url;
    uint32_t timeout_ms;
    float temperature;
    uint32_t max_tokens;
} agentos_llm_config_t;

/**
 * @brief 创建LLM服务
 */
agentos_error_t agentos_llm_service_create(
    const agentos_llm_config_t* manager,
    agentos_llm_service_t** out_service);

/**
 * @brief 销毁LLM服务
 */
void agentos_llm_service_destroy(agentos_llm_service_t* service);

/**
 * @brief 调用LLM生成响应
 */
agentos_error_t agentos_llm_service_call(
    agentos_llm_service_t* service,
    const char* prompt,
    char** out_response);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_LLM_CLIENT_H */
