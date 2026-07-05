/**
 * @file service.h
 * @brief 服务内部结构声明
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTRT_LLM_SERVICE_INTERNAL_H
#define AGENTRT_LLM_SERVICE_INTERNAL_H

#include "cache.h"
#include "cost_tracker.h"
#include "llm_service.h"
#include "platform.h"
#include "providers/registry.h"
#include "token_counter.h"

struct llm_service {
    provider_registry_t *registry;
    llm_cache_t *cache;
    cost_tracker_t *cost;
    token_counter_t *token_counter;
    agentrt_mutex_t lock; /* 保护 registry 和 cost 等 */
    void *rules;
    size_t rule_count;
};

#endif /* AGENTRT_LLM_SERVICE_INTERNAL_H */