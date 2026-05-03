/**
 * @file frag_monitor.c
 * @brief MemoryRovol 碎片率监控实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * OSS模式内存池碎片率周期性监控与告警系统。
 * - 每隔 N 秒采集碎片率并记录到环形历史缓冲区
 * - 超过 warn_threshold 时输出 WARN 日志
 * - 超过 critical_threshold 时输出 ERROR 日志
 * - 计算碎片率趋势（每小时变化率）用于预判
 */

#include "frag_monitor.h"
#include "logger.h"
#include "agentos.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

static uint64_t get_timestamp_sec(void) {
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (uint64_t)ts.tv_sec;
}

frag_monitor_t* frag_monitor_create(memory_pool_t* pool) {
    if (!pool) return NULL;

    frag_monitor_t* monitor = (frag_monitor_t*)calloc(1, sizeof(frag_monitor_t));
    if (!monitor) return NULL;

    monitor->pool = pool;
    monitor->warn_threshold = FRAG_MONITOR_DEFAULT_WARN_THRESHOLD;
    monitor->critical_threshold = FRAG_MONITOR_DEFAULT_CRITICAL_THRESHOLD;
    monitor->check_interval_sec = FRAG_MONITOR_DEFAULT_CHECK_INTERVAL_SEC;
    monitor->enabled = true;

    return monitor;
}

void frag_monitor_destroy(frag_monitor_t* monitor) {
    if (!monitor) return;
    free(monitor);
}

int frag_monitor_check(frag_monitor_t* monitor) {
    if (!monitor || !monitor->pool) return -1;
    if (!monitor->enabled) return 0;

    uint64_t now = get_timestamp_sec();
    uint64_t elapsed = monitor->last_check_time > 0
                       ? now - monitor->last_check_time : 0;
    if (elapsed < monitor->check_interval_sec && monitor->last_check_time > 0)
        return 0;

    double frag_ratio = memory_pool_get_fragmentation_ratio(monitor->pool);
    double utilization = memory_pool_get_utilization(monitor->pool);

    frag_monitor_stats_t* s = &monitor->stats;

    s->frag_ratios[s->head] = frag_ratio;
    s->timestamps[s->head] = now;
    s->head = (s->head + 1) % FRAG_MONITOR_HISTORY_SIZE;
    if (s->count < FRAG_MONITOR_HISTORY_SIZE) s->count++;

    if (s->count == 1 || frag_ratio < s->min_ratio) s->min_ratio = frag_ratio;
    if (s->count == 1 || frag_ratio > s->max_ratio) s->max_ratio = frag_ratio;

    double total = 0.0;
    for (size_t i = 0; i < s->count; i++) total += s->frag_ratios[i];
    s->avg_ratio = s->count > 0 ? total / (double)s->count : 0.0;

    if (s->count >= 2) {
        size_t oldest = (s->head + FRAG_MONITOR_HISTORY_SIZE - s->count) % FRAG_MONITOR_HISTORY_SIZE;
        double delta = frag_ratio - s->frag_ratios[oldest];
        double time_elapsed_h = (double)(now - s->timestamps[oldest]) / 3600.0;
        if (time_elapsed_h > 0.01) {
            s->trend_per_hour = delta / time_elapsed_h;
        }
    }

    const char* pool_name = monitor->pool->pool_name ? monitor->pool->pool_name : "unnamed";

    if (frag_ratio >= monitor->critical_threshold) {
        monitor->critical_count++;
        AGENTOS_LOG_ERROR(
            "FRAG CRITICAL: pool=%s frag_ratio=%.3f util=%.3f "
            "trend=%.4f/h alerts=%llu criticals=%llu",
            pool_name, frag_ratio, utilization,
            s->trend_per_hour,
            (unsigned long long)monitor->alert_count,
            (unsigned long long)monitor->critical_count);
    } else if (frag_ratio >= monitor->warn_threshold) {
        monitor->alert_count++;
        AGENTOS_LOG_WARN(
            "FRAG WARNING: pool=%s frag_ratio=%.3f util=%.3f "
            "trend=%.4f/h alerts=%llu",
            pool_name, frag_ratio, utilization,
            s->trend_per_hour,
            (unsigned long long)monitor->alert_count);
    }

    monitor->last_check_time = now;
    return 0;
}

const frag_monitor_stats_t* frag_monitor_get_stats(const frag_monitor_t* monitor) {
    if (!monitor) return NULL;
    return &monitor->stats;
}

int frag_monitor_set_thresholds(frag_monitor_t* monitor,
                                 double warn_threshold,
                                 double critical_threshold) {
    if (!monitor) return -1;
    if (warn_threshold < 0.0 || warn_threshold > 1.0) return -2;
    if (critical_threshold < 0.0 || critical_threshold > 1.0) return -2;
    if (warn_threshold >= critical_threshold) return -3;

    monitor->warn_threshold = warn_threshold;
    monitor->critical_threshold = critical_threshold;
    return 0;
}

int frag_monitor_set_check_interval(frag_monitor_t* monitor, uint64_t interval_sec) {
    if (!monitor) return -1;
    if (interval_sec < 1) return -2;
    monitor->check_interval_sec = interval_sec;
    return 0;
}

void frag_monitor_reset_stats(frag_monitor_t* monitor) {
    if (!monitor) return;
    memset(&monitor->stats, 0, sizeof(frag_monitor_stats_t));
    monitor->alert_count = 0;
    monitor->critical_count = 0;
}

uint64_t frag_monitor_get_alert_count(const frag_monitor_t* monitor) {
    if (!monitor) return 0;
    return monitor->alert_count;
}

uint64_t frag_monitor_get_critical_count(const frag_monitor_t* monitor) {
    if (!monitor) return 0;
    return monitor->critical_count;
}

bool frag_monitor_is_healthy(const frag_monitor_t* monitor) {
    if (!monitor) return false;
    if (monitor->stats.count == 0) return true;
    double last_ratio = monitor->stats.frag_ratios[
        (monitor->stats.head + FRAG_MONITOR_HISTORY_SIZE - 1) % FRAG_MONITOR_HISTORY_SIZE];
    return last_ratio < monitor->warn_threshold;
}
