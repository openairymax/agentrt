// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.

#ifndef AGENTOS_MEMORY_STATS_REPORTER_H
#define AGENTOS_MEMORY_STATS_REPORTER_H

#include <stddef.h>
#include <stdint.h>

/**
 * Memory statistics reporter - periodically logs memory usage stats.
 * Runs a background thread that reports every 60 seconds.
 */

typedef struct agentos_mem_stats {
    size_t total_allocations;
    size_t total_deallocations;
    size_t current_bytes_allocated;
    size_t peak_bytes_allocated;
    size_t active_blocks;
    size_t oom_events;
    size_t pressure_level;  /* 0=NORMAL, 1=WARNING, 2=DEGRADED, 3=CRITICAL, 4=FATAL */
} agentos_mem_stats_t;

/* Initialize the stats reporter (starts background thread) */
int agentos_mem_stats_reporter_init(void);

/* Shutdown the stats reporter (stops background thread) */
void agentos_mem_stats_reporter_shutdown(void);

/* Get current memory statistics snapshot */
int agentos_mem_stats_get(agentos_mem_stats_t* stats);

/* Update allocation counter (called by AGENTOS_MALLOC) */
void agentos_mem_stats_record_alloc(size_t bytes);

/* Update deallocation counter (called by AGENTOS_FREE) */
void agentos_mem_stats_record_dealloc(size_t bytes);

/* Record an OOM event */
void agentos_mem_stats_record_oom(void);

/* Set reporting interval in seconds (default: 60) */
void agentos_mem_stats_set_interval(unsigned int seconds);

#endif /* AGENTOS_MEMORY_STATS_REPORTER_H */
