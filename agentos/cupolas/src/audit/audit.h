/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * audit.h - Audit Log System Public Interface
 *
 * Design Principles:
 * - Async Write: Background thread batch writing, non-blocking
 * - Log Rotation: Automatic rotation and compression
 * - Structured: JSON format output
 */

#ifndef CUPOLAS_AUDIT_H
#define CUPOLAS_AUDIT_H

#include "../platform/platform.h"
#include "audit_queue.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Audit Logger Handle */
typedef struct audit_logger audit_logger_t;

/**
 * @brief Create audit logger
 * @param[in] log_dir Log directory path (must not be NULL)
 * @param[in] log_prefix Log file prefix (must not be NULL)
 * @param[in] max_file_size Max file size in bytes
 * @param[in] max_files Max number of files (0 for unlimited)
 * @return Logger handle, NULL on failure
 * @post On success, caller owns the returned handle
 * @note Thread-safe: Yes
 * @reentrant No (create/destroy must be paired)
 * @ownership Returned handle: caller owns, must call audit_logger_destroy
 */
audit_logger_t *audit_logger_create(const char *log_dir, const char *log_prefix,
                                    size_t max_file_size, int max_files);

/**
 * @brief Destroy audit logger
 * @param[in] logger Logger handle (must not be NULL)
 * @pre Handle was created by audit_logger_create
 * @post All resources are released, pending logs are flushed
 * @note Thread-safe: No, ensure no other threads access logger
 * @reentrant No
 * @ownership logger: caller transfers ownership
 */
void audit_logger_destroy(audit_logger_t *logger);

/**
 * @brief Log audit event
 * @param[in] logger Logger handle (must not be NULL)
 * @param[in] type Event type
 * @param[in] agent_id Agent identifier (must not be NULL)
 * @param[in] action Action performed (must not be NULL)
 * @param[in] resource Resource accessed (must not be NULL)
 * @param[in] detail Additional details (may be NULL)
 * @param[in] result Result code
 * @return 0 on success, negative on failure
 * @note Thread-safe: Yes
 * @reentrant Yes
 * @ownership All input strings: caller retains ownership
 */
int audit_logger_log(audit_logger_t *logger, audit_event_type_t type, const char *agent_id,
                     const char *action, const char *resource, const char *detail, int result);

/**
 * @brief Log permission audit event
 * @param[in] logger Logger handle (must not be NULL)
 * @param[in] agent_id Agent identifier (must not be NULL)
 * @param[in] action Action performed (must not be NULL)
 * @param[in] resource Resource accessed (must not be NULL)
 * @param[in] allowed 1 if allowed, 0 if denied
 * @return 0 on success, negative on failure
 * @note Thread-safe: Yes
 * @reentrant Yes
 * @ownership All input strings: caller retains ownership
 */
int audit_logger_log_permission(audit_logger_t *logger, const char *agent_id, const char *action,
                                const char *resource, int allowed);

/**
 * @brief Log sanitizer audit event
 * @param[in] logger Logger handle (must not be NULL)
 * @param[in] agent_id Agent identifier (must not be NULL)
 * @param[in] input Input string (must not be NULL)
 * @param[in] output Output string after sanitization (must not be NULL)
 * @param[in] passed 1 if passed, 0 if rejected
 * @return 0 on success, negative on failure
 * @note Thread-safe: Yes
 * @reentrant Yes
 * @ownership All input strings: caller retains ownership
 */
int audit_logger_log_sanitizer(audit_logger_t *logger, const char *agent_id, const char *input,
                               const char *output, int passed);

/**
 * @brief Log workbench audit event
 * @param[in] logger Logger handle (must not be NULL)
 * @param[in] agent_id Agent identifier (must not be NULL)
 * @param[in] command Command executed (must not be NULL)
 * @param[in] exit_code Exit code
 * @return 0 on success, negative on failure
 * @note Thread-safe: Yes
 * @reentrant Yes
 * @ownership All input strings: caller retains ownership
 */
int audit_logger_log_workbench(audit_logger_t *logger, const char *agent_id, const char *command,
                               int exit_code);

/**
 * @brief Flush log buffer
 * @param[in] logger Logger handle (must not be NULL)
 * @note Thread-safe: Yes
 * @reentrant Yes
 * @post All pending log entries are written to storage
 */
void audit_logger_flush(audit_logger_t *logger);

/**
 * @brief Get log statistics
 * @param[in] logger Logger handle (must not be NULL)
 * @param[out] total_logged Total logged count output (may be NULL)
 * @param[out] total_failed Total failed count output (may be NULL)
 * @note Thread-safe: Yes
 * @reentrant Yes
 * @ownership total_logged, total_failed: callee writes, caller owns
 */
void audit_logger_stats(audit_logger_t *logger, uint64_t *total_logged, uint64_t *total_failed);

/**
 * @brief 验证审计哈希链完整性（BAN-129 编码契约）
 *
 * 从给定条目列表重新计算 SHA-256 哈希链，验证每个条目的
 * curr_hash 是否与基于 prev_hash + 条目内容重新计算的哈希一致。
 * 任何不一致都表示审计日志被篡改。
 *
 * @param entries     审计条目数组（按时间顺序排列）
 * @param entry_count 条目数量
 * @param first_prev_hash 链起始哈希（通常为64个'0'字符）
 * @param out_invalid_index 输出第一个无效条目的索引（-1 表示全部有效，可为NULL）
 * @return true 如果哈希链完整且未被篡改，false 如果有任何不匹配
 * @note Thread-safe: Yes (只读访问条目)
 */
bool audit_logger_verify_chain(const audit_entry_t **entries, size_t entry_count,
                               const char *first_prev_hash, int *out_invalid_index);

#ifdef __cplusplus
}
#endif

#endif /* CUPOLAS_AUDIT_H */
