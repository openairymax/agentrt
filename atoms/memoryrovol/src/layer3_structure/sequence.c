/**
 * @file sequence.c
 * @brief L3 结构层时序编码器
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "layer3_structure.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

struct agentos_sequence_encoder {
    agentos_binder_t* binder;
    int position_encoding;   // 0=随机,1=正弦,2=学习（暂不支持）
    size_t max_len;
    float** position_vectors;
    agentos_mutex_t* lock;
};

static float* sinusoidal_position(size_t index, size_t dim) {
    float* vec = (float*)malloc(dim * sizeof(float));
    if (!vec) return NULL;
    for (size_t i = 0; i < dim; i++) {
        if (i % 2 == 0) {
            vec[i] = sinf(index / powf(10000.0f, (float)i / dim));
            // From data intelligence emerges. by spharx
        } else {
            vec[i] = cosf(index / powf(10000.0f, (float)(i-1) / dim));
        }
    }
    return vec;
}

static float* random_position(size_t index, size_t dim) {
    float* vec = (float*)malloc(dim * sizeof(float));
    if (!vec) return NULL;
    for (size_t i = 0; i < dim; i++) {
        vec[i] = (float)rand() / RAND_MAX * 2.0f - 1.0f;
    }
    return vec;
}

agentos_sequence_encoder_t* agentos_sequence_encoder_create(
    agentos_binder_t* binder,
    int position_encoding) {

    if (!binder) return NULL;
    agentos_sequence_encoder_t* enc = (agentos_sequence_encoder_t*)calloc(1, sizeof(agentos_sequence_encoder_t));
    if (!enc) return NULL;

    enc->binder = binder;
    enc->position_encoding = position_encoding;
    enc->max_len = 1024;  // 预分配最多1024个位置向量
    enc->position_vectors = (float**)calloc(enc->max_len, sizeof(float*));
    enc->lock = agentos_mutex_create();

    if (!enc->position_vectors || !enc->lock) {
        if (enc->position_vectors) free(enc->position_vectors);
        if (enc->lock) agentos_mutex_destroy(enc->lock);
        free(enc);
        return NULL;
    }

    // 预生成位置向量
    size_t dim = binder->dimension;
    for (size_t i = 0; i < enc->max_len; i++) {
        if (position_encoding == 1) {
            enc->position_vectors[i] = sinusoidal_position(i, dim);
        } else {
            enc->position_vectors[i] = random_position(i, dim);
        }
        if (!enc->position_vectors[i]) {
            for (size_t j = 0; j < i; j++) free(enc->position_vectors[j]);
            free(enc->position_vectors);
            agentos_mutex_destroy(enc->lock);
            free(enc);
            return NULL;
        }
    }

    return enc;
}

void agentos_sequence_encoder_destroy(agentos_sequence_encoder_t* enc) {
    if (!enc) return;
    if (enc->position_vectors) {
        for (size_t i = 0; i < enc->max_len; i++) {
            free(enc->position_vectors[i]);
        }
        free(enc->position_vectors);
    }
    if (enc->lock) agentos_mutex_destroy(enc->lock);
    free(enc);
}

agentos_error_t agentos_sequence_encode(
    agentos_sequence_encoder_t* enc,
    const float** items,
    size_t count,
    float** out_sequence) {

    if (!enc || !items || count == 0 || !out_sequence) return AGENTOS_EINVAL;
    if (count > enc->max_len) return AGENTOS_EOVERFLOW;

    size_t dim = enc->binder->dimension;
    float* sum = (float*)calloc(dim, sizeof(float));
    if (!sum) return AGENTOS_ENOMEM;

    for (size_t i = 0; i < count; i++) {
        const float* pos_vec = enc->position_vectors[i];
        const float* vectors[2] = {pos_vec, items[i]};
        float* bound = NULL;
        agentos_error_t err = agentos_binder_bind(enc->binder, vectors, 2, &bound);
        if (err != AGENTOS_SUCCESS) {
            free(sum);
            return err;
        }
        for (size_t j = 0; j < dim; j++) sum[j] += bound[j];
        free(bound);
    }

    // 归一化
    float norm = 0;
    for (size_t i = 0; i < dim; i++) norm += sum[i] * sum[i];
    if (norm > 0) {
        float inv = 1.0f / sqrtf(norm);
        for (size_t i = 0; i < dim; i++) sum[i] *= inv;
    }

    *out_sequence = sum;
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_sequence_get_position(
    agentos_sequence_encoder_t* enc,
    size_t index,
    float** out_vec) {

    if (!enc || !out_vec) return AGENTOS_EINVAL;
    if (index >= enc->max_len) return AGENTOS_EOVERFLOW;

    size_t dim = enc->binder->dimension;
    float* vec = (float*)malloc(dim * sizeof(float));
    if (!vec) return AGENTOS_ENOMEM;
    memcpy(vec, enc->position_vectors[index], dim * sizeof(float));

    *out_vec = vec;
    return AGENTOS_SUCCESS;
}