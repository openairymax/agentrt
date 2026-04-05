/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * workbench_process.h - Process Management Internal Interface
 */

#ifndef CUPOLAS_WORKBENCH_PROCESS_H
#define CUPOLAS_WORKBENCH_PROCESS_H

#include "../platform/platform.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Process attributes structure
 * 
 * Design principles:
 * - Cross-platform process creation
 * - Standard I/O redirection support
 * - Environment and working directory control
 */
typedef struct cupolas_process_attr {
    const char* working_dir;            /**< Working directory (NULL = inherit) */
    const char** env;                   /**< Environment variables (NULL = inherit) */
    bool redirect_stdin;                /**< Redirect standard input */
    bool redirect_stdout;               /**< Redirect standard output */
    bool redirect_stderr;               /**< Redirect standard error */
    cupolas_pipe_t stdin_pipe;          /**< Standard input pipe */
    cupolas_pipe_t stdout_pipe;         /**< Standard output pipe */
    cupolas_pipe_t stderr_pipe;         /**< Standard error pipe */
} cupolas_process_attr_t;

/**
 * @brief Process exit status structure
 */
typedef struct cupolas_exit_status {
    int code;                           /**< Exit code (if not signaled) */
    bool signaled;                      /**< True if terminated by signal */
    int signal;                         /**< Signal number (if signaled) */
} cupolas_exit_status_t;

/**
 * @brief Spawn child process
 * @param[out] proc Process handle output
 * @param[in] path Path to executable
 * @param[in] argv Argument array (NULL-terminated)
 * @param[in] attr Process attributes (NULL for defaults)
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads
 * @reentrant No
 * @ownership proc: caller provides buffer, function writes to it
 * @ownership path and argv: caller retains ownership
 * @ownership attr: caller retains ownership
 */
int cupolas_process_spawn(cupolas_process_t* proc, 
                        const char* path, 
                        char* const argv[],
                        const cupolas_process_attr_t* attr);

/**
 * @brief Wait for process to finish
 * @param[in] proc Process handle
 * @param[out] status Exit status output
 * @param[in] timeout_ms Timeout in milliseconds (0 = infinite wait)
 * @return 0 on success, CUPOLAS_ERROR_TIMEOUT on timeout, negative on failure
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant No
 * @ownership status: caller provides buffer, function writes to it
 */
int cupolas_process_wait(cupolas_process_t proc, cupolas_exit_status_t* status, uint32_t timeout_ms);

/**
 * @brief Terminate process
 * @param[in] proc Process handle
 * @param[in] signal Signal to send (ignored on Windows)
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads
 * @reentrant No
 */
int cupolas_process_terminate(cupolas_process_t proc, int signal);

/**
 * @brief Close process handle
 * @param[in] proc Process handle
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads
 * @reentrant No
 */
int cupolas_process_close(cupolas_process_t proc);

/**
 * @brief Get process ID
 * @param[in] proc Process handle
 * @return Process ID
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 */
cupolas_pid_t cupolas_process_getpid(cupolas_process_t proc);

#ifdef __cplusplus
}
#endif

#endif /* CUPOLAS_WORKBENCH_PROCESS_H */
