/**
 * @file persistence.h
 * @brief L4 模式层持久性计算器接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_PERSISTENCE_H
#define AGENTOS_PERSISTENCE_H

#include "layer4_pattern.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct agentos_persistence_calculator {
    double noise_factor;
    int max_dim;
    agentos_mutex_t* lock;
};
typedef struct agentos_persistence_calculator agentos_persistence_calculator_t;

typedef struct agentos_persistence_feature {
    int dimension;
    double birth;
    double death;
    double persistence;
    double confidence;
} agentos_persistence_feature_t;

agentos_error_t agentos_persistence_calculator_create(
    const void* manager,
    agentos_persistence_calculator_t** out_calc);

void agentos_persistence_calculator_destroy(
    agentos_persistence_calculator_t* calc);

agentos_error_t agentos_persistence_calculator_compute(
    agentos_persistence_calculator_t* calc,
    const float* vectors,
    size_t count,
    agentos_persistence_feature_t*** out_features,
    size_t* out_count);

void agentos_persistence_features_free(
    agentos_persistence_feature_t** features,
    size_t count);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_PERSISTENCE_H */
