/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * workbench.h - Virtual Workbench Public Interface: Isolated Execution Environment
 *
 * Design Principles:
 * - Isolated Execution: Each Agent runs in its own workbench
 * - Resource Control: CPU time, memory, process limits
 * - Security Boundary: File system, network isolation
 * - Observable: Monitor workbench status and resource usage
 *
 * Resource Control Notes:
 * - Linux: Uses cgroups v2
 * - Windows: Uses Job Objects
 * - macOS: Uses Mach ports for resource limits
 */

#ifndef CUPOLAS_WORKBENCH_H
#define CUPOLAS_WORKBENCH_H

#include "../platform/platform.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Workbench State */
typedef enum workbench_state {
    WORKBENCH_STATE_IDLE = 0,
    WORKBENCH_STATE_RUNNING,
    WORKBENCH_STATE_STOPPED,
    WORKBENCH_STATE_ERROR
} workbench_state_t;

/* Resource Limits Configuration */
typedef struct workbench_limits {
    size_t max_memory_bytes;       /* Max memory limit in bytes, 0 = unlimited */
    uint32_t max_cpu_time_ms;      /* Max CPU time in milliseconds, 0 = unlimited */
    size_t max_output_bytes;       /* Max output size in bytes, 0 = use default */
    uint32_t max_processes;        /* Max child processes, 0 = unlimited */
    uint32_t max_threads;          /* Max threads, 0 = unlimited */
    size_t max_file_size_bytes;    /* Max file size in bytes, 0 = unlimited */
} workbench_limits_t;

/* Workbench Configuration */
typedef struct workbench_config {
    const char* working_dir;
    const char** env_vars;
    size_t env_count;
    uint32_t timeout_ms;
    size_t max_output_size;
    bool redirect_stdin;
    bool redirect_stdout;
    bool redirect_stderr;
    workbench_limits_t limits;     /* Resource limits */
    bool enable_limits;           /* Enable resource control */
} workbench_config_t;

/* Workbench Execution Result */
typedef struct workbench_result {
    int exit_code;
    bool timed_out;
    bool signaled;
    int signal;
    char* stdout_data;
    size_t stdout_size;
    char* stderr_data;
    size_t stderr_size;
    uint64_t start_time_ms;
    uint64_t end_time_ms;
} workbench_result_t;

/* Workbench Handle */
typedef struct workbench workbench_t;

/**
 * @brief Create workbench
 * @param[in] config Workbench configuration (NULL for default config)
 * @return Workbench handle, NULL on failure
 * @post On success, caller owns the returned handle
 * @note Thread-safe: No, each workbench is single-threaded
 * @reentrant No (create/destroy must be paired)
 * @ownership Returned handle: caller owns, must call workbench_destroy
 */
workbench_t* workbench_create(const workbench_config_t* config);

/**
 * @brief Destroy workbench
 * @param[in] wb Workbench handle (must not be NULL)
 * @pre Handle was created by workbench_create
 * @post All resources are released, child processes are terminated
 * @note Thread-safe: No
 * @reentrant No
 * @ownership wb: caller transfers ownership
 */
void workbench_destroy(workbench_t* wb);

/**
 * @brief Execute command synchronously
 * @param[in] wb Workbench handle (must not be NULL)
 * @param[in] command Command to execute (must not be NULL)
 * @param[in] argv Argument array (NULL-terminated, must not be NULL)
 * @param[out] result Execution result (must not be NULL)
 * @return 0 on success, negative on failure
 * @note Thread-safe: No, each workbench instance is single-threaded
 * @reentrant No
 * @ownership command, argv: caller retains; result: callee allocates, caller must call workbench_result_free
 */
int workbench_execute(workbench_t* wb, const char* command, char* const argv[],
                      workbench_result_t* result);

/**
 * @brief Execute command asynchronously
 * @param[in] wb Workbench handle (must not be NULL)
 * @param[in] command Command to execute (must not be NULL)
 * @param[in] argv Argument array (NULL-terminated, must not be NULL)
 * @return 0 on success, negative on failure
 * @note Thread-safe: No
 * @reentrant No
 * @ownership command, argv: caller retains ownership
 */
int workbench_execute_async(workbench_t* wb, const char* command, char* const argv[]);

/**
 * @brief Wait for async execution to complete
 * @param[in] wb Workbench handle (must not be NULL)
 * @param[out] result Execution result (must not be NULL)
 * @param[in] timeout_ms Timeout in milliseconds (0 for infinite wait)
 * @return 0 on success, cupolas_ERROR_TIMEOUT on timeout, negative on other failure
 * @note Thread-safe: No
 * @reentrant No
 * @ownership result: callee allocates, caller must call workbench_result_free
 */
int workbench_wait(workbench_t* wb, workbench_result_t* result, uint32_t timeout_ms);

/**
 * @brief Terminate execution
 * @param[in] wb Workbench handle (must not be NULL)
 * @return 0 on success, negative on failure
 * @note Thread-safe: No
 * @reentrant No
 */
int workbench_terminate(workbench_t* wb);

/**
 * @brief Get workbench state
 * @param[in] wb Workbench handle (must not be NULL)
 * @return Workbench state
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
workbench_state_t workbench_get_state(workbench_t* wb);

/**
 * @brief Get process ID
 * @param[in] wb Workbench handle (must not be NULL)
 * @return Process ID, -1 on failure
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
int64_t workbench_get_pid(workbench_t* wb);

/**
 * @brief Write to stdin
 * @param[in] wb Workbench handle (must not be NULL)
 * @param[in] data Data to write (must not be NULL)
 * @param[in] size Data size in bytes
 * @param[out] written Bytes actually written (may be NULL)
 * @return 0 on success, negative on failure
 * @note Thread-safe: No
 * @reentrant No
 * @ownership data: caller retains ownership; written: callee writes, caller owns
 */
int workbench_write_stdin(workbench_t* wb, const void* data, size_t size, size_t* written);

/**
 * @brief Read from stdout
 * @param[in] wb Workbench handle (must not be NULL)
 * @param[out] buf Buffer to read into (must not be NULL)
 * @param[in] size Buffer size in bytes
 * @param[out] read_size Actual bytes read (may be NULL)
 * @return 0 on success, negative on failure
 * @note Thread-safe: No
 * @reentrant No
 * @ownership buf: caller owns; read_size: callee writes, caller owns
 */
int workbench_read_stdout(workbench_t* wb, void* buf, size_t size, size_t* read_size);

/**
 * @brief Read from stderr
 * @param[in] wb Workbench handle (must not be NULL)
 * @param[out] buf Buffer to read into (must not be NULL)
 * @param[in] size Buffer size in bytes
 * @param[out] read_size Actual bytes read (may be NULL)
 * @return 0 on success, negative on failure
 * @note Thread-safe: No
 * @reentrant No
 * @ownership buf: caller owns; read_size: callee writes, caller owns
 */
int workbench_read_stderr(workbench_t* wb, void* buf, size_t size, size_t* read_size);

/**
 * @brief Free execution result
 * @param[in] result Execution result to free (must not be NULL)
 * @post All dynamically allocated memory in result is freed
 * @note Thread-safe: N/A (result is not shared)
 * @reentrant No
 * @ownership result: caller transfers ownership
 */
void workbench_result_free(workbench_result_t* result);

/**
 * @brief Get default workbench configuration
 * @param[out] config Configuration output (must not be NULL)
 * @note Thread-safe: Yes
 * @reentrant Yes
 * @ownership config: callee writes, caller owns
 */
void workbench_default_config(workbench_config_t* config);

/**
 * @brief Set resource limits
 * @param[in] wb Workbench handle (must not be NULL)
 * @param[in] limits Resource limits (NULL to disable limits)
 * @return 0 on success, negative on failure
 * @note Thread-safe: Yes (but should not change limits while executing)
 * @reentrant Yes
 * @ownership limits: caller retains ownership
 */
int workbench_set_limits(workbench_t* wb, const workbench_limits_t* limits);

/**
 * @brief Get resource limits
 * @param[in] wb Workbench handle (must not be NULL)
 * @param[out] limits Resource limits output (must not be NULL)
 * @return 0 on success, negative on failure
 * @note Thread-safe: Yes
 * @reentrant Yes
 * @ownership limits: callee writes, caller owns
 */
int workbench_get_limits(workbench_t* wb, workbench_limits_t* limits);

/**
 * @brief Get current resource usage
 * @param[in] wb Workbench handle (must not be NULL)
 * @param[out] memory_usage Memory usage in bytes output (may be NULL)
 * @param[out] cpu_usage CPU time in milliseconds output (may be NULL)
 * @return 0 on success, negative on failure
 * @note Thread-safe: Yes
 * @reentrant Yes
 * @ownership memory_usage, cpu_usage: callee writes, caller owns
 */
int workbench_get_usage(workbench_t* wb, size_t* memory_usage, uint64_t* cpu_usage);

#ifdef __cplusplus
}
#endif

#endif /* CUPOLAS_WORKBENCH_H */
