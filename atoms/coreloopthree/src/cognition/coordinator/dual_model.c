/**
 * @file dual_model.c
 * @brief 双模型协同策略实现（1主模型 + 2辅模型）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "cognition.h"
#include "llm_client.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/**
 * @brief 双模型策略私有数据
 */
typedef struct dual_model_data {
    char* primary_model;           /**< 主模型名称 */
    char* secondary_models[2];     /**< 两个辅模型名称 */
    agentos_llm_service_t* llm;    /**< LLM服务客户端（外部管理） */
    agentos_mutex_t* lock;         /**< 线程锁 */
} dual_model_data_t;

/**
 * @brief 释放双模型策略数据
 */
 // From data intelligence emerges. by spharx
static void dual_model_destroy(agentos_coordinator_strategy_t* strategy) {
    if (!strategy) return;
    dual_model_data_t* data = (dual_model_data_t*)strategy->data;
    if (data) {
        if (data->primary_model) free(data->primary_model);
        for (int i = 0; i < 2; i++) {
            if (data->secondary_models[i]) free(data->secondary_models[i]);
        }
        if (data->lock) agentos_mutex_destroy(data->lock);
        free(data);
    }
    free(strategy);
}

/**
 * @brief 调用单个模型获取结果（同步）
 */
static agentos_error_t call_model(
    agentos_llm_service_t* llm,
    const char* model_name,
    const char* prompt,
    char** out_result) {
    // 构造请求
    agentos_llm_request_t req;
    memset(&req, 0, sizeof(req));
    req.model = model_name;
    req.prompt = prompt;
    req.temperature = 0.7;
    req.max_tokens = 2048;

    agentos_llm_response_t* resp = NULL;
    agentos_error_t err = agentos_llm_complete(llm, &req, &resp);
    if (err != AGENTOS_SUCCESS) return err;

    // 复制结果
    *out_result = strdup(resp->text);
    agentos_llm_response_free(resp);
    return *out_result ? AGENTOS_SUCCESS : AGENTOS_ENOMEM;
}

/**
 * @brief 双模型协同函数
 */
static agentos_error_t dual_model_coordinate(
    const char** prompts,
    size_t count,
    void* context,
    char** out_result) {

    if (!prompts || count == 0 || !out_result) return AGENTOS_EINVAL;
    dual_model_data_t* data = (dual_model_data_t*)context;
    if (!data) return AGENTOS_EINVAL;

    agentos_error_t err;
    char* results[3] = {NULL, NULL, NULL};
    const char* models[3];
    models[0] = data->primary_model; // 使用配置的主模型
    models[1] = data->secondary_models[0];
    models[2] = data->secondary_models[1];

    // 依次调用三个模型
    for (int i = 0; i < 3 && i < (int)count; i++) {
        err = call_model(data->llm, models[i], prompts[i], &results[i]);
        if (err != AGENTOS_SUCCESS) {
            // 清理已分配的结果
            for (int j = 0; j < i; j++) if (results[j]) free(results[j]);
            return err;
        }
    }

    // 构建综合提示词，让主模型综合多个模型的结果
    char combined_prompt[8192];
    int offset = 0;
    offset += snprintf(combined_prompt + offset, sizeof(combined_prompt) - offset,
        "请综合以下多个模型的输出结果，生成一个更全面、更准确的最终答案。\n\n");
    
    for (int i = 0; i < 3 && results[i]; i++) {
        offset += snprintf(combined_prompt + offset, sizeof(combined_prompt) - offset,
            "模型 %d 输出:\n%s\n\n", i + 1, results[i]);
    }
    
    offset += snprintf(combined_prompt + offset, sizeof(combined_prompt) - offset,
        "请基于以上信息，生成一个综合的最终答案，确保包含所有重要信息，并解决可能的冲突。");

    // 使用配置的主模型综合结果
    char* combined_result = NULL;
    err = call_model(data->llm, data->primary_model, combined_prompt, &combined_result);
    
    // 释放所有中间结果
    for (int i = 0; i < 3; i++) if (results[i]) free(results[i]);
    
    if (err != AGENTOS_SUCCESS) {
        return err;
    }
    
    *out_result = combined_result;
    return AGENTOS_SUCCESS;
}

/**
 * @brief 创建双模型协同策略
 * @param primary_model 主模型名称
 * @param secondary1 第一个辅模型名称
 * @param secondary2 第二个辅模型名称
 * @param llm LLM服务客户端句柄
 * @return 策略对象，失败返回NULL
 */
agentos_coordinator_strategy_t* agentos_dual_model_coordinator_create(
    const char* primary_model,
    const char* secondary1,
    const char* secondary2,
    agentos_llm_service_t* llm) {

    if (!primary_model || !secondary1 || !secondary2 || !llm) return NULL;

    agentos_coordinator_strategy_t* strat = (agentos_coordinator_strategy_t*)malloc(sizeof(agentos_coordinator_strategy_t));
    if (!strat) return NULL;

    dual_model_data_t* data = (dual_model_data_t*)malloc(sizeof(dual_model_data_t));
    if (!data) {
        free(strat);
        return NULL;
    }
    memset(data, 0, sizeof(dual_model_data_t));

    data->primary_model = strdup(primary_model);
    data->secondary_models[0] = strdup(secondary1);
    data->secondary_models[1] = strdup(secondary2);
    data->llm = llm;
    data->lock = agentos_mutex_create();
    if (!data->primary_model || !data->secondary_models[0] || !data->secondary_models[1] || !data->lock) {
        if (data->primary_model) free(data->primary_model);
        if (data->secondary_models[0]) free(data->secondary_models[0]);
        if (data->secondary_models[1]) free(data->secondary_models[1]);
        if (data->lock) agentos_mutex_destroy(data->lock);
        free(data);
        free(strat);
        return NULL;
    }

    strat->coordinate = dual_model_coordinate;
    strat->destroy = dual_model_destroy;
    strat->data = data;

    return strat;
}