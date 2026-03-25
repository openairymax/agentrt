/**
 * @file telemetry.c
 * @brief 可观测性实现（Prometheus 格式指标）
 * 
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "telemetry.h"
#include "logger.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* 最大指标数量 */
#define MAX_METRICS 256

/* 指标值结构 */
typedef struct metric_value {
    char*               label_values;
    double              value;
    uint64_t            count;      /* 用于直方图 */
    double              sum;        /* 用于直方图 */
    struct metric_value* next;
} metric_value_t;

/* 指标结构 */
typedef struct metric {
    char                name[64];
    char                help[256];
    metric_type_t       type;
    char*               labels;         /* 标签名称 */
    metric_value_t*     values;         /* 值链表 */
    pthread_mutex_t     lock;
} metric_t;

/* 可观测性结构 */
struct telemetry {
    metric_t            metrics[MAX_METRICS];
    size_t              metric_count;
    pthread_mutex_t     lock;
};

/* ========== 辅助函数 ========== */

/**
 * @brief 查找或创建指标值
 */
static metric_value_t* get_or_create_value(
    metric_t* m,
    const char* label_values) {
    
    /* 查找现有值 */
    metric_value_t* v = m->values;
    while (v) {
        if (strcmp(v->label_values, label_values ? label_values : "") == 0) {
            return v;
        }
        v = v->next;
    }
    
    /* 创建新值 */
    v = (metric_value_t*)calloc(1, sizeof(metric_value_t));
    if (!v) return NULL;
    
    v->label_values = strdup(label_values ? label_values : "");
    if (!v->label_values) {
        free(v);
        return NULL;
    }
    
    v->next = m->values;
    m->values = v;
    
    return v;
}

/**
 * @brief 查找指标
 */
static metric_t* find_metric(telemetry_t* t, const char* name) {
    for (size_t i = 0; i < t->metric_count; i++) {
        if (strcmp(t->metrics[i].name, name) == 0) {
            return &t->metrics[i];
        }
    }
    return NULL;
}

/* ========== 公共 API 实现 ========== */

telemetry_t* telemetry_create(void) {
    telemetry_t* t = (telemetry_t*)calloc(1, sizeof(telemetry_t));
    if (!t) return NULL;
    
    if (pthread_mutex_init(&t->lock, NULL) != 0) {
        free(t);
        return NULL;
    }
    
    /* 注册默认指标 */
    telemetry_register_counter(t, "dynamic_requests_total",
        "Total number of requests", "gateway,method");
    telemetry_register_counter(t, "dynamic_requests_failed_total",
        "Total number of failed requests", "gateway");
    telemetry_register_gauge(t, "dynamic_connections_active",
        "Number of active connections", "gateway");
    telemetry_register_gauge(t, "dynamic_sessions_active",
        "Number of active sessions", NULL);
    
    return t;
}

void telemetry_destroy(telemetry_t* t) {
    if (!t) return;
    
    pthread_mutex_lock(&t->lock);
    
    for (size_t i = 0; i < t->metric_count; i++) {
        metric_t* m = &t->metrics[i];
        free(m->labels);
        
        metric_value_t* v = m->values;
        while (v) {
            metric_value_t* next = v->next;
            free(v->label_values);
            free(v);
            v = next;
        }
        
        pthread_mutex_destroy(&m->lock);
    }
    
    pthread_mutex_unlock(&t->lock);
    pthread_mutex_destroy(&t->lock);
    free(t);
}

agentos_error_t telemetry_register_counter(
    telemetry_t* t,
    const char* name,
    const char* help,
    const char* labels) {
    
    if (!t || !name) return AGENTOS_EINVAL;
    
    pthread_mutex_lock(&t->lock);
    
    if (t->metric_count >= MAX_METRICS) {
        pthread_mutex_unlock(&t->lock);
        return AGENTOS_EBUSY;
    }
    
    /* 检查是否已存在 */
    if (find_metric(t, name)) {
        pthread_mutex_unlock(&t->lock);
        return AGENTOS_SUCCESS;
    }
    
    metric_t* m = &t->metrics[t->metric_count++];
    
    strncpy(m->name, name, sizeof(m->name) - 1);
    strncpy(m->help, help ? help : "", sizeof(m->help) - 1);
    m->type = METRIC_TYPE_COUNTER;
    m->labels = labels ? strdup(labels) : NULL;
    m->values = NULL;
    pthread_mutex_init(&m->lock, NULL);
    
    pthread_mutex_unlock(&t->lock);
    return AGENTOS_SUCCESS;
}

agentos_error_t telemetry_register_gauge(
    telemetry_t* t,
    const char* name,
    const char* help,
    const char* labels) {
    
    if (!t || !name) return AGENTOS_EINVAL;
    
    pthread_mutex_lock(&t->lock);
    
    if (t->metric_count >= MAX_METRICS) {
        pthread_mutex_unlock(&t->lock);
        return AGENTOS_EBUSY;
    }
    
    if (find_metric(t, name)) {
        pthread_mutex_unlock(&t->lock);
        return AGENTOS_SUCCESS;
    }
    
    metric_t* m = &t->metrics[t->metric_count++];
    
    strncpy(m->name, name, sizeof(m->name) - 1);
    strncpy(m->help, help ? help : "", sizeof(m->help) - 1);
    m->type = METRIC_TYPE_GAUGE;
    m->labels = labels ? strdup(labels) : NULL;
    m->values = NULL;
    pthread_mutex_init(&m->lock, NULL);
    
    pthread_mutex_unlock(&t->lock);
    return AGENTOS_SUCCESS;
}

agentos_error_t telemetry_increment_counter(
    telemetry_t* t,
    const char* name,
    double value,
    const char* label_values) {
    
    if (!t || !name) return AGENTOS_EINVAL;
    
    pthread_mutex_lock(&t->lock);
    metric_t* m = find_metric(t, name);
    pthread_mutex_unlock(&t->lock);
    
    if (!m) return AGENTOS_ENOENT;
    
    pthread_mutex_lock(&m->lock);
    metric_value_t* v = get_or_create_value(m, label_values);
    if (v) {
        v->value += value;
    }
    pthread_mutex_unlock(&m->lock);
    
    return v ? AGENTOS_SUCCESS : AGENTOS_ENOMEM;
}

agentos_error_t telemetry_set_gauge(
    telemetry_t* t,
    const char* name,
    double value,
    const char* label_values) {
    
    if (!t || !name) return AGENTOS_EINVAL;
    
    pthread_mutex_lock(&t->lock);
    metric_t* m = find_metric(t, name);
    pthread_mutex_unlock(&t->lock);
    
    if (!m) return AGENTOS_ENOENT;
    
    pthread_mutex_lock(&m->lock);
    metric_value_t* v = get_or_create_value(m, label_values);
    if (v) {
        v->value = value;
    }
    pthread_mutex_unlock(&m->lock);
    
    return v ? AGENTOS_SUCCESS : AGENTOS_ENOMEM;
}

agentos_error_t telemetry_observe_histogram(
    telemetry_t* t,
    const char* name,
    double value,
    const char* label_values) {
    
    if (!t || !name) return AGENTOS_EINVAL;
    
    pthread_mutex_lock(&t->lock);
    metric_t* m = find_metric(t, name);
    pthread_mutex_unlock(&t->lock);
    
    if (!m) return AGENTOS_ENOENT;
    
    pthread_mutex_lock(&m->lock);
    metric_value_t* v = get_or_create_value(m, label_values);
    if (v) {
        v->count++;
        v->sum += value;
    }
    pthread_mutex_unlock(&m->lock);
    
    return v ? AGENTOS_SUCCESS : AGENTOS_ENOMEM;
}

agentos_error_t telemetry_export_metrics(
    telemetry_t* t,
    char** out_metrics) {
    
    if (!t || !out_metrics) return AGENTOS_EINVAL;
    
    /* 计算所需缓冲区大小 */
    size_t size = 1024;  /* 初始大小 */
    
    pthread_mutex_lock(&t->lock);
    
    for (size_t i = 0; i < t->metric_count; i++) {
        metric_t* m = &t->metrics[i];
        size += 256;  /* HELP 和 TYPE 行 */
        
        pthread_mutex_lock(&m->lock);
        for (metric_value_t* v = m->values; v; v = v->next) {
            size += 256;
        }
        pthread_mutex_unlock(&m->lock);
    }
    
    /* 分配缓冲区 */
    char* buf = (char*)malloc(size);
    if (!buf) {
        pthread_mutex_unlock(&t->lock);
        return AGENTOS_ENOMEM;
    }
    
    char* p = buf;
    char* end = buf + size;
    
    /* 导出每个指标 */
    for (size_t i = 0; i < t->metric_count; i++) {
        metric_t* m = &t->metrics[i];
        
        /* HELP 行 */
        if (m->help[0]) {
            p += snprintf(p, end - p, "# HELP %s %s\n", m->name, m->help);
        }
        
        /* TYPE 行 */
        const char* type_str = 
            (m->type == METRIC_TYPE_COUNTER) ? "counter" :
            (m->type == METRIC_TYPE_GAUGE) ? "gauge" : "histogram";
        p += snprintf(p, end - p, "# TYPE %s %s\n", m->name, type_str);
        
        /* 值行 */
        pthread_mutex_lock(&m->lock);
        for (metric_value_t* v = m->values; v; v = v->next) {
            if (v->label_values[0]) {
                p += snprintf(p, end - p, "%s{%s} %.17g\n",
                    m->name, v->label_values, v->value);
            } else {
                p += snprintf(p, end - p, "%s %.17g\n", m->name, v->value);
            }
        }
        pthread_mutex_unlock(&m->lock);
        
        p += snprintf(p, end - p, "\n");
    }
    
    pthread_mutex_unlock(&t->lock);
    
    *out_metrics = buf;
    return AGENTOS_SUCCESS;
}
