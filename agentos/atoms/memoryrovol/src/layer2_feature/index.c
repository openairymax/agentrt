/**
 * @file layer2_feature.c
 * @brief L2 特征层实�?
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "../include/layer2_feature.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../../../agentos/commons/utils/memory/include/memory_compat.h"
#include "../../../agentos/commons/utils/string/include/string_compat.h"
#include <string.h>
#include <pthread.h>
#include <math.h>

#define DEFAULT_HNSW_M 16
#define DEFAULT_EF_CONSTRUCTION 200
#define DEFAULT_EF_SEARCH 100

/**
 * @brief HNSW 节点
 */
typedef struct hnsw_node {
    char* id;
    float* vector;
    float norm;           /* 预计算的向量L2范数平方 */
    uint32_t level;
    struct hnsw_node** neighbors;
    size_t* neighbor_counts;
} hnsw_node_t;

/**
 * @brief HNSW 索引
 */
typedef struct hnsw_index {
    hnsw_node_t** nodes;
    size_t node_count;
    size_t capacity;
    uint32_t dimension;
    uint32_t m;
    uint32_t ef_construction;
    uint32_t ef_search;
    pthread_mutex_t mutex;
} hnsw_index_t;

struct agentos_layer2_feature {
    hnsw_index_t* hnsw;
    agentos_index_type_t index_type;
    char index_path[256];
    char embedding_model[128];
    uint32_t dimension;
};

/**
 * @brief 计算余弦相似�?
 */
static float cosine_similarity(const float* a, const float* b, uint32_t dim) {
    float dot = 0.0f;
    float norm_a = 0.0f;
    float norm_b = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }
    if (norm_a == 0 || norm_b == 0) return 0.0f;
    return dot / (sqrtf(norm_a) * sqrtf(norm_b));
}

/* 使用预计算范数的余弦相似度计算 */
static float cosine_similarity_precomputed(const float* a, const float* b, float a_norm_sq, float b_norm_sq, uint32_t dim) {
    float dot = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        dot += a[i] * b[i];
    }
    if (a_norm_sq == 0 || b_norm_sq == 0) return 0.0f;
    return dot / (sqrtf(a_norm_sq) * sqrtf(b_norm_sq));
}

/**
 * @brief 生成简单文本向量（用于演示�?
 */
static float* generate_text_embedding(const char* text, uint32_t dim) {
    if (!text) return NULL;
    float* vector = (float*)AGENTOS_CALLOC(dim, sizeof(float));
    if (!vector) return NULL;
    size_t len = strlen(text);
    for (uint32_t i = 0; i < dim; i++) {
        vector[i] = (float)((unsigned char)text[i % len]) / 255.0f;
    }
    return vector;
}

/**
 * @brief 创建 HNSW 索引
 */
static hnsw_index_t* hnsw_create(uint32_t dimension, uint32_t m) {
    hnsw_index_t* index = (hnsw_index_t*)AGENTOS_CALLOC(1, sizeof(hnsw_index_t));
    if (!index) return NULL;
    index->dimension = dimension;
    index->m = m > 0 ? m : DEFAULT_HNSW_M;
    index->ef_construction = DEFAULT_EF_CONSTRUCTION;
    index->ef_search = DEFAULT_EF_SEARCH;
    index->capacity = 1024;
    index->nodes = (hnsw_node_t**)AGENTOS_CALLOC(index->capacity, sizeof(hnsw_node_t*));
    if (!index->nodes) {
        AGENTOS_FREE(index);
        return NULL;
    }
    pthread_mutex_init(&index->mutex, NULL);
    return index;
}

/**
 * @brief 销�?HNSW 索引
 */
static void hnsw_destroy(hnsw_index_t* index) {
    if (!index) return;
    for (size_t i = 0; i < index->node_count; i++) {
        hnsw_node_t* node = index->nodes[i];
        if (node) {
            if (node->id) AGENTOS_FREE(node->id);
            if (node->vector) AGENTOS_FREE(node->vector);
            if (node->neighbors) AGENTOS_FREE(node->neighbors);
            if (node->neighbor_counts) AGENTOS_FREE(node->neighbor_counts);
            AGENTOS_FREE(node);
        }
    }
    AGENTOS_FREE(index->nodes);
    pthread_mutex_destroy(&index->mutex);
    AGENTOS_FREE(index);
}

/**
 * @brief 添加向量到索�?
 */
static agentos_error_t hnsw_add(hnsw_index_t* index, const char* id, const float* vector) {
    if (!index || !id || !vector) return AGENTOS_EINVAL;

    pthread_mutex_lock(&index->mutex);

    if (index->node_count >= index->capacity) {
        size_t new_cap = index->capacity * 2;
        hnsw_node_t** new_nodes = (hnsw_node_t**)AGENTOS_REALLOC(index->nodes, new_cap * sizeof(hnsw_node_t*));
        if (!new_nodes) {
            pthread_mutex_unlock(&index->mutex);
            return AGENTOS_ENOMEM;
        }
        index->nodes = new_nodes;
        index->capacity = new_cap;
    }

    hnsw_node_t* node = (hnsw_node_t*)AGENTOS_CALLOC(1, sizeof(hnsw_node_t));
    if (!node) {
        pthread_mutex_unlock(&index->mutex);
        return AGENTOS_ENOMEM;
    }

    node->id = AGENTOS_STRDUP(id);
    node->vector = (float*)AGENTOS_MALLOC(index->dimension * sizeof(float));
    if (!node->vector) {
        AGENTOS_FREE(node->id);
        AGENTOS_FREE(node);
        pthread_mutex_unlock(&index->mutex);
        return AGENTOS_ENOMEM;
    }
    memcpy(node->vector, vector, index->dimension * sizeof(float));
    /* 预计算向量范数平方 */
    float norm_sq = 0.0f;
    for (uint32_t i = 0; i < index->dimension; i++) {
        norm_sq += vector[i] * vector[i];
    }
    node->norm = norm_sq;
    node->level = 1;
    node->neighbors = (hnsw_node_t**)AGENTOS_CALLOC(node->level + 1, sizeof(hnsw_node_t*));
    node->neighbor_counts = (size_t*)AGENTOS_CALLOC(node->level + 1, sizeof(size_t));

    index->nodes[index->node_count++] = node;

    pthread_mutex_unlock(&index->mutex);
    return AGENTOS_SUCCESS;
}

/**
 * @brief 搜索最近邻
 */
static agentos_error_t hnsw_search(hnsw_index_t* index, const float* query, uint32_t k, char*** out_ids, float** out_scores, size_t* out_count) {
    if (!index || !query || !out_ids || !out_scores || !out_count) return AGENTOS_EINVAL;

    pthread_mutex_lock(&index->mutex);

    if (index->node_count == 0) {
        pthread_mutex_unlock(&index->mutex);
        *out_ids = NULL;
        *out_scores = NULL;
        *out_count = 0;
        return AGENTOS_SUCCESS;
    }

    size_t result_count = k < index->node_count ? k : index->node_count;
    char** ids = (char**)AGENTOS_CALLOC(result_count, sizeof(char*));
    float* scores = (float*)AGENTOS_CALLOC(result_count, sizeof(float));

    if (!ids || !scores) {
        if (ids) AGENTOS_FREE(ids);
        if (scores) AGENTOS_FREE(scores);
        pthread_mutex_unlock(&index->mutex);
        return AGENTOS_ENOMEM;
    }

    for (size_t i = 0; i < result_count; i++) {
        scores[i] = -1.0f;
        ids[i] = NULL;
    }

    /* 预计算查询向量的范数平方 */
    float query_norm_sq = 0.0f;
    for (uint32_t d = 0; d < index->dimension; d++) {
        query_norm_sq += query[d] * query[d];
    }
    
    for (size_t i = 0; i < index->node_count; i++) {
        float sim = cosine_similarity_precomputed(query, index->nodes[i]->vector, 
                                                  query_norm_sq, index->nodes[i]->norm, index->dimension);
        for (size_t j = 0; j < result_count; j++) {
            if (sim > scores[j]) {
                for (size_t m = result_count - 1; m > j; m--) {
                    scores[m] = scores[m - 1];
                    if (ids[m - 1]) AGENTOS_FREE(ids[m - 1]);
                    ids[m] = ids[m - 1] ? AGENTOS_STRDUP(ids[m - 1]) : NULL;
                }
                scores[j] = sim;
                if (ids[j]) AGENTOS_FREE(ids[j]);
                ids[j] = AGENTOS_STRDUP(index->nodes[i]->id);
                break;
            }
        }
    }

    pthread_mutex_unlock(&index->mutex);

    *out_ids = ids;
    *out_scores = scores;
    *out_count = result_count;

    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_layer2_feature_create(
    const agentos_layer2_feature_config_t* manager,
    agentos_layer2_feature_t** out) {
    if (!out) return AGENTOS_EINVAL;

    agentos_layer2_feature_t* l2 = (agentos_layer2_feature_t*)AGENTOS_CALLOC(1, sizeof(agentos_layer2_feature_t));
    if (!l2) return AGENTOS_ENOMEM;

    l2->index_type = manager && manager->index_type ? manager->index_type : AGENTOS_INDEX_HNSW;
    l2->dimension = manager && manager->dimension ? manager->dimension : 128;
    l2->m = manager && manager->hnsw_m ? manager->hnsw_m : DEFAULT_HNSW_M;

    if (manager && manager->index_path) {
        snprintf(l2->index_path, sizeof(l2->index_path), "%s", manager->index_path);
    }
    if (manager && manager->embedding_model) {
        snprintf(l2->embedding_model, sizeof(l2->embedding_model), "%s", manager->embedding_model);
    }

    l2->hnsw = hnsw_create(l2->dimension, l2->m);
    if (!l2->hnsw) {
        AGENTOS_FREE(l2);
        return AGENTOS_ENOMEM;
    }

    *out = l2;
    return AGENTOS_SUCCESS;
}

void agentos_layer2_feature_destroy(agentos_layer2_feature_t* l2) {
    if (!l2) return;
    if (l2->hnsw) {
        hnsw_destroy(l2->hnsw);
    }
    AGENTOS_FREE(l2);
}

agentos_error_t agentos_layer2_feature_add(
    agentos_layer2_feature_t* l2,
    const char* id,
    const char* text) {
    if (!l2 || !id || !text) return AGENTOS_EINVAL;

    float* vector = generate_text_embedding(text, l2->dimension);
    if (!vector) return AGENTOS_ENOMEM;

    agentos_error_t err = hnsw_add(l2->hnsw, id, vector);
    AGENTOS_FREE(vector);

    return err;
}

agentos_error_t agentos_layer2_feature_remove(
    agentos_layer2_feature_t* l2,
    const char* id) {
    if (!l2 || !id) return AGENTOS_EINVAL;
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_layer2_feature_search(
    agentos_layer2_feature_t* l2,
    const char* query,
    uint32_t k,
    char*** out_ids,
    float** out_scores,
    size_t* out_count) {
    if (!l2 || !query || !out_ids || !out_scores || !out_count) return AGENTOS_EINVAL;

    float* vector = generate_text_embedding(query, l2->dimension);
    if (!vector) return AGENTOS_ENOMEM;

    agentos_error_t err = hnsw_search(l2->hnsw, vector, k, out_ids, out_scores, out_count);
    AGENTOS_FREE(vector);

    return err;
}
