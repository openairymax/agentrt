/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * workbench_limits.h - Resource Limits Runtime Enforcement: Cross-platform Implementation
 */

#ifndef CUPOLAS_WORKBENCH_LIMITS_H
#define CUPOLAS_WORKBENCH_LIMITS_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Resource limit types
 * 
 * Design principles:
 * - Cross-platform support: Windows Job Objects / Linux cgroups / macOS Mach ports
 * - Runtime enforcement: Beyond configuration-level restrictions
 * - Progressive limits: Support for soft and hard limits
 * - Observability: Audit logging when limits are triggered
 * 
 * Platform implementations:
 * - Linux: cgroups v2 (memory, cpu, pids)
 * - Windows: Job Objects with CPU/Memory limits
 * - macOS: Mach task with resource limits
 */
typedef enum limit_type {
    LIMIT_TYPE_MEMORY = 0,              /**< Memory usage limit */
    LIMIT_TYPE_CPU_TIME,                /**< CPU time limit */
    LIMIT_TYPE_CPU_WEIGHT,              /**< CPU weight/priority */
    LIMIT_TYPE_PROCESSES,               /**< Maximum number of processes */
    LIMIT_TYPE_THREADS,                 /**< Maximum number of threads */
    LIMIT_TYPE_FILE_SIZE,               /**< Maximum file size */
    LIMIT_TYPE_FILE_DESCRIPTORS,        /**< Maximum open file descriptors */
    LIMIT_TYPE_NETWORK_BANDWIDTH,       /**< Network bandwidth limit */
    LIMIT_TYPE_IO_BANDWIDTH             /**< I/O bandwidth limit */
} limit_type_t;

/**
 * @brief Limit enforcement modes
 */
typedef enum limit_mode {
    LIMIT_MODE_SOFT = 0,                /**< Soft limit (warning only) */
    LIMIT_MODE_HARD,                    /**< Hard limit (prevent allocation) */
    LIMIT_MODE_ENFORCED                 /**< Enforced limit (kill process) */
} limit_mode_t;

/**
 * @brief Limit status values
 */
typedef enum limit_status {
    LIMIT_STATUS_OK = 0,                /**< Within limits */
    LIMIT_STATUS_SOFT_EXCEEDED,         /**< Soft limit exceeded */
    LIMIT_STATUS_HARD_EXCEEDED,         /**< Hard limit exceeded */
    LIMIT_STATUS_KILLED                 /**< Process killed due to limit */
} limit_status_t;

/**
 * @brief Resource statistics structure
 */
typedef struct resource_stats {
    size_t memory_current;              /**< Current memory usage in bytes */
    size_t memory_peak;                 /**< Peak memory usage in bytes */
    size_t memory_limit;                /**< Memory limit in bytes */

    uint64_t cpu_time_ns;               /**< CPU time consumed in nanoseconds */
    uint32_t cpu_weight;                /**< CPU weight (1-10000) */

    uint32_t processes_current;         /**< Current number of processes */
    uint32_t processes_limit;           /**< Process limit */

    uint32_t threads_current;           /**< Current number of threads */
    uint32_t threads_limit;             /**< Thread limit */

    size_t file_size_current;           /**< Current file size in bytes */
    size_t file_size_limit;             /**< File size limit in bytes */

    uint32_t file_descriptors_current;  /**< Current open file descriptors */
    uint32_t file_descriptors_limit;    /**< File descriptor limit */
} resource_stats_t;

/**
 * @brief Limit context structure (opaque)
 */
typedef struct limit_context limit_context_t;

/**
 * @brief Create resource limit context
 * @param[in] memory_limit_bytes Memory limit in bytes (0 = unlimited)
 * @param[in] cpu_time_limit_ms CPU time limit in milliseconds (0 = unlimited)
 * @param[in] processes_limit Maximum number of processes (0 = unlimited)
 * @return Limit context handle, NULL on failure
 * @note Thread-safe: Safe to call from multiple threads (initialization only)
 * @reentrant No
 * @ownership Returns owned pointer: caller must call limits_destroy()
 */
limit_context_t* limits_create(size_t memory_limit_bytes,
                               uint32_t cpu_time_limit_ms,
                               uint32_t processes_limit);

/**
 * @brief Destroy resource limit context
 * @param[in] ctx Limit context handle (may be NULL)
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other operations)
 * @reentrant No
 * @ownership ctx: transferred to this function, will be freed
 */
void limits_destroy(limit_context_t* ctx);

/**
 * @brief Attach current process/thread to limit context
 * @param[in] ctx Limit context handle
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other operations)
 * @reentrant No
 */
int limits_attach(limit_context_t* ctx);

/**
 * @brief Detach from limit context
 * @param[in] ctx Limit context handle
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other operations)
 * @reentrant No
 */
void limits_detach(limit_context_t* ctx);

/**
 * @brief Set memory limit
 * @param[in] ctx Limit context handle
 * @param[in] limit_bytes Memory limit in bytes
 * @param[in] mode Limit enforcement mode
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other operations)
 * @reentrant No
 */
int limits_set_memory(limit_context_t* ctx, size_t limit_bytes, limit_mode_t mode);

/**
 * @brief Set CPU time limit
 * @param[in] ctx Limit context handle
 * @param[in] limit_ms CPU time limit in milliseconds
 * @param[in] mode Limit enforcement mode
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other operations)
 * @reentrant No
 */
int limits_set_cpu_time(limit_context_t* ctx, uint32_t limit_ms, limit_mode_t mode);

/**
 * @brief Set CPU weight
 * @param[in] ctx Limit context handle
 * @param[in] weight CPU weight (1-10000)
 * @param[in] mode Limit enforcement mode
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other operations)
 * @reentrant No
 */
int limits_set_cpu_weight(limit_context_t* ctx, uint32_t weight, limit_mode_t mode);

/**
 * @brief Set process limit
 * @param[in] ctx Limit context handle
 * @param[in] limit Maximum number of processes
 * @param[in] mode Limit enforcement mode
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other operations)
 * @reentrant No
 */
int limits_set_processes(limit_context_t* ctx, uint32_t limit, limit_mode_t mode);

/**
 * @brief Set thread limit
 * @param[in] ctx Limit context handle
 * @param[in] limit Maximum number of threads
 * @param[in] mode Limit enforcement mode
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other operations)
 * @reentrant No
 */
int limits_set_threads(limit_context_t* ctx, uint32_t limit, limit_mode_t mode);

/**
 * @brief Set file size limit
 * @param[in] ctx Limit context handle
 * @param[in] limit_bytes File size limit in bytes
 * @param[in] mode Limit enforcement mode
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other operations)
 * @reentrant No
 */
int limits_set_file_size(limit_context_t* ctx, size_t limit_bytes, limit_mode_t mode);

/**
 * @brief Set file descriptor limit
 * @param[in] ctx Limit context handle
 * @param[in] limit Maximum number of file descriptors
 * @param[in] mode Limit enforcement mode
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other operations)
 * @reentrant No
 */
int limits_set_file_descriptors(limit_context_t* ctx, uint32_t limit, limit_mode_t mode);

/**
 * @brief Get resource statistics
 * @param[in] ctx Limit context handle
 * @param[out] stats Resource statistics structure
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership stats: caller provides buffer, function writes to it
 */
int limits_get_stats(limit_context_t* ctx, resource_stats_t* stats);

/**
 * @brief Check if limit exceeded
 * @param[in] ctx Limit context handle
 * @param[in] type Resource type to check
 * @param[out] status Limit status output
 * @return true if limit exceeded, false otherwise
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership status: caller provides buffer, function writes to it
 */
bool limits_check(limit_context_t* ctx, limit_type_t type, limit_status_t* status);

/**
 * @brief Enforce limits (kill exceeding processes)
 * @param[in] ctx Limit context handle
 * @return Number of processes killed
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other operations)
 * @reentrant No
 */
int limits_enforce(limit_context_t* ctx);

/**
 * @brief Get limit status string
 * @param[in] status Limit status value
 * @return Status description string (static, do not free)
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 */
const char* limits_status_string(limit_status_t status);

/**
 * @brief Callback type for limit exceeded events
 * @param[in] type Resource type that exceeded limit
 * @param[in] status Limit status
 * @param[in] user_data User-provided data
 */
typedef void (*limits_exceeded_callback_t)(limit_type_t type, limit_status_t status,
                                           void* user_data);

/**
 * @brief Set limit exceeded callback
 * @param[in] ctx Limit context handle
 * @param[in] callback Callback function
 * @param[in] user_data User data passed to callback
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other operations)
 * @reentrant No
 */
void limits_set_exceeded_callback(limit_context_t* ctx,
                                  limits_exceeded_callback_t callback,
                                  void* user_data);

/**
 * @brief Check if resource limits are available on this platform
 * @return true if available, false otherwise
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 */
bool limits_is_available(void);

#ifdef __cplusplus
}
#endif

#endif /* CUPOLAS_WORKBENCH_LIMITS_H */
