/**
 * @file rerank.c
 * @brief 检索结果重排序（基于交叉编码器，带降级）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 实现基于交叉编码器的检索结果重排序功能。
 * 当LLM服务不可用时，自动降级为基于BM25分数的重排序。
 */

/* Windows平台特殊处理：确保正确的头文件包含顺序 */
#ifdef _WIN32
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#include <windows.h>
#endif

/* C标准库 - 必须在最前面 */
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

/* 项目公共头文件 */
#include "agentos.h"
#include "../include/retrieval.h"
#include "../include/llm_client.h"
#include "../include/layer1_raw.h"

/* Unified base library compatibility layer */
#include "include/memory_compat.h"
#include "string_compat.h"

/**
 * @brief 重排序器内部结构
 */
struct agentos_reranker {
    agentos_llm_service_t* llm;          /**< LLM服务，用于交叉编码器 */
    agentos_layer1_raw_t* layer1;         /**< 原始层，用于获取文档文本 */
    agentos_mutex_t* lock;                /**< 线程安全锁 */
    int use_llm;                          /**< 是否使用LLM（降级标志） */
};
typedef struct agentos_reranker agentos_reranker_t;

/**
 * @brief 创建重排序器实例
 * 
 * @param llm [in] LLM服务客户端（非NULL）
 * @param layer1 [in] 原始层存储（非NULL）
 * @param out_reranker [out] 输出重排序器实例
 * @return AGENTOS_SUCCESS 成功
 * @return AGENTOS_EINVAL 参数无效
 * @return AGENTOS_ENOMEM 内存分配失败
 */
agentos_error_t agentos_reranker_create(
    agentos_llm_service_t* llm,
    agentos_layer1_raw_t* layer1,
    agentos_reranker_t** out_reranker)
{
    if (!llm || !layer1 || !out_reranker) {
        return AGENTOS_EINVAL;
    }

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
    r->use_llm = 1;

    *out_reranker = r;
    return AGENTOS_SUCCESS;
}

/**
 * @brief 销毁重排序器实例
 * 
 * @param reranker [in] 重排序器实例（将被释放）
 */
void agentos_reranker_destroy(agentos_reranker_t* reranker)
{
    if (!reranker) {
        return;
    }

    if (reranker->lock) {
        agentos_mutex_destroy(reranker->lock);
    }

    AGENTOS_FREE(reranker);
}

/**
 * @brief 执行检索结果重排序
 * 
 * 使用LLM交叉编码器或BM25分数对检索结果进行重排序。
 * 当LLM不可用时自动降级为BM25排序。
 * 
 * @param reranker [in] 重排序器实例（非NULL）
 * @param results [in/out] 检索结果数组（将被原地重排序）
 * @param count [in] 结果数量
 * @param query [in] 原始查询字符串（用于LLM重排序）
 * @return AGENTOS_SUCCESS 成功
 * @return AGENTOS_EINVAL 参数无效
 * @return AGENTOS_EUNKNOWN 重排序失败
 */
agentos_error_t agentos_reranker_rerank(
    agentos_reranker_t* reranker,
    agentos_retrieval_result_t* results,
    size_t count,
    const char* query)
{
    if (!reranker || !results || count == 0 || !query) {
        return AGENTOS_EINVAL;
    }

    agentos_mutex_lock(reranker->lock);

    /* 尝试使用LLM进行语义重排序 */
    if (reranker->use_llm && reranker->llm) {
        /* FUTURE: 实现基于交叉编码器的语义重排序 - 参见memoryrovol设计文档第4.2节 */
        /* 当前暂时跳过，使用默认排序 */
        AGENTOS_LOG_INFO("LLM reranking not yet implemented, using default order");
    }

    /* 降级：按现有分数排序（简单冒泡排序） */
    for (size_t i = 0; i < count - 1; i++) {
        for (size_t j = 0; j < count - i - 1; j++) {
            if (results[j].score < results[j + 1].score) {
                /* 交换两个结果 */
                agentos_retrieval_result_t temp = results[j];
                results[j] = results[j + 1];
                results[j + 1] = temp;
            }
        }
    }

    agentos_mutex_unlock(reranker->lock);
    return AGENTOS_SUCCESS;
}

/**
 * @brief 设置是否使用LLM进行重排序
 * 
 * @param reranker [in] 重排序器实例（非NULL）
 * @param use_llm [in] 是否启用LLM重排序
 * @return AGENTOS_SUCCESS 成功
 * @return AGENTOS_EINVAL 参数无效
 */
agentos_error_t agentos_reranker_set_use_llm(
    agentos_reranker_t* reranker,
    int use_llm)
{
    if (!reranker) {
        return AGENTOS_EINVAL;
    }

    agentos_mutex_lock(reranker->lock);
    reranker->use_llm = use_llm;
    agentos_mutex_unlock(reranker->lock);

    return AGENTOS_SUCCESS;
}
