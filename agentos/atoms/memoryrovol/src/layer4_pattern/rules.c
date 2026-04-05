/**
 * @file rules.c
 * @brief L4 Pattern layer rule generator (with LLM service integration)
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "../include/layer4_pattern.h"
#include "../include/llm_client.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../../../agentos/commons/utils/memory/include/memory_compat.h"
#include "../../../agentos/commons/utils/string/include/string_compat.h"
#include <string.h>
#include <stdio.h>

struct agentos_rule_generator {
    agentos_llm_service_t* llm;
    agentos_mutex_t* lock;
};

agentos_error_t agentos_rule_generator_create(
    void* llm_service,
    agentos_rule_generator_t** out_gen) {

    if (!out_gen) return AGENTOS_EINVAL;
    agentos_rule_generator_t* gen = (agentos_rule_generator_t*)AGENTOS_CALLOC(1, sizeof(agentos_rule_generator_t));
    if (!gen) return AGENTOS_ENOMEM;


    gen->llm = (agentos_llm_service_t*)llm_service;
    gen->lock = agentos_mutex_create();
    if (!gen->lock) {
        AGENTOS_FREE(gen);
        return AGENTOS_ENOMEM;
    }

    *out_gen = gen;
    return AGENTOS_SUCCESS;
}

void agentos_rule_generator_destroy(agentos_rule_generator_t* gen) {
    if (!gen) return;
    if (gen->lock) agentos_mutex_destroy(gen->lock);
    AGENTOS_FREE(gen);
}

agentos_error_t agentos_rule_generator_generate(
    agentos_rule_generator_t* gen,
    const float* cluster_vectors,
    const char** cluster_ids,
    size_t count,
    char** out_rule) {

    if (!gen || !cluster_vectors || !cluster_ids || count == 0 || !out_rule)
        return AGENTOS_EINVAL;

    char prompt[4096] = {0};
    size_t offset = snprintf(prompt, sizeof(prompt),
        "You are a pattern analyzer. Given the following set of memory IDs that belong to the same cluster:\n");
    for (size_t i = 0; i < count && i < 20 && offset < sizeof(prompt) - 1; i++) {
        size_t len = strlen(cluster_ids[i]);
        if (offset + len + 2 < sizeof(prompt)) {
            memcpy(prompt + offset, cluster_ids[i], len);
            offset += len;
            prompt[offset++] = '\n';
            prompt[offset] = '\0';
        }
    }
    if (offset < sizeof(prompt) - 100) {
        snprintf(prompt + offset, sizeof(prompt) - offset,
        "\nPlease generate a JSON rule that captures the commons characteristics of this cluster. "
        "The rule should have fields: 'name', 'description', 'condition', 'action', and 'confidence'. "
        "Output only valid JSON.");

    // 调用LLM服务
    if (!gen->llm) {
        // 无LLM服务，返回简单占位符（生产环境应确保有LLM�?
        *out_rule = AGENTOS_STRDUP("{\"name\":\"Fallback pattern\",\"description\":\"No LLM available\",\"condition\":\"true\",\"action\":\"none\",\"confidence\":0.5}");
        if (!*out_rule) return AGENTOS_ENOMEM;
        return AGENTOS_SUCCESS;
    }

    agentos_llm_request_t req;
    memset(&req, 0, sizeof(req));
    req.model = "gpt-4";  // 或从配置读取
    req.prompt = prompt;
    req.max_tokens = 512;
    req.temperature = 0.7;

    agentos_llm_response_t* resp = NULL;
    agentos_error_t err = agentos_llm_complete(gen->llm, &req, &resp);
    if (err != AGENTOS_SUCCESS) return err;

    *out_rule = AGENTOS_STRDUP(resp->text);
    agentos_llm_response_free(resp);
    return *out_rule ? AGENTOS_SUCCESS : AGENTOS_ENOMEM;
}
