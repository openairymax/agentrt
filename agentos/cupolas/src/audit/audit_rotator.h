/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * audit_rotator.h - Audit Log Rotator Internal Interface
 */

#ifndef CUPOLAS_AUDIT_ROTATOR_H
#define CUPOLAS_AUDIT_ROTATOR_H

#include "../platform/platform.h"
#include "audit_queue.h"
#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Audit log rotator structure
 * 
 * Design principles:
 * - Automatic log rotation based on file size
 * - Configurable retention policy
 * - Thread-safe file operations
 * - Atomic rename for safe rotation
 */
typedef struct audit_rotator audit_rotator_t;

/**
 * @brief Create log rotator
 * @param[in] log_dir Directory for log files
 * @param[in] log_prefix Prefix for log filenames
 * @param[in] max_file_size Maximum size per file in bytes
 * @param[in] max_files Maximum number of files to retain
 * @return Rotator handle, NULL on failure
 * @note Thread-safe: Safe to call from multiple threads (initialization only)
 * @reentrant No
 * @ownership Returns owned pointer: caller must call audit_rotator_destroy()
 * @ownership log_dir and log_prefix: caller retains ownership
 */
audit_rotator_t* audit_rotator_create(const char* log_dir, const char* log_prefix,
                                       size_t max_file_size, int max_files);

/**
 * @brief Destroy log rotator and free resources
 * @param[in] rotator Rotator handle (may be NULL)
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other operations)
 * @reentrant No
 * @ownership rotator: transferred to this function, will be freed
 */
void audit_rotator_destroy(audit_rotator_t* rotator);

/**
 * @brief Write audit entry to current log file
 * @param[in] rotator Rotator handle
 * @param[in] entry Audit entry to write
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant No
 * @ownership entry: caller retains ownership
 */
int audit_rotator_write(audit_rotator_t* rotator, const audit_entry_t* entry);

/**
 * @brief Force log rotation
 * @param[in] rotator Rotator handle
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other operations)
 * @reentrant No
 */
int audit_rotator_rotate(audit_rotator_t* rotator);

/**
 * @brief Get current log file size
 * @param[in] rotator Rotator handle
 * @return Current file size in bytes
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 */
size_t audit_rotator_current_size(audit_rotator_t* rotator);

#ifdef __cplusplus
}
#endif

#endif /* CUPOLAS_AUDIT_ROTATOR_H */
