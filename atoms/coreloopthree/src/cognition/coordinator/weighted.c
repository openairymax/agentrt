/**
 * @file weighted.c
 * @brief 加权融合策略（根据权重组合多个模型输出）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "cognition.h
#include "../../../commons/utils/cognition/include/cognition_common.h""
#include "llm_client.h
#include "../../../commons/utils/cognition/include/cognition_common.h""
#include <stdlib.h
#include "../../../commons/utils/cognition/include/cognition_common.h">

/* Unified base library compatibility layer */
#include "../../../commons/utils/memory/include/memory_compat.h
#include "../../../commons/utils/cognition/include/cognition_common.h""
#include "../../../commons/utils/string/include/string_compat.h
#include "../../../commons/utils/cognition/include/cognition_common.h""
#include <string.h
#include "../../../commons/utils/cognition/include/cognition_common.h">
#include <stdio.h
#include "../../../commons/utils/cognition/include/cognition_common.h">

/**
 * @brief 加权融合私有数据
 */
typedef struct weighted_data {
    char** model_names;           /**< 模型名称数组 */
    float* weights;                /**< 对应权重 */
    size_t model_count;            /**< 模型数量 */
    agentos_llm_service_t* llm;    /**< LLM服务 */
    agentos_mutex_t* lock;
} weighted_data_t;

static void weighted_destroy(agentos_coordinator_strategy_t* strategy) {
    if (!strategy) return;
    weighted_data_t* data = (weighted_data_t*)strategy->data;
    if (data) {
        for (size_t i = 0; i < data->model_count; i++) {
            if (data->model_names[i]) AGENTOS_FREE(data->model_names[i]);
        }
        AGENTOS_FREE(data->model_names);
        AGENTOS_FREE(data->weights);
        if (data->lock) agentos_mutex_destroy(data->lock);
        AGENTOS_FREE(data);
    }
    AGENTOS_FREE(strategy);
}

/**
 * @brief 加权融合函数（简单拼接，实际可更复杂�?
 */
static agentos_error_t weighted_coordinate(
    const char** prompts,
    size_t count,
    void* context,
    char** out_result) {

    weighted_data_t* data = (weighted_data_t*)context;
    if (!data) return AGENTOS_EINVAL;

    size_t n = data->model_count;
    char** results = (char**)AGENTOS_CALLOC(n, sizeof(char*));
    if (!results) return AGENTOS_ENOMEM;

    agentos_error_t err = AGENTOS_SUCCESS;

    // 调用所有模�?
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
            results[i] = AGENTOS_STRDUP(resp->text);
            agentos_llm_response_free(resp);
        } else {
            results[i] = NULL;
        }
    }

    // 根据权重加权组合（简单拼接带权重标记�?
    size_t total_len = 0;
    for (size_t i = 0; i < n; i++) {
        if (results[i]) total_len += strlen(results[i]) + 32; // 预留标记
    }

    char* combined = (char*)AGENTOS_MALLOC(total_len + 1);
    if (!combined) {
        for (size_t i = 0; i < n; i++) if (results[i]) AGENTOS_FREE(results[i]);
        AGENTOS_FREE(results);
        return AGENTOS_ENOMEM;
    }
    combined[0] = '\0';

    size_t offset = 0;
    for (size_t i = 0; i < n; i++) {
        if (results[i]) {
            char buf[64];
            snprintf(buf, sizeof(buf), "[Model %s weight %.2f]: ", data->model_names[i], data->weights[i]);
            size_t buf_len = strlen(buf);
            size_t result_len = strlen(results[i]);
            size_t needed = buf_len + result_len + 2;
            if (offset + needed <= total_len) {
                memcpy(combined + offset, buf, buf_len);
                offset += buf_len;
                memcpy(combined + offset, results[i], result_len);
                offset += result_len;
                combined[offset++] = '\n';
            }
        }
    }
    combined[offset] = '\0';

    *out_result = combined;

    // 清理
    for (size_t i = 0; i < n; i++) if (results[i]) AGENTOS_FREE(results[i]);
    AGENTOS_FREE(results);

    return AGENTOS_SUCCESS;
}

/**
 * @brief 创建加权融合策略
 * @param model_names 模型名称数组
 * @param weights 权重数组（总和不必�?�?
 * @param model_count 数量
 * @param llm LLM服务客户�?
 * @return 策略对象
 */
agentos_coordinator_strategy_t* agentos_weighted_coordinator_create(
    const char** model_names,
    const float* weights,
    size_t model_count,
    agentos_llm_service_t* llm) {

    if (!model_names || !weights || model_count == 0 || !llm) return NULL;

    agentos_coordinator_strategy_t* strat = (agentos_coordinator_strategy_t*)AGENTOS_MALLOC(sizeof(agentos_coordinator_strategy_t));
    if (!strat) return NULL;

    weighted_data_t* data = (weighted_data_t*)AGENTOS_CALLOC(1, sizeof(weighted_data_t));
    if (!data) {
        AGENTOS_FREE(strat);
        return NULL;
    }

    data->model_names = (char**)AGENTOS_CALLOC(model_count, sizeof(char*));
    data->weights = (float*)AGENTOS_CALLOC(model_count, sizeof(float));
    if (!data->model_names || !data->weights) {
        if (data->model_names) AGENTOS_FREE(data->model_names);
        if (data->weights) AGENTOS_FREE(data->weights);
        AGENTOS_FREE(data);
        AGENTOS_FREE(strat);
        return NULL;
    }

    for (size_t i = 0; i < model_count; i++) {
        data->model_names[i] = AGENTOS_STRDUP(model_names[i]);
        data->weights[i] = weights[i];
        if (!data->model_names[i]) {
            for (size_t j = 0; j < i; j++) AGENTOS_FREE(data->model_names[j]);
            AGENTOS_FREE(data->model_names);
            AGENTOS_FREE(data->weights);
            AGENTOS_FREE(data);
            AGENTOS_FREE(strat);
            return NULL;
        }
    }
    data->model_count = model_count;
    data->llm = llm;
    data->lock = agentos_mutex_create();
    if (!data->lock) {
        for (size_t i = 0; i < model_count; i++) AGENTOS_FREE(data->model_names[i]);
        AGENTOS_FREE(data->model_names);
        AGENTOS_FREE(data->weights);
        AGENTOS_FREE(data);
        AGENTOS_FREE(strat);
        return NULL;
    }

    strat->coordinate = weighted_coordinate;
    strat->destroy = weighted_destroy;
    strat->data = data;

    return strat;
}
