/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * audit_queue.h - Audit Queue Internal Interface: Thread-safe Producer-Consumer Queue
 */

#ifndef CUPOLAS_AUDIT_QUEUE_H
#define CUPOLAS_AUDIT_QUEUE_H

#include "../platform/platform.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Audit event types
 * 
 * Design principles:
 * - Categorized event tracking
 * - Extensible enum for future event types
 */
typedef enum audit_event_type {
    AUDIT_EVENT_PERMISSION = 1,   /**< Permission check events */
    AUDIT_EVENT_SANITIZER,        /**< Input sanitization events */
    AUDIT_EVENT_WORKBENCH,        /**< Workbench execution events */
    AUDIT_EVENT_SYSTEM,           /**< System-level events */
    AUDIT_EVENT_CUSTOM            /**< Custom user-defined events */
} audit_event_type_t;

/**
 * @brief Audit log entry structure
 * 
 * Represents a single audit log record with full context
 */
typedef struct audit_entry {
    uint64_t                timestamp_ms;   /**< Event timestamp (milliseconds since epoch) */
    audit_event_type_t      type;           /**< Event type category */
    char*                   agent_id;       /**< Agent identifier */
    char*                   action;         /**< Action performed */
    char*                   resource;       /**< Resource accessed */
    char*                   detail;         /**< Additional details */
    int                     result;         /**< Event result (1=success, 0=failure) */
    struct audit_entry*     next;           /**< Next entry in linked list */
} audit_entry_t;

/**
 * @brief Thread-safe audit queue structure
 * 
 * Design principles:
 * - Thread-safe with mutex and condition variables
 * - High throughput with batch write support
 * - Graceful shutdown with queue draining
 * - Bounded capacity with backpressure
 */
typedef struct audit_queue {
    audit_entry_t*      head;           /**< Queue head (for pop operations) */
    audit_entry_t*      tail;           /**< Queue tail (for push operations) */
    size_t              size;           /**< Current number of entries */
    size_t              max_size;       /**< Maximum capacity (0 = unlimited) */
    cupolas_mutex_t     lock;           /**< Mutex for thread safety */
    cupolas_cond_t      not_empty;      /**< Condition: queue not empty */
    cupolas_cond_t      not_full;       /**< Condition: queue not full */
    bool                shutdown;       /**< Shutdown flag */
    cupolas_atomic64_t  total_pushed;   /**< Total entries pushed */
    cupolas_atomic64_t  total_popped;   /**< Total entries popped */
} audit_queue_t;

/**
 * @brief Create audit queue
 * @param[in] max_size Maximum number of entries (0 = unlimited)
 * @return Queue handle, NULL on failure
 * @note Thread-safe: Safe to call from multiple threads (initialization only)
 * @reentrant No
 * @ownership Returns owned pointer: caller must call audit_queue_destroy()
 */
audit_queue_t* audit_queue_create(size_t max_size);

/**
 * @brief Destroy audit queue and free all resources
 * @param[in] queue Queue handle (may be NULL)
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other operations)
 * @reentrant No
 * @ownership queue: transferred to this function, will be freed
 */
void audit_queue_destroy(audit_queue_t* queue);

/**
 * @brief Push audit entry to queue (blocking)
 * @param[in] queue Queue handle
 * @param[in] entry Audit entry (ownership transferred to queue)
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant No
 * @ownership entry: transferred to queue
 */
int audit_queue_push(audit_queue_t* queue, audit_entry_t* entry);

/**
 * @brief Push audit entry to queue (non-blocking)
 * @param[in] queue Queue handle
 * @param[in] entry Audit entry (ownership transferred to queue)
 * @return 0 on success, CUPOLAS_ERROR_WOULD_BLOCK if queue full, negative on failure
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant No
 * @ownership entry: transferred to queue
 */
int audit_queue_try_push(audit_queue_t* queue, audit_entry_t* entry);

/**
 * @brief Pop audit entry from queue (blocking)
 * @param[in] queue Queue handle
 * @param[out] entry Output pointer to audit entry
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant No
 * @ownership entry: transferred to caller, must call audit_entry_destroy()
 */
int audit_queue_pop(audit_queue_t* queue, audit_entry_t** entry);

/**
 * @brief Pop audit entry from queue (with timeout)
 * @param[in] queue Queue handle
 * @param[out] entry Output pointer to audit entry
 * @param[in] timeout_ms Timeout in milliseconds
 * @return 0 on success, CUPOLAS_ERROR_TIMEOUT on timeout, negative on failure
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant No
 * @ownership entry: transferred to caller, must call audit_entry_destroy()
 */
int audit_queue_timed_pop(audit_queue_t* queue, audit_entry_t** entry, uint32_t timeout_ms);

/**
 * @brief Pop audit entry from queue (non-blocking)
 * @param[in] queue Queue handle
 * @param[out] entry Output pointer to audit entry
 * @return 0 on success, CUPOLAS_ERROR_WOULD_BLOCK if queue empty, negative on failure
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant No
 * @ownership entry: transferred to caller, must call audit_entry_destroy()
 */
int audit_queue_try_pop(audit_queue_t* queue, audit_entry_t** entry);

/**
 * @brief Pop multiple audit entries in batch
 * @param[in] queue Queue handle
 * @param[out] entries Array of audit entry pointers (caller-allocated)
 * @param[in] max_count Maximum number of entries to pop
 * @param[out] actual_count Actual number of entries popped
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant No
 * @ownership entries: transferred to caller, must call audit_entry_destroy() for each
 */
int audit_queue_pop_batch(audit_queue_t* queue, audit_entry_t** entries, 
                           size_t max_count, size_t* actual_count);

/**
 * @brief Shutdown queue and optionally wait for draining
 * @param[in] queue Queue handle
 * @param[in] wait_empty If true, block until queue is empty
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other operations)
 * @reentrant No
 */
void audit_queue_shutdown(audit_queue_t* queue, bool wait_empty);

/**
 * @brief Get current queue size
 * @param[in] queue Queue handle
 * @return Number of entries in queue
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 */
size_t audit_queue_size(audit_queue_t* queue);

/**
 * @brief Create audit entry
 * @param[in] type Event type
 * @param[in] agent_id Agent identifier
 * @param[in] action Action performed
 * @param[in] resource Resource accessed
 * @param[in] detail Additional details
 * @param[in] result Event result
 * @return Audit entry handle, NULL on failure
 * @note Thread-safe: Safe to call from multiple threads
 * @reentrant Yes
 * @ownership Returns owned pointer: caller must call audit_entry_destroy()
 * @ownership All string parameters: caller retains ownership
 */
audit_entry_t* audit_entry_create(audit_event_type_t type,
                                   const char* agent_id,
                                   const char* action,
                                   const char* resource,
                                   const char* detail,
                                   int result);

/**
 * @brief Destroy audit entry and free resources
 * @param[in] entry Audit entry (may be NULL)
 * @note Thread-safe: Safe to call from multiple threads
 * @reentrant No
 * @ownership entry: transferred to this function, will be freed
 */
void audit_entry_destroy(audit_entry_t* entry);

/**
 * @brief Get queue statistics
 * @param[in] queue Queue handle
 * @param[out] total_pushed Total entries pushed (may be NULL)
 * @param[out] total_popped Total entries popped (may be NULL)
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 */
void audit_queue_stats(audit_queue_t* queue, uint64_t* total_pushed, uint64_t* total_popped);

#ifdef __cplusplus
}
#endif

#endif /* CUPOLAS_AUDIT_QUEUE_H */
