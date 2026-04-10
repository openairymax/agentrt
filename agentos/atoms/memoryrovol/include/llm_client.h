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
 * @brief LLM请求结构
 */
typedef struct agentos_llm_request {
    const char* model;       /**< 模型名称 */
    const char* prompt;      /**< 提示文本 */
    float temperature;       /**< 温度参数 (0.0-2.0) */
    uint32_t max_tokens;     /**< 最大生成token数 */
    const char* system_prompt; /**< 可选系统提示 */
} agentos_llm_request_t;

/**
 * @brief LLM响应结构
 */
typedef struct agentos_llm_response {
    char* text;              /**< 生成的文本 */
    uint32_t usage_tokens;   /**< 使用token数 */
    uint32_t total_tokens;   /**< 总token数 */
    uint32_t finish_reason;  /**< 完成原因 */
} agentos_llm_response_t;

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
 * @brief 调用LLM生成响应 (简化接口)
 */
agentos_error_t agentos_llm_service_call(
    agentos_llm_service_t* service,
    const char* prompt,
    char** out_response);

/**
 * @brief 完整LLM调用接口
 */
agentos_error_t agentos_llm_complete(
    agentos_llm_service_t* service,
    const agentos_llm_request_t* request,
    agentos_llm_response_t** out_response);

/**
 * @brief 释放LLM响应
 */
void agentos_llm_response_free(agentos_llm_response_t* response);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_LLM_CLIENT_H */
