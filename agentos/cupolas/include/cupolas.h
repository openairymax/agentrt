/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * cupolas.h - AgentOS Security Dome Public Interface
 *
 * cupolas is AgentOS's security dome module providing:
 * - Permission裁决引擎 (Permission Engine): Rule-based access control
 * - 输入净化器 (Sanitizer): Injection attack prevention
 * - 审计日志 (Audit): Operation tracking and compliance
 * - 虚拟工位 (Workbench): Isolated execution environment
 *
 * Design Principles:
 * - Security by Default: Every Agent operates within the dome with least privilege
 * - High Performance: Caching + Async writes
 * - Cross-Platform: Windows/Linux/macOS
 *
 * Error Handling:
 * - All functions return agentos_error_t error codes
 * - Success returns AGENTOS_OK (0)
 * - Error codes are defined in agentos/atoms/corekern/include/error.h
 *
 * @note For backward compatibility, cupolas_ERROR_* aliases are preserved
 */

#ifndef CUPOLAS_H
#define CUPOLAS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Unified Error Codes (Following AgentOS Standard)
 * ============================================================================ */

typedef int agentos_error_t;

#define AGENTOS_OK                     0
#define AGENTOS_ERR_UNKNOWN           -1
#define AGENTOS_ERR_INVALID_PARAM     -2
#define AGENTOS_ERR_NULL_POINTER      -3
#define AGENTOS_ERR_OUT_OF_MEMORY     -4
#define AGENTOS_ERR_BUFFER_TOO_SMALL  -5
#define AGENTOS_ERR_NOT_FOUND         -6
#define AGENTOS_ERR_ALREADY_EXISTS    -7
#define AGENTOS_ERR_TIMEOUT           -8
#define AGENTOS_ERR_NOT_SUPPORTED     -9
#define AGENTOS_ERR_PERMISSION_DENIED -10
#define AGENTOS_ERR_IO               -11
#define AGENTOS_ERR_STATE_ERROR      -13
#define AGENTOS_ERR_OVERFLOW         -14

/* Backward Compatibility Aliases */
#define cupolas_OK                    AGENTOS_OK
#define cupolas_ERROR_UNKNOWN         AGENTOS_ERR_UNKNOWN
#define cupolas_ERROR_INVALID_ARG     AGENTOS_ERR_INVALID_PARAM
#define cupolas_ERROR_NO_MEMORY       AGENTOS_ERR_OUT_OF_MEMORY
#define cupolas_ERROR_NOT_FOUND       AGENTOS_ERR_NOT_FOUND
#define cupolas_ERROR_PERMISSION      AGENTOS_ERR_PERMISSION_DENIED
#define cupolas_ERROR_BUSY            AGENTOS_ERR_STATE_ERROR
#define cupolas_ERROR_TIMEOUT         AGENTOS_ERR_TIMEOUT
#define cupolas_ERROR_WOULD_BLOCK     AGENTOS_ERR_STATE_ERROR
#define cupolas_ERROR_OVERFLOW        AGENTOS_ERR_OVERFLOW
#define cupolas_ERROR_NOT_SUPPORTED   AGENTOS_ERR_NOT_SUPPORTED
#define cupolas_ERROR_IO             AGENTOS_ERR_IO

/* ============================================================================
 * Initialization and Cleanup
 * ============================================================================ */

/**
 * @brief Initialize cupolas module
 * @param[in] config_path Configuration file path (NULL for default config)
 * @param[out] error Optional error code output
 * @return 0 on success, negative on failure
 * @post On success, module is initialized and ready
 * @note Thread-safe: Multiple threads may call init, only first succeeds
 * @ownership config_path string: caller retains ownership, may be NULL
 */
int cupolas_init(const char* config_path, agentos_error_t* error);

/**
 * @brief Cleanup cupolas module
 * @pre cupolas_init() must have been called
 * @post All resources are released, no further API calls safe except init
 * @note Thread-safe: Blocks until all operations complete
 * @reentrant No
 */
void cupolas_cleanup(void);

/**
 * @brief Get version string
 * @return Version string (static, do not free)
 * @note Thread-safe: Always safe to call
 * @reentrant Yes
 */
const char* cupolas_version(void);

/* ============================================================================
 * Permission Management
 * ============================================================================ */

/**
 * @brief Check permission for an agent action on a resource
 * @param[in] agent_id Agent identifier (must not be NULL)
 * @param[in] action Action type: "read", "write", "execute" (must not be NULL)
 * @param[in] resource Resource path (must not be NULL)
 * @param[in] context Optional context information (may be NULL)
 * @return 1 allowed, 0 denied, negative on error
 * @note Thread-safe: Yes, uses internal locking
 * @reentrant Yes, but same agent_id/action/resource from different threads may race
 * @ownership All input strings: caller retains ownership
 */
int cupolas_check_permission(const char* agent_id, const char* action,
                           const char* resource, const char* context);

/**
 * @brief Add a permission rule
 * @param[in] agent_id Agent ID pattern (NULL or "*" for wildcard)
 * @param[in] action Action pattern (NULL or "*" for wildcard)
 * @param[in] resource Resource pattern with glob support
 * @param[in] allow 1 to allow, 0 to deny
 * @param[in] priority Higher value = higher priority
 * @return 0 on success, negative on failure
 * @note Thread-safe: Yes
 * @reentrant Yes
 * @ownership All input strings: caller retains ownership
 */
int cupolas_add_permission_rule(const char* agent_id, const char* action,
                               const char* resource, int allow, int priority);

/**
 * @brief Clear permission cache
 * @note Thread-safe: Yes
 * @reentrant Yes
 * @post All cached permissions are invalidated
 */
void cupolas_clear_permission_cache(void);

/* ============================================================================
 * Input Sanitization
 * ============================================================================ */

/**
 * @brief Sanitize input string
 * @param[in] input Input string to sanitize (must not be NULL)
 * @param[out] output Output buffer (must not be NULL)
 * @param[in] output_size Size of output buffer in bytes
 * @return 0 on success, negative on failure
 * @note Thread-safe: Yes
 * @reentrant Yes
 * @ownership input: caller retains; output: callee writes, caller owns buffer
 */
int cupolas_sanitize_input(const char* input, char* output, size_t output_size);

/* ============================================================================
 * Command Execution
 * ============================================================================ */

/**
 * @brief Execute command in isolated workbench
 * @param[in] command Command path (must not be NULL)
 * @param[in] argv Argument array (NULL-terminated, must not be NULL)
 * @param[out] exit_code Exit code output (may be NULL)
 * @param[out] stdout_buf Standard output buffer (may be NULL)
 * @param[in] stdout_size Standard output buffer size
 * @param[out] stderr_buf Standard error buffer (may be NULL)
 * @param[in] stderr_size Standard error buffer size
 * @return 0 on success, negative on failure
 * @note Thread-safe: No, each workbench instance is single-threaded
 * @reentrant No
 * @ownership All input strings: caller retains; output buffers: caller owns
 */
int cupolas_execute_command(const char* command, char* const argv[],
                          int* exit_code, char* stdout_buf, size_t stdout_size,
                          char* stderr_buf, size_t stderr_size);

/* ============================================================================
 * Audit Logging
 * ============================================================================ */

/**
 * @brief Flush audit log
 * @note Thread-safe: Yes
 * @reentrant Yes
 * @post All pending audit entries are written to storage
 */
void cupolas_flush_audit_log(void);

/* ============================================================================
 * iOS-Level Security Modules
 * ============================================================================ */

/* Security sub-module headers are located in src/security/ directory */
/* Users should include specific headers:
 * #include "cupolas_signature.h"
 * #include "cupolas_vault.h"
 * #include "cupolas_entitlements.h"
 * #include "cupolas_runtime_protection.h"
 * #include "cupolas_network_security.h"
 */

/* ============================================================================
 * SafetyGuard Framework
 * ============================================================================ */

/* SafetyGuard headers are located in src/guards/ directory */
/* Users should include specific headers:
 * #include "guard_core.h"
 * #include "guard_integration.h"
 * #include "guard_rules.h"      (future)
 * #include "guard_models.h"     (future)
 */

#ifdef __cplusplus
}
#endif

#endif /* CUPOLAS_H */
