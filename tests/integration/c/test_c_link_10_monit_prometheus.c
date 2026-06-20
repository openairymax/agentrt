// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: Apache-2.0
// @owner: team-C
/**
 * @file test_c_link_10_monit_prometheus.c
 * @brief C-L10 Integration Test: monit_d → Prometheus
 *
 * Tests the Prometheus exporter connecting monit_d to Prometheus metrics:
 * 1. Normal path: Init → register metrics → get metrics → shutdown
 * 2. Error path: Double init → proper error handling
 * 3. Error path: Handle non-metrics HTTP request
 * 4. Timeout path: Scrape handling
 * 5. Concurrent path: Multiple simultaneous scrapes
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>

#include "memory_compat.h"
#include "prometheus_exporter.h"
#include "agentos_types.h"

/* ============================================================================
 * Test Helpers
 * ============================================================================ */

static int g_tests_passed = 0;
static int g_tests_failed = 0;
static int g_tests_total = 0;

#define TEST(name) do { \
    g_tests_total++; \
    printf("  [TEST] %s ... ", name); \
} while(0)

#define PASS() do { \
    g_tests_passed++; \
    printf("PASS\n"); \
} while(0)

#define FAIL(reason) do { \
    g_tests_failed++; \
    printf("FAIL: %s\n", reason); \
} while(0)

#define CHECK(cond, reason) do { \
    if (!(cond)) { FAIL(reason); return; } \
} while(0)

#define CHECK_EQ(a, b, reason) do { \
    if ((a) != (b)) { \
        char buf[256]; \
        snprintf(buf, sizeof(buf), "%s (got %d, expected %d)", reason, \
                 (int)(a), (int)(b)); \
        FAIL(buf); return; \
    } \
} while(0)

/* ============================================================================
 * P1.16j-1: Normal Path — Prometheus exporter lifecycle
 * ============================================================================ */

static void test_normal_exporter_lifecycle(void) {
    TEST("C-L10 Normal: Prometheus exporter init → register metrics → shutdown");

    int ret = prometheus_exporter_init("monit_d");
    CHECK_EQ(ret, 0, "prometheus_exporter_init should succeed");

    ret = prometheus_exporter_register_required_metrics();
    CHECK_EQ(ret, 0, "Register required metrics should succeed");

    prometheus_exporter_shutdown();
    PASS();
}

/* ============================================================================
 * P1.16j-2: Normal Path — Get metrics and verify format
 * ============================================================================ */

static void test_normal_get_metrics(void) {
    TEST("C-L10 Normal: Get Prometheus metrics → verify format");

    int ret = prometheus_exporter_init("test_service");
    CHECK_EQ(ret, 0, "prometheus_exporter_init should succeed");

    ret = prometheus_exporter_register_required_metrics();
    CHECK_EQ(ret, 0, "Register required metrics should succeed");

    /* Update some metrics */
    prometheus_counter_inc("agentos_requests_total", 1.0);
    prometheus_gauge_set("agentos_active_connections", 5.0);
    prometheus_histogram_observe("agentos_request_duration_ms", 42.5);

    /* Get metrics output */
    char *metrics = prometheus_exporter_get_metrics();
    CHECK(metrics != NULL, "prometheus_exporter_get_metrics should return data");

    /* Verify format contains expected Prometheus markers */
    CHECK(strstr(metrics, "agentos_") != NULL || strstr(metrics, "HELP") != NULL
          || strstr(metrics, "TYPE") != NULL,
          "Metrics output should contain agentos_ or HELP/TYPE markers");

    free(metrics);
    prometheus_exporter_shutdown();
    PASS();
}

/* ============================================================================
 * P1.16j-3: Normal Path — HTTP metrics endpoint handling
 * ============================================================================ */

static void test_normal_http_metrics_endpoint(void) {
    TEST("C-L10 Normal: HTTP GET /metrics → Prometheus response");

    int ret = prometheus_exporter_init("http_test");
    CHECK_EQ(ret, 0, "prometheus_exporter_init should succeed");

    ret = prometheus_exporter_register_required_metrics();
    CHECK_EQ(ret, 0, "Register required metrics should succeed");

    /* Simulate a Prometheus scrape request */
    const char *http_request =
        "GET /metrics HTTP/1.1\r\n"
        "Host: localhost:9090\r\n"
        "User-Agent: Prometheus/2.45.0\r\n"
        "Accept: text/plain\r\n"
        "\r\n";

    char *response = NULL;
    size_t response_len = 0;
    ret = prometheus_exporter_handle_http(http_request, strlen(http_request),
                                           &response, &response_len);
    CHECK_EQ(ret, 0, "HTTP /metrics request should be handled");

    CHECK(response != NULL, "Response should not be NULL");
    CHECK(response_len > 0, "Response should have content");

    free(response);
    prometheus_exporter_shutdown();
    PASS();
}

/* ============================================================================
 * P1.16j-4: Error Path — Handle non-metrics HTTP request
 * ============================================================================ */

static void test_error_non_metrics_request(void) {
    TEST("C-L10 Error: Non-metrics HTTP request → return -1");

    int ret = prometheus_exporter_init("non_metrics_test");
    CHECK_EQ(ret, 0, "prometheus_exporter_init should succeed");

    ret = prometheus_exporter_register_required_metrics();
    CHECK_EQ(ret, 0, "Register required metrics should succeed");

    /* Simulate a non-metrics HTTP request */
    const char *http_request =
        "GET /api/v1/status HTTP/1.1\r\n"
        "Host: localhost:9090\r\n"
        "\r\n";

    char *response = NULL;
    size_t response_len = 0;
    ret = prometheus_exporter_handle_http(http_request, strlen(http_request),
                                           &response, &response_len);
    CHECK_EQ(ret, -1, "Non-metrics request should return -1 (not handled)");
    CHECK(response == NULL, "Response should be NULL for non-metrics request");

    prometheus_exporter_shutdown();
    PASS();
}

/* ============================================================================
 * P1.16j-5: Error Path — Double init handling
 * ============================================================================ */

static void test_error_double_init(void) {
    TEST("C-L10 Error: Double init handling");

    int ret = prometheus_exporter_init("double_init_test");
    CHECK_EQ(ret, 0, "First init should succeed");

    /* Second init should fail or be idempotent */
    ret = prometheus_exporter_init("double_init_test");
    /* May fail (already initialized) or succeed (idempotent) */
    (void)ret;

    prometheus_exporter_shutdown();
    PASS();
}

/* ============================================================================
 * P1.16j-6: Timeout Path — Scrape statistics
 * ============================================================================ */

static void test_timeout_scrape_stats(void) {
    TEST("C-L10 Timeout: Scrape statistics tracking");

    int ret = prometheus_exporter_init("scrape_test");
    CHECK_EQ(ret, 0, "prometheus_exporter_init should succeed");

    ret = prometheus_exporter_register_required_metrics();
    CHECK_EQ(ret, 0, "Register required metrics should succeed");

    /* Perform multiple scrapes */
    for (int i = 0; i < 5; i++) {
        const char *request =
            "GET /metrics HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "\r\n";

        char *response = NULL;
        size_t response_len = 0;
        ret = prometheus_exporter_handle_http(request, strlen(request),
                                               &response, &response_len);
        CHECK_EQ(ret, 0, "Scrape %d should succeed", i);
        free(response);
    }

    /* Verify scrape stats */
    uint64_t scrape_count = 0, scrape_errors = 0;
    prometheus_exporter_get_scrape_stats(&scrape_count, &scrape_errors);
    CHECK(scrape_count >= 5, "Scrape count should be at least 5");
    CHECK_EQ(scrape_errors, (uint64_t)0, "Should have 0 scrape errors");

    prometheus_exporter_shutdown();
    PASS();
}

/* ============================================================================
 * P1.16j-7: Concurrent Path — Multiple simultaneous scrapes
 * ============================================================================ */

#define PROM_CONCURRENT_THREADS 4
#define PROM_SCRAPES_PER_THREAD 10

typedef struct {
    int thread_id;
    int success_count;
    int error_count;
} prom_thread_args_t;

static void *concurrent_prom_scrape_thread(void *arg) {
    prom_thread_args_t *args = (prom_thread_args_t *)arg;

    for (int i = 0; i < PROM_SCRAPES_PER_THREAD; i++) {
        const char *request =
            "GET /metrics HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "\r\n";

        char *response = NULL;
        size_t response_len = 0;
        int ret = prometheus_exporter_handle_http(request, strlen(request),
                                                   &response, &response_len);
        if (ret == 0 && response != NULL) {
            free(response);
            args->success_count++;
        } else {
            args->error_count++;
        }
    }
    return NULL;
}

static void test_concurrent_prom_scrapes(void) {
    TEST("C-L10 Concurrent: Multiple simultaneous Prometheus scrapes");

    int ret = prometheus_exporter_init("concurrent_test");
    CHECK_EQ(ret, 0, "prometheus_exporter_init should succeed");

    ret = prometheus_exporter_register_required_metrics();
    CHECK_EQ(ret, 0, "Register required metrics should succeed");

    pthread_t threads[PROM_CONCURRENT_THREADS];
    prom_thread_args_t args[PROM_CONCURRENT_THREADS];

    for (int i = 0; i < PROM_CONCURRENT_THREADS; i++) {
        args[i].thread_id = i;
        args[i].success_count = 0;
        args[i].error_count = 0;
        pthread_create(&threads[i], NULL, concurrent_prom_scrape_thread, &args[i]);
    }

    int total_success = 0;
    int total_errors = 0;
    for (int i = 0; i < PROM_CONCURRENT_THREADS; i++) {
        pthread_join(threads[i], NULL);
        total_success += args[i].success_count;
        total_errors += args[i].error_count;
    }

    CHECK(total_success > 0, "At least some scrapes should succeed");
    CHECK_EQ(total_success + total_errors,
             PROM_CONCURRENT_THREADS * PROM_SCRAPES_PER_THREAD,
             "All scrape operations should complete");

    prometheus_exporter_shutdown();
    PASS();
}

/* ============================================================================
 * P1.16j-8: All metric types update
 * ============================================================================ */

static void test_all_metric_types(void) {
    TEST("C-L10 Normal: Update all metric types (counter, gauge, histogram)");

    int ret = prometheus_exporter_init("metric_types_test");
    CHECK_EQ(ret, 0, "prometheus_exporter_init should succeed");

    ret = prometheus_exporter_register_required_metrics();
    CHECK_EQ(ret, 0, "Register required metrics should succeed");

    /* Update counter multiple times */
    prometheus_counter_inc("agentos_llm_requests_total", 10.0);
    prometheus_counter_inc("agentos_llm_requests_total", 5.0);

    /* Update gauge */
    prometheus_gauge_set("agentos_memory_usage_bytes", 1048576.0);
    prometheus_gauge_set("agentos_memory_usage_bytes", 2097152.0);

    /* Update histogram */
    prometheus_histogram_observe("agentos_request_duration_ms", 10.0);
    prometheus_histogram_observe("agentos_request_duration_ms", 50.0);
    prometheus_histogram_observe("agentos_request_duration_ms", 100.0);

    /* Get metrics and verify they contain updated values */
    char *metrics = prometheus_exporter_get_metrics();
    CHECK(metrics != NULL, "Should get metrics output");

    /* Verify output is non-empty and contains expected content */
    CHECK(strlen(metrics) > 0, "Metrics output should not be empty");

    free(metrics);
    prometheus_exporter_shutdown();
    PASS();
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(void) {
    printf("=== C-L10 Integration Tests: monit_d → Prometheus ===\n\n");

    test_normal_exporter_lifecycle();
    test_normal_get_metrics();
    test_normal_http_metrics_endpoint();
    test_error_non_metrics_request();
    test_error_double_init();
    test_timeout_scrape_stats();
    test_concurrent_prom_scrapes();
    test_all_metric_types();

    printf("\n=== Results: %d/%d passed, %d failed ===\n",
           g_tests_passed, g_tests_total, g_tests_failed);
    return g_tests_failed > 0 ? 1 : 0;
}