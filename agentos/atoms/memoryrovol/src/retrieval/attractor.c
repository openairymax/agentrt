/**
 * @file attractor.c
 * @brief 吸引子网络检索实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "../include/retrieval.h"
#include "../include/layer2_feature.h"
#include "agentos.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include <agentos/utils/memory/memory_compat.h>
#include <agentos/utils/string/string_compat.h>
#include <string.h>
#include <math.h>

struct agentos_attractor_network {
    agentos_layer2_feature_t* layer2;        /**< 特征层，用于获取向量 */
    agentos_retrieval_config_t manager;        /**< 配置 */
    agentos_mutex_t* lock;                    /**< 线程锁 */
};

agentos_error_t agentos_attractor_network_create(
    agentos_layer2_feature_t* layer2,
    const agentos_retrieval_config_t* manager,
    agentos_attractor_network_t** out_net) {


    if (!layer2 || !out_net) return AGENTOS_EINVAL;

    agentos_attractor_network_t* net = (agentos_attractor_network_t*)AGENTOS_CALLOC(1, sizeof(agentos_attractor_network_t));
    if (!net) {
        AGENTOS_LOG_ERROR("Failed to allocate attractor network");
        return AGENTOS_ENOMEM;
    }

    net->layer2 = layer2;
    if (manager) {
        net->manager = *manager;
    } else {
        net->manager.max_iterations = 10;
        net->manager.tolerance = 1e-6f;
        net->manager.beta = 1.0f;
    }
    net->lock = agentos_mutex_create();
    if (!net->lock) {
        AGENTOS_FREE(net);
        return AGENTOS_ENOMEM;
    }

    *out_net = net;
    return AGENTOS_SUCCESS;
}

void agentos_attractor_network_destroy(agentos_attractor_network_t* net) {
    if (!net) return;
    if (net->lock) agentos_mutex_destroy(net->lock);
    AGENTOS_FREE(net);
}

/**
 * @brief 使用现代Hopfield网络迭代更新状态
 */
static agentos_error_t attractor_iterate(
    agentos_attractor_network_t* net,
    const float* query_vector,
    const char** candidate_ids,
    size_t candidate_count,
    char** best_id,
    float* best_confidence) {

    if (candidate_count == 0) {
        *best_id = NULL;
        *best_confidence = 0.0f;
        return AGENTOS_SUCCESS;
    }

    // 获取所有候选向量的维度（假设第一个即可）
    size_t dim = 0;
    agentos_feature_vector_t* first_vec = NULL;
    agentos_error_t err = agentos_layer2_feature_get_vector(net->layer2, candidate_ids[0], &first_vec);
    if (err != AGENTOS_SUCCESS) {
        AGENTOS_LOG_ERROR("Failed to get vector for %s", candidate_ids[0]);
        return err;
    }
    dim = first_vec->dim;
    agentos_feature_vector_free(first_vec);

    // 构建候选向量矩�?
    float* patterns = (float*)AGENTOS_MALLOC(candidate_count * dim * sizeof(float));
    if (!patterns || (candidate_count > 0 && dim > 0 && candidate_count * dim / dim != candidate_count)) {
        AGENTOS_LOG_ERROR("Failed to allocate patterns matrix");
        return AGENTOS_ENOMEM;
    }

    for (size_t i = 0; i < candidate_count; i++) {
        agentos_feature_vector_t* vec = NULL;
        err = agentos_layer2_feature_get_vector(net->layer2, candidate_ids[i], &vec);
        if (err != AGENTOS_SUCCESS) {
            AGENTOS_FREE(patterns);
            return err;
        }
        memcpy(patterns + i * dim, vec->data, dim * sizeof(float));
        agentos_feature_vector_free(vec);
    }

    // 初始状�?= 查询向量（需归一化）
    float* state = (float*)AGENTOS_MALLOC(dim * sizeof(float));
    if (!state) {
        AGENTOS_FREE(patterns);
        return AGENTOS_ENOMEM;
    }
    memcpy(state, query_vector, dim * sizeof(float));

    // 迭代
    float* new_state = (float*)AGENTOS_MALLOC(dim * sizeof(float));
    if (!new_state) {
        AGENTOS_FREE(patterns);
        AGENTOS_FREE(state);
        return AGENTOS_ENOMEM;
    }

    float* proj = (float*)AGENTOS_MALLOC(candidate_count * sizeof(float));
    if (!proj) {
        AGENTOS_FREE(patterns);
        AGENTOS_FREE(state);
        AGENTOS_FREE(new_state);
        return AGENTOS_ENOMEM;
    }

    for (uint32_t iter = 0; iter < net->manager.max_iterations; iter++) {
        memset(proj, 0, candidate_count * sizeof(float));
        for (size_t i = 0; i < candidate_count; i++) {
            const float* pat = patterns + i * dim;
            float dot = 0.0f;
            for (size_t j = 0; j < dim; j++) {
                dot += pat[j] * state[j];
            }
            proj[i] = dot;
        }

        // 更新状�?
        for (size_t j = 0; j < dim; j++) {
            float sum = 0.0f;
            for (size_t i = 0; i < candidate_count; i++) {
                sum += patterns[i * dim + j] * proj[i];
            }
            new_state[j] = tanhf(net->manager.beta * sum);
        }

        // 检查收�?
        float diff = 0.0f;
        for (size_t j = 0; j < dim; j++) {
            float d = state[j] - new_state[j];
            diff += d * d;
        }
        memcpy(state, new_state, dim * sizeof(float));
        if (diff < net->manager.tolerance) break;
    }

    AGENTOS_FREE(proj);

    // 找到与最终状态最接近的候选模式
    float best_sim = -1.0f;
    int best_idx = -1;
    for (size_t i = 0; i < candidate_count; i++) {
        const float* pat = patterns + i * dim;
        float dot = 0.0f, norm_pat = 0.0f, norm_state = 0.0f;
        for (size_t j = 0; j < dim; j++) {
            dot += pat[j] * state[j];
            norm_pat += pat[j] * pat[j];
            norm_state += state[j] * state[j];
        }
        float sim = dot / (sqrtf(norm_pat) * sqrtf(norm_state) + 1e-8f);
        if (sim > best_sim) {
            best_sim = sim;
            best_idx = i;
        }
    }

    if (best_idx >= 0) {
        *best_id = AGENTOS_STRDUP(candidate_ids[best_idx]);
        *best_confidence = best_sim;
    } else {
        *best_id = NULL;
        *best_confidence = 0.0f;
    }

    AGENTOS_FREE(patterns);
    AGENTOS_FREE(state);
    AGENTOS_FREE(new_state);
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_attractor_network_retrieve(
    agentos_attractor_network_t* net,
    const float* query_vector,
    const char** candidate_ids,
    size_t candidate_count,
    char** out_best_id,
    float* out_confidence) {

    if (!net || !query_vector || !out_best_id || !out_confidence) return AGENTOS_EINVAL;
    return attractor_iterate(net, query_vector, candidate_ids, candidate_count, out_best_id, out_confidence);
}
