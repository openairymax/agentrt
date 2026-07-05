/**
 * @file coordinator_adapter.c
 * @brief 协调器公共API适配层 - 将内部base_t桥接到公共strategy_t
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "agentrt.h"
#include "memory_compat.h"
#include "strategy.h"
#include "string_compat.h"

#include <stdlib.h>
#include <string.h>
#include "error.h"
#include "error_compat.h"

#define ATM_RET_ERR(c) \
    do { agentrt_error_push_ex((c), __FILE__, __LINE__, __func__, "%s", agentrt_error_str(c)); return (c); } while(0)


extern agentrt_error_t agentrt_coordinator_dual_model_create(const char *primary_model,
                                                             const char *secondary_model,
                                                             float primary_weight,
                                                             float secondary_weight,
                                                             agentrt_coordinator_base_t **out_base);

extern agentrt_error_t agentrt_coordinator_majority_create(size_t min_voters, float threshold,
                                                           agentrt_coordinator_base_t **out_base);

extern agentrt_error_t agentrt_coordinator_weighted_create(const char **model_names,
                                                           const float *weights, size_t model_count,
                                                           agentrt_coordinator_base_t **out_base);

extern agentrt_error_t agentrt_coordinator_arbiter_create(
    const char *arbiter_model, void (*callback)(const char *question, char *answer, size_t max_len),
    agentrt_coordinator_base_t **out_base);

typedef struct {
    agentrt_coordinator_base_t *base;
} strategy_adapter_data_t;

static agentrt_error_t adapter_coordinate(const char **prompts, size_t count, void *context,
                                          char **out_result)
{
    if (!context || !out_result)
        ATM_RET_ERR(AGENTRT_EINVAL);

    agentrt_coordinator_strategy_t *strategy = (agentrt_coordinator_strategy_t *)context;
    strategy_adapter_data_t *adapter = (strategy_adapter_data_t *)strategy->data;
    if (!adapter || !adapter->base || !adapter->base->coordinate)
        ATM_RET_ERR(AGENTRT_EINVAL);

    agentrt_coordination_context_t ctx;
    __builtin_memset(&ctx, 0, sizeof(ctx));

    return adapter->base->coordinate(adapter->base, &ctx, prompts, count, out_result);
}

static void adapter_destroy(agentrt_coordinator_strategy_t *strategy)
{
    if (!strategy)
        return;
    strategy_adapter_data_t *adapter = (strategy_adapter_data_t *)strategy->data;
    if (adapter) {
        if (adapter->base && adapter->base->destroy) {
            adapter->base->destroy(adapter->base);
        }
        AGENTRT_FREE(adapter);
    }
    AGENTRT_FREE(strategy);
}

static agentrt_coordinator_strategy_t *wrap_base_to_strategy(agentrt_coordinator_base_t *base)
{
    if (!base) return NULL;

    agentrt_coordinator_strategy_t *strategy =
        (agentrt_coordinator_strategy_t *)AGENTRT_CALLOC(1, sizeof(agentrt_coordinator_strategy_t));
    if (!strategy) {
        if (base->destroy)
            base->destroy(base);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }

    strategy_adapter_data_t *adapter =
        (strategy_adapter_data_t *)AGENTRT_CALLOC(1, sizeof(strategy_adapter_data_t));
    if (!adapter) {
        if (base->destroy)
            base->destroy(base);
        AGENTRT_FREE(strategy);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }

    adapter->base = base;
    strategy->coordinate = adapter_coordinate;
    strategy->destroy = adapter_destroy;
    strategy->data = adapter;

    return strategy;
}

agentrt_coordinator_strategy_t *agentrt_dual_model_coordinator_create(const char *primary_model,
                                                                      const char *secondary1,
                                                                      const char *secondary2,
                                                                      agentrt_llm_service_t *llm)
{

    agentrt_coordinator_base_t *base = NULL;

    if (secondary2 && secondary2[0]) {
        const char *model_names[3] = {primary_model, secondary1, secondary2};
        float weights[3] = {0.5f, 0.3f, 0.2f};
        agentrt_error_t err = agentrt_coordinator_weighted_create(model_names, weights, 3, &base);
        if (err != AGENTRT_SUCCESS || !base) return NULL;
    } else {
        agentrt_error_t err =
            agentrt_coordinator_dual_model_create(primary_model, secondary1, 0.7f, 0.3f, &base);
        if (err != AGENTRT_SUCCESS || !base) return NULL;
    }

    strategy_adapter_data_t *adapter =
        (strategy_adapter_data_t *)AGENTRT_MALLOC(sizeof(strategy_adapter_data_t));
    if (!adapter) return NULL;
    adapter->base = base;
    adapter->base->llm = (struct agentrt_llm_service *)llm;

    return wrap_base_to_strategy(base);
}

agentrt_coordinator_strategy_t *agentrt_majority_coordinator_create(const char **model_names,
                                                                    size_t model_count,
                                                                    agentrt_llm_service_t *llm)
{

    agentrt_coordinator_base_t *base = NULL;
    agentrt_error_t err = agentrt_coordinator_majority_create(model_count, 0.5f, &base);
    if (err != AGENTRT_SUCCESS || !base) return NULL;

    base->llm = (struct agentrt_llm_service *)llm;

    return wrap_base_to_strategy(base);
}

agentrt_coordinator_strategy_t *agentrt_weighted_coordinator_create(const char **model_names,
                                                                    const float *weights,
                                                                    size_t model_count,
                                                                    agentrt_llm_service_t *llm)
{
    if (!llm || !model_names || !weights || model_count == 0) return NULL;

    agentrt_coordinator_base_t *base = NULL;
    agentrt_error_t err =
        agentrt_coordinator_weighted_create(model_names, weights, model_count, &base);
    if (err != AGENTRT_SUCCESS || !base) return NULL;

    return wrap_base_to_strategy(base);
}

agentrt_coordinator_strategy_t *agentrt_arbiter_model_create(const char *arbiter_model,
                                                             agentrt_llm_service_t *llm)
{
    if (!llm || !arbiter_model) return NULL;

    agentrt_coordinator_base_t *base = NULL;
    agentrt_error_t err = agentrt_coordinator_arbiter_create(arbiter_model, NULL, &base);
    if (err != AGENTRT_SUCCESS || !base) return NULL;

    /* P2.7: 注入 LLM 句柄到 base->llm，使 arbiter_coordinate 可调用 LLM 仲裁。
     * 类型说明：strategy.h 的 agentrt_llm_service_t typedef 为 struct llm_service，
     * 但 base->llm 字段类型为 struct agentrt_llm_service*（planner LLM client）。
     * 调用方应传入 llm_client.h 的 agentrt_llm_service_t* (struct agentrt_llm_service*)，
     * 此处 cast 与 agentrt_dual_model_coordinator_create 等保持一致。 */
    base->llm = (struct agentrt_llm_service *)llm;

    return wrap_base_to_strategy(base);
}

agentrt_coordinator_strategy_t *
agentrt_arbiter_human_create(void (*callback)(const char *question, char *answer, size_t max_len))
{
    agentrt_coordinator_base_t *base = NULL;
    agentrt_error_t err = agentrt_coordinator_arbiter_create(NULL, callback, &base);
    if (err != AGENTRT_SUCCESS || !base) return NULL;

    return wrap_base_to_strategy(base);
}
