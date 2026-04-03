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
#include "../../../commons/utils/memory/include/memory_compat.h"
#include "../../../commons/utils/string/include/string_compat.h"

/**
 * @brief 轮询调度策略内部结构
 */
struct agentos_round_robin_dispatch {
    void* registry_ctx;                          /**< 注册中心上下文 */
    agent_registry_get_agents_func get_agents;   /**< 获取候选Agent列表的函数 */
    size_t last_index;                           /**< 上次选择的Agent索引 */
};

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
agentos_error_t agentos_dispatching_round_robin_create(
    void* registry_ctx,
    agent_registry_get_agents_func get_agents_func,
    agentos_dispatching_strategy_t** out_strategy)
{
    if (!registry_ctx || !get_agents_func || !out_strategy) {
        return AGENTOS_EINVAL;
    }

    struct agentos_round_robin_dispatch* rr =
        (struct agentos_round_robin_dispatch*)AGENTOS_CALLOC(1, sizeof(*rr));
    if (!rr) {
        return AGENTOS_ENOMEM;
    }

    rr->registry_ctx = registry_ctx;
    rr->get_agents = get_agents_func;
    rr->last_index = (size_t)-1;

    agentos_dispatching_strategy_t* strategy =
        (agentos_dispatching_strategy_t*)AGENTOS_CALLOC(1, sizeof(*strategy));
    if (!strategy) {
        AGENTOS_FREE(rr);
        return AGENTOS_ENOMEM;
    }

    strategy->context = rr;
    strategy->select_agent = NULL; /* TODO: 实现轮询选择逻辑 */

    *out_strategy = strategy;
    return AGENTOS_SUCCESS;
}
