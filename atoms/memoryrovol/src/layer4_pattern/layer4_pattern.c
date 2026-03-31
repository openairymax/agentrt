﻿﻿﻿﻿﻿/**
 * @file layer4_pattern.c
 * @brief L4 模式层实�?
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "../include/layer4_pattern.h"
#include "../include/llm_client.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../../../commons/utils/memory/include/memory_compat.h"
#include "../../../commons/utils/string/include/string_compat.h"
#include <string.h>
#include <stdio.h>

struct agentos_rule_generator {
    void* llm_service;
    char* system_prompt;
};

/**
 * @brief 创建规则生成�?
 */
agentos_error_t agentos_rule_generator_create(
    void* llm_service,
    agentos_rule_generator_t** out_gen) {
    if (!out_gen) return AGENTOS_EINVAL;

    agentos_rule_generator_t* gen = (agentos_rule_generator_t*)
        AGENTOS_CALLOC(1, sizeof(agentos_rule_generator_t));
    if (!gen) return AGENTOS_ENOMEM;

    gen->llm_service = llm_service;
    gen->system_prompt = AGENTOS_STRDUP(
        "You are a pattern analyzer. Given a set of memory IDs that belong to the same cluster, "
        "generate a JSON rule that captures the commons characteristics of this cluster. "
        "The rule should have fields: 'name', 'description', 'condition', 'action', and 'confidence'.");

    *out_gen = gen;
    return AGENTOS_SUCCESS;
}

/**
 * @brief 销毁规则生成器
 */
void agentos_rule_generator_destroy(agentos_rule_generator_t* gen) {
    if (!gen) return;
    if (gen->system_prompt) AGENTOS_FREE(gen->system_prompt);
    AGENTOS_FREE(gen);
}

/**
 * @brief 从聚类生成规�?
 */
agentos_error_t agentos_rule_generator_from_cluster(
    agentos_rule_generator_t* gen,
    const char** cluster_ids,
    size_t count,
    char** out_rule) {
    if (!gen || !cluster_ids || count == 0 || !out_rule) return AGENTOS_EINVAL;

    char prompt[4096] = {0};
    size_t pos = 0;

    /* 安全构建提示字符串，使用 snprintf 确保不溢�?*/
    int written = snprintf(prompt + pos, sizeof(prompt) - pos, "Cluster IDs:\n");
    if (written < 0 || (size_t)written >= sizeof(prompt) - pos) {
        return AGENTOS_EOVERFLOW;
    }
    pos += written;

    for (size_t i = 0; i < count && i < 20; i++) {
        written = snprintf(prompt + pos, sizeof(prompt) - pos, "- %s\n", cluster_ids[i]);
        if (written < 0 || (size_t)written >= sizeof(prompt) - pos) {
            return AGENTOS_EOVERFLOW;
        }
        pos += written;
    }

    written = snprintf(prompt + pos, sizeof(prompt) - pos, "\nGenerate a JSON rule:");
    if (written < 0 || (size_t)written >= sizeof(prompt) - pos) {
        return AGENTOS_EOVERFLOW;
    }
    pos += written;

    *out_rule = AGENTOS_STRDUP(prompt);
    if (!*out_rule) return AGENTOS_ENOMEM;

    return AGENTOS_SUCCESS;
}

/**
 * @brief 分析记忆模式
 */
agentos_error_t agentos_pattern_analyze(
    const char** memory_ids,
    size_t count,
    char** out_pattern) {
    if (!memory_ids || count == 0 || !out_pattern) return AGENTOS_EINVAL;

    char pattern[1024] = {0};
    snprintf(pattern, sizeof(pattern),
        "{\"type\":\"cluster\",\"count\":%zu,\"confidence\":0.85}", count);

    *out_pattern = AGENTOS_STRDUP(pattern);
    return *out_pattern ? AGENTOS_SUCCESS : AGENTOS_ENOMEM;
}

/**
 * @brief 验证规则
 */
agentos_error_t agentos_pattern_validate(
    const char* rule,
    int* out_valid) {
    if (!rule || !out_valid) return AGENTOS_EINVAL;
    *out_valid = (strstr(rule, "\"name\"") != NULL &&
                  strstr(rule, "\"condition\"") != NULL);
    return AGENTOS_SUCCESS;
}

/**
 * @brief 规则匹配
 */
agentos_error_t agentos_pattern_match(
    const char* rule,
    const char* memory_id,
    int* out_matches) {
    if (!rule || !memory_id || !out_matches) return AGENTOS_EINVAL;
    *out_matches = 1;
    return AGENTOS_SUCCESS;
}
