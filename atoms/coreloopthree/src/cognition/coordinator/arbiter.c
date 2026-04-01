/**
 * @file arbiter.c
 * @brief 外部仲裁策略（调用仲裁器模型或人工接口）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "strategy.h"
#include "agentos.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../../../commons/utils/memory/include/memory_compat.h"
#include "../../../commons/utils/string/include/string_compat.h"
#include <string.h>
#include <stdio.h>

/**
 * @brief 外部仲裁私有数据
 */
typedef struct arbiter_data {
    char* arbiter_model;           /**< 仲裁模型名称（可为 NULL，表示人工） */
    agentos_mutex_t* lock;
    void (*human_callback)(const char* question, char* answer, size_t max_len); /**< 人工回调 */
} arbiter_data_t;

static void arbiter_destroy(agentos_coordinator_base_t* base) {
    if (!base) return;
    arbiter_data_t* data = (arbiter_data_t*)base->data;
    if (data) {
        if (data->arbiter_model) AGENTOS_FREE(data->arbiter_model);
        if (data->lock) agentos_mutex_destroy(data->lock);
        AGENTOS_FREE(data);
    }
    AGENTOS_FREE(base);
}

/**
 * @brief 外部仲裁函数
 */
static agentos_error_t arbiter_coordinate(
    agentos_coordinator_base_t* base,
    const agentos_coordination_context_t* context,
    const char** inputs,
    size_t input_count,
    char** out_result) {
    if (!base || !out_result) return AGENTOS_EINVAL;

    arbiter_data_t* data = (arbiter_data_t*)base->data;
    if (!data) return AGENTOS_EINVAL;

    agentos_mutex_lock(data->lock);

    // 如果配置了人工回调，使用人工仲裁
    if (data->human_callback && input_count > 0) {
        char question[1024];
        snprintf(question, sizeof(question), "多个模型输出不一致，请选择最佳结果：\n");
        
        for (size_t i = 0; i < input_count && i < 5; i++) {
            char option[256];
            snprintf(option, sizeof(option), "%zu. %s\n", i + 1, inputs[i]);
            strncat(question, option, sizeof(question) - strlen(question) - 1);
        }

        char answer[512];
        data->human_callback(question, answer, sizeof(answer));

        // 解析用户选择（简单实现）
        int choice = atoi(answer);
        if (choice >= 1 && choice <= (int)input_count) {
            *out_result = AGENTOS_STRDUP(inputs[choice - 1]);
        } else {
            *out_result = AGENTOS_STRDUP("invalid_choice");
        }

        agentos_mutex_unlock(data->lock);
        return AGENTOS_SUCCESS;
    }

    // 否则使用第一个模型的结果作为默认仲裁
    if (input_count > 0) {
        *out_result = AGENTOS_STRDUP(inputs[0]);
    } else {
        *out_result = AGENTOS_STRDUP("no_input");
    }

    agentos_mutex_unlock(data->lock);
    return AGENTOS_SUCCESS;
}

/**
 * @brief 创建外部仲裁协调器
 */
agentos_error_t agentos_coordinator_arbiter_create(
    const char* arbiter_model,
    void (*human_callback)(const char* question, char* answer, size_t max_len),
    agentos_coordinator_base_t** out_base) {
    if (!out_base) return AGENTOS_EINVAL;

    agentos_coordinator_base_t* base = (agentos_coordinator_base_t*)AGENTOS_CALLOC(1, sizeof(agentos_coordinator_base_t));
    if (!base) return AGENTOS_ENOMEM;

    arbiter_data_t* data = (arbiter_data_t*)AGENTOS_CALLOC(1, sizeof(arbiter_data_t));
    if (!data) {
        AGENTOS_FREE(base);
        return AGENTOS_ENOMEM;
    }

    data->lock = agentos_mutex_create();
    if (!data->lock) {
        AGENTOS_FREE(data);
        AGENTOS_FREE(base);
        return AGENTOS_ENOMEM;
    }

    if (arbiter_model) {
        data->arbiter_model = AGENTOS_STRDUP(arbiter_model);
        if (!data->arbiter_model) {
            agentos_mutex_destroy(data->lock);
            AGENTOS_FREE(data);
            AGENTOS_FREE(base);
            return AGENTOS_ENOMEM;
        }
    }

    data->human_callback = human_callback;

    base->data = data;
    base->coordinate = arbiter_coordinate;
    base->destroy = arbiter_destroy;

    *out_base = base;
    return AGENTOS_SUCCESS;
}
