﻿﻿﻿﻿﻿﻿﻿﻿﻿/**
 * @file rerank.c
 * @brief 检索结果重排序（基于交叉编码器，带降级）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

/* 确保标准库头文件在最前面包含 */
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

/* 项目头文件 */
#include "../include/retrieval.h"
#include "../include/llm_client.h"
#include "../include/layer1_raw.h"
#include "agentos.h"

/* Unified base library compatibility layer */
#include "../../../commons/utils/memory/include/memory_compat.h"
#include "../../../commons/utils/string/include/string_compat.h"

struct agentos_reranker {
    agentos_llm_service_t* llm;          /**< LLM服务，用于交叉编码器 */
    agentos_layer1_raw_t* layer1;         /**< 原始层，用于获取文档文本 */
    agentos_mutex_t* lock;
    int use_llm;                          /**< 是否使用LLM（降级标志） */
};

agentos_error_t agentos_reranker_create(
    agentos_llm_service_t* llm,
    agentos_layer1_raw_t* layer1,
    agentos_reranker_t** out_reranker) {
    if (!llm || !layer1 || !out_reranker) return AGENTOS_EINVAL;

    agentos_reranker_t* r = (agentos_reranker_t*)AGENTOS_CALLOC(1, sizeof(agentos_reranker_t));
    if (!r) {
        AGENTOS_LOG_ERROR("Failed to allocate reranker");
        return AGENTOS_ENOMEM;
    }

    r->llm = llm;
    r->layer1 = layer1;
    r->lock = agentos_mutex_create();
    if (!r->lock) {
        AGENTOS_FREE(r);
        return AGENTOS_ENOMEM;
    }
    r->use_llm = 1;  // 默认使用LLM

    *out_reranker = r;
    return AGENTOS_SUCCESS;
}

void agentos_reranker_destroy(agentos_reranker_t* reranker) {
    if (!reranker) return;
    if (reranker->lock) agentos_mutex_destroy(reranker->lock);
    AGENTOS_FREE(reranker);
}

/**
 * 设置是否使用LLM（降级控制）
 */
void agentos_reranker_set_use_llm(agentos_reranker_t* reranker, int use_llm) {
    if (reranker) reranker->use_llm = use_llm;
}

/**
 * 使用交叉编码器对单个候选重打分
 */
static float cross_encoder_score(
    agentos_llm_service_t* llm,
    const char* query,
    const char* document) {

    // 构建prompt，要求模型输出一�?-1之间的分�?
    char prompt[8192];
    int n = snprintf(prompt, sizeof(prompt),
        "You are a relevance judge. Given a query and a document, output a single number between 0 and 1 indicating how relevant the document is to the query.\n"
        "Query: %s\nDocument: %s\nRelevance score (0-1):",
        query, document);

    if (n >= (int)sizeof(prompt)) {
        AGENTOS_LOG_WARN("Prompt too long, truncated");
    }

    agentos_llm_request_t req;
    memset(&req, 0, sizeof(req));
    req.model = "cross-encoder"; // 实际应配�?
    req.prompt = prompt;
    req.max_tokens = 10;
    req.temperature = 0.0;
    req.stop = NULL;

    agentos_llm_response_t* resp = NULL;
    if (agentos_llm_complete(llm, &req, &resp) != AGENTOS_SUCCESS) {
        return -1.0f;  // 失败标记
    }
    char* endptr;
    errno = 0;
    float score = strtof(resp->text, &endptr);
    if (endptr == resp->text || *endptr != '\0' || errno != 0) {
        score = 0.5f;  // 转换失败，使用默认�?
    }
    agentos_llm_response_free(resp);
    if (score < 0 || score > 1) score = 0.5f;
    return score;
}

agentos_error_t agentos_reranker_rerank(
    agentos_reranker_t* reranker,
    const char* query,
    const char** candidate_ids,
    const float* initial_scores,
    size_t candidate_count,
    char*** out_reranked_ids,
    float** out_reranked_scores) {

    if (!reranker || !query || !candidate_ids || candidate_count == 0 ||
        !out_reranked_ids || !out_reranked_scores) {
        AGENTOS_LOG_ERROR("Invalid parameters to rerank");
        return AGENTOS_EINVAL;
    }

    // 如果不使用LLM，直接返回初始顺序（降级�?
    if (!reranker->use_llm) {
        char** ids = (char**)AGENTOS_MALLOC(candidate_count * sizeof(char*));
        float* scores = (float*)AGENTOS_MALLOC(candidate_count * sizeof(float));
        if (!ids || !scores) {
            AGENTOS_FREE(ids);
            AGENTOS_FREE(scores);
            return AGENTOS_ENOMEM;
        }
        for (size_t i = 0; i < candidate_count; i++) {
            ids[i] = AGENTOS_STRDUP(candidate_ids[i]);
            scores[i] = initial_scores ? initial_scores[i] : 0.5f;
        }
        *out_reranked_ids = ids;
        *out_reranked_scores = scores;
        return AGENTOS_SUCCESS;
    }

    // 为每个候选计算交叉编码器分数
    float* scores = (float*)AGENTOS_MALLOC(candidate_count * sizeof(float));
    if (!scores) {
        AGENTOS_LOG_ERROR("Failed to allocate scores array");
        return AGENTOS_ENOMEM;
    }

    int llm_success = 0;
    for (size_t i = 0; i < candidate_count; i++) {
        // �?layer1 读取原始文档内容
        void* data = NULL;
        size_t len = 0;
        agentos_error_t err = agentos_layer1_raw_read(reranker->layer1, candidate_ids[i], &data, &len);
        if (err != AGENTOS_SUCCESS) {
            AGENTOS_LOG_WARN("Failed to read document %s, using initial score", candidate_ids[i]);
            scores[i] = (initial_scores) ? initial_scores[i] : 0.5f;
            continue;
        }
        char* doc = (char*)data;
        if (doc[len - 1] != '\0') {
            char* new_doc = (char*)AGENTOS_MALLOC(len + 1);
            if (!new_doc) {
                AGENTOS_FREE(data);
                scores[i] = 0.5f;
                continue;
            }
            memcpy(new_doc, data, len);
            new_doc[len] = '\0';
            AGENTOS_FREE(data);
            doc = new_doc;
        }
        float score = cross_encoder_score(reranker->llm, query, doc);
        if (score >= 0) {
            scores[i] = score;
            llm_success++;
        } else {
            scores[i] = (initial_scores) ? initial_scores[i] : 0.5f;
        }
        AGENTOS_FREE(doc);
    }

    // 如果所有LLM调用都失败，下次降级
    if (llm_success == 0) {
        AGENTOS_LOG_WARN("All LLM calls failed, disabling reranker for future use");
        reranker->use_llm = 0;
        // 返回原始顺序
        for (size_t i = 0; i < candidate_count; i++) {
            scores[i] = initial_scores ? initial_scores[i] : 0.5f;
        }
    }

    // 按分数降序排序（简单选择排序�?
    size_t* indices = (size_t*)AGENTOS_MALLOC(candidate_count * sizeof(size_t));
    if (!indices) {
        AGENTOS_FREE(scores);
        return AGENTOS_ENOMEM;
    }
    for (size_t i = 0; i < candidate_count; i++) indices[i] = i;

    for (size_t i = 0; i < candidate_count - 1; i++) {
        for (size_t j = i + 1; j < candidate_count; j++) {
            if (scores[indices[j]] > scores[indices[i]]) {
                size_t tmp = indices[i];
                indices[i] = indices[j];
                indices[j] = tmp;
            }
        }
    }

    char** reranked_ids = (char**)AGENTOS_MALLOC(candidate_count * sizeof(char*));
    float* reranked_scores = (float*)AGENTOS_MALLOC(candidate_count * sizeof(float));
    if (!reranked_ids || !reranked_scores) {
        AGENTOS_FREE(indices);
        AGENTOS_FREE(scores);
        AGENTOS_FREE(reranked_ids);
        AGENTOS_FREE(reranked_scores);
        return AGENTOS_ENOMEM;
    }

    for (size_t i = 0; i < candidate_count; i++) {
        size_t idx = indices[i];
        reranked_ids[i] = AGENTOS_STRDUP(candidate_ids[idx]);
        reranked_scores[i] = scores[idx];
    }

    AGENTOS_FREE(indices);
    AGENTOS_FREE(scores);

    *out_reranked_ids = reranked_ids;
    *out_reranked_scores = reranked_scores;
    return AGENTOS_SUCCESS;
}
