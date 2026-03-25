/**
 * @file binder.c
 * @brief L3 结构层绑定算子实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "layer3_structure.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

struct agentos_binder {
    size_t dimension;
    int Q;
    int use_complex;
    float** bind_matrices;  // Q个维度为dimension*dimension的矩阵
    agentos_mutex_t* lock;
};

static float** create_random_matrices(size_t dim, int Q) {
    float** mats = (float**)malloc(Q * sizeof(float*));
    if (!mats) return NULL;
    for (int k = 0; k < Q; k++) {
        mats[k] = (float*)malloc(dim * dim * sizeof(float));
        if (!mats[k]) {
        // From data intelligence emerges. by spharx
            for (int i = 0; i < k; i++) free(mats[i]);
            free(mats);
            return NULL;
        }
        // 随机初始化 [-1,1]
        for (size_t i = 0; i < dim; i++) {
            for (size_t j = 0; j < dim; j++) {
                mats[k][i * dim + j] = (float)rand() / RAND_MAX * 2.0f - 1.0f;
            }
        }
    }
    return mats;
}

static void bind_real(const float* a, const float* b, float* out, size_t dim, int Q, float** mats) {
    memset(out, 0, dim * sizeof(float));
    for (int k = 0; k < Q; k++) {
        float* ta = (float*)alloca(dim * sizeof(float));
        float* tb = (float*)alloca(dim * sizeof(float));
        for (size_t i = 0; i < dim; i++) {
            ta[i] = 0; tb[i] = 0;
            for (size_t j = 0; j < dim; j++) {
                ta[i] += mats[k][i * dim + j] * a[j];
                tb[i] += mats[k][i * dim + j] * b[j];
            }
        }
        for (size_t i = 0; i < dim; i++) {
            out[i] += ta[i] * tb[i];
        }
    }
    // 归一化
    float norm = 0;
    for (size_t i = 0; i < dim; i++) norm += out[i] * out[i];
    if (norm > 0) {
        float inv = 1.0f / sqrtf(norm);
        for (size_t i = 0; i < dim; i++) out[i] *= inv;
    }
}

static void bind_complex(const float* a, const float* b, float* out, size_t dim) {
    for (size_t i = 0; i < dim; i++) {
        out[i] = a[i] * b[i];
    }
}

agentos_binder_t* agentos_binder_create(size_t dimension, int Q, int use_complex) {
    if (dimension == 0) return NULL;
    if (Q < 1 || Q > (int)dimension) Q = 1;

    agentos_binder_t* binder = (agentos_binder_t*)calloc(1, sizeof(agentos_binder_t));
    if (!binder) return NULL;

    binder->dimension = dimension;
    binder->Q = Q;
    binder->use_complex = use_complex;
    binder->lock = agentos_mutex_create();
    if (!binder->lock) {
        free(binder);
        return NULL;
    }

    if (!use_complex) {
        binder->bind_matrices = create_random_matrices(dimension, Q);
        if (!binder->bind_matrices) {
            agentos_mutex_destroy(binder->lock);
            free(binder);
            return NULL;
        }
    }

    return binder;
}

void agentos_binder_destroy(agentos_binder_t* binder) {
    if (!binder) return;
    if (binder->bind_matrices) {
        for (int i = 0; i < binder->Q; i++) free(binder->bind_matrices[i]);
        free(binder->bind_matrices);
    }
    if (binder->lock) agentos_mutex_destroy(binder->lock);
    free(binder);
}

agentos_error_t agentos_binder_bind(
    agentos_binder_t* binder,
    const float* vectors[],
    size_t count,
    float** out_vector) {

    if (!binder || !vectors || count == 0 || !out_vector) return AGENTOS_EINVAL;

    size_t dim = binder->dimension;
    float* result = (float*)malloc(dim * sizeof(float));
    if (!result) return AGENTOS_ENOMEM;

    agentos_mutex_lock(binder->lock);

    if (count == 1) {
        memcpy(result, vectors[0], dim * sizeof(float));
    } else {
        memcpy(result, vectors[0], dim * sizeof(float));
        for (size_t i = 1; i < count; i++) {
            float* temp = (float*)alloca(dim * sizeof(float));
            if (binder->use_complex) {
                bind_complex(result, vectors[i], temp, dim);
            } else {
                bind_real(result, vectors[i], temp, dim, binder->Q, binder->bind_matrices);
            }
            memcpy(result, temp, dim * sizeof(float));
        }
    }

    agentos_mutex_unlock(binder->lock);
    *out_vector = result;
    return AGENTOS_SUCCESS;
}

int agentos_binder_get_q(agentos_binder_t* binder) {
    return binder ? binder->Q : 0;
}

void agentos_binder_set_q(agentos_binder_t* binder, int Q) {
    if (!binder || Q < 1 || Q > (int)binder->dimension) return;
    agentos_mutex_lock(binder->lock);
    if (Q != binder->Q) {
        if (binder->bind_matrices) {
            for (int i = 0; i < binder->Q; i++) free(binder->bind_matrices[i]);
            free(binder->bind_matrices);
        }
        binder->Q = Q;
        binder->bind_matrices = create_random_matrices(binder->dimension, Q);
    }
    agentos_mutex_unlock(binder->lock);
}