/*
 * Copyright (c) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
 *
 * @file test_memoryrov_throughput.c
 * @brief INT-07: MemoryRovol L1 Write Throughput Benchmark
 *
 * Benchmarks the MemoryRovol L1 raw storage write throughput.
 * Target: >10,000 records/s sequential write.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <pthread.h>

#include "memoryrovol.h"
#include "config.h"

/* ==================== Test Macros ==================== */

typedef struct {
    int passed;
    int failed;
    int total;
} TestStats;

static TestStats g_test_stats = {0, 0, 0};

#define TEST_ASSERT(condition, message)                                       \
    do {                                                                      \
        g_test_stats.total++;                                                 \
        if (condition) {                                                      \
            g_test_stats.passed++;                                            \
            printf("  PASS: %s\n", message);                                  \
        } else {                                                              \
            g_test_stats.failed++;                                            \
            fprintf(stderr, "  FAIL: %s (at %s:%d)\n", message, __FILE__,     \
                    __LINE__);                                                \
        }                                                                     \
    } while (0)

#define TEST_ASSERT_EQ(expected, actual, message)                             \
    do {                                                                      \
        g_test_stats.total++;                                                 \
        if ((expected) == (actual)) {                                         \
            g_test_stats.passed++;                                            \
            printf("  PASS: %s\n", message);                                  \
        } else {                                                              \
            g_test_stats.failed++;                                            \
            fprintf(stderr, "  FAIL: %s (expected=%d, actual=%d, at %s:%d)\n",\
                    message, (int)(expected), (int)(actual), __FILE__,        \
                    __LINE__);                                                \
        }                                                                     \
    } while (0)

#define TEST_CASE_START(name)                                                 \
    printf("\n============================================================\n");\
    printf("INT-07: %s\n", name);                                             \
    printf("============================================================\n")

#define TEST_CASE_END() printf("\n")

/* ==================== Timing Helpers ==================== */

static inline double timespec_diff_us(struct timespec *start, struct timespec *end)
{
    return (double)(end->tv_sec - start->tv_sec) * 1e6 +
           (double)(end->tv_nsec - start->tv_nsec) / 1e3;
}

static inline double timespec_diff_s(struct timespec *start, struct timespec *end)
{
    return (double)(end->tv_sec - start->tv_sec) +
           (double)(end->tv_nsec - start->tv_nsec) / 1e9;
}

/* ==================== Constants ==================== */

#define WARMUP_RECORDS      1000
#define SEQ_RECORDS         50000
#define BATCH_SIZE          100
#define BATCH_TOTAL         50000
#define MIXED_TOTAL         50000
#define CONCURRENT_THREADS  4
#define CONCURRENT_PER_THR  12500
#define LATENCY_SAMPLES     10000
#define LARGE_RECORD_1K     1024
#define LARGE_RECORD_10K    10240
#define LARGE_TOTAL         10000
#define THROUGHPUT_TARGET   10000.0  /* records/s */

/* ==================== Helpers ==================== */

static agentrt_memoryrov_handle_t *create_handle(void)
{
    agentrt_memoryrov_config_t *cfg = NULL;
    agentrt_error_t err = agentrt_memoryrov_config_default(&cfg);
    if (err != AGENTRT_OK || cfg == NULL) {
        return NULL;
    }
    agentrt_memoryrov_handle_t *handle = NULL;
    err = agentrt_memoryrov_init(cfg, &handle);
    agentrt_memoryrov_config_free(cfg);
    if (err != AGENTRT_OK) {
        return NULL;
    }
    return handle;
}

static void generate_payload(char *buf, size_t len, int seq)
{
    memset(buf, 'A' + (seq % 26), len - 1);
    buf[len - 1] = '\0';
}

static void free_record_ids(char **ids, int count)
{
    for (int i = 0; i < count; i++) {
        if (ids[i]) {
            free(ids[i]);
        }
    }
}

/* ==================== INT-07.1 Sequential Write Throughput ==================== */

static void test_sequential_write_throughput(void)
{
    TEST_CASE_START("07.1 Sequential write throughput");

    agentrt_memoryrov_handle_t *handle = create_handle();
    TEST_ASSERT(handle != NULL, "Handle creation succeeded");

    /* Warmup */
    char warmup_buf[64];
    char *warmup_id = NULL;
    for (int i = 0; i < WARMUP_RECORDS; i++) {
        generate_payload(warmup_buf, sizeof(warmup_buf), i);
        agentrt_error_t err = agentrt_memoryrov_write_raw(
            handle, warmup_buf, strlen(warmup_buf), NULL, &warmup_id);
        if (err == AGENTRT_OK && warmup_id) {
            free(warmup_id);
            warmup_id = NULL;
        }
    }

    /* Benchmark */
    char buf[64];
    char *record_ids[SEQ_RECORDS];
    memset(record_ids, 0, sizeof(record_ids));
    int write_ok = 0;

    struct timespec t_start, t_end;
    clock_gettime(CLOCK_MONOTONIC, &t_start);

    for (int i = 0; i < SEQ_RECORDS; i++) {
        generate_payload(buf, sizeof(buf), i);
        agentrt_error_t err = agentrt_memoryrov_write_raw(
            handle, buf, strlen(buf), NULL, &record_ids[i]);
        if (err == AGENTRT_OK) {
            write_ok++;
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &t_end);

    double elapsed_s = timespec_diff_s(&t_start, &t_end);
    double throughput = (double)write_ok / elapsed_s;

    printf("  Sequential write: %d/%d records in %.3f s\n",
           write_ok, SEQ_RECORDS, elapsed_s);
    printf("  Throughput: %.0f records/s\n", throughput);

    TEST_ASSERT(throughput > THROUGHPUT_TARGET,
                "Sequential write throughput > 10K records/s");

    /* Cleanup */
    for (int i = 0; i < SEQ_RECORDS; i++) {
        if (record_ids[i]) {
            agentrt_memoryrov_delete_raw(handle, record_ids[i]);
            free(record_ids[i]);
        }
    }
    agentrt_memoryrov_cleanup(handle);

    TEST_CASE_END();
}

/* ==================== INT-07.2 Batch Write Throughput ==================== */

static void test_batch_write_throughput(void)
{
    TEST_CASE_START("07.2 Batch write throughput");

    agentrt_memoryrov_handle_t *handle = create_handle();
    TEST_ASSERT(handle != NULL, "Handle creation succeeded");

    /* Warmup */
    char warmup_buf[64];
    char *wid = NULL;
    for (int i = 0; i < WARMUP_RECORDS; i++) {
        generate_payload(warmup_buf, sizeof(warmup_buf), i);
        agentrt_error_t err = agentrt_memoryrov_write_raw(
            handle, warmup_buf, strlen(warmup_buf), NULL, &wid);
        if (err == AGENTRT_OK && wid) {
            free(wid);
            wid = NULL;
        }
    }

    /* Batch benchmark: write in batches of BATCH_SIZE, measure total */
    int total_batches = BATCH_TOTAL / BATCH_SIZE;
    char buf[64];
    char *record_ids[BATCH_TOTAL];
    memset(record_ids, 0, sizeof(record_ids));
    int write_ok = 0;

    struct timespec t_start, t_end;
    clock_gettime(CLOCK_MONOTONIC, &t_start);

    for (int b = 0; b < total_batches; b++) {
        for (int j = 0; j < BATCH_SIZE; j++) {
            int idx = b * BATCH_SIZE + j;
            generate_payload(buf, sizeof(buf), idx);
            agentrt_error_t err = agentrt_memoryrov_write_raw(
                handle, buf, strlen(buf), NULL, &record_ids[idx]);
            if (err == AGENTRT_OK) {
                write_ok++;
            }
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &t_end);

    double elapsed_s = timespec_diff_s(&t_start, &t_end);
    double throughput = (double)write_ok / elapsed_s;

    printf("  Batch write (%d/batch): %d/%d records in %.3f s\n",
           BATCH_SIZE, write_ok, BATCH_TOTAL, elapsed_s);
    printf("  Throughput: %.0f records/s\n", throughput);

    TEST_ASSERT(throughput > THROUGHPUT_TARGET,
                "Batch write throughput > 10K records/s");

    /* Cleanup */
    for (int i = 0; i < BATCH_TOTAL; i++) {
        if (record_ids[i]) {
            agentrt_memoryrov_delete_raw(handle, record_ids[i]);
            free(record_ids[i]);
        }
    }
    agentrt_memoryrov_cleanup(handle);

    TEST_CASE_END();
}

/* ==================== INT-07.3 Mixed Read/Write Throughput ==================== */

static void test_mixed_read_write_throughput(void)
{
    TEST_CASE_START("07.3 Mixed read/write throughput");

    agentrt_memoryrov_handle_t *handle = create_handle();
    TEST_ASSERT(handle != NULL, "Handle creation succeeded");

    /* Pre-populate some records for reads */
    #define PREPOP_COUNT 5000
    char prepop_buf[64];
    char *prepop_ids[PREPOP_COUNT];
    memset(prepop_ids, 0, sizeof(prepop_ids));
    int prepop_ok = 0;

    for (int i = 0; i < PREPOP_COUNT; i++) {
        generate_payload(prepop_buf, sizeof(prepop_buf), i);
        agentrt_error_t err = agentrt_memoryrov_write_raw(
            handle, prepop_buf, strlen(prepop_buf), NULL, &prepop_ids[i]);
        if (err == AGENTRT_OK) {
            prepop_ok++;
        }
    }

    /* Warmup */
    char warmup_buf[64];
    char *wid = NULL;
    for (int i = 0; i < WARMUP_RECORDS; i++) {
        generate_payload(warmup_buf, sizeof(warmup_buf), i + PREPOP_COUNT);
        agentrt_error_t err = agentrt_memoryrov_write_raw(
            handle, warmup_buf, strlen(warmup_buf), NULL, &wid);
        if (err == AGENTRT_OK && wid) {
            free(wid);
            wid = NULL;
        }
    }

    /* Mixed benchmark: 70% writes, 30% reads */
    int write_count = (int)(MIXED_TOTAL * 0.7);
    int read_count = MIXED_TOTAL - write_count;
    char buf[64];
    char *new_ids = NULL;
    void *read_data = NULL;
    size_t read_len = 0;
    int ops_ok = 0;

    struct timespec t_start, t_end;
    clock_gettime(CLOCK_MONOTONIC, &t_start);

    for (int i = 0; i < MIXED_TOTAL; i++) {
        if ((i % 10) < 7) {
            /* Write */
            generate_payload(buf, sizeof(buf), i + PREPOP_COUNT + WARMUP_RECORDS);
            agentrt_error_t err = agentrt_memoryrov_write_raw(
                handle, buf, strlen(buf), NULL, &new_ids);
            if (err == AGENTRT_OK) {
                ops_ok++;
                if (new_ids) {
                    free(new_ids);
                    new_ids = NULL;
                }
            }
        } else {
            /* Read from pre-populated records */
            int ridx = i % prepop_ok;
            if (prepop_ids[ridx]) {
                agentrt_error_t err = agentrt_memoryrov_get_raw(
                    handle, prepop_ids[ridx], &read_data, &read_len);
                if (err == AGENTRT_OK) {
                    ops_ok++;
                    if (read_data) {
                        free(read_data);
                        read_data = NULL;
                    }
                }
            }
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &t_end);

    double elapsed_s = timespec_diff_s(&t_start, &t_end);
    double throughput = (double)ops_ok / elapsed_s;

    printf("  Mixed R/W (70%%W/30%%R): %d/%d ops in %.3f s\n",
           ops_ok, MIXED_TOTAL, elapsed_s);
    printf("  Combined throughput: %.0f ops/s\n", throughput);

    TEST_ASSERT(throughput > THROUGHPUT_TARGET,
                "Mixed R/W throughput > 10K ops/s");

    /* Cleanup */
    for (int i = 0; i < PREPOP_COUNT; i++) {
        if (prepop_ids[i]) {
            agentrt_memoryrov_delete_raw(handle, prepop_ids[i]);
            free(prepop_ids[i]);
        }
    }
    agentrt_memoryrov_cleanup(handle);

    #undef PREPOP_COUNT
    TEST_CASE_END();
}

/* ==================== INT-07.4 Concurrent Write Throughput ==================== */

typedef struct {
    agentrt_memoryrov_handle_t *handle;
    int record_count;
    int writes_ok;
    int thread_id;
} concurrent_args_t;

static void *concurrent_write_worker(void *arg)
{
    concurrent_args_t *args = (concurrent_args_t *)arg;
    char buf[64];
    char *rid = NULL;
    int ok = 0;

    for (int i = 0; i < args->record_count; i++) {
        generate_payload(buf, sizeof(buf), args->thread_id * 100000 + i);
        agentrt_error_t err = agentrt_memoryrov_write_raw(
            args->handle, buf, strlen(buf), NULL, &rid);
        if (err == AGENTRT_OK) {
            ok++;
            if (rid) {
                free(rid);
                rid = NULL;
            }
        }
    }

    args->writes_ok = ok;
    return NULL;
}

static void test_concurrent_write_throughput(void)
{
    TEST_CASE_START("07.4 Concurrent write throughput");

    agentrt_memoryrov_handle_t *handle = create_handle();
    TEST_ASSERT(handle != NULL, "Handle creation succeeded");

    /* Warmup (single-threaded) */
    char warmup_buf[64];
    char *wid = NULL;
    for (int i = 0; i < WARMUP_RECORDS; i++) {
        generate_payload(warmup_buf, sizeof(warmup_buf), i);
        agentrt_error_t err = agentrt_memoryrov_write_raw(
            handle, warmup_buf, strlen(warmup_buf), NULL, &wid);
        if (err == AGENTRT_OK && wid) {
            free(wid);
            wid = NULL;
        }
    }

    /* Concurrent benchmark */
    pthread_t threads[CONCURRENT_THREADS];
    concurrent_args_t args[CONCURRENT_THREADS];

    struct timespec t_start, t_end;
    clock_gettime(CLOCK_MONOTONIC, &t_start);

    for (int t = 0; t < CONCURRENT_THREADS; t++) {
        args[t].handle = handle;
        args[t].record_count = CONCURRENT_PER_THR;
        args[t].writes_ok = 0;
        args[t].thread_id = t;
        pthread_create(&threads[t], NULL, concurrent_write_worker, &args[t]);
    }

    for (int t = 0; t < CONCURRENT_THREADS; t++) {
        pthread_join(threads[t], NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &t_end);

    int total_ok = 0;
    for (int t = 0; t < CONCURRENT_THREADS; t++) {
        total_ok += args[t].writes_ok;
    }

    double elapsed_s = timespec_diff_s(&t_start, &t_end);
    double throughput = (double)total_ok / elapsed_s;
    int total_expected = CONCURRENT_THREADS * CONCURRENT_PER_THR;

    printf("  Concurrent write (%d threads): %d/%d records in %.3f s\n",
           CONCURRENT_THREADS, total_ok, total_expected, elapsed_s);
    printf("  Total throughput: %.0f records/s\n", throughput);
    printf("  Per-thread throughput: %.0f records/s\n",
           throughput / CONCURRENT_THREADS);

    TEST_ASSERT(throughput > THROUGHPUT_TARGET,
                "Concurrent write throughput > 10K records/s");

    agentrt_memoryrov_cleanup(handle);

    TEST_CASE_END();
}

/* ==================== INT-07.5 Write Latency Distribution ==================== */

static int cmp_double(const void *a, const void *b)
{
    double da = *(const double *)a;
    double db = *(const double *)b;
    return (da > db) - (da < db);
}

static void test_write_latency_distribution(void)
{
    TEST_CASE_START("07.5 Write latency distribution");

    agentrt_memoryrov_handle_t *handle = create_handle();
    TEST_ASSERT(handle != NULL, "Handle creation succeeded");

    /* Warmup */
    char warmup_buf[64];
    char *wid = NULL;
    for (int i = 0; i < WARMUP_RECORDS; i++) {
        generate_payload(warmup_buf, sizeof(warmup_buf), i);
        agentrt_error_t err = agentrt_memoryrov_write_raw(
            handle, warmup_buf, strlen(warmup_buf), NULL, &wid);
        if (err == AGENTRT_OK && wid) {
            free(wid);
            wid = NULL;
        }
    }

    /* Latency measurement */
    double *latencies = (double *)malloc(LATENCY_SAMPLES * sizeof(double));
    TEST_ASSERT(latencies != NULL, "Latency array allocation");

    char buf[64];
    char *rid = NULL;
    int valid_samples = 0;

    for (int i = 0; i < LATENCY_SAMPLES; i++) {
        generate_payload(buf, sizeof(buf), i + WARMUP_RECORDS);

        struct timespec ts, te;
        clock_gettime(CLOCK_MONOTONIC, &ts);

        agentrt_error_t err = agentrt_memoryrov_write_raw(
            handle, buf, strlen(buf), NULL, &rid);

        clock_gettime(CLOCK_MONOTONIC, &te);

        if (err == AGENTRT_OK) {
            latencies[valid_samples++] = timespec_diff_us(&ts, &te);
            if (rid) {
                free(rid);
                rid = NULL;
            }
        }
    }

    TEST_ASSERT(valid_samples > 0, "Collected latency samples");

    qsort(latencies, valid_samples, sizeof(double), cmp_double);

    double p50 = latencies[(int)(valid_samples * 0.50)];
    double p95 = latencies[(int)(valid_samples * 0.95)];
    double p99 = latencies[(int)(valid_samples * 0.99)];
    double min_lat = latencies[0];
    double max_lat = latencies[valid_samples - 1];

    printf("  Write latency (%d samples, us):\n", valid_samples);
    printf("    Min:  %.1f us\n", min_lat);
    printf("    P50:  %.1f us\n", p50);
    printf("    P95:  %.1f us\n", p95);
    printf("    P99:  %.1f us\n", p99);
    printf("    Max:  %.1f us\n", max_lat);

    /* P99 should be under 1000 us (1 ms) for >10K/s target */
    TEST_ASSERT(p99 < 1000.0, "P99 latency < 1000 us (1 ms)");
    TEST_ASSERT(p50 < 100.0, "P50 latency < 100 us");

    free(latencies);
    agentrt_memoryrov_cleanup(handle);

    TEST_CASE_END();
}

/* ==================== INT-07.6 Large Record Throughput ==================== */

static void test_large_record_throughput(void)
{
    TEST_CASE_START("07.6 Large record throughput");

    agentrt_memoryrov_handle_t *handle = create_handle();
    TEST_ASSERT(handle != NULL, "Handle creation succeeded");

    /* Warmup with small records */
    char warmup_buf[64];
    char *wid = NULL;
    for (int i = 0; i < WARMUP_RECORDS; i++) {
        generate_payload(warmup_buf, sizeof(warmup_buf), i);
        agentrt_error_t err = agentrt_memoryrov_write_raw(
            handle, warmup_buf, strlen(warmup_buf), NULL, &wid);
        if (err == AGENTRT_OK && wid) {
            free(wid);
            wid = NULL;
        }
    }

    /* 1KB records */
    char *buf_1k = (char *)malloc(LARGE_RECORD_1K);
    TEST_ASSERT(buf_1k != NULL, "1KB buffer allocation");
    char *rid = NULL;
    int ok_1k = 0;

    struct timespec t1_start, t1_end;
    clock_gettime(CLOCK_MONOTONIC, &t1_start);

    for (int i = 0; i < LARGE_TOTAL; i++) {
        generate_payload(buf_1k, LARGE_RECORD_1K, i);
        agentrt_error_t err = agentrt_memoryrov_write_raw(
            handle, buf_1k, LARGE_RECORD_1K - 1, NULL, &rid);
        if (err == AGENTRT_OK) {
            ok_1k++;
            if (rid) {
                free(rid);
                rid = NULL;
            }
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &t1_end);

    double elapsed_1k = timespec_diff_s(&t1_start, &t1_end);
    double throughput_1k = (double)ok_1k / elapsed_1k;

    printf("  1KB records: %d/%d in %.3f s -> %.0f records/s\n",
           ok_1k, LARGE_TOTAL, elapsed_1k, throughput_1k);

    /* 10KB records */
    char *buf_10k = (char *)malloc(LARGE_RECORD_10K);
    TEST_ASSERT(buf_10k != NULL, "10KB buffer allocation");
    int ok_10k = 0;

    struct timespec t10_start, t10_end;
    clock_gettime(CLOCK_MONOTONIC, &t10_start);

    for (int i = 0; i < LARGE_TOTAL; i++) {
        generate_payload(buf_10k, LARGE_RECORD_10K, i);
        agentrt_error_t err = agentrt_memoryrov_write_raw(
            handle, buf_10k, LARGE_RECORD_10K - 1, NULL, &rid);
        if (err == AGENTRT_OK) {
            ok_10k++;
            if (rid) {
                free(rid);
                rid = NULL;
            }
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &t10_end);

    double elapsed_10k = timespec_diff_s(&t10_start, &t10_end);
    double throughput_10k = (double)ok_10k / elapsed_10k;

    printf("  10KB records: %d/%d in %.3f s -> %.0f records/s\n",
           ok_10k, LARGE_TOTAL, elapsed_10k, throughput_10k);

    /* Throughput degradation ratio */
    if (throughput_1k > 0.0) {
        double degradation = 1.0 - (throughput_10k / throughput_1k);
        printf("  Throughput degradation (10KB vs 1KB): %.1f%%\n",
               degradation * 100.0);
        TEST_ASSERT(degradation < 0.9,
                    "Throughput degradation < 90% for 10KB records");
    }

    /* 1KB should still meet throughput target (relaxed for large records) */
    TEST_ASSERT(throughput_1k > THROUGHPUT_TARGET * 0.5,
                "1KB record throughput > 5K records/s (50% of baseline)");

    free(buf_1k);
    free(buf_10k);
    agentrt_memoryrov_cleanup(handle);

    TEST_CASE_END();
}

/* ==================== Main ==================== */

int main(void)
{
    printf("============================================================\n");
    printf("INT-07: MemoryRovol L1 Write Throughput Benchmark\n");
    printf("Target: >10,000 records/s sequential write\n");
    printf("============================================================\n");

    test_sequential_write_throughput();
    test_batch_write_throughput();
    test_mixed_read_write_throughput();
    test_concurrent_write_throughput();
    test_write_latency_distribution();
    test_large_record_throughput();

    printf("\n============================================================\n");
    printf("INT-07 Test Statistics\n");
    printf("============================================================\n");
    printf("Total:  %d\n", g_test_stats.total);
    printf("Passed: %d\n", g_test_stats.passed);
    printf("Failed: %d\n", g_test_stats.failed);
    printf("Rate:   %.2f%%\n",
           g_test_stats.total > 0
               ? (float)g_test_stats.passed / g_test_stats.total * 100.0f
               : 0.0f);
    printf("============================================================\n");

    if (g_test_stats.failed > 0) {
        printf("FAILED!\n");
        return 1;
    }

    printf("ALL PASSED!\n");
    return 0;
}
