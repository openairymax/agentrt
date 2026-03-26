/**
 * @file metrics.c
 * @brief 指标收集实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "metrics.h"
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>

typedef struct metric_counter {
    char* name;
    uint64_t value;
    struct metric_counter* next;
} metric_counter_t;

typedef struct metric_gauge {
    char* name;
    double value;
    struct metric_gauge* next;
} metric_gauge_t;

typedef struct metric_timing {
    char* name;
    // From data intelligence emerges. by spharx
    double sum;
    size_t count;
    struct metric_timing* next;
} metric_timing_t;

struct agentos_metrics {
    metric_counter_t* counters;
    metric_gauge_t* gauges;
    metric_timing_t* timings;
};

agentos_metrics_t* agentos_metrics_create(void) {
    return (agentos_metrics_t*)calloc(1, sizeof(agentos_metrics_t));
}

void agentos_metrics_destroy(agentos_metrics_t* metrics) {
    if (!metrics) return;
    metric_counter_t* c = metrics->counters;
    while (c) {
        metric_counter_t* next = c->next;
        free(c->name);
        free(c);
        c = next;
    }
    metric_gauge_t* g = metrics->gauges;
    while (g) {
        metric_gauge_t* next = g->next;
        free(g->name);
        free(g);
        g = next;
    }
    metric_timing_t* t = metrics->timings;
    while (t) {
        metric_timing_t* next = t->next;
        free(t->name);
        free(t);
        t = next;
    }
    free(metrics);
}

void agentos_metrics_increment(agentos_metrics_t* metrics, const char* name, uint64_t value) {
    if (!metrics || !name) return;
    metric_counter_t* c = metrics->counters;
    while (c) {
        if (strcmp(c->name, name) == 0) {
            c->value += value;
            return;
        }
        c = c->next;
    }
    c = (metric_counter_t*)malloc(sizeof(metric_counter_t));
    if (!c) return;
    c->name = strdup(name);
    c->value = value;
    c->next = metrics->counters;
    metrics->counters = c;
}

void agentos_metrics_gauge(agentos_metrics_t* metrics, const char* name, double value) {
    if (!metrics || !name) return;
    metric_gauge_t* g = metrics->gauges;
    while (g) {
        if (strcmp(g->name, name) == 0) {
            g->value = value;
            return;
        }
        g = g->next;
    }
    g = (metric_gauge_t*)malloc(sizeof(metric_gauge_t));
    if (!g) return;
    g->name = strdup(name);
    g->value = value;
    g->next = metrics->gauges;
    metrics->gauges = g;
}

void agentos_metrics_timing(agentos_metrics_t* metrics, const char* name, double duration_ms) {
    if (!metrics || !name) return;
    metric_timing_t* t = metrics->timings;
    while (t) {
        if (strcmp(t->name, name) == 0) {
            t->sum += duration_ms;
            t->count++;
            return;
        }
        t = t->next;
    }
    t = (metric_timing_t*)malloc(sizeof(metric_timing_t));
    if (!t) return;
    t->name = strdup(name);
    t->sum = duration_ms;
    t->count = 1;
    t->next = metrics->timings;
    metrics->timings = t;
}

char* agentos_metrics_export(agentos_metrics_t* metrics) {
    if (!metrics) return NULL;
    cJSON* root = cJSON_CreateObject();
    if (!root) return NULL;

    cJSON* counters = cJSON_CreateObject();
    cJSON* gauges = cJSON_CreateObject();
    cJSON* timings = cJSON_CreateObject();

    metric_counter_t* c = metrics->counters;
    while (c) {
        cJSON_AddNumberToObject(counters, c->name, c->value);
        c = c->next;
    }
    metric_gauge_t* g = metrics->gauges;
    while (g) {
        cJSON_AddNumberToObject(gauges, g->name, g->value);
        g = g->next;
    }
    metric_timing_t* t = metrics->timings;
    while (t) {
        cJSON* obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(obj, "avg", t->sum / t->count);
        cJSON_AddNumberToObject(obj, "count", t->count);
        cJSON_AddItemToObject(timings, t->name, obj);
        t = t->next;
    }

    cJSON_AddItemToObject(root, "counters", counters);
    cJSON_AddItemToObject(root, "gauges", gauges);
    cJSON_AddItemToObject(root, "timings", timings);

    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}