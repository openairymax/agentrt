/**
 * @file validator.c
 * @brief L4 模式层模式验证器
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "layer4_pattern.h"
#include "validator.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "include/memory_compat.h"
#include "string_compat.h"
#include <string.h>
#include <math.h>

agentos_error_t agentos_pattern_validator_create(
    const void* manager,
    agentos_pattern_validator_t** out_validator) {

    (void)manager;
    if (!out_validator) return AGENTOS_EINVAL;
    agentos_pattern_validator_t* val = (agentos_pattern_validator_t*)AGENTOS_CALLOC(1, sizeof(agentos_pattern_validator_t));
    if (!val) return AGENTOS_ENOMEM;

    val->min_confidence = 0.5;
    val->min_support = 3;
    val->lock = agentos_mutex_create();
    if (!val->lock) {
        AGENTOS_FREE(val);
        return AGENTOS_ENOMEM;
    }

    *out_validator = val;
    return AGENTOS_SUCCESS;
}

void agentos_pattern_validator_destroy(agentos_pattern_validator_t* validator) {
    if (!validator) return;
    if (validator->lock) agentos_mutex_destroy(validator->lock);
    AGENTOS_FREE(validator);
}

agentos_error_t agentos_pattern_validator_validate(
    agentos_pattern_validator_t* validator,
    const agentos_pattern_t* pattern,
    const float* test_vectors,
    size_t test_count,
    int* out_valid,
    float* out_confidence) {

    if (!validator || !pattern || !out_valid) return AGENTOS_EINVAL;

    int valid = 1;
    float confidence = pattern->confidence;

    if (confidence < validator->min_confidence) {
        valid = 0;
    }

    if (test_vectors && test_count > 0 && pattern->centroid) {
        int dim = pattern->dimension;
        double avg_dist = 0.0;
        for (size_t i = 0; i < test_count; i++) {
            double dist = 0.0;
            for (int j = 0; j < dim; j++) {
                double diff = test_vectors[i * dim + j] - pattern->centroid[j];
                dist += diff * diff;
            }
            avg_dist += sqrt(dist);
        }
        avg_dist /= test_count;

        if (avg_dist > 0.5) valid = 0;
    }

    *out_valid = valid;
    if (out_confidence) *out_confidence = confidence;
    return AGENTOS_SUCCESS;
}
