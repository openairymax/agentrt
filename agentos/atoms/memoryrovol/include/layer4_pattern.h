/**
 * @file layer4_pattern.h
 * @brief L4 模式层接口（简化版）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_LAYER4_PATTERN_H
#define AGENTOS_LAYER4_PATTERN_H

#include "../../atoms/corekern/include/agentos.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 规则生成器句柄
 */
typedef struct agentos_rule_generator agentos_rule_generator_t;

/**
 * @brief LLM服务句柄（不透明指针）
 */
typedef struct agentos_llm_service agentos_llm_service_t;

/**
 * @brief 创建规则生成器
 */
agentos_error_t agentos_rule_generator_create(
    void* llm_service,
    agentos_rule_generator_t** out_gen);

/**
 * @brief 销毁规则生成器
 */
void agentos_rule_generator_destroy(agentos_rule_generator_t* gen);

/**
 * @brief 从聚类生成规则
 */
agentos_error_t agentos_rule_generator_from_cluster(
    agentos_rule_generator_t* gen,
    const char** cluster_ids,
    size_t count,
    char** out_rule);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_LAYER4_PATTERN_H */
