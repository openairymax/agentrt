/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * audit_overflow.h - Audit Overflow Handler: Disk Spillover for Queue Backpressure
 */

#ifndef CUPOLAS_AUDIT_OVERFLOW_H
#define CUPOLAS_AUDIT_OVERFLOW_H

#include "../platform/platform.h"
#include "audit_queue.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum overflow_level {
    OVERFLOW_LEVEL_NORMAL = 0,
    OVERFLOW_LEVEL_WARNING = 1,
    OVERFLOW_LEVEL_CRITICAL = 2,
    OVERFLOW_LEVEL_SPILLING = 3
} overflow_level_t;

typedef struct overflow_stats {
    uint64_t total_events_received;
    uint64_t events_written_to_disk;
    uint64_t events_dropped;
    uint64_t disk_write_errors;
    uint64_t last_flush_time_ms;
    overflow_level_t current_level;
} overflow_stats_t;

typedef void (*overflow_callback_t)(overflow_level_t level, size_t queue_size, size_t max_size, void* user_data);

typedef struct overflow_handler overflow_handler_t;

overflow_handler_t* overflow_handler_create(const char* overflow_dir,
                                           size_t max_file_size_mb,
                                           uint32_t flush_interval_ms);

void overflow_handler_destroy(overflow_handler_t* handler);

int overflow_handler_write(overflow_handler_t* handler, audit_entry_t* entry);

void overflow_handler_flush(overflow_handler_t* handler);

void overflow_handler_get_stats(overflow_handler_t* handler, overflow_stats_t* stats);

void overflow_handler_reset_stats(overflow_handler_t* handler);

overflow_level_t overflow_handler_check_level(size_t current_size, size_t max_size);

int overflow_handler_set_callback(overflow_handler_t* handler,
                                  overflow_callback_t callback,
                                  void* user_data);

typedef struct audit_queue_ex audit_queue_ex_t;

audit_queue_ex_t* audit_queue_ex_create(size_t max_size,
                                         const char* overflow_dir,
                                         size_t max_file_size_mb);

void audit_queue_ex_destroy(audit_queue_ex_t* queue);

int audit_queue_ex_push(audit_queue_ex_t* queue, audit_entry_t* entry);

int audit_queue_ex_push_with_callback(audit_queue_ex_t* queue,
                                       audit_entry_t* entry,
                                       overflow_callback_t callback,
                                       void* user_data);

int audit_queue_ex_pop(audit_queue_ex_t* queue, audit_entry_t** entry);

int audit_queue_ex_pop_batch(audit_queue_ex_t* queue,
                               audit_entry_t** entries,
                               size_t max_count,
                               size_t* actual_count);

void audit_queue_ex_shutdown(audit_queue_ex_t* queue, bool wait_empty);

size_t audit_queue_ex_size(audit_queue_ex_t* queue);

void audit_queue_ex_get_stats(audit_queue_ex_t* queue,
                               uint64_t* pushed,
                               uint64_t* popped,
                               uint64_t* spilled,
                               uint64_t* dropped);

overflow_level_t audit_queue_ex_get_overflow_level(audit_queue_ex_t* queue);

int audit_queue_ex_set_overflow_callback(audit_queue_ex_t* queue,
                                          overflow_callback_t callback,
                                          void* user_data);

#ifdef __cplusplus
}
#endif

#endif /* CUPOLAS_AUDIT_OVERFLOW_H */
