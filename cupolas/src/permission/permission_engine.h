/**
 * @file permission_engine.h
 * @brief 权限引擎内部结构声明
 * @author Spharx
 * @date 2024
 */

#ifndef DOMAIN_PERMISSION_ENGINE_H
#define DOMAIN_PERMISSION_ENGINE_H

#include "permission.h"
#include "permission_rule.h"
#include "permission_cache.h"
#include "../platform/platform.h"

struct permission_engine {
    rule_manager_t*     rules;
    cache_manager_t*    cache;
    cupolas_rwlock_t      rwlock;
    char*               rules_path;
    uint64_t            last_load_time;
    cupolas_atomic32_t    ref_count;
};

#endif /* DOMAIN_PERMISSION_ENGINE_H */
