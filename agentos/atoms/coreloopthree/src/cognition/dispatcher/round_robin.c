/**
 * @file round_robin.c
 * @brief 轮询调度策略，依次选择Agent
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "cognition.h"
#include "agent_registry.h"
#include "strategy.h"
#include <stdlib.h>
#include <string.h>

/* Unified base library compatibility layer */
#include "memory_compat.h"
#include "string_compat.h"

/**
 * @brief 轮询调度策略内部结构
 */
struct agentos_round_robin_dispatch {
    void* registry_ctx;
    agent_registry_get_agents_func get_agents;
    size_t last_index;
    agentos_mutex_t* lock;
};

/**
 * @brief 销毁轮询调度策略实例
 */
static void rr_destroy(agentos_dispatching_strategy_t* strategy)
{
    if (!strategy) return;
    struct agentos_round_robin_dispatch* rr =
        (struct agentos_round_robin_dispatch*)strategy->data;
    if (rr) {
        agentos_mutex_destroy(rr->lock);
        AGENTOS_FREE(rr);
    }
    AGENTOS_FREE(strategy);
}

/**
 * @brief 使用轮询策略选择下一个Agent（依次循环选择）
 *
 * @param task [in] 任务描述
 * @param candidates [in] 候选Agent列表
 * @param count [in] 候选数量
 * @param context [in] 策略上下文（agentos_round_robin_dispatch*）
 * @param out_agent_id [out] 选中的Agent ID
 * @return AGENTOS_SUCCESS 成功
 */
static agentos_error_t rr_select(
    const agentos_task_node_t* task,
    const void** candidates,
    size_t count,
    void* context,
    char** out_agent_id)
{
    (void)task;

    struct agentos_round_robin_dispatch* rr =
        (struct agentos_round_robin_dispatch*)context;
    if (!rr || !out_agent_id) return AGENTOS_EINVAL;

    agent_info_t** agents = NULL;
    size_t agent_count = 0;
    agentos_error_t err;

    if (candidates && count > 0) {
        agents = (agent_info_t**)candidates;
        agent_count = count;
    } else {
        err = rr->get_agents(rr->registry_ctx, NULL, &agents, &agent_count);
        if (err != AGENTOS_SUCCESS) return err;
        if (agent_count == 0) return AGENTOS_ENOENT;
    }

    size_t next_index;
    agentos_mutex_lock(rr->lock);
    next_index = (rr->last_index + 1) % agent_count;
    rr->last_index = next_index;
    agentos_mutex_unlock(rr->lock);

    if (agents[next_index]) {
        *out_agent_id = AGENTOS_STRDUP(agents[next_index]->agent_id);
        return *out_agent_id ? AGENTOS_SUCCESS : AGENTOS_ENOMEM;
    }

    return AGENTOS_ENOENT;
}

/**
 * @brief 创建轮询调度策略实例
 *
 * @param registry_ctx [in] 注册中心上下文（非NULL）
 * @param get_agents_func [in] 获取候选Agent列表的函数指针（非NULL）
 * @param out_strategy [out] 输出策略实例
 * @return AGENTOS_SUCCESS 成功
 * @return AGENTOS_EINVAL 参数无效
 * @return AGENTOS_ENOMEM 内存分配失败
 */
agentos_dispatching_strategy_t* agentos_dispatching_round_robin_create(
    void* registry_ctx,
    agent_registry_get_agents_func get_agents_func)
{
    if (!registry_ctx || !get_agents_func) {
        return NULL;
    }

    struct agentos_round_robin_dispatch* rr =
        (struct agentos_round_robin_dispatch*)AGENTOS_CALLOC(1, sizeof(*rr));
    if (!rr) {
        return NULL;
    }

    rr->registry_ctx = registry_ctx;
    rr->get_agents = get_agents_func;
    rr->last_index = (size_t)-1;
    rr->lock = agentos_mutex_create();
    if (!rr->lock) {
        AGENTOS_FREE(rr);
        return NULL;
    }

    agentos_dispatching_strategy_t* strategy =
        (agentos_dispatching_strategy_t*)AGENTOS_CALLOC(1, sizeof(*strategy));
    if (!strategy) {
        AGENTOS_FREE(rr);
        return NULL;
    }

    strategy->data = rr;
    strategy->dispatch = rr_select;
    strategy->destroy = rr_destroy;

    return strategy;
}
