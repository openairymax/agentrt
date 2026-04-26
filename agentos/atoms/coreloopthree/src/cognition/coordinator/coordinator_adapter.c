/**
 * @file coordinator_adapter.c
 * @brief 协调器公共API适配层 - 将内部base_t桥接到公共strategy_t
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "strategy.h"
#include "agentos.h"
#include <stdlib.h>

#include "memory_compat.h"
#include "string_compat.h"
#include <string.h>

extern agentos_error_t agentos_coordinator_dual_model_create(
    const char* primary_model, const char* secondary_model,
    float primary_weight, float secondary_weight,
    agentos_coordinator_base_t** out_base);

extern agentos_error_t agentos_coordinator_majority_create(
    size_t min_voters, float threshold,
    agentos_coordinator_base_t** out_base);

extern agentos_error_t agentos_coordinator_weighted_create(
    const char** model_names, const float* weights,
    size_t model_count, agentos_coordinator_base_t** out_base);

extern agentos_error_t agentos_coordinator_arbiter_create(
    const char* arbiter_model,
    void (*callback)(const char* question, char* answer, size_t max_len),
    agentos_coordinator_base_t** out_base);

typedef struct {
    agentos_coordinator_base_t* base;
} strategy_adapter_data_t;

static agentos_error_t adapter_coordinate(
    const char** prompts,
    size_t count,
    void* context,
    char** out_result) {
    if (!context || !out_result) return AGENTOS_EINVAL;

    agentos_coordinator_strategy_t* strategy = (agentos_coordinator_strategy_t*)context;
    strategy_adapter_data_t* adapter = (strategy_adapter_data_t*)strategy->data;
    if (!adapter || !adapter->base || !adapter->base->coordinate) return AGENTOS_EINVAL;

    agentos_coordination_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    return adapter->base->coordinate(adapter->base, &ctx, prompts, count, out_result);
}

static void adapter_destroy(agentos_coordinator_strategy_t* strategy) {
    if (!strategy) return;
    strategy_adapter_data_t* adapter = (strategy_adapter_data_t*)strategy->data;
    if (adapter) {
        if (adapter->base && adapter->base->destroy) {
            adapter->base->destroy(adapter->base);
        }
        AGENTOS_FREE(adapter);
    }
    AGENTOS_FREE(strategy);
}

static agentos_coordinator_strategy_t* wrap_base_to_strategy(
    agentos_coordinator_base_t* base) {
    if (!base) return NULL;

    agentos_coordinator_strategy_t* strategy =
        (agentos_coordinator_strategy_t*)AGENTOS_CALLOC(1, sizeof(agentos_coordinator_strategy_t));
    if (!strategy) {
        if (base->destroy) base->destroy(base);
        return NULL;
    }

    strategy_adapter_data_t* adapter =
        (strategy_adapter_data_t*)AGENTOS_CALLOC(1, sizeof(strategy_adapter_data_t));
    if (!adapter) {
        if (base->destroy) base->destroy(base);
        AGENTOS_FREE(strategy);
        return NULL;
    }

    adapter->base = base;
    strategy->coordinate = adapter_coordinate;
    strategy->destroy = adapter_destroy;
    strategy->data = adapter;

    return strategy;
}

agentos_coordinator_strategy_t* agentos_dual_model_coordinator_create(
    const char* primary_model,
    const char* secondary1,
    const char* secondary2,
    agentos_llm_service_t* llm) {

    agentos_coordinator_base_t* base = NULL;

    if (secondary2 && secondary2[0]) {
        const char* model_names[3] = { primary_model, secondary1, secondary2 };
        float weights[3] = { 0.5f, 0.3f, 0.2f };
        agentos_error_t err = agentos_coordinator_weighted_create(
            model_names, weights, 3, &base);
        if (err != AGENTOS_SUCCESS || !base) return NULL;
    } else {
        agentos_error_t err = agentos_coordinator_dual_model_create(
            primary_model, secondary1, 0.7f, 0.3f, &base);
        if (err != AGENTOS_SUCCESS || !base) return NULL;
    }

    strategy_adapter_data_t* adapter = (strategy_adapter_data_t*)AGENTOS_MALLOC(sizeof(strategy_adapter_data_t));
    if (!adapter) return NULL;
    adapter->base = base;
    adapter->base->llm = llm;

    return wrap_base_to_strategy(base);
}

agentos_coordinator_strategy_t* agentos_majority_coordinator_create(
    const char** model_names,
    size_t model_count,
    agentos_llm_service_t* llm) {

    agentos_coordinator_base_t* base = NULL;
    agentos_error_t err = agentos_coordinator_majority_create(
        model_count, 0.5f, &base);
    if (err != AGENTOS_SUCCESS || !base) return NULL;

    base->llm = llm;

    return wrap_base_to_strategy(base);
}

agentos_coordinator_strategy_t* agentos_weighted_coordinator_create(
    const char** model_names,
    const float* weights,
    size_t model_count,
    agentos_llm_service_t* llm) {
    (void)llm;

    agentos_coordinator_base_t* base = NULL;
    agentos_error_t err = agentos_coordinator_weighted_create(
        model_names, weights, model_count, &base);
    if (err != AGENTOS_SUCCESS || !base) return NULL;

    return wrap_base_to_strategy(base);
}

agentos_coordinator_strategy_t* agentos_arbiter_model_create(
    const char* arbiter_model,
    agentos_llm_service_t* llm) {
    (void)llm;

    agentos_coordinator_base_t* base = NULL;
    agentos_error_t err = agentos_coordinator_arbiter_create(
        arbiter_model, NULL, &base);
    if (err != AGENTOS_SUCCESS || !base) return NULL;

    return wrap_base_to_strategy(base);
}

agentos_coordinator_strategy_t* agentos_arbiter_human_create(
    void (*callback)(const char* question, char* answer, size_t max_len)) {
    agentos_coordinator_base_t* base = NULL;
    agentos_error_t err = agentos_coordinator_arbiter_create(
        NULL, callback, &base);
    if (err != AGENTOS_SUCCESS || !base) return NULL;

    return wrap_base_to_strategy(base);
}
