/**
 * @file rules.h
 * @brief L4 规则生成器接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_RULES_H
#define AGENTOS_RULES_H

#include "layer4_pattern.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct agentos_rule_generator {
    void* llm;
    agentos_mutex_t* lock;
};
typedef struct agentos_rule_generator agentos_rule_generator_t;

struct agentos_pattern_rule {
    int id;
    char antecedent[256];
    char consequent[256];
    double support;
    double confidence;
    double lift;
};
typedef struct agentos_pattern_rule agentos_pattern_rule_t;

agentos_error_t agentos_rule_generator_create(
    void* llm_service,
    agentos_rule_generator_t** out_gen);

void agentos_rule_generator_destroy(agentos_rule_generator_t* gen);

agentos_error_t agentos_rule_generator_generate(
    agentos_rule_generator_t* gen,
    const agentos_persistence_feature_t** features,
    size_t feature_count,
    agentos_pattern_rule_t*** out_rules,
    size_t* out_count);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_RULES_H */
