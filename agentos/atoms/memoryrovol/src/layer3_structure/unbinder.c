/**
 * @file unbinder.c
 * @brief L3 结构层解绑算子实�?
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "layer3_structure.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include <agentos/memory.h>
#include <agentos/string.h>
#include <string.h>
#include <math.h>


agentos_unbinder_t* agentos_unbinder_create(agentos_binder_t* binder) {
    if (!binder) return NULL;
    agentos_unbinder_t* unb = (agentos_unbinder_t*)AGENTOS_CALLOC(1, sizeof(agentos_unbinder_t));
    if (!unb) return NULL;
    unb->binder = binder;
    unb->lock = agentos_mutex_create();
    if (!unb->lock) {
        AGENTOS_FREE(unb);
        return NULL;
    }
    return unb;
}

void agentos_unbinder_destroy(agentos_unbinder_t* unbinder) {
    if (!unbinder) return;
    if (unbinder->lock) agentos_mutex_destroy(unbinder->lock);
    AGENTOS_FREE(unbinder);
}

/**
 * 实数域解绑（简化版：仅支持Q=1且使用伪逆）
 */
static void unbind_real_q1(const float* bound, const float* known, float* out,
                           size_t dim, float* mat) {
    if (mat) {
        float gk[dim];
        memset(gk, 0, dim * sizeof(float));
        for (size_t i = 0; i < dim; i++) {
            for (size_t j = 0; j < dim; j++) {
                gk[i] += mat[i * dim + j] * known[j];
            }
        }
        float gu[dim];
        for (size_t i = 0; i < dim; i++) {
            if (fabsf(gk[i]) > 1e-6f)
                gu[i] = bound[i] / gk[i];
            else
                gu[i] = 0;
        }
        for (size_t i = 0; i < dim; i++) {
            out[i] = 0;
            for (size_t j = 0; j < dim; j++) {
                out[i] += mat[j * dim + i] * gu[j];
            }
        }
    } else {
        for (size_t i = 0; i < dim; i++) {
            if (fabsf(known[i]) > 1e-6f)
                out[i] = bound[i] / known[i];
            else
                out[i] = 0;
        }
    }
}

/**
 * 复数域解�?
 */
static void unbind_complex_op(const float* bound, const float* known, float* out, size_t dim) {
    for (size_t i = 0; i < dim; i++) {
        if (fabsf(known[i]) > 1e-6)
            out[i] = bound[i] / known[i];
        else
            out[i] = 0;
    }
}

agentos_error_t agentos_unbinder_unbind(
    agentos_unbinder_t* unbinder,
    const float* bound_vector,
    const float* known_vectors[],
    size_t known_count,
    float*** out_vectors,
    size_t* out_count) {

    if (!unbinder || !bound_vector || !known_vectors || known_count == 0 || !out_vectors || !out_count)
        return AGENTOS_EINVAL;

    agentos_binder_t* binder = unbinder->binder;
    size_t dim = binder->dimension;
    int use_complex = binder->use_complex;
    int Q = (binder->Q && dim > 0) ? (int)binder->Q[0] : 1;

    // 目前只支持已知一个向量的情况
    if (known_count != 1) return AGENTOS_ENOTSUP;

    float** results = (float**)AGENTOS_MALLOC(sizeof(float*));
    if (!results) return AGENTOS_ENOMEM;

    float* unknown = (float*)AGENTOS_MALLOC(dim * sizeof(float));
    if (!unknown) {
        AGENTOS_FREE(results);
        return AGENTOS_ENOMEM;
    }

    agentos_mutex_lock(unbinder->lock);

    if (use_complex) {
        unbind_complex_op(bound_vector, known_vectors[0], unknown, dim);
    } else {
        if (Q == 1 && binder->bind_matrices) {
            float g_output[dim];
            memset(g_output, 0, dim * sizeof(float));
            binder->bind_matrices(binder, known_vectors[0], g_output);
            unbind_real_q1(bound_vector, g_output, unknown, dim, NULL);
        } else {
            memset(unknown, 0, dim * sizeof(float));
        }
    }

    agentos_mutex_unlock(unbinder->lock);

    results[0] = unknown;
    *out_vectors = results;
    *out_count = 1;
    return AGENTOS_SUCCESS;
}
