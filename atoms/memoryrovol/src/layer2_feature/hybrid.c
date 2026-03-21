/**
 * @file hybrid.c
 * @brief 混合检索实现（加权融合、RRF）
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#include "layer2_feature.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define RRF_K 60

typedef struct {
    char* id;
    float score;
    int bm25_rank;
    int vec_rank;
} hybrid_item_t;

static int cmp_score_desc(const void* a, const void* b) {
    const hybrid_item_t* ha = (const hybrid_item_t*)a;
    const hybrid_item_t* hb = (const hybrid_item_t*)b;
    if (hb->score > ha->score) return 1;
    if (hb->score < ha->score) return -1;
    return 0;
}

agentos_error_t agentos_hybrid_search_weighted(
    agentos_layer2_feature_t* layer,
    agentos_bm25_index_t* bm25_idx,
    const char* query,
    uint32_t top_k,
    float vector_weight,
    char*** out_record_ids,
    float** out_scores,
    size_t* out_count) {

    if (!layer || !bm25_idx || !query || !out_record_ids || !out_scores || !out_count)
        return AGENTOS_EINVAL;

    // 获取向量搜索结果（取2倍候选）
    char** vec_ids = NULL;
    float* vec_scores = NULL;
    size_t vec_count = 0;
    agentos_error_t err = agentos_layer2_feature_search(layer, query, top_k * 2, &vec_ids, &vec_scores, &vec_count);
    if (err != AGENTOS_SUCCESS) return err;

    // 获取BM25搜索结果
    char** bm25_ids = NULL;
    float* bm25_scores = NULL;
    size_t bm25_count = 0;
    err = agentos_bm25_search(bm25_idx, query, top_k * 2, &bm25_ids, &bm25_scores, &bm25_count);
    if (err != AGENTOS_SUCCESS) {
        for (size_t i = 0; i < vec_count; i++) free(vec_ids[i]);
        free(vec_ids);
        free(vec_scores);
        return err;
    }

    // 合并去重
    size_t cap = vec_count + bm25_count;
    hybrid_item_t* items = calloc(cap, sizeof(hybrid_item_t));
    if (!items) {
        for (size_t i = 0; i < vec_count; i++) free(vec_ids[i]);
        free(vec_ids); free(vec_scores);
        for (size_t i = 0; i < bm25_count; i++) free(bm25_ids[i]);
        free(bm25_ids); free(bm25_scores);
        return AGENTOS_ENOMEM;
    }

    size_t item_count = 0;
    // 加入向量结果
    for (size_t i = 0; i < vec_count; i++) {
        items[item_count].id = strdup(vec_ids[i]);
        items[item_count].score = vec_scores[i] * vector_weight;
        items[item_count].vec_rank = i + 1;
        item_count++;
    }
    // 加入BM25结果，若已存在则累加
    for (size_t i = 0; i < bm25_count; i++) {
        int found = -1;
        for (size_t j = 0; j < item_count; j++) {
            if (strcmp(items[j].id, bm25_ids[i]) == 0) {
                found = j;
                break;
            }
        }
        if (found >= 0) {
            items[found].score += bm25_scores[i] * (1 - vector_weight);
            items[found].bm25_rank = i + 1;
            free(bm25_ids[i]); // 不再需要
        } else {
            if (item_count >= cap) {
                cap *= 2;
                hybrid_item_t* new_items = realloc(items, cap * sizeof(hybrid_item_t));
                if (!new_items) {
                    // 内存不足，放弃该结果
                    free(bm25_ids[i]);
                    continue;
                }
                items = new_items;
            }
            items[item_count].id = strdup(bm25_ids[i]);
            items[item_count].score = bm25_scores[i] * (1 - vector_weight);
            items[item_count].bm25_rank = i + 1;
            item_count++;
            free(bm25_ids[i]);
        }
    }
    free(bm25_ids);
    free(bm25_scores);
    for (size_t i = 0; i < vec_count; i++) free(vec_ids[i]);
    free(vec_ids);
    free(vec_scores);

    // 按分数排序
    qsort(items, item_count, sizeof(hybrid_item_t), cmp_score_desc);

    size_t result_count = (item_count < top_k) ? item_count : top_k;
    char** result_ids = malloc(result_count * sizeof(char*));
    float* result_scores = malloc(result_count * sizeof(float));
    if (!result_ids || !result_scores) {
        for (size_t i = 0; i < item_count; i++) free(items[i].id);
        free(items);
        if (result_ids) free(result_ids);
        if (result_scores) free(result_scores);
        return AGENTOS_ENOMEM;
    }

    for (size_t i = 0; i < result_count; i++) {
        result_ids[i] = items[i].id;  // 转移所有权
        result_scores[i] = items[i].score;
    }
    // 释放未使用的 items
    for (size_t i = result_count; i < item_count; i++) {
        free(items[i].id);
    }
    free(items);

    *out_record_ids = result_ids;
    *out_scores = result_scores;
    *out_count = result_count;
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_hybrid_search_rrf(
    agentos_layer2_feature_t* layer,
    agentos_bm25_index_t* bm25_idx,
    const char* query,
    uint32_t top_k,
    char*** out_record_ids,
    float** out_scores,
    size_t* out_count) {

    if (!layer || !bm25_idx || !query || !out_record_ids || !out_scores || !out_count)
        return AGENTOS_EINVAL;

    // 获取向量和 BM25 结果
    char** vec_ids = NULL;
    float* vec_scores = NULL;
    size_t vec_count = 0;
    agentos_error_t err = agentos_layer2_feature_search(layer, query, top_k * 2, &vec_ids, &vec_scores, &vec_count);
    if (err != AGENTOS_SUCCESS) return err;

    char** bm25_ids = NULL;
    float* bm25_scores = NULL;
    size_t bm25_count = 0;
    err = agentos_bm25_search(bm25_idx, query, top_k * 2, &bm25_ids, &bm25_scores, &bm25_count);
    if (err != AGENTOS_SUCCESS) {
        for (size_t i = 0; i < vec_count; i++) free(vec_ids[i]);
        free(vec_ids);
        free(vec_scores);
        return err;
    }

    // 构建候选集合
    size_t cap = vec_count + bm25_count;
    hybrid_item_t* items = calloc(cap, sizeof(hybrid_item_t));
    if (!items) {
        for (size_t i = 0; i < vec_count; i++) free(vec_ids[i]);
        free(vec_ids); free(vec_scores);
        for (size_t i = 0; i < bm25_count; i++) free(bm25_ids[i]);
        free(bm25_ids); free(bm25_scores);
        return AGENTOS_ENOMEM;
    }

    size_t item_count = 0;
    for (size_t i = 0; i < vec_count; i++) {
        items[item_count].id = strdup(vec_ids[i]);
        items[item_count].score = 1.0f / (RRF_K + i + 1);
        items[item_count].vec_rank = i + 1;
        item_count++;
    }
    for (size_t i = 0; i < bm25_count; i++) {
        int found = -1;
        for (size_t j = 0; j < item_count; j++) {
            if (strcmp(items[j].id, bm25_ids[i]) == 0) {
                found = j;
                break;
            }
        }
        if (found >= 0) {
            items[found].score += 1.0f / (RRF_K + i + 1);
            items[found].bm25_rank = i + 1;
            free(bm25_ids[i]);
        } else {
            if (item_count >= cap) {
                cap *= 2;
                hybrid_item_t* new_items = realloc(items, cap * sizeof(hybrid_item_t));
                if (!new_items) {
                    free(bm25_ids[i]);
                    continue;
                }
                items = new_items;
            }
            items[item_count].id = strdup(bm25_ids[i]);
            items[item_count].score = 1.0f / (RRF_K + i + 1);
            items[item_count].bm25_rank = i + 1;
            item_count++;
            free(bm25_ids[i]);
        }
    }
    free(bm25_ids);
    free(bm25_scores);
    for (size_t i = 0; i < vec_count; i++) free(vec_ids[i]);
    free(vec_ids);
    free(vec_scores);

    qsort(items, item_count, sizeof(hybrid_item_t), cmp_score_desc);

    size_t result_count = (item_count < top_k) ? item_count : top_k;
    char** result_ids = malloc(result_count * sizeof(char*));
    float* result_scores = malloc(result_count * sizeof(float));
    if (!result_ids || !result_scores) {
        for (size_t i = 0; i < item_count; i++) free(items[i].id);
        free(items);
        if (result_ids) free(result_ids);
        if (result_scores) free(result_scores);
        return AGENTOS_ENOMEM;
    }

    for (size_t i = 0; i < result_count; i++) {
        result_ids[i] = items[i].id;
        result_scores[i] = items[i].score;
    }
    for (size_t i = result_count; i < item_count; i++) {
        free(items[i].id);
    }
    free(items);

    *out_record_ids = result_ids;
    *out_scores = result_scores;
    *out_count = result_count;
    return AGENTOS_SUCCESS;
}