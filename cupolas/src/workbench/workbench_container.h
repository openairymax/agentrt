/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * workbench_container.h - Container Runtime Implementation: Docker/runc-based Isolated Execution
 */

#ifndef CUPOLAS_WORKBENCH_CONTAINER_H
#define CUPOLAS_WORKBENCH_CONTAINER_H

#include "workbench.h"
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Container runtime types
 * 
 * Design principles:
 * - Isolation via containerization (Docker/runc)
 * - Resource control through cgroups
 * - Security with seccomp and capabilities
 * - Filesystem isolation with overlay filesystems
 * 
 * Supported runtime options:
 * - Docker: High-level container runtime
 * - runc: OCI-compliant low-level runtime
 * - crun: Lightweight C container runtime
 */
typedef enum container_runtime {
    CONTAINER_RUNTIME_DOCKER = 0,   /**< Docker daemon-based runtime */
    CONTAINER_RUNTIME_RUNC,         /**< OCI runc runtime */
    CONTAINER_RUNTIME_CRUN,         /**< Lightweight crun runtime */
    CONTAINER_RUNTIME_AUTO          /**< Auto-detect available runtime */
} container_runtime_t;

/**
 * @brief Container configuration structure
 * 
 * Provides comprehensive container setup with:
 * - Image and command configuration
 * - Resource limits (memory, CPU, PIDs)
 * - Logging configuration
 * - Image pull policies
 */
typedef struct container_config {
    const char* image;                  /**< Container image name */
    const char* command;                /**< Command to execute */
    const char** args;                  /**< Command arguments */
    size_t args_count;                  /**< Number of arguments */

    const char* workdir;                /**< Working directory */
    const char** env_vars;              /**< Environment variables */
    size_t env_count;                   /**< Number of environment variables */

    container_runtime_t runtime;        /**< Container runtime to use */

    struct {
        const char* network_mode;       /**< Network mode: bridge, none, host */
        bool readonly_rootfs;           /**< Read-only root filesystem */
        const char* user;               /**< User/group ID */
        size_t memory_limit;            /**< Memory limit in bytes */
        int cpu_shares;                 /**< CPU weight/shares */
        int cpu_quota;                  /**< CPU quota in microseconds */
        int pids_limit;                 /**< Maximum number of PIDs */
    } resources;

    struct {
        bool enable_logging;            /**< Enable container logging */
        const char* log_driver;         /**< Log driver: json-file, syslog */
        size_t log_max_size;            /**< Max log file size in bytes */
        int log_max_files;              /**< Maximum number of log files */
    } logging;

    struct {
        bool use_cache;                 /**< Use image cache */
        bool pull_latest;               /**< Always pull latest image */
        const char* registry_auth;      /**< Registry auth config path */
    } image_policy;
} container_config_t;

/**
 * @brief Container state enumeration
 */
typedef enum container_state {
    CONTAINER_STATE_CREATED = 0,        /**< Container created but not running */
    CONTAINER_STATE_RUNNING,            /**< Container is running */
    CONTAINER_STATE_PAUSED,             /**< Container is paused */
    CONTAINER_STATE_STOPPED,            /**< Container is stopped */
    CONTAINER_STATE_RESTARTING,         /**< Container is restarting */
    CONTAINER_STATE_DEAD,               /**< Container is dead */
    CONTAINER_STATE_UNKNOWN             /**< Container state unknown */
} container_state_t;

/**
 * @brief Container information structure
 */
typedef struct container_info {
    char container_id[64];              /**< Container ID (64 characters) */
    char name[256];                     /**< Container name */
    container_state_t state;            /**< Container state */
    int exit_code;                      /**< Exit code */
    uint64_t exit_time;                 /**< Exit timestamp */

    struct {
        size_t memory_usage;            /**< Current memory usage in bytes */
        size_t memory_limit;            /**< Memory limit in bytes */
        uint64_t cpu_usage;             /**< CPU usage in nanoseconds */
        uint64_t pids_current;          /**< Current number of PIDs */
    } stats;

    struct {
        uint64_t started_at;            /**< Start timestamp */
        uint64_t finished_at;           /**< Finish timestamp */
        size_t rx_bytes;                /**< Bytes received */
        size_t tx_bytes;                /**< Bytes transmitted */
    } metrics;
} container_info_t;

/**
 * @brief Container execution result
 */
typedef struct container_result {
    int exit_code;                      /**< Process exit code */
    char* stdout_data;                  /**< Standard output data */
    size_t stdout_size;                 /**< Standard output size */
    char* stderr_data;                  /**< Standard error data */
    size_t stderr_size;                 /**< Standard error size */
    uint64_t duration_ns;               /**< Execution duration in nanoseconds */
    int oom_killed;                     /**< Whether killed by OOM */
} container_result_t;

/**
 * @brief Create container manager
 * @param[in] config Container configuration (NULL for default config)
 * @return Container manager handle, NULL on failure
 * @note Thread-safe: Safe to call from multiple threads (initialization only)
 * @reentrant No
 * @ownership Returns owned pointer: caller must call container_manager_destroy()
 * @ownership config: caller retains ownership
 */
void* container_manager_create(const container_config_t* config);

/**
 * @brief Destroy container manager
 * @param[in] mgr Container manager handle (may be NULL)
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other operations)
 * @reentrant No
 * @ownership mgr: transferred to this function, will be freed
 */
void container_manager_destroy(void* mgr);

/**
 * @brief Pull container image
 * @param[in] mgr Container manager handle
 * @param[in] image Image name to pull
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other operations)
 * @reentrant No
 * @ownership image: caller retains ownership
 */
int container_pull_image(void* mgr, const char* image);

/**
 * @brief Start container
 * @param[in] mgr Container manager handle
 * @param[in] name Container name (NULL for auto-generated)
 * @param[out] result Execution result (may be NULL)
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other operations)
 * @reentrant No
 * @ownership name: caller retains ownership
 * @ownership result: caller provides buffer, function writes to it
 */
int container_start(void* mgr, const char* name, container_result_t* result);

/**
 * @brief Stop container
 * @param[in] mgr Container manager handle
 * @param[in] timeout_ms Stop timeout in milliseconds
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other operations)
 * @reentrant No
 */
int container_stop(void* mgr, uint32_t timeout_ms);

/**
 * @brief Remove container
 * @param[in] mgr Container manager handle
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other operations)
 * @reentrant No
 */
int container_remove(void* mgr);

/**
 * @brief Get container information
 * @param[in] mgr Container manager handle
 * @param[out] info Container info structure
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership info: caller provides buffer, function writes to it
 */
int container_get_info(void* mgr, container_info_t* info);

/**
 * @brief Get container resource statistics
 * @param[in] mgr Container manager handle
 * @param[out] info Container stats structure
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership info: caller provides buffer, function writes to it
 */
int container_get_stats(void* mgr, container_info_t* info);

/**
 * @brief Pause container
 * @param[in] mgr Container manager handle
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other operations)
 * @reentrant No
 */
int container_pause(void* mgr);

/**
 * @brief Unpause container
 * @param[in] mgr Container manager handle
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other operations)
 * @reentrant No
 */
int container_unpause(void* mgr);

/**
 * @brief Wait for container to finish
 * @param[in] mgr Container manager handle
 * @param[in] timeout_ms Timeout in milliseconds (0 = infinite wait)
 * @param[out] exit_code Exit code output (may be NULL)
 * @return 0 on success, CUPOLAS_ERROR_TIMEOUT on timeout, negative on failure
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant No
 * @ownership exit_code: caller provides buffer, function writes to it
 */
int container_wait(void* mgr, uint32_t timeout_ms, int* exit_code);

/**
 * @brief Execute command in running container
 * @param[in] mgr Container manager handle
 * @param[in] command Command to execute
 * @param[in] args Command arguments
 * @param[in] arg_count Number of arguments
 * @param[out] result Execution result
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other operations)
 * @reentrant No
 * @ownership command and args: caller retains ownership
 * @ownership result: caller provides buffer, function writes to it
 */
int container_exec(void* mgr, const char* command, const char** args,
                  size_t arg_count, container_result_t* result);

/**
 * @brief Get container logs
 * @param[in] mgr Container manager handle
 * @param[in] tail Number of lines (0 = all lines)
 * @param[out] output Output buffer
 * @param[in] size Output buffer size
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership output: caller provides buffer, function writes to it
 */
int container_get_logs(void* mgr, size_t tail, char* output, size_t size);

/**
 * @brief Check if container runtime is available
 * @param[in] runtime Runtime type to check
 * @return true if available, false otherwise
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 */
bool container_runtime_is_available(container_runtime_t runtime);

/**
 * @brief Initialize container configuration with defaults
 * @param[out] config Container configuration to initialize
 * @note Thread-safe: Safe to call from multiple threads
 * @reentrant Yes
 * @ownership config: caller provides buffer, function writes to it
 */
void container_config_init(container_config_t* config);

/**
 * @brief Free container result memory
 * @param[in] result Container result to free (may be NULL)
 * @note Thread-safe: Safe to call from multiple threads
 * @reentrant No
 * @ownership result: transferred to this function, will be freed
 */
void container_result_free(container_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* CUPOLAS_WORKBENCH_CONTAINER_H */
