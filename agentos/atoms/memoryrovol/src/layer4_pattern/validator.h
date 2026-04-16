/**
 * @file validator.h
 * @brief L4 模式验证器接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_VALIDATOR_H
#define AGENTOS_VALIDATOR_H

#include "layer4_pattern.h"
#include "persistence.h"
#include "rules.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct agentos_pattern_validator {
    double min_confidence;
    int min_support;
    agentos_mutex_t* lock;
};
typedef struct agentos_pattern_validator agentos_pattern_validator_t;

struct agentos_pattern {
    int id;
    int dimension;
    double birth;
    double death;
    double persistence;
    double confidence;
    char description[512];
    float* centroid;
    agentos_pattern_rule_t* rules;
    size_t rule_count;
};
typedef struct agentos_pattern agentos_pattern_t;

agentos_error_t agentos_pattern_validator_create(
    const void* manager,
    agentos_pattern_validator_t** out_val);

void agentos_pattern_validator_destroy(agentos_pattern_validator_t* val);

agentos_error_t agentos_pattern_validator_validate(
    agentos_pattern_validator_t* validator,
    const agentos_pattern_t* pattern,
    const float* test_vectors,
    size_t test_count,
    int* out_valid,
    float* out_confidence);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_VALIDATOR_H */
