/**
 * @file priority.c
 * @brief 优先级调度策略，选择优先级最高的Agent
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "cognition.h"
#include "agent_registry.h"
#include "strategy.h"
#include <stdlib.h>
#include <string.h>

/* Unified base library compatibility layer */
#include "../../../commons/utils/memory/include/memory_compat.h"
#include "../../../commons/utils/string/include/string_compat.h"

/**
 * @brief 优先级调度策略内部结构
 */
struct agentos_priority_dispatch {
    void* registry_ctx;                          /**< 注册中心上下文 */
    agent_registry_get_agents_func get_agents;   /**< 获取候选Agent列表的函数 */
};

/**
 * @brief 创建优先级调度策略实例
 *
 * @param registry_ctx [in] 注册中心上下文（非NULL）
 * @param get_agents_func [in] 获取候选Agent列表的函数指针（非NULL）
 * @param out_strategy [out] 输出策略实例
 * @return AGENTOS_SUCCESS 成功
 * @return AGENTOS_EINVAL 参数无效
 * @return AGENTOS_ENOMEM 内存分配失败
 */
agentos_error_t agentos_dispatching_priority_create(
    void* registry_ctx,
    agent_registry_get_agents_func get_agents_func,
    agentos_dispatching_strategy_t** out_strategy)
{
    if (!registry_ctx || !get_agents_func || !out_strategy) {
        return AGENTOS_EINVAL;
    }

    struct agentos_priority_dispatch* priority =
        (struct agentos_priority_dispatch*)AGENTOS_CALLOC(1, sizeof(*priority));
    if (!priority) {
        return AGENTOS_ENOMEM;
    }

    priority->registry_ctx = registry_ctx;
    priority->get_agents = get_agents_func;

    agentos_dispatching_strategy_t* strategy =
        (agentos_dispatching_strategy_t*)AGENTOS_CALLOC(1, sizeof(*strategy));
    if (!strategy) {
        AGENTOS_FREE(priority);
        return AGENTOS_ENOMEM;
    }

    strategy->context = priority;
    strategy->select_agent = NULL; /* TODO: 实现选择逻辑 */

    *out_strategy = strategy;
    return AGENTOS_SUCCESS;
}
