/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * bench_stream_validation.c - Stream Validation Latency Benchmark (INT-04)
 *
 * INT-04.1: 性能基准测试
 *   - 测试输入大小: 1, 3, 15, 30, 60 字符
 *   - 每个大小迭代 10,000 次
 *   - 测量总时间，计算 P50 / P99 延迟
 *   - 目标: P99 < 50ms per semantic unit
 *
 * INT-04.2: 延迟曲线
 *   - 测试输入长度: 5, 10, 20, 30, 50, 100, 200 字符
 *   - 测量检测延迟，打印延迟曲线报告
 */

#include "cognition.h"
#include "memory_compat.h"
#include "stream_critic.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ============================================================================
 * 基准字符串模板
 * ============================================================================ */

static const char *BENCH_BASE_STRING =
    "The quick brown fox jumps over the lazy dog. "
    "Pack my box with five dozen liquor jugs. "
    "How vexingly quick daft zebras jump. "
    "The five boxing wizards jump quickly. "
    "Sphinx of black quartz, judge my vow. "
    "Two driven jocks help fax my big quiz. "
    "Five quacking zephyrs jolt my wax bed. "
    "The jay, pig, fox, zebra, and my wolves quack. "
    "Waltz, bad nymph, for quick jigs vex. "
    "Quick zephyrs blow, vexing daft Jim. ";

#define BENCH_BASE_STRING_LEN (sizeof(BENCH_BASE_STRING) - 1)

/* ============================================================================
 * 辅助: 生成指定长度的测试输入
 * ============================================================================ */
static char *generate_test_input(size_t target_len)
{
    char *buf = (char *)AGENTOS_MALLOC(target_len + 1);
    if (!buf)
        return NULL;

    for (size_t i = 0; i < target_len; i++) {
        buf[i] = BENCH_BASE_STRING[i % BENCH_BASE_STRING_LEN];
    }
    buf[target_len] = '\0';
    return buf;
}

/* ============================================================================
 * 辅助: 高精度计时
 * ============================================================================ */
static double elapsed_ns(struct timespec *start, struct timespec *end)
{
    return ((double)end->tv_sec - (double)start->tv_sec) * 1e9 +
           ((double)end->tv_nsec - (double)start->tv_nsec);
}

/* ============================================================================
 * 辅助: 排序比较函数 (用于 qsort)
 * ============================================================================ */
static int cmp_double(const void *a, const void *b)
{
    double da = *(const double *)a;
    double db = *(const double *)b;
    if (da < db)
        return -1;
    if (da > db)
        return 1;
    return 0;
}

/* ============================================================================
 * 辅助: 排序并计算百分位
 * ============================================================================ */
static double percentile(double *samples, size_t count, double pct)
{
    if (count == 0)
        return 0.0;
    qsort(samples, count, sizeof(double), cmp_double);
    size_t idx = (size_t)((double)count * pct / 100.0);
    if (idx >= count)
        idx = count - 1;
    return samples[idx];
}

/* ============================================================================
 * 辅助: 格式化打印延迟值
 * ============================================================================ */
static void print_latency(double ns, const char *label)
{
    if (ns >= 1e9)
        printf("    %-28s %.3f s\n", label, ns / 1e9);
    else if (ns >= 1e6)
        printf("    %-28s %.3f ms\n", label, ns / 1e6);
    else if (ns >= 1e3)
        printf("    %-28s %.3f us\n", label, ns / 1e3);
    else
        printf("    %-28s %.0f ns\n", label, ns);
}

/* ============================================================================
 * INT-04.1: 性能基准测试
 * ============================================================================ */
static void bench_int04_1(void)
{
    printf("\n========== INT-04.1: Performance Benchmark ==========\n");
    printf("  Iterations per size: 10,000\n");
    printf("  Target: P99 < 50 ms per semantic unit\n\n");

    size_t input_sizes[] = {1, 3, 15, 30, 60};
    const char *size_labels[] = {"very short (1)", "short (3)", "medium (15)",
                                  "long (30)", "extra long (60)"};
    const size_t num_sizes = sizeof(input_sizes) / sizeof(input_sizes[0]);
    const int iterations = 10000;

    int all_pass = 1;

    for (size_t si = 0; si < num_sizes; si++) {
        size_t input_len = input_sizes[si];
        printf("  --- Input: %s chars ---\n", size_labels[si]);

        /* 生成测试输入 */
        char *test_input = generate_test_input(input_len);
        assert(test_input != NULL);

        /* 创建 stream critic */
        sc_stream_critic_t *critic = NULL;
        agentos_error_t err = sc_stream_critic_create(NULL, &critic);
        assert(err == AGENTOS_SUCCESS);
        assert(critic != NULL);

        /* 分配延迟样本数组 */
        double *latencies = (double *)AGENTOS_MALLOC(
            (size_t)iterations * sizeof(double));
        assert(latencies != NULL);

        /* 预热: 运行少量迭代以稳定缓存 */
        for (int w = 0; w < 100; w++) {
            sc_validation_result_t vr;
            sc_stream_validator(critic, test_input, input_len, NULL, 0, &vr);
        }

        /* 基准测试: 逐次计时 */
        double total_ns = 0.0;
        for (int i = 0; i < iterations; i++) {
            sc_validation_result_t vr;
            struct timespec t_start, t_end;

            clock_gettime(CLOCK_MONOTONIC, &t_start);
            sc_stream_validator(critic, test_input, input_len, NULL, 0, &vr);
            clock_gettime(CLOCK_MONOTONIC, &t_end);

            double lat = elapsed_ns(&t_start, &t_end);
            latencies[i] = lat;
            total_ns += lat;
        }

        /* 计算统计量 */
        double avg_ns = total_ns / (double)iterations;
        double p50 = percentile(latencies, (size_t)iterations, 50.0);
        double p99 = percentile(latencies, (size_t)iterations, 99.0);

        /* 输出结果 */
        print_latency(avg_ns, "avg latency:");
        print_latency(p50, "P50 latency:");
        print_latency(p99, "P99 latency:");

        /* 目标检查: P99 < 50 ms */
        double target_ns = 50.0 * 1e6; /* 50 ms */
        int pass = (p99 < target_ns) ? 1 : 0;
        printf("    %-28s %s (P99=%.3f ms, target=50.000 ms)\n",
               "P99 target check:",
               pass ? "PASS" : "FAIL",
               p99 / 1e6);
        if (!pass)
            all_pass = 0;

        /* 清理 */
        AGENTOS_FREE(latencies);
        sc_stream_critic_destroy(critic);
        AGENTOS_FREE(test_input);
        printf("\n");
    }

    printf("  INT-04.1 Overall: %s\n\n", all_pass ? "PASS" : "FAIL");
}

/* ============================================================================
 * INT-04.2: 延迟曲线 (Latency Curve by Input Length)
 * ============================================================================ */
static void bench_int04_2(void)
{
    printf("========== INT-04.2: Latency Curve by Input Length ==========\n");
    printf("  Iterations per length: 1,000\n\n");

    size_t input_lengths[] = {5, 10, 20, 30, 50, 100, 200};
    const size_t num_lengths = sizeof(input_lengths) / sizeof(input_lengths[0]);
    const int iterations = 1000;

    printf("  %-16s %-20s %-20s %-20s\n",
           "Input Length", "Avg Latency (ns)", "P50 (ns)", "P99 (ns)");
    printf("  %-16s %-20s %-20s %-20s\n",
           "------------", "----------------", "--------", "--------");

    for (size_t li = 0; li < num_lengths; li++) {
        size_t input_len = input_lengths[li];

        /* 生成测试输入 */
        char *test_input = generate_test_input(input_len);
        assert(test_input != NULL);

        /* 创建 stream critic */
        sc_stream_critic_t *critic = NULL;
        agentos_error_t err = sc_stream_critic_create(NULL, &critic);
        assert(err == AGENTOS_SUCCESS);
        assert(critic != NULL);

        /* 分配延迟样本数组 */
        double *latencies = (double *)AGENTOS_MALLOC(
            (size_t)iterations * sizeof(double));
        assert(latencies != NULL);

        /* 预热 */
        for (int w = 0; w < 50; w++) {
            sc_validation_result_t vr;
            sc_stream_validator(critic, test_input, input_len, NULL, 0, &vr);
        }

        /* 基准测试 */
        double total_ns = 0.0;
        for (int i = 0; i < iterations; i++) {
            sc_validation_result_t vr;
            struct timespec t_start, t_end;

            clock_gettime(CLOCK_MONOTONIC, &t_start);
            sc_stream_validator(critic, test_input, input_len, NULL, 0, &vr);
            clock_gettime(CLOCK_MONOTONIC, &t_end);

            double lat = elapsed_ns(&t_start, &t_end);
            latencies[i] = lat;
            total_ns += lat;
        }

        double avg_ns = total_ns / (double)iterations;
        double p50 = percentile(latencies, (size_t)iterations, 50.0);
        double p99 = percentile(latencies, (size_t)iterations, 99.0);

        printf("  %-16zu %-20.0f %-20.0f %-20.0f\n",
               input_len, avg_ns, p50, p99);

        /* 清理 */
        AGENTOS_FREE(latencies);
        sc_stream_critic_destroy(critic);
        AGENTOS_FREE(test_input);
    }

    printf("\n  INT-04.2: Latency curve report complete.\n");
    printf("  Relationship: latency grows with input length due to\n");
    printf("  string scanning (safety checks, keyword detection) and\n");
    printf("  validation scoring across multiple aspects.\n\n");
}

/* ============================================================================
 * 主函数
 * ============================================================================ */
int main(void)
{
    printf("============================================================\n");
    printf("  Stream Validation Latency Benchmark\n");
    printf("  File: bench_stream_validation.c\n");
    printf("  INT-04: Stream Validation Performance\n");
    printf("============================================================\n");

    bench_int04_1();
    bench_int04_2();

    printf("============================================================\n");
    printf("  Summary\n");
    printf("============================================================\n");
    printf("  INT-04.1: Performance benchmark (P99 < 50ms)\n");
    printf("  INT-04.2: Latency curve by input length (5..200 chars)\n");
    printf("============================================================\n");

    return 0;
}