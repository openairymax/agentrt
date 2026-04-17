/**
 * @file metrics.c
 * @brief 监控指标采集实现（生产级版本）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * 
 * 功能：
 * 1. 指标采集与聚合
 * 2. OpenTelemetry 兼容
 * 3. 指标导出（Prometheus 格式）
 * 4. 线程安全
 */

#include "monitor_service.h"
#include "platform.h"
#include "error.h"
#include "daemon_errors.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ==================== 配置常量 ==================== */

#define MAX_METRICS 1024
#define MAX_LABELS_PER_METRIC 16
#define MAX_SERIES_PER_METRIC 256
#define MAX_LABEL_VALUE_LEN 256
#define DEFAULT_EXPORT_INTERVAL_MS 60000
#define DEFAULT_RETENTION_SECONDS 3600

/* ==================== 指标配置类型 ==================== */

typedef struct {
    uint32_t export_interval_ms;
    uint32_t retention_seconds;
} metrics_config_t;

/* ==================== 指标类型 ==================== */
/* metric_type_t 已在 monitor_service.h 中定义 */

/* ==================== 标签 ==================== */

typedef struct {
    char* key;
    char* value;
} metric_label_t;

/* ==================== 时间序列 ==================== */

typedef struct {
    metric_label_t labels[MAX_LABELS_PER_METRIC];
    size_t label_count;
    double value;
    uint64_t timestamp;
    uint64_t update_count;
} metric_series_t;

/* ==================== 直方图桶 ==================== */

typedef struct {
    double boundary;
    uint64_t count;
} histogram_bucket_t;

/* ==================== 直方图数据 ==================== */

typedef struct {
    histogram_bucket_t* buckets;
    size_t bucket_count;
    double sum;
    uint64_t count;
} histogram_data_t;

/* ==================== 指标定义 ==================== */

typedef struct {
    char* name;
    char* description;
    char* unit;
    metric_type_t type;
    metric_series_t series[MAX_SERIES_PER_METRIC];
    size_t series_count;
    histogram_data_t histogram;
    agentos_mutex_t lock;
    int initialized;
} metric_t;

/* ==================== 指标存储 ==================== */

typedef struct {
    metric_t metrics[MAX_METRICS];
    size_t metric_count;
    agentos_mutex_t global_lock;
    uint64_t export_interval_ms;
    uint64_t retention_seconds;
    int initialized;
} metrics_storage_t;

static metrics_storage_t g_metrics = {0};

/* ==================== 内部函数 ==================== */

/**
 * @brief 查找指标索引
 */
static int find_metric_index(const char* name) {
    for (size_t i = 0; i < g_metrics.metric_count; i++) {
        if (strcmp(g_metrics.metrics[i].name, name) == 0) {
            return (int)i;
        }
    }
    return -1;
}

/**
 * @brief 查找时间序列索引
 */
static int find_series_index(metric_t* metric, const metric_label_t* labels, size_t label_count) {
    for (size_t i = 0; i < metric->series_count; i++) {
        metric_series_t* series = &metric->series[i];
        
        if (series->label_count != label_count) {
            continue;
        }
        
        int match = 1;
        for (size_t j = 0; j < label_count && match; j++) {
            int found = 0;
            for (size_t k = 0; k < series->label_count; k++) {
                if (strcmp(series->labels[k].key, labels[j].key) == 0 &&
                    strcmp(series->labels[k].value, labels[j].value) == 0) {
                    found = 1;
                    break;
                }
            }
            if (!found) match = 0;
        }
        
        if (match) return (int)i;
    }
    return -1;
}

/**
 * @brief 创建时间序列
 */
static int create_series(metric_t* metric, const metric_label_t* labels, size_t label_count) {
    if (metric->series_count >= MAX_SERIES_PER_METRIC) {
        return -1;
    }
    
    metric_series_t* series = &metric->series[metric->series_count];
    series->label_count = label_count;
    
    for (size_t i = 0; i < label_count; i++) {
        series->labels[i].key = strdup(labels[i].key);
        series->labels[i].value = strdup(labels[i].value);
    }
    
    series->value = 0.0;
    series->timestamp = agentos_time_ms();
    series->update_count = 0;
    
    metric->series_count++;
    return (int)(metric->series_count - 1);
}

/**
 * @brief 格式化标签为 Prometheus 格式
 */
static void format_labels(char* buf, size_t buf_size, const metric_label_t* labels, size_t label_count) {
    if (label_count == 0) {
        buf[0] = '\0';
        return;
    }
    
    size_t pos = 0;
    buf[pos++] = '{';
    
    for (size_t i = 0; i < label_count && pos < buf_size - 2; i++) {
        if (i > 0) {
            buf[pos++] = ',';
            buf[pos++] = ' ';
        }
        
        int written = snprintf(buf + pos, buf_size - pos, "%s=\"%s\"", 
                               labels[i].key, labels[i].value);
        if (written > 0) {
            pos += (size_t)written;
        }
    }
    
    buf[pos++] = '}';
    buf[pos] = '\0';
}

/* ==================== 公共接口实现 ==================== */

/**
 * @brief 初始化指标系统
 */
int metrics_init(const metrics_config_t* manager) {
    if (g_metrics.initialized) {
        return AGENTOS_OK;
    }
    
    agentos_mutex_init(&g_metrics.global_lock);
    
    g_metrics.metric_count = 0;
    g_metrics.export_interval_ms = manager ? manager->export_interval_ms : DEFAULT_EXPORT_INTERVAL_MS;
    g_metrics.retention_seconds = manager ? manager->retention_seconds : DEFAULT_RETENTION_SECONDS;
    
    /* 初始化所有指标的互斥锁 */
    for (size_t i = 0; i < MAX_METRICS; i++) {
        agentos_mutex_init(&g_metrics.metrics[i].lock);
    }
    
    g_metrics.initialized = 1;
    return AGENTOS_OK;
}

/**
 * @brief 关闭指标系统
 */
void metrics_shutdown(void) {
    if (!g_metrics.initialized) {
        return;
    }
    
    agentos_mutex_lock(&g_metrics.global_lock);
    
    /* 释放所有指标 */
    for (size_t i = 0; i < g_metrics.metric_count; i++) {
        metric_t* metric = &g_metrics.metrics[i];
        
        agentos_mutex_lock(&metric->lock);
        
        free(metric->name);
        free(metric->description);
        free(metric->unit);
        
        /* 释放时间序列 */
        for (size_t j = 0; j < metric->series_count; j++) {
            metric_series_t* series = &metric->series[j];
            for (size_t k = 0; k < series->label_count; k++) {
                free(series->labels[k].key);
                free(series->labels[k].value);
            }
        }
        
        /* 释放直方图桶 */
        if (metric->histogram.buckets) {
            free(metric->histogram.buckets);
        }
        
        agentos_mutex_unlock(&metric->lock);
        agentos_mutex_destroy(&metric->lock);
    }
    
    g_metrics.metric_count = 0;
    g_metrics.initialized = 0;
    
    agentos_mutex_unlock(&g_metrics.global_lock);
    agentos_mutex_destroy(&g_metrics.global_lock);
}

/**
 * @brief 注册指标
 */
int metrics_register(const char* name,
                     const char* description,
                     const char* unit,
                     metric_type_t type,
                     const double* histogram_buckets,
                     size_t bucket_count) {
    if (!name) {
        AGENTOS_ERROR(AGENTOS_ERR_INVALID_PARAM, "name is NULL");
    }
    
    if (!g_metrics.initialized) {
        AGENTOS_ERROR(AGENTOS_ERR_STATE_ERROR, "Metrics system not initialized");
    }
    
    agentos_mutex_lock(&g_metrics.global_lock);
    
    /* 检查是否已存在 */
    if (find_metric_index(name) >= 0) {
        agentos_mutex_unlock(&g_metrics.global_lock);
        AGENTOS_ERROR(AGENTOS_ERR_ALREADY_EXISTS, "Metric already exists");
    }
    
    /* 检查容量 */
    if (g_metrics.metric_count >= MAX_METRICS) {
        agentos_mutex_unlock(&g_metrics.global_lock);
        AGENTOS_ERROR(AGENTOS_ERR_OVERFLOW, "Too many metrics");
    }
    
    /* 创建指标 */
    metric_t* metric = &g_metrics.metrics[g_metrics.metric_count];
    metric->name = strdup(name);
    metric->description = description ? strdup(description) : NULL;
    metric->unit = unit ? strdup(unit) : NULL;
    metric->type = type;
    metric->series_count = 0;
    metric->initialized = 1;
    
    /* 初始化直方图 */
    if (type == METRIC_TYPE_HISTOGRAM && histogram_buckets && bucket_count > 0) {
        metric->histogram.buckets = (histogram_bucket_t*)malloc(
            sizeof(histogram_bucket_t) * bucket_count);
        if (metric->histogram.buckets) {
            for (size_t i = 0; i < bucket_count; i++) {
                metric->histogram.buckets[i].boundary = histogram_buckets[i];
                metric->histogram.buckets[i].count = 0;
            }
            metric->histogram.bucket_count = bucket_count;
            metric->histogram.sum = 0.0;
            metric->histogram.count = 0;
        }
    }
    
    g_metrics.metric_count++;
    
    agentos_mutex_unlock(&g_metrics.global_lock);
    return AGENTOS_OK;
}

/**
 * @brief 记录 Counter
 */
int metrics_counter_inc(const char* name, const metric_label_t* labels, size_t label_count) {
    if (!name || !g_metrics.initialized) {
        return AGENTOS_ERR_INVALID_PARAM;
    }
    
    agentos_mutex_lock(&g_metrics.global_lock);
    
    int idx = find_metric_index(name);
    if (idx < 0) {
        agentos_mutex_unlock(&g_metrics.global_lock);
        return AGENTOS_ERR_NOT_FOUND;
    }
    
    metric_t* metric = &g_metrics.metrics[idx];
    agentos_mutex_lock(&metric->lock);
    agentos_mutex_unlock(&g_metrics.global_lock);
    
    /* 查找或创建时间序列 */
    int series_idx = find_series_index(metric, labels, label_count);
    if (series_idx < 0) {
        series_idx = create_series(metric, labels, label_count);
    }
    
    if (series_idx >= 0) {
        metric->series[series_idx].value += 1.0;
        metric->series[series_idx].timestamp = agentos_time_ms();
        metric->series[series_idx].update_count++;
    }
    
    agentos_mutex_unlock(&metric->lock);
    return AGENTOS_OK;
}

/**
 * @brief 记录 Counter（增加值）
 */
int metrics_counter_add(const char* name, double value, const metric_label_t* labels, size_t label_count) {
    if (!name || value < 0 || !g_metrics.initialized) {
        return AGENTOS_ERR_INVALID_PARAM;
    }
    
    agentos_mutex_lock(&g_metrics.global_lock);
    
    int idx = find_metric_index(name);
    if (idx < 0) {
        agentos_mutex_unlock(&g_metrics.global_lock);
        return AGENTOS_ERR_NOT_FOUND;
    }
    
    metric_t* metric = &g_metrics.metrics[idx];
    agentos_mutex_lock(&metric->lock);
    agentos_mutex_unlock(&g_metrics.global_lock);
    
    int series_idx = find_series_index(metric, labels, label_count);
    if (series_idx < 0) {
        series_idx = create_series(metric, labels, label_count);
    }
    
    if (series_idx >= 0) {
        metric->series[series_idx].value += value;
        metric->series[series_idx].timestamp = agentos_time_ms();
        metric->series[series_idx].update_count++;
    }
    
    agentos_mutex_unlock(&metric->lock);
    return AGENTOS_OK;
}

/**
 * @brief 设置 Gauge
 */
int metrics_gauge_set(const char* name, double value, const metric_label_t* labels, size_t label_count) {
    if (!name || !g_metrics.initialized) {
        return AGENTOS_ERR_INVALID_PARAM;
    }
    
    agentos_mutex_lock(&g_metrics.global_lock);
    
    int idx = find_metric_index(name);
    if (idx < 0) {
        agentos_mutex_unlock(&g_metrics.global_lock);
        return AGENTOS_ERR_NOT_FOUND;
    }
    
    metric_t* metric = &g_metrics.metrics[idx];
    agentos_mutex_lock(&metric->lock);
    agentos_mutex_unlock(&g_metrics.global_lock);
    
    int series_idx = find_series_index(metric, labels, label_count);
    if (series_idx < 0) {
        series_idx = create_series(metric, labels, label_count);
    }
    
    if (series_idx >= 0) {
        metric->series[series_idx].value = value;
        metric->series[series_idx].timestamp = agentos_time_ms();
        metric->series[series_idx].update_count++;
    }
    
    agentos_mutex_unlock(&metric->lock);
    return AGENTOS_OK;
}

/**
 * @brief 记录 Histogram
 */
int metrics_histogram_observe(const char* name, double value, const metric_label_t* labels, size_t label_count) {
    if (!name || !g_metrics.initialized) {
        return AGENTOS_ERR_INVALID_PARAM;
    }
    
    agentos_mutex_lock(&g_metrics.global_lock);
    
    int idx = find_metric_index(name);
    if (idx < 0) {
        agentos_mutex_unlock(&g_metrics.global_lock);
        return AGENTOS_ERR_NOT_FOUND;
    }
    
    metric_t* metric = &g_metrics.metrics[idx];
    agentos_mutex_lock(&metric->lock);
    agentos_mutex_unlock(&g_metrics.global_lock);
    
    /* 更新直方图桶 */
    for (size_t i = 0; i < metric->histogram.bucket_count; i++) {
        if (value <= metric->histogram.buckets[i].boundary) {
            metric->histogram.buckets[i].count++;
        }
    }
    
    metric->histogram.sum += value;
    metric->histogram.count++;
    
    /* 同时更新时间序列 */
    int series_idx = find_series_index(metric, labels, label_count);
    if (series_idx < 0) {
        series_idx = create_series(metric, labels, label_count);
    }
    
    if (series_idx >= 0) {
        metric->series[series_idx].value = value;
        metric->series[series_idx].timestamp = agentos_time_ms();
        metric->series[series_idx].update_count++;
    }
    
    agentos_mutex_unlock(&metric->lock);
    return AGENTOS_OK;
}

/**
 * @brief 导出为 Prometheus 格式
 */
char* metrics_export_prometheus(void) {
    if (!g_metrics.initialized) {
        return NULL;
    }
    
    size_t buf_size = 64 * 1024;  /* 64KB */
    char* buf = (char*)malloc(buf_size);
    if (!buf) return NULL;
    
    size_t pos = 0;
    
    agentos_mutex_lock(&g_metrics.global_lock);
    
    for (size_t i = 0; i < g_metrics.metric_count && pos < buf_size - 1024; i++) {
        metric_t* metric = &g_metrics.metrics[i];
        agentos_mutex_lock(&metric->lock);
        
        /* 写入 HELP 注释 */
        if (metric->description) {
            pos += snprintf(buf + pos, buf_size - pos, 
                           "# HELP %s %s\n", metric->name, metric->description);
        }
        
        /* 写入 TYPE 注释 */
        const char* type_str = "untyped";
        switch (metric->type) {
            case METRIC_TYPE_COUNTER: type_str = "counter"; break;
            case METRIC_TYPE_GAUGE: type_str = "gauge"; break;
            case METRIC_TYPE_HISTOGRAM: type_str = "histogram"; break;
            case METRIC_TYPE_SUMMARY: type_str = "summary"; break;
        }
        pos += snprintf(buf + pos, buf_size - pos, 
                       "# TYPE %s %s\n", metric->name, type_str);
        
        /* 写入时间序列值 */
        for (size_t j = 0; j < metric->series_count && pos < buf_size - 256; j++) {
            metric_series_t* series = &metric->series[j];
            
            char labels_buf[1024];
            format_labels(labels_buf, sizeof(labels_buf), series->labels, series->label_count);
            
            pos += snprintf(buf + pos, buf_size - pos, 
                           "%s%s %.17g %llu\n", 
                           metric->name, labels_buf, series->value,
                           (unsigned long long)series->timestamp);
        }
        
        /* 写入直方图数据 */
        if (metric->type == METRIC_TYPE_HISTOGRAM && metric->histogram.bucket_count > 0) {
            for (size_t b = 0; b < metric->histogram.bucket_count && pos < buf_size - 256; b++) {
                pos += snprintf(buf + pos, buf_size - pos,
                               "%s_bucket{le=\"%.17g\"} %llu\n",
                               metric->name, metric->histogram.buckets[b].boundary,
                               (unsigned long long)metric->histogram.buckets[b].count);
            }
            pos += snprintf(buf + pos, buf_size - pos,
                           "%s_bucket{le=\"+Inf\"} %llu\n",
                           metric->name, (unsigned long long)metric->histogram.count);
            pos += snprintf(buf + pos, buf_size - pos,
                           "%s_sum %.17g\n", metric->name, metric->histogram.sum);
            pos += snprintf(buf + pos, buf_size - pos,
                           "%s_count %llu\n", metric->name, 
                           (unsigned long long)metric->histogram.count);
        }
        
        pos += snprintf(buf + pos, buf_size - pos, "\n");
        
        agentos_mutex_unlock(&metric->lock);
    }
    
    agentos_mutex_unlock(&g_metrics.global_lock);
    
    return buf;
}

/**
 * @brief 获取指标值
 */
int metrics_get_value(const char* name, const metric_label_t* labels, size_t label_count, double* value) {
    if (!name || !value || !g_metrics.initialized) {
        return AGENTOS_ERR_INVALID_PARAM;
    }
    
    agentos_mutex_lock(&g_metrics.global_lock);
    
    int idx = find_metric_index(name);
    if (idx < 0) {
        agentos_mutex_unlock(&g_metrics.global_lock);
        return AGENTOS_ERR_NOT_FOUND;
    }
    
    metric_t* metric = &g_metrics.metrics[idx];
    agentos_mutex_lock(&metric->lock);
    agentos_mutex_unlock(&g_metrics.global_lock);
    
    int series_idx = find_series_index(metric, labels, label_count);
    if (series_idx < 0) {
        agentos_mutex_unlock(&metric->lock);
        return AGENTOS_ERR_NOT_FOUND;
    }
    
    *value = metric->series[series_idx].value;
    
    agentos_mutex_unlock(&metric->lock);
    return AGENTOS_OK;
}

/**
 * @brief 获取所有指标数量
 */
size_t metrics_get_count(void) {
    return g_metrics.metric_count;
}

/**
 * @brief 获取时间序列数量
 */
size_t metrics_get_series_count(const char* name) {
    if (!name || !g_metrics.initialized) {
        return 0;
    }
    
    agentos_mutex_lock(&g_metrics.global_lock);
    
    int idx = find_metric_index(name);
    if (idx < 0) {
        agentos_mutex_unlock(&g_metrics.global_lock);
        return 0;
    }
    
    metric_t* metric = &g_metrics.metrics[idx];
    agentos_mutex_lock(&metric->lock);
    agentos_mutex_unlock(&g_metrics.global_lock);
    
    size_t count = metric->series_count;
    
    agentos_mutex_unlock(&metric->lock);
    return count;
}
