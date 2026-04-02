/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * permission_engine.c - Permission Engine Implementation
 */

/**
 * @file permission_engine.c
 * @brief Permission Engine Implementation
 * @author Spharx
 * @date 2024
 */

#include "permission_engine.h"
#include <stdlib.h>
#include <string.h>

#define DEFAULT_CACHE_CAPACITY 1024
#define DEFAULT_CACHE_TTL_MS 60000

permission_engine_t* permission_engine_create(const char* rules_path) {
    permission_engine_t* engine = (permission_engine_t*)cupolas_mem_alloc(sizeof(permission_engine_t));
    if (!engine) return NULL;
    
    memset(engine, 0, sizeof(permission_engine_t));
    
    if (cupolas_rwlock_init(&engine->rwlock) != cupolas_OK) {
        cupolas_mem_free(engine);
        return NULL;
    }
    
    engine->rules = rule_manager_create(rules_path);
    if (!engine->rules) {
        cupolas_rwlock_destroy(&engine->rwlock);
        cupolas_mem_free(engine);
        return NULL;
    }
    
    engine->cache = cache_manager_create(DEFAULT_CACHE_CAPACITY, DEFAULT_CACHE_TTL_MS);
    if (!engine->cache) {
        rule_manager_destroy(engine->rules);
        cupolas_rwlock_destroy(&engine->rwlock);
        cupolas_mem_free(engine);
        return NULL;
    }
    
    if (rules_path) {
        engine->rules_path = cupolas_strdup(rules_path);
    }
    
    cupolas_atomic_store32(&engine->ref_count, 1);
    
    return engine;
}

/**
 * @brief Destroy permission engine and release all resources
 * @param[in] engine Permission engine to destroy (may be NULL, safe to call)
 * @note Thread-safe: Uses reference counting, safe for multiple callers
 * @reentrant No
 * @ownership Transfers ownership: engine will be freed when ref_count reaches 0
 *
 * @details
 * This function uses reference counting. The engine is only actually destroyed
 * when the last reference is released (ref_count reaches 0). If other components
 * still hold references via permission_engine_ref(), this call simply decrements
 * the counter.
 */
void permission_engine_destroy(permission_engine_t* engine) {
    if (!engine) return;
    
    if (cupolas_atomic_sub32(&engine->ref_count, 1) > 1) {
        return;
    }
    
    cupolas_rwlock_wrlock(&engine->rwlock);
    
    if (engine->rules) {
        rule_manager_destroy(engine->rules);
        engine->rules = NULL;
    }
    
    if (engine->cache) {
        cache_manager_destroy(engine->cache);
        engine->cache = NULL;
    }
    
    cupolas_mem_free(engine->rules_path);
    
    cupolas_rwlock_unlock(&engine->rwlock);
    cupolas_rwlock_destroy(&engine->rwlock);
    cupolas_mem_free(engine);
}

/**
 * @brief Acquire a new reference to the permission engine
 * @param[in] engine Permission engine to reference (may be NULL)
 * @return Same engine pointer with incremented ref_count, or NULL if input is NULL
 * @note Thread-safe: Uses atomic increment operation
 * @reentrant Yes
 * @ownership Caller receives shared ownership, must call unref when done
 *
 * @details
 * Use this function when multiple components need to share the same engine instance.
 * Each call to ref() must be balanced with a corresponding call to unref().
 */
permission_engine_t* permission_engine_ref(permission_engine_t* engine) {
    if (!engine) return NULL;
    
    cupolas_atomic_inc32(&engine->ref_count);
    return engine;
}

/**
 * @brief Release a reference to the permission engine (alias for destroy)
 * @param[in] engine Permission engine to unreference
 * @note Thread-safe: Uses atomic decrement operation
 * @reentrant No
 * @ownership Releases shared ownership
 *
 * @details
 * Convenience wrapper around permission_engine_destroy(). Prefer using this
 * name when you acquired the reference via permission_engine_ref() for clarity.
 */
void permission_engine_unref(permission_engine_t* engine) {
    permission_engine_destroy(engine);
}

/**
 * @brief Check if an agent is permitted to perform an action on a resource
 * @param[in] engine Permission engine instance
 * @param[in] agent_id Agent identifier (e.g., "tool_d", "llm_d", "agent_123")
 * @param[in] action Action to perform (e.g., "read", "write", "execute", "delete")
 * @param[in] resource Resource being accessed (e.g., "/api/users", "database:production")
 * @param[in] context Additional context for policy evaluation (can be NULL)
 * @return Non-zero if allowed, 0 if denied, negative on error
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 *
 * @details
 * This is the core RBAC decision function. It implements:
 * 1. Cache lookup: Checks LRU cache first for fast path
 * 2. Rule matching: Evaluates YAML rules against request parameters
 * 3. Cache update: Stores decision in cache for future requests
 *
 * Performance characteristics:
 * - Cache hit: O(1) average case
 * - Cache miss: O(n) where n = number of rules (typically < 1000)
 *
 * Example:
 * @code
 * int result = permission_engine_check(engine,
 *     "tool_d",          // agent_id
 *     "execute",         // action
 *     "/bin/bash",       // resource
 *     "task_456");       // context
 *
 * if (result > 0) {
 *     // Permission granted
 * } else if (result == 0) {
 *     // Permission denied
 * } else {
 *     // Error occurred
 * }
 * @endcode
 */
int permission_engine_check(permission_engine_t* engine,
                            const char* agent_id,
                            const char* action,
                            const char* resource,
                            const char* context) {
    if (!engine) return 0;
    
    int cached = cache_manager_get(engine->cache, agent_id, action, resource, context);
    if (cached >= 0) {
        return cached;
    }
    
    int result = rule_manager_match(engine->rules, agent_id, action, resource, context);
    
    cache_manager_put(engine->cache, agent_id, action, resource, context, result);
    
    return result;
}

int permission_engine_reload(permission_engine_t* engine) {
    if (!engine) return cupolas_ERROR_INVALID_ARG;
    
    int ret = rule_manager_reload(engine->rules);
    if (ret == cupolas_OK) {
        cache_manager_clear(engine->cache);
        engine->last_load_time = cupolas_time_ms();
    }
    
    return ret;
}

void permission_engine_clear_cache(permission_engine_t* engine) {
    if (!engine) return;
    cache_manager_clear(engine->cache);
}

int permission_engine_add_rule(permission_engine_t* engine,
                               const char* agent_id,
                               const char* action,
                               const char* resource,
                               int allow,
                               int priority) {
    if (!engine) return cupolas_ERROR_INVALID_ARG;
    
    int ret = rule_manager_add(engine->rules, agent_id, action, resource, allow, priority);
    if (ret == cupolas_OK) {
        cache_manager_clear(engine->cache);
    }
    
    return ret;
}

size_t permission_engine_rule_count(permission_engine_t* engine) {
    if (!engine) return 0;
    return rule_manager_count(engine->rules);
}

void permission_engine_cache_stats(permission_engine_t* engine,
                                   uint64_t* hit_count,
                                   uint64_t* miss_count) {
    if (!engine) {
        if (hit_count) *hit_count = 0;
        if (miss_count) *miss_count = 0;
        return;
    }
    
    cache_manager_stats(engine->cache, hit_count, miss_count);
}
