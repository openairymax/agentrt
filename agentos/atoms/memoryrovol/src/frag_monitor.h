/**
 * @file frag_monitor.h
 * @brief MemoryRovol 碎片率监控接口 (OSS Mode Fragmentation Monitoring)
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * OSS模式下跟踪内存池碎片率并在超过阈值时发出告警。
 * 支持周期性检查、历史统计和趋势分析。
 */

#ifndef FRAG_MONITOR_H
#define FRAG_MONITOR_H

#include "memory_pool.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FRAG_MONITOR_HISTORY_SIZE 64
#define FRAG_MONITOR_DEFAULT_CHECK_INTERVAL_SEC 30
#define FRAG_MONITOR_DEFAULT_WARN_THRESHOLD 0.30
#define FRAG_MONITOR_DEFAULT_CRITICAL_THRESHOLD 0.50

typedef struct {
    double frag_ratios[FRAG_MONITOR_HISTORY_SIZE];
    uint64_t timestamps[FRAG_MONITOR_HISTORY_SIZE];
    size_t count;
    size_t head;
    double min_ratio;
    double max_ratio;
    double avg_ratio;
    double trend_per_hour;
} frag_monitor_stats_t;

typedef struct {
    memory_pool_t* pool;
    frag_monitor_stats_t stats;
    double warn_threshold;
    double critical_threshold;
    uint64_t last_check_time;
    uint64_t check_interval_sec;
    uint64_t alert_count;
    uint64_t critical_count;
    bool enabled;
} frag_monitor_t;

frag_monitor_t* frag_monitor_create(memory_pool_t* pool);
void frag_monitor_destroy(frag_monitor_t* monitor);

int frag_monitor_check(frag_monitor_t* monitor);
const frag_monitor_stats_t* frag_monitor_get_stats(const frag_monitor_t* monitor);

int frag_monitor_set_thresholds(frag_monitor_t* monitor,
                                 double warn_threshold,
                                 double critical_threshold);

int frag_monitor_set_check_interval(frag_monitor_t* monitor, uint64_t interval_sec);
void frag_monitor_reset_stats(frag_monitor_t* monitor);

uint64_t frag_monitor_get_alert_count(const frag_monitor_t* monitor);
uint64_t frag_monitor_get_critical_count(const frag_monitor_t* monitor);
bool frag_monitor_is_healthy(const frag_monitor_t* monitor);

#ifdef __cplusplus
}
#endif

#endif /* FRAG_MONITOR_H */
