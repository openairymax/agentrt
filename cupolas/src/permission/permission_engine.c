/**
 * @file permission_engine.c
 * @brief 权限引擎实现
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

permission_engine_t* permission_engine_ref(permission_engine_t* engine) {
    if (!engine) return NULL;
    
    cupolas_atomic_inc32(&engine->ref_count);
    return engine;
}

void permission_engine_unref(permission_engine_t* engine) {
    permission_engine_destroy(engine);
}

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
