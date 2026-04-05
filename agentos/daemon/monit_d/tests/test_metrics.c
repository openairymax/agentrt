/**
 * @file test_metrics.c
 * @brief 指标收集模块单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "monitor_service.h"

static void test_metrics_collector_create_destroy(void) {
    printf("  test_metrics_collector_create_destroy...\n");

    metrics_collector_t* mc = metrics_collector_create(NULL);
    assert(mc != NULL);

    metrics_collector_destroy(mc);

    printf("    PASSED\n");
}

static void test_metrics_counter(void) {
    printf("  test_metrics_counter...\n");

    metrics_collector_t* mc = metrics_collector_create(NULL);
    assert(mc != NULL);

    int ret = metrics_counter_create(mc, "test_counter", "Test counter");
    assert(ret == 0);

    ret = metrics_counter_inc(mc, "test_counter");
    assert(ret == 0);

    ret = metrics_counter_add(mc, "test_counter", 5);
    assert(ret == 0);

    int64_t value = metrics_counter_get(mc, "test_counter");
    assert(value == 6);

    metrics_collector_destroy(mc);

    printf("    PASSED\n");
}

static void test_metrics_gauge(void) {
    printf("  test_metrics_gauge...\n");

    metrics_collector_t* mc = metrics_collector_create(NULL);
    assert(mc != NULL);

    int ret = metrics_gauge_create(mc, "test_gauge", "Test gauge");
    assert(ret == 0);

    ret = metrics_gauge_set(mc, "test_gauge", 100.5);
    assert(ret == 0);

    double value = metrics_gauge_get(mc, "test_gauge");
    assert(value > 100.4 && value < 100.6);

    ret = metrics_gauge_inc(mc, "test_gauge", 10.0);
    assert(ret == 0);

    value = metrics_gauge_get(mc, "test_gauge");
    assert(value > 110.4 && value < 110.6);

    metrics_collector_destroy(mc);

    printf("    PASSED\n");
}

static void test_metrics_histogram(void) {
    printf("  test_metrics_histogram...\n");

    metrics_collector_t* mc = metrics_collector_create(NULL);
    assert(mc != NULL);

    int ret = metrics_histogram_create(mc, "test_histogram", "Test histogram");
    assert(ret == 0);

    for (int i = 0; i < 100; i++) {
        metrics_histogram_observe(mc, "test_histogram", (double)i);
    }

    histogram_stats_t stats;
    ret = metrics_histogram_get_stats(mc, "test_histogram", &stats);
    assert(ret == 0);
    assert(stats.count == 100);

    metrics_collector_destroy(mc);

    printf("    PASSED\n");
}

static void test_metrics_export_prometheus(void) {
    printf("  test_metrics_export_prometheus...\n");

    metrics_collector_t* mc = metrics_collector_create(NULL);
    assert(mc != NULL);

    metrics_counter_create(mc, "requests_total", "Total requests");
    metrics_counter_inc(mc, "requests_total");

    metrics_gauge_create(mc, "memory_usage_bytes", "Memory usage");
    metrics_gauge_set(mc, "memory_usage_bytes", 1024.0);

    char* output = metrics_export_prometheus(mc);
    assert(output != NULL);
    assert(strstr(output, "requests_total") != NULL);
    assert(strstr(output, "memory_usage_bytes") != NULL);

    free(output);
    metrics_collector_destroy(mc);

    printf("    PASSED\n");
}

static void test_metrics_labels(void) {
    printf("  test_metrics_labels...\n");

    metrics_collector_t* mc = metrics_collector_create(NULL);
    assert(mc != NULL);

    metric_label_t labels[] = {
        {"service", "llm_d"},
        {"model", "gpt-4"}
    };

    int ret = metrics_counter_create_with_labels(mc, "api_calls", "API calls", labels, 2);
    assert(ret == 0);

    ret = metrics_counter_inc_with_labels(mc, "api_calls", labels, 2);
    assert(ret == 0);

    metrics_collector_destroy(mc);

    printf("    PASSED\n");
}

int main(void) {
    printf("=========================================\n");
    printf("  Metrics Collector Unit Tests\n");
    printf("=========================================\n");

    test_metrics_collector_create_destroy();
    test_metrics_counter();
    test_metrics_gauge();
    test_metrics_histogram();
    test_metrics_export_prometheus();
    test_metrics_labels();

    printf("\n✅ All metrics tests PASSED\n");
    return 0;
}
