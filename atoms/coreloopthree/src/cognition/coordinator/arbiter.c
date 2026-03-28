/**
 * @file arbiter.c
 * @brief 外部仲裁策略（调用仲裁器模型或人工接口）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "cognition.h
#include "../../../bases/utils/cognition/include/cognition_common.h""
#include "llm_client.h
#include "../../../bases/utils/cognition/include/cognition_common.h""
#include <stdlib.h
#include "../../../bases/utils/cognition/include/cognition_common.h">

/* Unified base library compatibility layer */
#include "../../../bases/utils/memory/include/memory_compat.h
#include "../../../bases/utils/cognition/include/cognition_common.h""
#include "../../../bases/utils/string/include/string_compat.h
#include "../../../bases/utils/cognition/include/cognition_common.h""
#include <string.h
#include "../../../bases/utils/cognition/include/cognition_common.h">
#include <stdio.h
#include "../../../bases/utils/cognition/include/cognition_common.h">

/**
 * @brief 外部仲裁私有数据
 */
typedef struct arbiter_data {
    char* arbiter_model;           /**< 仲裁模型名称（可为NULL，表示人工） */
    agentos_llm_service_t* llm;    /**< LLM服务（若为NULL则触发人工） */
    agentos_mutex_t* lock;
    void (*human_callback)(const char* question, char* answer, size_t max_len); /**< 人工回调 */
} arbiter_data_t;

static void arbiter_destroy(agentos_coordinator_strategy_t* strategy) {
    if (!strategy) return;
    arbiter_data_t* data = (arbiter_data_t*)strategy->data;
    if (data) {
        if (data->arbiter_model) AGENTOS_FREE(data->arbiter_model);
        if (data->lock) agentos_mutex_destroy(data->lock);
        AGENTOS_FREE(data);
    }
    AGENTOS_FREE(strategy);
}

/**
 * @brief 外部仲裁函数
 */
static agentos_error_t arbiter_coordinate(
    const char** prompts,
    size_t count,
    void* context,
    char** out_result) {

    arbiter_data_t* data = (arbiter_data_t*)context;
    if (!data) return AGENTOS_EINVAL;

    if (data->arbiter_model && data->llm) {
        // 使用仲裁模型：将所有prompts拼接，让模型选择
        // 构造一个综合prompt
        size_t total_len = 0;
        for (size_t i = 0; i < count; i++) {
            total_len += strlen(prompts[i]) + 32;
        }
        char* combined_prompt = (char*)AGENTOS_MALLOC(total_len + 1);
        if (!combined_prompt) return AGENTOS_ENOMEM;
        combined_prompt[0] = '\0';
        size_t offset = 0;

        for (size_t i = 0; i < count; i++) {
            char buf[64];
            int len = snprintf(buf, sizeof(buf), "Candidate %zu:\n", i+1);
            if (offset + len < total_len) {
                memcpy(combined_prompt + offset, buf, len);
                offset += len;
            }
            size_t prompt_len = strlen(prompts[i]);
            if (offset + prompt_len + 3 < total_len) {
                memcpy(combined_prompt + offset, prompts[i], prompt_len);
                offset += prompt_len;
                combined_prompt[offset++] = '\n';
                combined_prompt[offset++] = '\n';
            }
        }
        const char* suffix = "Based on the above candidates, produce the final answer.";
        size_t suffix_len = strlen(suffix);
        if (offset + suffix_len + 1 < total_len) {
            memcpy(combined_prompt + offset, suffix, suffix_len);
            offset += suffix_len;
        }
        combined_prompt[offset] = '\0';

        agentos_llm_request_t req;
        memset(&req, 0, sizeof(req));
        req.model = data->arbiter_model;
        req.prompt = combined_prompt;
        req.temperature = 0.5;
        req.max_tokens = 4096;

        agentos_llm_response_t* resp = NULL;
        agentos_error_t err = agentos_llm_complete(data->llm, &req, &resp);
        AGENTOS_FREE(combined_prompt);
        if (err == AGENTOS_SUCCESS && resp) {
            *out_result = AGENTOS_STRDUP(resp->text);
            agentos_llm_response_free(resp);
            return *out_result ? AGENTOS_SUCCESS : AGENTOS_ENOMEM;
        }
        return err;
    } else if (data->human_callback) {
        // 人工仲裁：将问题输出给回调，由外部决�?
        size_t total_len = 0;
        for (size_t i = 0; i < count; i++) {
            total_len += strlen(prompts[i]) + 64;
        }
        char* question = (char*)AGENTOS_MALLOC(total_len + 1);
        if (!question) return AGENTOS_ENOMEM;
        question[0] = '\0';
        size_t q_offset = 0;
        for (size_t i = 0; i < count; i++) {
            char buf[4096];
            int len = snprintf(buf, sizeof(buf), "Model %zu output:\n%s\n", i+1, prompts[i]);
            if (q_offset + len < total_len) {
                memcpy(question + q_offset, buf, len);
                q_offset += len;
            }
        }
        const char* final_prompt = "Please provide the final answer:";
        size_t fp_len = strlen(final_prompt);
        if (q_offset + fp_len + 1 < total_len) {
            memcpy(question + q_offset, final_prompt, fp_len);
            q_offset += fp_len;
        }
        question[q_offset] = '\0';

        char answer[4096];
        data->human_callback(question, answer, sizeof(answer));
        AGENTOS_FREE(question);
        *out_result = AGENTOS_STRDUP(answer);
        return *out_result ? AGENTOS_SUCCESS : AGENTOS_ENOMEM;
    }

    return AGENTOS_ENOTSUP;
}

/**
 * @brief 创建外部仲裁策略（模型仲裁）
 * @param arbiter_model 仲裁模型名称
 * @param llm LLM服务客户�?
 * @return 策略对象
 */
agentos_coordinator_strategy_t* agentos_arbiter_model_create(
    const char* arbiter_model,
    agentos_llm_service_t* llm) {

    if (!arbiter_model || !llm) return NULL;

    agentos_coordinator_strategy_t* strat = (agentos_coordinator_strategy_t*)AGENTOS_MALLOC(sizeof(agentos_coordinator_strategy_t));
    if (!strat) return NULL;

    arbiter_data_t* data = (arbiter_data_t*)AGENTOS_MALLOC(sizeof(arbiter_data_t));
    if (!data) {
        AGENTOS_FREE(strat);
        return NULL;
    }
    memset(data, 0, sizeof(arbiter_data_t));

    data->arbiter_model = AGENTOS_STRDUP(arbiter_model);
    data->llm = llm;
    data->human_callback = NULL;
    data->lock = agentos_mutex_create();
    if (!data->arbiter_model || !data->lock) {
        if (data->arbiter_model) AGENTOS_FREE(data->arbiter_model);
        if (data->lock) agentos_mutex_destroy(data->lock);
        AGENTOS_FREE(data);
        AGENTOS_FREE(strat);
        return NULL;
    }

    strat->coordinate = arbiter_coordinate;
    strat->destroy = arbiter_destroy;
    strat->data = data;

    return strat;
}

/**
 * @brief 创建外部仲裁策略（人工仲裁）
 * @param callback 人工回调函数
 * @return 策略对象
 */
agentos_coordinator_strategy_t* agentos_arbiter_human_create(
    void (*callback)(const char* question, char* answer, size_t max_len)) {

    if (!callback) return NULL;

    agentos_coordinator_strategy_t* strat = (agentos_coordinator_strategy_t*)AGENTOS_MALLOC(sizeof(agentos_coordinator_strategy_t));
    if (!strat) return NULL;

    arbiter_data_t* data = (arbiter_data_t*)AGENTOS_CALLOC(1, sizeof(arbiter_data_t));
    if (!data) {
        AGENTOS_FREE(strat);
        return NULL;
    }

    data->human_callback = callback;
    data->arbiter_model = NULL;
    data->llm = NULL;
    data->lock = agentos_mutex_create();
    if (!data->lock) {
        AGENTOS_FREE(data);
        AGENTOS_FREE(strat);
        return NULL;
    }

    strat->coordinate = arbiter_coordinate;
    strat->destroy = arbiter_destroy;
    strat->data = data;

    return strat;
}