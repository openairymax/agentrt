/**
 * @file layer2_feature.h
 * @brief L2 特征层接口（简化版）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_LAYER2_FEATURE_H
#define AGENTOS_LAYER2_FEATURE_H

#include "agentos.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief L2 特征层句柄
 */
typedef struct agentos_layer2_feature agentos_layer2_feature_t;

/**
 * @brief 索引类型
 */
typedef enum {
    AGENTOS_INDEX_HNSW = 0,
    AGENTOS_INDEX_IVF,
    AGENTOS_INDEX_FLAT
} agentos_index_type_t;

/**
 * @brief L2 配置
 */
typedef struct agentos_layer2_feature_config {
    const char* index_path;
    const char* embedding_model;
    uint32_t dimension;
    agentos_index_type_t index_type;
    uint32_t hnsw_m;
    uint32_t ivf_nlist;
} agentos_layer2_feature_config_t;

/**
 * @brief 创建L2特征层
 */
agentos_error_t agentos_layer2_feature_create(
    const agentos_layer2_feature_config_t* manager,
    agentos_layer2_feature_t** out);

/**
 * @brief 销毁L2特征层
 */
void agentos_layer2_feature_destroy(agentos_layer2_feature_t* l2);

/**
 * @brief 添加特征向量
 */
agentos_error_t agentos_layer2_feature_add(
    agentos_layer2_feature_t* l2,
    const char* id,
    const char* text);

/**
 * @brief 删除特征向量
 */
agentos_error_t agentos_layer2_feature_remove(
    agentos_layer2_feature_t* l2,
    const char* id);

/**
 * @brief 搜索相似向量
 */
agentos_error_t agentos_layer2_feature_search(
    agentos_layer2_feature_t* l2,
    const char* query,
    uint32_t k,
    char*** out_ids,
    float** out_scores,
    size_t* out_count);

agentos_error_t agentos_layer2_feature_stats(
    agentos_layer2_feature_t* l2,
    size_t* out_count);

typedef struct agentos_bm25_index agentos_bm25_index_t;

agentos_bm25_index_t* agentos_bm25_index_create(size_t capacity);
void agentos_bm25_index_destroy(agentos_bm25_index_t* index);
int agentos_bm25_index_add_doc(agentos_bm25_index_t* index, const char* doc_id,
                                const char** terms, size_t term_count);
int agentos_bm25_index_search(const agentos_bm25_index_t* index, const char** query_terms,
                               size_t query_count, size_t max_results,
                               char*** out_doc_ids, double** out_scores,
                               size_t* out_count);
size_t agentos_bm25_index_doc_count(const agentos_bm25_index_t* index);
void agentos_bm25_index_reset(agentos_bm25_index_t* index);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_LAYER2_FEATURE_H */
