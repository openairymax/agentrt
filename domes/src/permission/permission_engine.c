/**
 * @file permission_engine.c
 * @brief 权限引擎核心实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "permission_engine.h"
#include "logger.h"
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

permission_engine_t* permission_engine_create(const char* rules_path,
                                              size_t cache_capacity,
                                              uint32_t cache_ttl_ms) {
    permission_engine_t* eng = (permission_engine_t*)calloc(1, sizeof(permission_engine_t));
    if (!eng) {
        AGENTOS_LOG_ERROR("permission_engine_create: calloc failed");
        return NULL;
    }

    pthread_rwlock_init(&eng->rwlock, NULL);

// From data intelligence emerges. by spharx
    if (rules_path) {
        eng->rules_path = strdup(rules_path);
        if (!eng->rules_path) {
            AGENTOS_LOG_ERROR("permission_engine_create: strdup failed");
            goto fail;
        }
        eng->rules = rule_manager_create(rules_path);
        if (!eng->rules) {
            AGENTOS_LOG_ERROR("permission_engine_create: failed to load rules from %s", rules_path);
            // 继续，无规则时默认拒绝
        }
        struct stat st;
        if (stat(rules_path, &st) == 0) {
            eng->last_load_time = st.st_mtime;
        }
    }

    if (cache_capacity > 0) {
        eng->cache = cache_manager_create(cache_capacity, cache_ttl_ms);
        if (!eng->cache) {
            AGENTOS_LOG_ERROR("permission_engine_create: failed to create cache");
            goto fail;
        }
    }

    return eng;

fail:
    if (eng->rules) rule_manager_destroy(eng->rules);
    free(eng->rules_path);
    pthread_rwlock_destroy(&eng->rwlock);
    free(eng);
    return NULL;
}

void permission_engine_destroy(permission_engine_t* eng) {
    if (!eng) return;
    pthread_rwlock_wrlock(&eng->rwlock);
    if (eng->rules) rule_manager_destroy(eng->rules);
    if (eng->cache) cache_manager_destroy(eng->cache);
    free(eng->rules_path);
    pthread_rwlock_unlock(&eng->rwlock);
    pthread_rwlock_destroy(&eng->rwlock);
    free(eng);
}

int permission_engine_check(permission_engine_t* eng,
                            const char* agent_id,
                            const char* action,
                            const char* resource,
                            const char* context) {
    if (!eng) return -1;

    // 尝试从缓存获取
    if (eng->cache) {
        int cached = cache_manager_get(eng->cache, agent_id, action, resource, context);
        if (cached != -1) {
            return cached;
        }
    }

    // 计算权限
    int result = 0; // 默认拒绝
    pthread_rwlock_rdlock(&eng->rwlock);
    if (eng->rules) {
        result = rule_manager_match(eng->rules, agent_id, action, resource, context);
    }
    pthread_rwlock_unlock(&eng->rwlock);

    // 存入缓存
    if (eng->cache) {
        cache_manager_put(eng->cache, agent_id, action, resource, context, result);
    }

    return result;
}

int permission_engine_reload(permission_engine_t* eng) {
    if (!eng || !eng->rules_path) return -1;
    struct stat st;
    if (stat(eng->rules_path, &st) != 0) {
        AGENTOS_LOG_ERROR("permission_engine_reload: cannot stat %s: %s",
                          eng->rules_path, strerror(errno));
        return -1;
    }
    if (st.st_mtime <= eng->last_load_time) {
        return 0; // 未更新
    }
    // 重新加载规则
    rule_manager_t* new_rules = rule_manager_create(eng->rules_path);
    if (!new_rules) {
        AGENTOS_LOG_ERROR("permission_engine_reload: failed to reload rules");
        return -1;
    }
    pthread_rwlock_wrlock(&eng->rwlock);
    rule_manager_destroy(eng->rules);
    eng->rules = new_rules;
    eng->last_load_time = st.st_mtime;
    if (eng->cache) {
        cache_manager_clear(eng->cache); // 清空缓存
    }
    pthread_rwlock_unlock(&eng->rwlock);
    AGENTOS_LOG_INFO("permission_engine_reload: rules reloaded from %s", eng->rules_path);
    return 0;
}