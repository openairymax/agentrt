/**
 * @file test_domes_metrics.c
 * @brief domes 性能指标单元测试
 * @author Spharx
 * @date 2024
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../../src/domes_metrics.h"
#include "../../src/platform/platform.h"

#define TEST_PASS(name) printf("[PASS] %s\n", name)
#define TEST_FAIL(name, msg) do { printf("[FAIL] %s: %s\n", name, msg); return 1; } while(0)

/* ============================================================================
 * 指标基础功能测试
 * ============================================================================ */

static int test_metrics_init_shutdown(void) {
    assert(metrics_init(1000) == 0);
    metrics_shutdown();

    assert(metrics_init(500) == 0);
    metrics_shutdown();

    TEST_PASS("metrics_init_shutdown");
    return 0;
}

static int test_metrics_counter(void) {
    assert(metrics_init(1000) == 0);

    const char* labels[] = {"agent1", "read"};
    const char* label_names[] = {"agent", "action"};

    metrics_counter_inc(METRIC_PERMISSIONS_TOTAL, labels, 1.0);
    metrics_counter_inc(METRIC_PERMISSIONS_TOTAL, labels, 2.0);

    char buffer[4096];
    size_t len = metrics_export_prometheus(buffer, sizeof(buffer));
    assert(len > 0);
    assert(strstr(buffer, "domes_permissions_total") != NULL);

    metrics_reset();
    metrics_shutdown();

    TEST_PASS("metrics_counter");
    return 0;
}

static int test_metrics_gauge(void) {
    assert(metrics_init(1000) == 0);

    const char* labels[] = {"test_gauge"};
    metrics_gauge_set(METRIC_THREAD_COUNT, labels, 10.0);
    metrics_gauge_add(METRIC_THREAD_COUNT, labels, 5.0);
    metrics_gauge_sub(METRIC_THREAD_COUNT, labels, 3.0);

    char buffer[4096];
    size_t len = metrics_export_json(buffer, sizeof(buffer));
    assert(len > 0);

    metrics_shutdown();

    TEST_PASS("metrics_gauge");
    return 0;
}

static int test_metrics_histogram(void) {
    assert(metrics_init(1000) == 0);

    const char* labels[] = {"read_op"};

    for (int i = 0; i < 100; i++) {
        double value = (double)(i + 1) * 0.001;
        metrics_histogram_observe(METRIC_PERMISSIONS_DURATION_SECONDS, labels, value);
    }

    char buffer[4096];
    size_t len = metrics_export_prometheus(buffer, sizeof(buffer));
    assert(len > 0);
    assert(strstr(buffer, "_bucket{le=") != NULL);
    assert(strstr(buffer, "_sum") != NULL);
    assert(strstr(buffer, "_count") != NULL);

    metrics_shutdown();

    TEST_PASS("metrics_histogram");
    return 0;
}

static int test_metrics_export_formats(void) {
    assert(metrics_init(1000) == 0);

    metrics_counter_inc(METRIC_PERMISSIONS_TOTAL, NULL, 1.0);
    metrics_gauge_set(METRIC_THREAD_COUNT, NULL, 4.0);

    char prometheus_buf[4096];
    char json_buf[4096];

    size_t prometheus_len = metrics_export_prometheus(prometheus_buf, sizeof(prometheus_buf));
    size_t json_len = metrics_export_json(json_buf, sizeof(json_buf));

    assert(prometheus_len > 0);
    assert(json_len > 0);
    assert(strstr(prometheus_buf, "# HELP") != NULL);
    assert(strstr(prometheus_buf, "# TYPE") != NULL);
    assert(strstr(json_buf, "{\"metrics\":[") != NULL);

    metrics_shutdown();

    TEST_PASS("metrics_export_formats");
    return 0;
}

static int test_metrics_predefined_names(void) {
    assert(strcmp(METRIC_PERMISSIONS_TOTAL, "domes_permissions_total") == 0);
    assert(strcmp(METRIC_SANITIZER_INPUT_TOTAL, "domes_sanitizer_input_total") == 0);
    assert(strcmp(METRIC_WORKBENCH_EXECUTIONS_TOTAL, "domes_workbench_executions_total") == 0);
    assert(strcmp(METRIC_ERRORS_TOTAL, "domes_errors_total") == 0);

    TEST_PASS("metrics_predefined_names");
    return 0;
}

/* ============================================================================
 * 主测试入口
 * ============================================================================ */

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    printf("========================================\n");
    printf("domes Metrics Unit Tests\n");
    printf("========================================\n\n");

    printf("--- Metrics Basic Tests ---\n");
    if (test_metrics_init_shutdown() != 0) return 1;
    if (test_metrics_counter() != 0) return 1;
    if (test_metrics_gauge() != 0) return 1;
    if (test_metrics_histogram() != 0) return 1;
    if (test_metrics_export_formats() != 0) return 1;
    if (test_metrics_predefined_names() != 0) return 1;

    printf("\n========================================\n");
    printf("All metrics tests passed!\n");
    printf("========================================\n");

    return 0;
}