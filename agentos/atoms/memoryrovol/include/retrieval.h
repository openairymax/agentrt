/**
 * @file retrieval.h
 * @brief 检索系统接口（简化版）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_RETRIEVAL_H
#define AGENTOS_RETRIEVAL_H

#include "agentos.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 检索结果
 */
typedef struct agentos_retrieval_result {
    char* record_id;
    float score;
    char* metadata;
} agentos_retrieval_result_t;

/**
 * @brief 检索配置
 */
typedef struct agentos_retrieval_config {
    uint32_t cache_size;
    uint32_t rerank_top_k;
    int enable_attractor;
    uint32_t max_iterations;
    float tolerance;
    float beta;
} agentos_retrieval_config_t;

typedef struct agentos_feature_vector {
    size_t dim;
    float* data;
} agentos_feature_vector_t;

void agentos_feature_vector_free(agentos_feature_vector_t* vec);

agentos_error_t agentos_layer2_feature_get_vector(
    void* layer2,
    const char* id,
    agentos_feature_vector_t** out_vec);

/* 前向声明：吸引子网络类型（定义在 retrieval/attractor.c） */
struct agentos_attractor_network;
typedef struct agentos_attractor_network agentos_attractor_network_t;

/* 前向声明：检索缓存类型（定义在 retrieval/cache.c） */
struct agentos_retrieval_cache;
typedef struct agentos_retrieval_cache agentos_retrieval_cache_t;

/**
 * @brief 执行检索
 */
agentos_error_t agentos_retrieval_search(
    const char* query,
    uint32_t limit,
    agentos_retrieval_result_t** out_results,
    size_t* out_count);

/**
 * @brief 释放检索结果
 */
void agentos_retrieval_results_free(
    agentos_retrieval_result_t* results,
    size_t count);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_RETRIEVAL_H */
