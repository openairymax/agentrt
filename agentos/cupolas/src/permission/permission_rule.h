/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * permission_rule.h - Permission Rule Manager Internal Interface
 */

#ifndef CUPOLAS_PERMISSION_RULE_H
#define CUPOLAS_PERMISSION_RULE_H

#include "../platform/platform.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Permission rule structure
 * 
 * Design principles:
 * - Pattern matching with glob support
 * - Priority-based rule evaluation
 * - Wildcard support via NULL fields
 */
typedef struct permission_rule {
    char*                   agent_id;       /**< Agent ID (NULL = wildcard) */
    char*                   action;         /**< Action name (NULL = wildcard) */
    char*                   resource;       /**< Resource pattern (supports glob) */
    char*                   resource_pattern; /**< Pre-compiled pattern (optional) */
    int                     allow;          /**< Permission result (1=allow, 0=deny) */
    int                     priority;       /**< Priority (higher number = higher priority) */
    struct permission_rule* next;           /**< Next rule in linked list */
} permission_rule_t;

/**
 * @brief Rule manager structure
 * 
 * Provides rule storage and matching with:
 * - Thread-safe read-write lock
 * - Hot-reload support with mtime tracking
 * - Version control for change detection
 */
typedef struct rule_manager {
    permission_rule_t*  rules;          /**< Linked list of rules */
    cupolas_rwlock_t    rwlock;         /**< Read-write lock for thread safety */
    char*               path;           /**< Path to rules configuration file */
    uint64_t            last_mtime;     /**< Last modification time of config file */
    cupolas_atomic32_t  version;        /**< Rule version counter */
} rule_manager_t;

/**
 * @brief Create rule manager and load rules from YAML file
 * @param[in] path Path to YAML configuration file
 * @return Rule manager handle, NULL on failure
 * @note Thread-safe: Safe to call from multiple threads (initialization only)
 * @reentrant No
 * @ownership Returns owned pointer: caller must call rule_manager_destroy()
 * @ownership path: caller retains ownership
 */
rule_manager_t* rule_manager_create(const char* path);

/**
 * @brief Destroy rule manager and free all resources
 * @param[in] mgr Rule manager handle (may be NULL)
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other operations)
 * @reentrant No
 * @ownership mgr: transferred to this function, will be freed
 */
void rule_manager_destroy(rule_manager_t* mgr);

/**
 * @brief Match rules against given parameters
 * @param[in] mgr Rule manager handle
 * @param[in] agent_id Agent identifier
 * @param[in] action Action being performed
 * @param[in] resource Resource being accessed
 * @param[in] context Context information (currently unused)
 * @return 1=allowed, 0=denied
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership All parameters: caller retains ownership
 */
int rule_manager_match(rule_manager_t* mgr,
                       const char* agent_id,
                       const char* action,
                       const char* resource,
                       const char* context);

/**
 * @brief Reload rules from configuration file
 * @param[in] mgr Rule manager handle
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other operations)
 * @reentrant No
 */
int rule_manager_reload(rule_manager_t* mgr);

/**
 * @brief Add a new rule to the manager
 * @param[in] mgr Rule manager handle
 * @param[in] agent_id Agent ID (NULL = wildcard)
 * @param[in] action Action name (NULL = wildcard)
 * @param[in] resource Resource pattern (supports glob syntax)
 * @param[in] allow Permission result (1=allow, 0=deny)
 * @param[in] priority Priority level (higher number = higher priority)
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other operations)
 * @reentrant No
 * @ownership All string parameters: caller retains ownership
 */
int rule_manager_add(rule_manager_t* mgr,
                     const char* agent_id,
                     const char* action,
                     const char* resource,
                     int allow,
                     int priority);

/**
 * @brief Clear all rules from the manager
 * @param[in] mgr Rule manager handle
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other operations)
 * @reentrant No
 */
void rule_manager_clear(rule_manager_t* mgr);

/**
 * @brief Get number of rules
 * @param[in] mgr Rule manager handle
 * @return Number of rules in the manager
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 */
size_t rule_manager_count(rule_manager_t* mgr);

#ifdef __cplusplus
}
#endif

#endif /* CUPOLAS_PERMISSION_RULE_H */
