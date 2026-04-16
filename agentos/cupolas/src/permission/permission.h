/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * permission.h - Permission Engine Public Interface
 *
 * Design Principles:
 * - Least Privilege: Default deny, explicit allow
 * - High Performance: Caching + rule priority sorting
 * - Extensible: Dynamic rule loading support
 */

#ifndef CUPOLAS_PERMISSION_H
#define CUPOLAS_PERMISSION_H

#include "../platform/platform.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Permission Engine Handle */
typedef struct permission_engine permission_engine_t;

/**
 * @brief Create permission engine
 * @param[in] rules_path Rules file path (YAML format), NULL for empty rule set
 * @return Engine handle, NULL on failure
 * @post On success, caller owns the returned handle
 * @note Thread-safe: Yes
 * @reentrant No (create/destroy must be paired)
 * @ownership Returned handle: caller owns, must call permission_engine_destroy
 */
permission_engine_t* permission_engine_create(const char* rules_path);

/**
 * @brief Destroy permission engine
 * @param[in] engine Engine handle (must not be NULL)
 * @pre Handle was created by permission_engine_create
 * @post All resources are released
 * @note Thread-safe: No, ensure no other threads access engine
 * @reentrant No
 * @ownership engine: caller transfers ownership
 */
void permission_engine_destroy(permission_engine_t* engine);

/**
 * @brief Increment reference count
 * @param[in] engine Engine handle (must not be NULL)
 * @return Engine handle (same as input)
 * @note Thread-safe: Yes
 * @reentrant Yes
 * @ownership engine: caller retains ownership, returns same handle
 */
permission_engine_t* permission_engine_ref(permission_engine_t* engine);

/**
 * @brief Decrement reference count
 * @param[in] engine Engine handle (must not be NULL)
 * @note Thread-safe: Yes
 * @reentrant No
 */
void permission_engine_unref(permission_engine_t* engine);

/**
 * @brief Check permission
 * @param[in] engine Engine handle (must not be NULL)
 * @param[in] agent_id Agent identifier (must not be NULL)
 * @param[in] action Action type: "read", "write", "execute" (must not be NULL)
 * @param[in] resource Resource path (must not be NULL)
 * @param[in] context Optional context information (may be NULL)
 * @return 1 allowed, 0 denied, negative on error
 * @note Thread-safe: Yes
 * @reentrant Yes, but concurrent calls with same params may race on cache
 * @ownership All input strings: caller retains ownership
 */
int permission_engine_check(permission_engine_t* engine,
                            const char* agent_id,
                            const char* action,
                            const char* resource,
                            const char* context);

/**
 * @brief Reload rules file
 * @param[in] engine Engine handle (must not be NULL)
 * @return 0 on success, negative on failure
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
int permission_engine_reload(permission_engine_t* engine);

/**
 * @brief Clear cache
 * @param[in] engine Engine handle (must not be NULL)
 * @note Thread-safe: Yes
 * @reentrant Yes
 * @post All cached permissions are invalidated
 */
void permission_engine_clear_cache(permission_engine_t* engine);

/**
 * @brief Add rule
 * @param[in] engine Engine handle (must not be NULL)
 * @param[in] agent_id Agent ID pattern (NULL or "*" for wildcard)
 * @param[in] action Action pattern (NULL or "*" for wildcard)
 * @param[in] resource Resource pattern with glob support (e.g., "/data/star")
 * @param[in] allow 1 to allow, 0 to deny
 * @param[in] priority Higher value = higher priority
 * @return 0 on success, negative on failure
 * @note Thread-safe: Yes
 * @reentrant Yes
 * @ownership All input strings: caller retains ownership
 */
int permission_engine_add_rule(permission_engine_t* engine,
                               const char* agent_id,
                               const char* action,
                               const char* resource,
                               int allow,
                               int priority);

/**
 * @brief Get rule count
 * @param[in] engine Engine handle (must not be NULL)
 * @return Number of rules
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
size_t permission_engine_rule_count(permission_engine_t* engine);

/**
 * @brief Get cache statistics
 * @param[in] engine Engine handle (must not be NULL)
 * @param[out] hit_count Cache hit count output (may be NULL)
 * @param[out] miss_count Cache miss count output (may be NULL)
 * @note Thread-safe: Yes
 * @reentrant Yes
 * @ownership hit_count, miss_count: callee writes, caller owns
 */
void permission_engine_cache_stats(permission_engine_t* engine,
                                   uint64_t* hit_count,
                                   uint64_t* miss_count);

#ifdef __cplusplus
}
#endif

#endif /* CUPOLAS_PERMISSION_H */
