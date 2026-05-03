/**
 * @file monitoring_metrics.c
 * @brief 监控指标管理实现 - 指标收集、更新、查询
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "monitoring_metrics.h"
#include "agentos.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_METRICS 256
#define MAX_NAME_LEN 128
#define MAX_DESC_LEN 256
#define MAX_UNIT_LEN 32

typedef struct metric_entry {
    char name[MAX_NAME_LEN];
    char description[MAX_DESC_LEN];
    char unit[MAX_UNIT_LEN];
    metric_type_t type;
    double value;
    uint64_t update_count;
} metric_entry_t;

struct metrics_collector {
    metric_entry_t entries[MAX_METRICS];
    size_t count;
    agentos_mutex_t* lock;
};

metrics_collector_t* monitoring_metrics_create(void) {
    metrics_collector_t* collector = (metrics_collector_t*)AGENTOS_CALLOC(1, sizeof(metrics_collector_t));
    if (!collector) return NULL;

    collector->count = 0;
    collector->lock = agentos_mutex_create();
    if (!collector->lock) {
        AGENTOS_FREE(collector);
        return NULL;
    }

    return collector;
}

void monitoring_metrics_destroy(metrics_collector_t* collector) {
    if (!collector) return;
    if (collector->lock) agentos_mutex_free(collector->lock);
    AGENTOS_FREE(collector);
}

static agentos_error_t add_metric(metrics_collector_t* collector,
                                  const char* name, const char* description,
                                  const char* unit, metric_type_t type) {
    if (!collector || !name) return AGENTOS_EINVAL;

    agentos_mutex_lock(collector->lock);

    if (collector->count >= MAX_METRICS) {
        agentos_mutex_unlock(collector->lock);
        return AGENTOS_ENOMEM;
    }

    for (size_t i = 0; i < collector->count; i++) {
        if (strcmp(collector->entries[i].name, name) == 0) {
            agentos_mutex_unlock(collector->lock);
            return AGENTOS_EEXIST;
        }
    }

    metric_entry_t* entry = &collector->entries[collector->count];
    snprintf(entry->name, sizeof(entry->name), "%s", name);
    snprintf(entry->description, sizeof(entry->description), "%s", description ? description : "");
    snprintf(entry->unit, sizeof(entry->unit), "%s", unit ? unit : "");
    entry->type = type;
    entry->value = 0.0;
    entry->update_count = 0;
    collector->count++;

    agentos_mutex_unlock(collector->lock);
    return AGENTOS_SUCCESS;
}

agentos_error_t monitoring_add_counter(metrics_collector_t* collector, const char* name,
                                       const char* description, const char* unit) {
    return add_metric(collector, name, description, unit, METRIC_COUNTER);
}

agentos_error_t monitoring_add_gauge(metrics_collector_t* collector, const char* name,
                                     const char* description, const char* unit) {
    return add_metric(collector, name, description, unit, METRIC_GAUGE);
}

agentos_error_t monitoring_update_metric(metrics_collector_t* collector, const char* name, double value) {
    if (!collector || !name) return AGENTOS_EINVAL;

    agentos_mutex_lock(collector->lock);

    for (size_t i = 0; i < collector->count; i++) {
        if (strcmp(collector->entries[i].name, name) == 0) {
            if (collector->entries[i].type == METRIC_COUNTER) {
                collector->entries[i].value += value;
            } else {
                collector->entries[i].value = value;
            }
            collector->entries[i].update_count++;
            agentos_mutex_unlock(collector->lock);
            return AGENTOS_SUCCESS;
        }
    }

    agentos_mutex_unlock(collector->lock);
    return AGENTOS_ENOENT;
}

agentos_error_t monitoring_get_metric(metrics_collector_t* collector, const char* name, double* out_value) {
    if (!collector || !name || !out_value) return AGENTOS_EINVAL;

    agentos_mutex_lock(collector->lock);

    for (size_t i = 0; i < collector->count; i++) {
        if (strcmp(collector->entries[i].name, name) == 0) {
            *out_value = collector->entries[i].value;
            agentos_mutex_unlock(collector->lock);
            return AGENTOS_SUCCESS;
        }
    }

    agentos_mutex_unlock(collector->lock);
    return AGENTOS_ENOENT;
}

agentos_error_t monitoring_export_metrics_json(metrics_collector_t* collector, char** out_json) {
    if (!collector || !out_json) return AGENTOS_EINVAL;

    agentos_mutex_lock(collector->lock);

    size_t buf_size = 256 + collector->count * 512;
    char* buf = (char*)AGENTOS_MALLOC(buf_size);
    if (!buf) {
        agentos_mutex_unlock(collector->lock);
        return AGENTOS_ENOMEM;
    }

    size_t pos = 0;
    int written = snprintf(buf + pos, buf_size - pos, "{\"metrics\":[");
    if (written < 0) { AGENTOS_FREE(buf); agentos_mutex_unlock(collector->lock); return AGENTOS_EOVERFLOW; }
    pos += (size_t)written;

    for (size_t i = 0; i < collector->count; i++) {
        metric_entry_t* e = &collector->entries[i];
        const char* type_str = "unknown";
        switch (e->type) {
            case METRIC_COUNTER: type_str = "counter"; break;
            case METRIC_GAUGE: type_str = "gauge"; break;
            case METRIC_HISTOGRAM: type_str = "histogram"; break;
            case METRIC_SUMMARY: type_str = "summary"; break;
        }
        written = snprintf(buf + pos, buf_size - pos,
            "%s{\"name\":\"%s\",\"type\":\"%s\",\"value\":%.6f,\"description\":\"%s\",\"unit\":\"%s\",\"updates\":%lu}",
            i > 0 ? "," : "",
            e->name, type_str, e->value, e->description, e->unit,
            (unsigned long)e->update_count);
        if (written < 0) { AGENTOS_FREE(buf); agentos_mutex_unlock(collector->lock); return AGENTOS_EOVERFLOW; }
        pos += (size_t)written;
        if (pos >= buf_size - 64) break;
    }

    written = snprintf(buf + pos, buf_size - pos, "],\"count\":%zu}", collector->count);
    if (written < 0) { AGENTOS_FREE(buf); agentos_mutex_unlock(collector->lock); return AGENTOS_EOVERFLOW; }

    agentos_mutex_unlock(collector->lock);
    *out_json = buf;
    return AGENTOS_SUCCESS;
}
