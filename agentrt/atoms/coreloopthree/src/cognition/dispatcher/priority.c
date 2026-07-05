/**
 * @file priority.c
 * @brief 优先级调度策略，选择优先级最高的Agent
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "agent_registry.h"
#include "cognition.h"
#include "strategy.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

/* Unified base library compatibility layer */
#include "memory_compat.h"
#include "string_compat.h"

/**
 * @brief 优先级调度策略内部结构
 */
struct agentrt_priority_dispatch {
    void *registry_ctx;                        /**< 注册中心上下文 */
    agent_registry_get_agents_func get_agents; /**< 获取候选Agent列表的函数 */
};

/**
 * @brief 销毁优先级调度策略实例
 */
static void priority_destroy(agentrt_dispatching_strategy_t *strategy)
{
    if (!strategy)
        return;
    struct agentrt_priority_dispatch *priority = (struct agentrt_priority_dispatch *)strategy->data;
    if (priority)
        AGENTRT_FREE(priority);
    AGENTRT_FREE(strategy);
}

/**
 * @brief 使用优先级策略选择最优Agent（选择优先级最高的）
 *
 * @param task [in] 任务描述
 * @param candidates [in] 候选Agent列表
 * @param count [in] 候选数量
 * @param context [in] 策略上下文（agentrt_priority_dispatch*）
 * @param out_agent_id [out] 选中的Agent ID
 * @return AGENTRT_SUCCESS 成功
 */
static agentrt_error_t priority_select(const agentrt_task_node_t __attribute__((unused)) * task,
                                       const void **candidates, size_t count, void *context,
                                       char **out_agent_id)
{

    struct agentrt_priority_dispatch *priority = (struct agentrt_priority_dispatch *)context;
    if (!priority || !out_agent_id)
        return AGENTRT_EINVAL;

    agent_info_t **agents = NULL;
    size_t agent_count = 0;
    agentrt_error_t err;

    if (candidates && count > 0) {
        agents = (agent_info_t **)candidates;
        agent_count = count;
    } else {
        err = priority->get_agents(priority->registry_ctx, NULL, &agents, &agent_count);
        if (err != AGENTRT_SUCCESS)
            return err;
        if (agent_count == 0)
            return AGENTRT_ENOENT;
    }

    int best_index = -1;
    int best_priority = INT_MIN;

    for (size_t i = 0; i < agent_count; i++) {
        agent_info_t *agent = agents[i];
        if (!agent)
            continue;
        if (agent->priority > best_priority) {
            best_priority = agent->priority;
            best_index = (int)i;
        }
    }

    if (best_index >= 0 && agents[best_index]) {
        *out_agent_id = AGENTRT_STRDUP(agents[best_index]->agent_id);
        return *out_agent_id ? AGENTRT_SUCCESS : AGENTRT_ENOMEM;
    }

    return AGENTRT_ENOENT;
}

/**
 * @brief 创建优先级调度策略实例
 *
 * @param registry_ctx [in] 注册中心上下文（非NULL）
 * @param get_agents_func [in] 获取候选Agent列表的函数指针（非NULL）
 * @param out_strategy [out] 输出策略实例
 * @return AGENTRT_SUCCESS 成功
 * @return AGENTRT_EINVAL 参数无效
 * @return AGENTRT_ENOMEM 内存分配失败
 */
agentrt_error_t agentrt_dispatching_priority_create(void *registry_ctx,
                                                    agent_registry_get_agents_func get_agents_func,
                                                    agentrt_dispatching_strategy_t **out_strategy)
{
    if (!registry_ctx || !get_agents_func || !out_strategy) {
        return AGENTRT_EINVAL;
    }

    struct agentrt_priority_dispatch *priority =
        (struct agentrt_priority_dispatch *)AGENTRT_CALLOC(1, sizeof(*priority));
    if (!priority) {
        return AGENTRT_ENOMEM;
    }

    priority->registry_ctx = registry_ctx;
    priority->get_agents = get_agents_func;

    agentrt_dispatching_strategy_t *strategy =
        (agentrt_dispatching_strategy_t *)AGENTRT_CALLOC(1, sizeof(*strategy));
    if (!strategy) {
        AGENTRT_FREE(priority);
        return AGENTRT_ENOMEM;
    }

    strategy->data = priority;
    strategy->dispatch = priority_select;
    strategy->destroy = priority_destroy;

    *out_strategy = strategy;
    return AGENTRT_SUCCESS;
}
