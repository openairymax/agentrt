/**
 * @file similarity.c
 * @brief 相似度计算函数
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#include "layer2_feature.h"
#include <math.h>

float agentos_similarity_cosine(const float* a, const float* b, size_t dim) {
    float dot = 0.0f, na = 0.0f, nb = 0.0f;
    for (size_t i = 0; i < dim; i++) {
        dot += a[i] * b[i];
        na += a[i] * a[i];
        nb += b[i] * b[i];
    }
    if (na == 0.0f || nb == 0.0f) return 0.0f;
    return dot / (sqrtf(na) * sqrtf(nb));
}

float agentos_similarity_l2_squared(const float* a, const float* b, size_t dim) {
    float dist = 0.0f;
    for (size_t i = 0; i < dim; i++) {
        float d = a[i] - b[i];
        dist += d * d;
    }
    return dist;
}

float agentos_distance_to_score(float distance, const char* metric) {
    if (strcmp(metric, "cosine") == 0) {
        return 1.0f - distance;
    } else if (strcmp(metric, "l2") == 0) {
        return expf(-distance);
    } else if (strcmp(metric, "ip") == 0) {
        return distance;
    }
    return 0.0f;
}

void agentos_similarity_normalize(float* vec, size_t dim) {
    float norm = 0.0f;
    for (size_t i = 0; i < dim; i++) {
        norm += vec[i] * vec[i];
    }
    if (norm > 0.0f) {
        float inv = 1.0f / sqrtf(norm);
        for (size_t i = 0; i < dim; i++) {
            vec[i] *= inv;
        }
    }
}