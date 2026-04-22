/**
 * @file test_metrics.c
 * @brief 指标收集模块单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include "monitor_service.h"

static void test_monitor_service_create_destroy(void) {
    printf("  test_monitor_service_create_destroy...\n");

    monitor_service_t* svc = NULL;
    int ret = monitor_service_create(NULL, &svc);
    assert(ret == 0);
    assert(svc != NULL);

    ret = monitor_service_destroy(svc);
    assert(ret == 0);

    printf("    PASSED\n");
}

static void test_monitor_record_metric(void) {
    printf("  test_monitor_record_metric...\n");

    monitor_service_t* svc = NULL;
    int ret = monitor_service_create(NULL, &svc);
    assert(ret == 0);

    metric_info_t metric;
    memset(&metric, 0, sizeof(metric));
    metric.name = "test_counter";
    metric.description = "Test counter metric";
    metric.type = METRIC_TYPE_COUNTER;
    metric.value = 42.0;

    ret = monitor_service_record_metric(svc, &metric);
    assert(ret == 0);

    monitor_service_destroy(svc);

    printf("    PASSED\n");
}

static void test_monitor_gauge_metric(void) {
    printf("  test_monitor_gauge_metric...\n");

    monitor_service_t* svc = NULL;
    int ret = monitor_service_create(NULL, &svc);
    assert(ret == 0);

    metric_info_t metric;
    memset(&metric, 0, sizeof(metric));
    metric.name = "memory_usage_bytes";
    metric.description = "Memory usage";
    metric.type = METRIC_TYPE_GAUGE;
    metric.value = 1024.0;

    ret = monitor_service_record_metric(svc, &metric);
    assert(ret == 0);

    monitor_service_destroy(svc);

    printf("    PASSED\n");
}

static void test_monitor_histogram_metric(void) {
    printf("  test_monitor_histogram_metric...\n");

    monitor_service_t* svc = NULL;
    int ret = monitor_service_create(NULL, &svc);
    assert(ret == 0);

    metric_info_t metric;
    memset(&metric, 0, sizeof(metric));
    metric.name = "request_duration_ms";
    metric.description = "Request duration histogram";
    metric.type = METRIC_TYPE_HISTOGRAM;
    metric.value = 150.5;

    ret = monitor_service_record_metric(svc, &metric);
    assert(ret == 0);

    monitor_service_destroy(svc);

    printf("    PASSED\n");
}

static void test_monitor_get_metrics(void) {
    printf("  test_monitor_get_metrics...\n");

    monitor_service_t* svc = NULL;
    int ret = monitor_service_create(NULL, &svc);
    assert(ret == 0);

    metric_info_t metric;
    memset(&metric, 0, sizeof(metric));
    metric.name = "requests_total";
    metric.description = "Total requests";
    metric.type = METRIC_TYPE_COUNTER;
    metric.value = 100.0;

    monitor_service_record_metric(svc, &metric);

    metric_info_t** metrics = NULL;
    size_t count = 0;
    ret = monitor_service_get_metrics(svc, "requests_total", &metrics, &count);
    if (ret == 0 && metrics != NULL) {
        printf("    Found %zu metrics\n", count);
    }

    monitor_service_destroy(svc);

    printf("    PASSED\n");
}

static void test_monitor_labels(void) {
    printf("  test_monitor_labels...\n");

    monitor_service_t* svc = NULL;
    int ret = monitor_service_create(NULL, &svc);
    assert(ret == 0);

    char* labels[] = {"service:llm_d", "model:gpt-4"};
    metric_info_t metric;
    memset(&metric, 0, sizeof(metric));
    metric.name = "api_calls";
    metric.description = "API calls";
    metric.type = METRIC_TYPE_COUNTER;
    metric.labels = labels;
    metric.label_count = 2;
    metric.value = 1.0;

    ret = monitor_service_record_metric(svc, &metric);
    assert(ret == 0);

    monitor_service_destroy(svc);

    printf("    PASSED\n");
}

int main(void) {
    printf("=========================================\n");
    printf("  Metrics Collector Unit Tests\n");
    printf("=========================================\n");

    test_monitor_service_create_destroy();
    test_monitor_record_metric();
    test_monitor_gauge_metric();
    test_monitor_histogram_metric();
    test_monitor_get_metrics();
    test_monitor_labels();

    printf("\nAll metrics tests PASSED\n");
    return 0;
}
