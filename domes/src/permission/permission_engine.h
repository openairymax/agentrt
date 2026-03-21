/**
 * @file permission_engine.h
 * @brief 权限引擎内部结构声明
 */
#ifndef DOMAIN_PERMISSION_ENGINE_H
#define DOMAIN_PERMISSION_ENGINE_H

#include "permission.h"
#include "permission_rule.h"
#include "permission_cache.h"
#include <pthread.h>

struct permission_engine {
    rule_manager_t*     rules;
    cache_manager_t*    cache;
    pthread_rwlock_t    rwlock;
    char*               rules_path;
    time_t              last_load_time;
};

#endif /* DOMAIN_PERMISSION_ENGINE_H */