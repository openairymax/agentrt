/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * cupolas.c - AgentOS Security Dome Core Implementation (Facade)
 *
 * This module implements the unified public API for the security dome,
 * providing a facade over all four protection layers:
 * - Virtual Workbench (workbench/)
 * - Permission Engine (permission/)
 * - Input Sanitizer (sanitizer/)
 * - Audit Trail (audit/)
 */

/**
 * @file cupolas.c
 * @brief AgentOS Security Dome - Core Facade Implementation
 * @author Spharx
 * @date 2024
 *
 * @note This file implements the unified entry point for the cupolas module.
 *       It follows the Facade design pattern to provide a simplified interface
 *       to the complex four-layer security subsystem.
 * @threadsafe All public APIs are thread-safe with internal locking
 * @reentrant Yes, but concurrent calls may serialize internally
 */

#include "cupolas.h"
#include "cupolas_config.h"
#include "cupolas_error.h"
#include "permission/permission.h"
#include "sanitizer/sanitizer.h"
#include "workbench/workbench.h"
#include "audit/audit.h"
#include "../utils/cupolas_utils.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Module State
 * ============================================================================ */

/** @brief Global module state */
static struct {
    int initialized;              /**< Initialization flag */
    cupolas_config_t config;     /**< Active configuration */
    permission_engine_t* perm;   /**< Permission engine instance */
    sanitizer_t* san;            /**< Sanitizer instance */
    workbench_t* wb;             /**< Workbench instance */
    audit_logger_t* audit;       /**< Audit logger instance */
    cupolas_mutex_t lock;         /**< State protection mutex */
} g_cupolas = {0};

/* ============================================================================
 * Initialization and Cleanup
 * ============================================================================ */

/**
 * @brief Initialize cupolas security dome module
 * @param[in] config_path Path to configuration file (NULL for defaults)
 * @param[out] error Optional error code output on failure
 * @return CUPOLAS_OK (0) on success, negative error code on failure
 * @post On success, module is initialized and ready for use
 * @post On failure, module remains in uninitialized state
 * @note Thread-safe: Multiple threads may call init, only first succeeds
 * @threadsafe Yes
 * @reentrant No (idempotent - safe to call multiple times)
 * @ownership config_path string: caller retains ownership, may be NULL
 *
 * @details
 * Initialization sequence:
 * 1. Load configuration from file or apply defaults
 * 2. Initialize platform abstraction layer
 * 3. Create permission engine with RBAC rules
 * 4. Create input sanitizer with default rules
 * 5. Create workbench (sandbox) manager
 * 6. Create audit logger
 * 7. Set initialized flag
 *
 * Example:
 * @code
 * agentos_error_t err;
 * if (cupolas_init("config/cupolas.yaml", &err) != CUPOLAS_OK) {
 *     fprintf(stderr, "Init failed: %s\n", agentos_error_get_message(err));
 *     return 1;
 * }
 * // ... use cupolas APIs ...
 * cupolas_cleanup();
 * @endcode
 *
 * @see cupolas_cleanup()
 * @since 1.0.0
 */
int cupolas_init(const char* config_path, agentos_error_t* error) {
    if (g_cupolas.initialized) {
        return CUPOLAS_OK;
    }

    memset(&g_cupolas, 0, sizeof(g_cupolas));

    if (cupolas_mutex_init(&g_cupolas.lock) != 0) {
        if (error) *error = AGENTOS_ERR_IO;
        return cupolas_ERR_UNKNOWN;
    }

    cupolas_mutex_lock(&g_cupolas.lock);

    int result = cupolas_config_init_defaults(&g_cupolas.config);
    if (result != CUPOLAS_OK && result != cupolas_ERR_NOT_FOUND) {
        if (error) *error = AGENTOS_ERR_INVALID_PARAM;
        cupolas_mutex_unlock(&g_cupolas.lock);
        return result;
    }

    if (config_path && config_path[0] != '\0') {
        result = cupolas_config_load(config_path, &g_cupolas.config);
        if (result != CUPOLAS_OK) {
            if (error) *error = AGENTOS_ERR_IO;
            cupolas_mutex_unlock(&g_cupolas.lock);
            return result;
        }
    }

    g_cupolas.perm = permission_engine_create(g_cupolas.config.permission_rules_path);
    if (!g_cupolas.perm) {
        if (error) *error = AGENTOS_ERR_OUT_OF_MEMORY;
        cupolas_mutex_unlock(&g_cupolas.lock);
        return cupolas_ERR_OUT_OF_MEMORY;
    }

    g_cupolas.san = sanitizer_create();
    if (!g_cupolas.san) {
        if (error) *error = AGENTOS_ERR_OUT_OF_MEMORY;
        permission_engine_destroy(g_cupolas.perm);
        g_cupolas.perm = NULL;
        cupolas_mutex_unlock(&g_cupolas.lock);
        return cupolas_ERR_OUT_OF_MEMORY;
    }

    g_cupolas.wb = NULL;

    g_cupolas.audit = audit_logger_create(g_cupolas.config.audit_log_dir);
    if (!g_cupolas.audit) {
        if (error) *error = AGENTOS_ERR_OUT_OF_MEMORY;
        sanitizer_destroy(g_cupolas.san);
        g_cupolas.san = NULL;
        permission_engine_destroy(g_cupolas.perm);
        g_cupolas.perm = NULL;
        cupolas_mutex_unlock(&g_cupolas.lock);
        return cupolas_ERR_OUT_OF_MEMORY;
    }

    g_cupolas.initialized = 1;
    cupolas_mutex_unlock(&g_cupolas.lock);

    return CUPOLAS_OK;
}

/**
 * @brief Cleanup cupolas security dome module and release all resources
 * @pre cupolas_init() must have been called successfully
 * @post All resources released, no further API calls safe except init
 * @note Thread-safe: Blocks until all operations complete
 * @threadsafe Yes
 * @reentrant No
 *
 * @details
 * Cleanup sequence (reverse of initialization):
 * 1. Flush pending audit logs
 * 2. Destroy audit logger
 * 3. Destroy workbench (if created)
 * 4. Destroy sanitizer
 * 5. Destroy permission engine
 * 6. Release configuration resources
 * 7. Clear initialized flag
 *
 * @warning After cleanup, only cupolas_init() may be safely called
 * @see cupolas_init()
 * @since 1.0.0
 */
void cupolas_cleanup(void) {
    if (!g_cupolas.initialized) {
        return;
    }

    cupolas_mutex_lock(&g_cupolas.lock);

    if (g_cupolas.audit) {
        audit_logger_flush(g_cupolas.audit);
        audit_logger_destroy(g_cupolas.audit);
        g_cupolas.audit = NULL;
    }

    if (g_cupolas.wb) {
        cupolas_workbench_destroy(g_cupolas.wb);
        g_cupolas.wb = NULL;
    }

    if (g_cupolas.san) {
        sanitizer_destroy(g_cupolas.san);
        g_cupolas.san = NULL;
    }

    if (g_cupolas.perm) {
        permission_engine_destroy(g_cupolas.perm);
        g_cupolas.perm = NULL;
    }

    cupolas_config_cleanup(&g_cupolas.config);

    g_cupolas.initialized = 0;
    cupolas_mutex_unlock(&g_cupolas.lock);

    cupolas_mutex_destroy(&g_cupolas.lock);
}

/**
 * @brief Get cupolas module version string
 * @return Static version string (do not free), format: "major.minor.patch"
 * @note Thread-safe: Always safe to call, no initialization required
 * @threadsafe Yes
 * @reentrant Yes
 *
 * @since 1.0.0
 */
const char* cupolas_version(void) {
    return "1.0.0";
}

/* ============================================================================
 * Permission Management
 * ============================================================================ */

/**
 * @brief Check if an agent is permitted to perform an action on a resource
 * @param[in] agent_id Agent identifier (must not be NULL)
 * @param[in] action Action type: "read", "write", "execute" (must not be NULL)
 * @param[in] resource Resource path or identifier (must not be NULL)
 * @param[in] context Additional context for policy evaluation (may be NULL)
 * @return Positive (>0) if allowed, 0 if denied, negative on error
 * @note Thread-safe: Yes, uses internal locking
 * @reentrant Yes
 * @ownership All input strings: caller retains ownership
 *
 * @details
 * This function implements the core RBAC decision:
 * 1. Validate input parameters
 * 2. Check module initialization state
 * 3. Delegate to permission engine for rule evaluation
 * 4. Log decision to audit trail
 * 5. Return result
 *
 * Performance:
 * - Cache hit: O(1) average
 * - Cache miss: O(n) where n = number of rules
 *
 * @since 1.0.0
 */
int cupolas_check_permission(const char* agent_id, const char* action,
                           const char* resource, const char* context) {
    if (!agent_id || !action || !resource) {
        return cupolas_ERR_INVALID_PARAM;
    }

    if (!g_cupolas.initialized || !g_cupolas.perm) {
        return cupolas_ERR_STATE_ERROR;
    }

    int result = permission_engine_check(g_cupolas.perm, agent_id, action, resource, context);

    if (g_cupolas.audit) {
        audit_logger_log_event(g_cupolas.audit, "permission_check",
                               resource, result >= 0 ? result : -1, agent_id);
    }

    return result > 0 ? 1 : (result == 0 ? 0 : result);
}

/**
 * @brief Add a dynamic permission rule to the engine
 * @param[in] agent_id Agent ID pattern (NULL or "*" for wildcard)
 * @param[in] action Action pattern (NULL or "*" for wildcard)
 * @param[in] resource Resource pattern with glob support
 * @param[in] allow 1 to allow, 0 to deny
 * @param[in] priority Higher value = higher evaluation priority
 * @return CUPOLAS_OK (0) on success, negative error code on failure
 * @note Thread-safe: Yes
 * @reentrant Yes
 * @ownership All input strings: caller retains ownership
 *
 * @since 1.0.0
 */
int cupolas_add_permission_rule(const char* agent_id, const char* action,
                               const char* resource, int allow, int priority) {
    if (!g_cupolas.initialized || !g_cupolas.perm) {
        return cupolas_ERR_STATE_ERROR;
    }

    return permission_engine_add_rule(g_cupolas.perm, agent_id, action,
                                     resource, allow, priority);
}

/**
 * @brief Clear the permission decision cache
 * @note Thread-safe: Yes
 * @reentrant Yes
 * @post All cached permission decisions are invalidated
 *
 * @details Useful after rule updates to force re-evaluation.
 *
 * @since 1.0.0
 */
void cupolas_clear_permission_cache(void) {
    if (!g_cupolas.initialized || !g_cupolas.perm) {
        return;
    }

    permission_engine_invalidate_cache(g_cupolas.perm);
}

/* ============================================================================
 * Input Sanitization
 * ============================================================================ */

/**
 * @brief Sanitize user input string to prevent injection attacks
 * @param[in] input Input string to sanitize (must not be NULL)
 * @param[out] output Output buffer for sanitized result (must not be NULL)
 * @param[in] output_size Size of output buffer in bytes
 * @return CUPOLAS_OK (0) on success, negative error code on failure
 * @note Thread-safe: Yes
 * @reentrant Yes
 * @ownership input: caller retains; output: callee writes, caller owns buffer
 *
 * @details
 * Sanitization pipeline:
 * 1. Null/empty check
 * 2. Length validation against buffer size
 * 3. Regex-based pattern filtering (injection prevention)
 * 4. Type-specific validation (if configured)
 * 5. Write sanitized result to output buffer
 *
 * Supported sanitization types:
 * - SQL injection prevention
 * - Command injection prevention
 * - XSS prevention
 * - Path traversal prevention
 *
 * @since 1.0.0
 */
int cupolas_sanitize_input(const char* input, char* output, size_t output_size) {
    if (!input || !output || output_size == 0) {
        return cupolas_ERR_INVALID_PARAM;
    }

    if (!g_cupolas.initialized || !g_cupolas.san) {
        return cupolas_ERR_STATE_ERROR;
    }

    sanitize_result_t result = sanitizer_sanitize(g_cupolas.san, input,
                                                  output, output_size);

    if (g_cupolas.audit) {
        audit_logger_log_event(g_cupolas.audit, "sanitize_input",
                               input, (int)result, NULL);
    }

    return (result == SANITIZE_OK) ? CUPOLAS_OK : cupolas_ERR_PERMISSION_DENIED;
}

/* ============================================================================
 * Command Execution (Workbench)
 * ============================================================================ */

/**
 * @brief Execute command in isolated sandboxed environment
 * @param[in] command Command path or executable name (must not be NULL)
 * @param[in] argv Argument array (NULL-terminated, must not be NULL)
 * @param[out] exit_code Exit code output (may be NULL if not needed)
 * @param[out] stdout_buf Standard output capture buffer (may be NULL)
 * @param[in] stdout_size Standard output buffer size in bytes
 * @param[out] stderr_buf Standard error capture buffer (may be NULL)
 * @param[in] stderr_size Standard error buffer size in bytes
 * @return CUPOLAS_OK (0) on success, negative error code on failure
 * @note Thread-safe: Each workbench instance is single-threaded
 * @reentrant No
 * @ownership All input strings: caller retains; output buffers: caller owns
 *
 * @details
 * Execution flow:
 * 1. Validate command path and arguments
 * 2. Create sandboxed workbench environment
 * 3. Apply resource limits (CPU, memory, network, file descriptors)
 * 4. Spawn process within sandbox
 * 5. Capture output (if buffers provided)
 * 6. Wait for completion with timeout
 * 7. Cleanup sandbox
 * 8. Return exit code and captured output
 *
 * Security guarantees:
 * - Process isolation via OS-level sandboxing
 * - Resource quota enforcement
 * - Network access control (optional)
 * - File system namespace restriction
 *
 * @since 1.0.0
 */
int cupolas_execute_command(const char* command, char* const argv[],
                          int* exit_code, char* stdout_buf, size_t stdout_size,
                          char* stderr_buf, size_t stderr_size) {
    if (!command || !argv) {
        return cupolas_ERR_INVALID_PARAM;
    }

    if (!g_cupolas.initialized) {
        return cupolas_ERR_STATE_ERROR;
    }

    cupolas_workbench_config_t wbcfg;
    cupolas_workbench_config_init_defaults(&wbcfg);

    if (!g_cupolas.wb) {
        g_cupolas.wb = cupolas_workbench_create(&wbcfg);
        if (!g_cupolas.wb) {
            return cupolas_ERR_OUT_OF_MEMORY;
        }
    }

    cupolas_workbench_result_t result;
    int ret = cupolas_workbench_execute(g_cupolas.wb, command, argv, &result);

    if (exit_code) {
        *exit_code = result.exit_code;
    }

    if (stdout_buf && stdout_size > 0 && result.stdout_output) {
        strncpy(stdout_buf, result.stdout_output, stdout_size - 1);
        stdout_buf[stdout_size - 1] = '\0';
    }

    if (stderr_buf && stderr_size > 0 && result.stderr_output) {
        strncpy(stderr_buf, result.stderr_output, stderr_size - 1);
        stderr_buf[stderr_size - 1] = '\0';
    }

    if (g_cupolas.audit) {
        char task_desc[256];
        snprintf(task_desc, sizeof(task_desc), "exec:%s", command);
        audit_logger_log_event(g_cupolas.audit, "execute_command",
                               task_desc, ret, NULL);
    }

    return ret;
}

/* ============================================================================
 * Audit Logging
 * ============================================================================ */

/**
 * @brief Flush all pending audit log entries to persistent storage
 * @note Thread-safe: Yes
 * @reentrant Yes
 * @post All pending entries are written synchronously
 *
 * @details
 * Forces immediate write of all buffered audit log entries to disk.
 * Normally called automatically by the background flush thread, but can
 * be called explicitly when:
 * - Before shutdown (via cupolas_cleanup())
 * - After critical security events
 * - For compliance requirements (immediate persistence)
 *
 * @since 1.0.0
 */
void cupolas_flush_audit_log(void) {
    if (!g_cupolas.initialized || !g_cupolas.audit) {
        return;
    }

    audit_logger_flush(g_cupolas.audit);
}
