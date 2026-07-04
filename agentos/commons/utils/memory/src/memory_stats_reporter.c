// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
// Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.

/**
 * @file memory_stats_reporter.c
 * @brief Memory statistics periodic reporter (SEC-15)
 *
 * Runs a background thread that logs memory usage stats every 60 seconds.
 * Integrates with AGENTOS_MALLOC/AGENTOS_FREE via memory_compat.h.
 */

#include "memory_stats_reporter.h"

#include "platform.h"
#include "svc_logger.h"

#include <string.h>

/* ==================== Internal state ==================== */

static agentos_mem_stats_t g_stats;
static agentos_mutex_t g_stats_mutex;
static agentos_thread_t g_reporter_thread;
static volatile int g_shutdown_flag = 1;  /* 1 = not running, 0 = running */
static unsigned int g_interval_seconds = 60;
static int g_initialized = 0;

/* ==================== Background thread ==================== */

static void *reporter_thread_func(void *arg)
{
    (void)arg;

    SVC_LOG_INFO("memory_stats_reporter: background thread started (interval=%us)",
                 g_interval_seconds);

    while (!g_shutdown_flag) {
        /* Sleep in 1-second increments to check shutdown flag promptly */
        unsigned int remaining = g_interval_seconds;
        while (remaining > 0 && !g_shutdown_flag) {
            unsigned int sleep_step = remaining > 1 ? 1 : remaining;
            agentos_sleep_ms(sleep_step * 1000);
            remaining -= sleep_step;
        }

        if (g_shutdown_flag)
            break;

        /* Take a snapshot under mutex */
        agentos_mem_stats_t snapshot;
        agentos_mutex_lock(&g_stats_mutex);
        __builtin_memcpy(&snapshot, &g_stats, sizeof(snapshot));
        agentos_mutex_unlock(&g_stats_mutex);

        /* Log the stats */
        SVC_LOG_INFO("memory_stats: allocs=%zu deallocs=%zu current_bytes=%zu "
                     "peak_bytes=%zu active_blocks=%zu oom_events=%zu pressure=%zu",
                     snapshot.total_allocations,
                     snapshot.total_deallocations,
                     snapshot.current_bytes_allocated,
                     snapshot.peak_bytes_allocated,
                     snapshot.active_blocks,
                     snapshot.oom_events,
                     snapshot.pressure_level);
    }

    SVC_LOG_INFO("memory_stats_reporter: background thread stopped");
    return NULL;
}

/* ==================== Public API ==================== */

int agentos_mem_stats_reporter_init(void)
{
    if (g_initialized) {
        return 0;  /* already initialized */
    }

    /* Zero out stats */
    __builtin_memset(&g_stats, 0, sizeof(g_stats));

    /* Initialize mutex */
    int err = agentos_mutex_init(&g_stats_mutex);
    if (err != 0) {
        SVC_LOG_ERROR("memory_stats_reporter: mutex init failed (%d)", err);
        return err;
    }

    /* Start background thread */
    g_shutdown_flag = 0;
    err = agentos_thread_create(&g_reporter_thread, reporter_thread_func, NULL);
    if (err != 0) {
        SVC_LOG_ERROR("memory_stats_reporter: thread create failed (%d)", err);
        g_shutdown_flag = 1;
        agentos_mutex_destroy(&g_stats_mutex);
        return err;
    }

    g_initialized = 1;
    SVC_LOG_INFO("memory_stats_reporter: initialized (interval=%us)", g_interval_seconds);
    return 0;
}

void agentos_mem_stats_reporter_shutdown(void)
{
    if (!g_initialized) {
        return;
    }

    /* Signal thread to stop */
    g_shutdown_flag = 1;

    /* Wait for thread to finish */
    agentos_thread_join(g_reporter_thread, NULL);

    /* Destroy mutex */
    agentos_mutex_destroy(&g_stats_mutex);

    g_initialized = 0;
    SVC_LOG_INFO("memory_stats_reporter: shutdown complete");
}

int agentos_mem_stats_get(agentos_mem_stats_t *stats)
{
    if (!stats) {
        return -1;
    }

    agentos_mutex_lock(&g_stats_mutex);
    __builtin_memcpy(stats, &g_stats, sizeof(*stats));
    agentos_mutex_unlock(&g_stats_mutex);

    return 0;
}

void agentos_mem_stats_record_alloc(size_t bytes)
{
    agentos_mutex_lock(&g_stats_mutex);
    g_stats.total_allocations++;
    g_stats.current_bytes_allocated += bytes;
    g_stats.active_blocks++;

    if (g_stats.current_bytes_allocated > g_stats.peak_bytes_allocated) {
        g_stats.peak_bytes_allocated = g_stats.current_bytes_allocated;
    }

    agentos_mutex_unlock(&g_stats_mutex);
}

void agentos_mem_stats_record_dealloc(size_t bytes)
{
    agentos_mutex_lock(&g_stats_mutex);
    g_stats.total_deallocations++;

    if (g_stats.current_bytes_allocated >= bytes) {
        g_stats.current_bytes_allocated -= bytes;
    } else {
        g_stats.current_bytes_allocated = 0;
    }

    if (g_stats.active_blocks > 0) {
        g_stats.active_blocks--;
    }

    agentos_mutex_unlock(&g_stats_mutex);
}

void agentos_mem_stats_record_oom(void)
{
    agentos_mutex_lock(&g_stats_mutex);
    g_stats.oom_events++;
    agentos_mutex_unlock(&g_stats_mutex);
}

void agentos_mem_stats_set_interval(unsigned int seconds)
{
    if (seconds == 0) {
        seconds = 60;  /* enforce minimum */
    }

    agentos_mutex_lock(&g_stats_mutex);
    g_interval_seconds = seconds;
    agentos_mutex_unlock(&g_stats_mutex);
}
