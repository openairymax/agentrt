/**
 * @file llm_client.h
 * @brief LLM客户端接口 - CoreLoopThree本地副本
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 本文件是 llm_client 的公共接口，
 * 仅包含 CoreLoopThree 所需的类型声明和函数原型。
 * 实现位于 MemoryRovol 独立仓库或内置 LLM 适配器中。
 *
 * BAN-35 合规：CoreLoopThree 使用内置 memory 子系统
 */

#ifndef AGENTOS_LLM_CLIENT_H
#define AGENTOS_LLM_CLIENT_H

#include "agentos.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct agentos_llm_service agentos_llm_service_t;

typedef struct agentos_llm_config {
    const char* model_name;
    const char* api_key;
    const char* base_url;
    uint32_t timeout_ms;
    float temperature;
    uint32_t max_tokens;
} agentos_llm_config_t;

typedef struct agentos_llm_request {
    const char* model;
    const char* prompt;
    float temperature;
    uint32_t max_tokens;
    const char* system_prompt;
} agentos_llm_request_t;

typedef struct agentos_llm_response {
    char* text;
    uint32_t usage_tokens;
    uint32_t total_tokens;
    uint32_t finish_reason;
} agentos_llm_response_t;

#ifndef MEMORYROVOL_OSS

typedef struct agentos_dual_think_config agentos_dual_think_config_t;

typedef struct agentos_dual_think_result agentos_dual_think_result_t;

agentos_error_t agentos_llm_dual_think(
    agentos_llm_service_t* service,
    const agentos_dual_think_config_t* config,
    const char* user_prompt,
    agentos_dual_think_result_t** out_result);

void agentos_llm_dual_result_free(agentos_dual_think_result_t* result);

agentos_error_t agentos_llm_dual_think_simple(
    agentos_llm_service_t* service,
    const char* user_prompt,
    char** out_response);

const agentos_dual_think_config_t* agentos_dual_think_config_default(void);

#endif

agentos_error_t agentos_llm_service_create(
    const agentos_llm_config_t* manager,
    agentos_llm_service_t** out_service);

void agentos_llm_service_destroy(agentos_llm_service_t* service);

agentos_error_t agentos_llm_service_call(
    agentos_llm_service_t* service,
    const char* prompt,
    char** out_response);

int agentos_llm_service_is_available(const agentos_llm_service_t* service);

agentos_error_t agentos_llm_complete(
    agentos_llm_service_t* service,
    const agentos_llm_request_t* request,
    agentos_llm_response_t** out_response);

void agentos_llm_response_free(agentos_llm_response_t* response);

#ifdef __cplusplus
}
#endif

#endif
