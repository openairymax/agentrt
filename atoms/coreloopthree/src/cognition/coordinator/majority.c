/**
 * @file majority.c
 * @brief 多数投票协同策略（取多数模型的一致输出）
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#include "cognition.h"
#include "llm_client.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/**
 * @brief 多数投票私有数据
 */
typedef struct majority_data {
    char** model_names;           /**< 模型名称数组 */
    size_t model_count;            /**< 模型数量 */
    agentos_llm_service_t* llm;    /**< LLM服务客户端 */
    agentos_mutex_t* lock;
} majority_data_t;

static void majority_destroy(agentos_coordinator_strategy_t* strategy) {
    if (!strategy) return;
    majority_data_t* data = (majority_data_t*)strategy->data;
    if (data) {
        for (size_t i = 0; i < data->model_count; i++) {
            if (data->model_names[i]) free(data->model_names[i]);
        }
        free(data->model_names);
        if (data->lock) agentos_mutex_destroy(data->lock);
        free(data);
    }
    free(strategy);
}

/**
 * @brief 计算字符串的哈希值（用于比较内容）
 */
static uint64_t str_hash(const char* s) {
    uint64_t hash = 5381;
    int c;
    while ((c = *s++))
        hash = ((hash << 5) + hash) + c;
    return hash;
}

/**
 * @brief 多数投票协调函数
 */
static agentos_error_t majority_coordinate(
    const char** prompts,
    size_t count,
    void* context,
    char** out_result) {

    majority_data_t* data = (majority_data_t*)context;
    if (!data || !data->llm) return AGENTOS_EINVAL;

    // 为每个模型分配结果缓冲区
    size_t n = data->model_count;
    char** results = (char**)calloc(n, sizeof(char*));
    if (!results) return AGENTOS_ENOMEM;

    agentos_error_t err = AGENTOS_SUCCESS;

    // 调用所有模型（并行可用线程池，此处顺序简化）
    for (size_t i = 0; i < n && i < count; i++) {
        agentos_llm_request_t req;
        memset(&req, 0, sizeof(req));
        req.model = data->model_names[i];
        req.prompt = prompts[i];
        req.temperature = 0.7;
        req.max_tokens = 2048;

        agentos_llm_response_t* resp = NULL;
        err = agentos_llm_complete(data->llm, &req, &resp);
        if (err == AGENTOS_SUCCESS && resp) {
            results[i] = strdup(resp->text);
            agentos_llm_response_free(resp);
        } else {
            results[i] = NULL;
        }
    }

    // 统计多数结果
    uint64_t* hashes = (uint64_t*)malloc(n * sizeof(uint64_t));
    if (!hashes) {
        for (size_t i = 0; i < n; i++) if (results[i]) free(results[i]);
        free(results);
        return AGENTOS_ENOMEM;
    }

    for (size_t i = 0; i < n; i++) {
        hashes[i] = results[i] ? str_hash(results[i]) : 0;
    }

    size_t best_count = 0;
    int best_idx = -1;
    for (size_t i = 0; i < n; i++) {
        if (!results[i]) continue;
        size_t cnt = 1;
        for (size_t j = i + 1; j < n; j++) {
            if (results[j] && hashes[i] == hashes[j]) cnt++;
        }
        if (cnt > best_count) {
            best_count = cnt;
            best_idx = i;
        }
    }

    if (best_idx >= 0) {
        *out_result = strdup(results[best_idx]);
    } else {
        *out_result = NULL;
        err = AGENTOS_ENOENT;  // 无有效结果
    }

    // 清理
    for (size_t i = 0; i < n; i++) if (results[i]) free(results[i]);
    free(results);
    free(hashes);

    return err;
}

/**
 * @brief 创建多数投票协同策略
 * @param model_names 模型名称数组
 * @param model_count 模型数量
 * @param llm LLM服务客户端
 * @return 策略对象
 */
agentos_coordinator_strategy_t* agentos_majority_coordinator_create(
    const char** model_names,
    size_t model_count,
    agentos_llm_service_t* llm) {

    if (!model_names || model_count == 0 || !llm) return NULL;

    agentos_coordinator_strategy_t* strat = (agentos_coordinator_strategy_t*)malloc(sizeof(agentos_coordinator_strategy_t));
    if (!strat) return NULL;

    majority_data_t* data = (majority_data_t*)malloc(sizeof(majority_data_t));
    if (!data) {
        free(strat);
        return NULL;
    }
    memset(data, 0, sizeof(majority_data_t));

    data->model_names = (char**)calloc(model_count, sizeof(char*));
    if (!data->model_names) {
        free(data);
        free(strat);
        return NULL;
    }

    for (size_t i = 0; i < model_count; i++) {
        data->model_names[i] = strdup(model_names[i]);
        if (!data->model_names[i]) {
            for (size_t j = 0; j < i; j++) free(data->model_names[j]);
            free(data->model_names);
            free(data);
            free(strat);
            return NULL;
        }
    }
    data->model_count = model_count;
    data->llm = llm;
    data->lock = agentos_mutex_create();
    if (!data->lock) {
        for (size_t i = 0; i < model_count; i++) free(data->model_names[i]);
        free(data->model_names);
        free(data);
        free(strat);
        return NULL;
    }

    strat->coordinate = majority_coordinate;
    strat->destroy = majority_destroy;
    strat->data = data;

    return strat;
}