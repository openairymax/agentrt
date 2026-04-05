/**
 * @file service.h
 * @brief 服务内部结构声明
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef LLM_SERVICE_INTERNAL_H
#define LLM_SERVICE_INTERNAL_H

#include "llm_service.h"
#include "providers/registry.h"
#include "cache.h"
#include "cost_tracker.h"
#include "token_counter.h"
#include "platform.h"

struct llm_service {
    provider_registry_t* registry;
    cache_t* cache;
    cost_tracker_t* cost;
    token_counter_t* token_counter;
    agentos_mutex_t lock;   /* 保护 registry 和 cost 等 */
};

#endif /* LLM_SERVICE_INTERNAL_H */