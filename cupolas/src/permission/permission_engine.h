/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * permission_engine.h - Permission Engine Internal Structures
 */

#ifndef CUPOLAS_PERMISSION_ENGINE_H
#define CUPOLAS_PERMISSION_ENGINE_H

#include "permission.h"
#include "permission_rule.h"
#include "permission_cache.h"
#include "../platform/platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Permission engine internal structure
 * 
 * Design principles:
 * - Thread-safe with read-write lock
 * - Hot-reload support with version tracking
 * - Reference counting for safe memory management
 * - LRU cache integration for O(1) lookups
 */
struct permission_engine {
    rule_manager_t*     rules;          /**< Rule manager (loaded from YAML) */
    cache_manager_t*    cache;          /**< LRU cache for permission results */
    cupolas_rwlock_t    rwlock;         /**< Read-write lock for thread safety */
    char*               rules_path;     /**< Path to rules configuration file */
    uint64_t            last_load_time; /**< Last reload timestamp (milliseconds) */
    cupolas_atomic32_t  ref_count;      /**< Reference count for memory management */
};

#ifdef __cplusplus
}
#endif

#endif /* CUPOLAS_PERMISSION_ENGINE_H */
