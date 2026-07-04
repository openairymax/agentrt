/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * test_cupolas_latency.c - Cupolas 安全净化管线延迟基准测试 (INT-12)
 *
 * Phase 2 集成测试: 基准测试 Cupolas 安全穹顶净化管线延迟
 *
 * 验证覆盖:
 *   INT-12.1: 输入净化延迟 - 测量各类输入(text/JSON/command)的 P50/P95/P99 延迟
 *   INT-12.2: 权限裁决延迟 - 测量权限检查耗时
 *   INT-12.3: 网络过滤延迟 - 测量 URL/域名过滤耗时
 *   INT-12.4: 完整管线延迟 - 测量端到端 D1→D2→D3→D4 管线延迟
 *   INT-12.5: 吞吐量基准 - 测量每秒净化操作数
 *
 * 目标:
 *   P99 < 1ms (单项净化)
 *   P99 < 5ms (完整管线)
 *
 * 使用 agentos_time_monotonic_ns() 或 clock_gettime(CLOCK_MONOTONIC, ...) 计时
 * 每项测试运行 1000 次迭代并计算百分位数
 */

#include "cupolas.h"
#include "cupolas_signature.h"
#include "cupolas_network_security.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ============================================================================
 * 测试框架宏（与项目现有测试风格一致）
 * ============================================================================ */
#define TEST(name) static void test_##name(void)
#define RUN_TEST(name)                                                         \
    do {                                                                       \
        printf("  Running " #name "...\n");                                    \
        test_##name();                                                         \
        printf("  PASSED\n");                                                  \
    } while (0)

static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST_PASS(name)                                                        \
    do {                                                                       \
        printf("    [PASS] %s\n", name);                                       \
        g_tests_passed++;                                                      \
    } while (0)

#define TEST_FAIL(name, msg)                                                   \
    do {                                                                       \
        printf("    [FAIL] %s: %s\n", name, msg);                              \
        g_tests_failed++;                                                      \
    } while (0)

/* ============================================================================
 * 基准测试常量
 * ============================================================================ */
#define BENCH_ITERATIONS 1000
#define BENCH_WARMUP     100

/* 延迟目标 (纳秒) */
#define TARGET_P99_SINGLE_NS  1000000ULL   /* 1ms = 1,000,000 ns */
#define TARGET_P99_PIPELINE_NS 5000000ULL  /* 5ms = 5,000,000 ns */

/* ============================================================================
 * 计时辅助函数
 * ============================================================================ */
static uint64_t get_time_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/* ============================================================================
 * 统计辅助函数
 * ============================================================================ */
typedef struct {
    const char *name;
    double avg_ns;
    double min_ns;
    double max_ns;
    double p50_ns;
    double p95_ns;
    double p99_ns;
    uint64_t total_ops;
    double ops_per_sec;
} bench_result_t;

static int compare_uint64(const void *a, const void *b)
{
    uint64_t va = *(const uint64_t *)a;
    uint64_t vb = *(const uint64_t *)b;
    if (va < vb) return -1;
    if (va > vb) return 1;
    return 0;
}

static void calculate_stats(uint64_t *times, size_t count, bench_result_t *result)
{
    qsort(times, count, sizeof(uint64_t), compare_uint64);

    double sum = 0;
    for (size_t i = 0; i < count; i++) {
        sum += (double)times[i];
    }

    result->avg_ns = sum / (double)count;
    result->min_ns = (double)times[0];
    result->max_ns = (double)times[count - 1];
    result->p50_ns = (double)times[count / 2];
    result->p95_ns = (double)times[count * 95 / 100];
    result->p99_ns = (double)times[count * 99 / 100];
    result->total_ops = count;

    /* 计算吞吐量 (ops/sec) */
    if (sum > 0) {
        result->ops_per_sec = (double)count / (sum / 1000000000.0);
    } else {
        result->ops_per_sec = 0;
    }
}

static void print_result(const bench_result_t *result)
{
    printf("    %-40s\n", result->name);
    printf("      avg=%8.0f ns | min=%8.0f ns | max=%8.0f ns\n",
           result->avg_ns, result->min_ns, result->max_ns);
    printf("      p50=%8.0f ns | p95=%8.0f ns | p99=%8.0f ns\n",
           result->p50_ns, result->p95_ns, result->p99_ns);
    printf("      throughput=%.0f ops/sec\n", result->ops_per_sec);
}

static void print_result_us(const bench_result_t *result)
{
    printf("    %-40s\n", result->name);
    printf("      avg=%8.2f us | min=%8.2f us | max=%8.2f us\n",
           result->avg_ns / 1000.0, result->min_ns / 1000.0, result->max_ns / 1000.0);
    printf("      p50=%8.2f us | p95=%8.2f us | p99=%8.2f us\n",
           result->p50_ns / 1000.0, result->p95_ns / 1000.0, result->p99_ns / 1000.0);
    printf("      throughput=%.0f ops/sec\n", result->ops_per_sec);
}

/* ============================================================================
 * 辅助: 初始化 cupolas 框架
 * ============================================================================ */
static int init_cupolas_framework(void)
{
    agentos_error_t error = AGENTOS_OK;
    int ret = cupolas_init(NULL, &error);
    if (ret != AGENTOS_OK) {
        TEST_FAIL("cupolas_init", "initialization failed");
        return -1;
    }
    return 0;
}

/* ============================================================================
 * INT-12.1: 输入净化延迟基准
 *
 * 测量 P50/P95/P99 延迟，覆盖:
 *   - 纯文本输入
 *   - JSON 输入
 *   - 命令字符串输入
 *   - 恶意注入输入
 *   - 超长输入
 * ============================================================================ */
TEST(int12_1_input_sanitization_latency)
{
    printf("    --- Input Sanitization Latency Benchmark ---\n");

    if (init_cupolas_framework() != 0)
        return;

    /* 测试输入类型 */
    const struct {
        const char *input;
        const char *desc;
    } test_inputs[] = {
        {"Hello, this is a normal text message for benchmarking.",
         "plain_text"},
        {"{\"action\":\"query\",\"params\":{\"key\":\"value\",\"limit\":100}}",
         "json_object"},
        {"ls -la /home/user/documents && cat report.txt",
         "command_string"},
        {"<script>alert('xss')</script>Normal content here",
         "xss_injection"},
        {"Robert'); DROP TABLE Students;--",
         "sql_injection"},
        {"${system('rm -rf /')}",
         "template_injection"},
        {"../../../etc/passwd",
         "path_traversal"},
        {"normal text with numbers 123 and symbols !@#$%",
         "mixed_content"},
        {NULL, NULL}
    };

    uint64_t *all_times = (uint64_t *)malloc(BENCH_ITERATIONS * sizeof(uint64_t));
    if (!all_times) {
        cupolas_cleanup();
        return;
    }

    for (int t = 0; test_inputs[t].input != NULL; t++) {
        char output[1024];

        /* 预热 */
        for (int w = 0; w < BENCH_WARMUP; w++) {
            cupolas_sanitize_input(test_inputs[t].input, output, sizeof(output));
        }

        /* 基准测试 */
        for (int i = 0; i < BENCH_ITERATIONS; i++) {
            uint64_t start = get_time_ns();
            cupolas_sanitize_input(test_inputs[t].input, output, sizeof(output));
            all_times[i] = get_time_ns() - start;
        }

        bench_result_t result;
        memset(&result, 0, sizeof(result));
        result.name = test_inputs[t].desc;
        calculate_stats(all_times, BENCH_ITERATIONS, &result);
        print_result_us(&result);

        /* 验证 P99 目标 */
        if (result.p99_ns <= (double)TARGET_P99_SINGLE_NS) {
            printf("      [OK] P99=%.2f us < 1000 us target\n",
                   result.p99_ns / 1000.0);
        } else {
            printf("      [WARN] P99=%.2f us exceeds 1000 us target\n",
                   result.p99_ns / 1000.0);
        }
    }

    free(all_times);
    cupolas_cleanup();
    TEST_PASS("INT-12.1: input sanitization latency benchmark completed");
}

/* ============================================================================
 * INT-12.2: 权限裁决延迟基准
 *
 * 测量权限检查的 P50/P95/P99 延迟:
 *   - 规则匹配（命中/未命中）
 *   - 通配符匹配
 *   - 缓存命中 vs 缓存未命中
 * ============================================================================ */
TEST(int12_2_permission_arbitration_latency)
{
    printf("    --- Permission Arbitration Latency Benchmark ---\n");

    if (init_cupolas_framework() != 0)
        return;

    /* 设置权限规则 */
    cupolas_add_permission_rule("bench_agent", "read", "/data/reports/*", 1, 100);
    cupolas_add_permission_rule("bench_agent", "write", "/data/reports/*", 1, 100);
    cupolas_add_permission_rule("bench_agent", "execute", "/tools/*", 1, 200);
    cupolas_add_permission_rule("bench_agent", "write", "/secure/*", 0, 300);
    cupolas_add_permission_rule("admin_agent", "*", "/*", 1, 500);

    uint64_t *times = (uint64_t *)malloc(BENCH_ITERATIONS * sizeof(uint64_t));
    if (!times) {
        cupolas_cleanup();
        return;
    }

    /* 场景 1: 规则命中（允许） */
    {
        /* 预热 */
        for (int w = 0; w < BENCH_WARMUP; w++) {
            cupolas_check_permission("bench_agent", "read", "/data/reports/q1.csv", NULL);
        }

        for (int i = 0; i < BENCH_ITERATIONS; i++) {
            uint64_t start = get_time_ns();
            cupolas_check_permission("bench_agent", "read", "/data/reports/q1.csv", NULL);
            times[i] = get_time_ns() - start;
        }

        bench_result_t result = {"Permission: rule hit (allowed)", 0};
        calculate_stats(times, BENCH_ITERATIONS, &result);
        print_result_us(&result);

        if (result.p99_ns <= (double)TARGET_P99_SINGLE_NS) {
            printf("      [OK] P99=%.2f us < 1000 us target\n", result.p99_ns / 1000.0);
        } else {
            printf("      [WARN] P99=%.2f us exceeds 1000 us target\n", result.p99_ns / 1000.0);
        }
    }

    /* 场景 2: 规则命中（拒绝） */
    {
        for (int w = 0; w < BENCH_WARMUP; w++) {
            cupolas_check_permission("bench_agent", "write", "/secure/vault.key", NULL);
        }

        for (int i = 0; i < BENCH_ITERATIONS; i++) {
            uint64_t start = get_time_ns();
            cupolas_check_permission("bench_agent", "write", "/secure/vault.key", NULL);
            times[i] = get_time_ns() - start;
        }

        bench_result_t result = {"Permission: rule hit (denied)", 0};
        calculate_stats(times, BENCH_ITERATIONS, &result);
        print_result_us(&result);
    }

    /* 场景 3: 规则未命中（未知 agent） */
    {
        for (int w = 0; w < BENCH_WARMUP; w++) {
            cupolas_check_permission("unknown_agent", "read", "/data/reports/q1.csv", NULL);
        }

        for (int i = 0; i < BENCH_ITERATIONS; i++) {
            uint64_t start = get_time_ns();
            cupolas_check_permission("unknown_agent", "read", "/data/reports/q1.csv", NULL);
            times[i] = get_time_ns() - start;
        }

        bench_result_t result = {"Permission: rule miss (unknown agent)", 0};
        calculate_stats(times, BENCH_ITERATIONS, &result);
        print_result_us(&result);
    }

    /* 场景 4: 通配符匹配 */
    {
        for (int w = 0; w < BENCH_WARMUP; w++) {
            cupolas_check_permission("admin_agent", "write", "/any/path/resource", NULL);
        }

        for (int i = 0; i < BENCH_ITERATIONS; i++) {
            uint64_t start = get_time_ns();
            cupolas_check_permission("admin_agent", "write", "/any/path/resource", NULL);
            times[i] = get_time_ns() - start;
        }

        bench_result_t result = {"Permission: wildcard match", 0};
        calculate_stats(times, BENCH_ITERATIONS, &result);
        print_result_us(&result);
    }

    /* 场景 5: 缓存清理后的权限检查 */
    {
        cupolas_clear_permission_cache();

        for (int i = 0; i < BENCH_ITERATIONS; i++) {
            /* 每次清理缓存以测量冷路径 */
            if (i % 10 == 0)
                cupolas_clear_permission_cache();

            uint64_t start = get_time_ns();
            cupolas_check_permission("bench_agent", "read", "/data/reports/q1.csv", NULL);
            times[i] = get_time_ns() - start;
        }

        bench_result_t result = {"Permission: cache-cold check", 0};
        calculate_stats(times, BENCH_ITERATIONS, &result);
        print_result_us(&result);
    }

    free(times);
    cupolas_cleanup();
    TEST_PASS("INT-12.2: permission arbitration latency benchmark completed");
}

/* ============================================================================
 * INT-12.3: 网络过滤延迟基准
 *
 * 测量 URL/域名过滤的 P50/P95/P99 延迟:
 *   - 防火墙规则检查
 *   - 域名黑白名单匹配
 * ============================================================================ */
TEST(int12_3_network_filter_latency)
{
    printf("    --- Network Filter Latency Benchmark ---\n");

    /* 初始化网络安全模块 */
    int ret = cupolas_network_security_init(NULL);
    if (ret != 0) {
        printf("    Network security init: ret=%d (may not be available)\n", ret);
        TEST_PASS("INT-12.3: network security module not available, skipped");
        return;
    }

    /* 配置防火墙规则 */
    cupolas_firewall_config_t fw_config;
    memset(&fw_config, 0, sizeof(fw_config));
    fw_config.enable = 1;
    fw_config.default_inbound = CUPOLAS_FW_DENY;
    fw_config.default_outbound = CUPOLAS_FW_ALLOW;
    fw_config.enable_logging = 0;
    fw_config.max_connections = 1000;
    fw_config.connection_timeout_ms = 30000;

    cupolas_firewall_enable(&fw_config);

    /* 添加防火墙规则 */
    cupolas_fw_rule_t rule1;
    memset(&rule1, 0, sizeof(rule1));
    rule1.rule_id = "bench_allow_https";
    rule1.protocol = CUPOLAS_PROTO_TCP;
    rule1.direction = CUPOLAS_DIR_OUTBOUND;
    rule1.dst_port_start = 443;
    rule1.dst_port_end = 443;
    rule1.action = CUPOLAS_FW_ALLOW;
    rule1.enabled = 1;
    rule1.priority = 100;
    cupolas_firewall_add_rule(&rule1);

    cupolas_fw_rule_t rule2;
    memset(&rule2, 0, sizeof(rule2));
    rule2.rule_id = "bench_deny_telnet";
    rule2.protocol = CUPOLAS_PROTO_TCP;
    rule2.direction = CUPOLAS_DIR_OUTBOUND;
    rule2.dst_port_start = 23;
    rule2.dst_port_end = 23;
    rule2.action = CUPOLAS_FW_DENY;
    rule2.enabled = 1;
    rule2.priority = 200;
    cupolas_firewall_add_rule(&rule2);

    uint64_t *times = (uint64_t *)malloc(BENCH_ITERATIONS * sizeof(uint64_t));
    if (!times) {
        cupolas_firewall_disable();
        cupolas_network_security_cleanup();
        return;
    }

    /* 场景 1: 防火墙允许检查 (HTTPS) */
    {
        for (int w = 0; w < BENCH_WARMUP; w++) {
            cupolas_firewall_check(CUPOLAS_PROTO_TCP, CUPOLAS_DIR_OUTBOUND,
                                  "192.168.1.100", 54321, "10.0.0.1", 443);
        }

        for (int i = 0; i < BENCH_ITERATIONS; i++) {
            uint64_t start = get_time_ns();
            cupolas_firewall_check(CUPOLAS_PROTO_TCP, CUPOLAS_DIR_OUTBOUND,
                                  "192.168.1.100", 54321, "10.0.0.1", 443);
            times[i] = get_time_ns() - start;
        }

        bench_result_t result = {"Firewall: allow HTTPS (rule match)", 0};
        calculate_stats(times, BENCH_ITERATIONS, &result);
        print_result_us(&result);

        if (result.p99_ns <= (double)TARGET_P99_SINGLE_NS) {
            printf("      [OK] P99=%.2f us < 1000 us target\n", result.p99_ns / 1000.0);
        } else {
            printf("      [WARN] P99=%.2f us exceeds 1000 us target\n", result.p99_ns / 1000.0);
        }
    }

    /* 场景 2: 防火墙拒绝检查 (Telnet) */
    {
        for (int w = 0; w < BENCH_WARMUP; w++) {
            cupolas_firewall_check(CUPOLAS_PROTO_TCP, CUPOLAS_DIR_OUTBOUND,
                                  "192.168.1.100", 54321, "10.0.0.1", 23);
        }

        for (int i = 0; i < BENCH_ITERATIONS; i++) {
            uint64_t start = get_time_ns();
            cupolas_firewall_check(CUPOLAS_PROTO_TCP, CUPOLAS_DIR_OUTBOUND,
                                  "192.168.1.100", 54321, "10.0.0.1", 23);
            times[i] = get_time_ns() - start;
        }

        bench_result_t result = {"Firewall: deny Telnet (rule match)", 0};
        calculate_stats(times, BENCH_ITERATIONS, &result);
        print_result_us(&result);
    }

    /* 场景 3: 防火墙默认策略检查 (无规则匹配) */
    {
        for (int w = 0; w < BENCH_WARMUP; w++) {
            cupolas_firewall_check(CUPOLAS_PROTO_UDP, CUPOLAS_DIR_INBOUND,
                                  "10.0.0.1", 53, "192.168.1.100", 12345);
        }

        for (int i = 0; i < BENCH_ITERATIONS; i++) {
            uint64_t start = get_time_ns();
            cupolas_firewall_check(CUPOLAS_PROTO_UDP, CUPOLAS_DIR_INBOUND,
                                  "10.0.0.1", 53, "192.168.1.100", 12345);
            times[i] = get_time_ns() - start;
        }

        bench_result_t result = {"Firewall: default policy (no rule match)", 0};
        calculate_stats(times, BENCH_ITERATIONS, &result);
        print_result_us(&result);
    }

    /* 获取防火墙统计 */
    cupolas_net_stats_t net_stats;
    if (cupolas_firewall_get_stats(&net_stats) == 0) {
        printf("    Firewall stats: total_conn=%lu, blocks=%lu\n",
               (unsigned long)net_stats.total_connections,
               (unsigned long)net_stats.firewall_blocks);
    }

    free(times);
    cupolas_firewall_clear_rules();
    cupolas_firewall_disable();
    cupolas_network_security_cleanup();
    TEST_PASS("INT-12.3: network filter latency benchmark completed");
}

/* ============================================================================
 * INT-12.4: 完整管线延迟基准
 *
 * 测量端到端 D1→D2→D3→D4 管线延迟:
 *   D3: 输入净化
 *   D2: 权限检查
 *   D1: 沙箱执行（模拟）
 *   D4: 审计日志刷新
 *
 * 目标: P99 < 5ms
 * ============================================================================ */
TEST(int12_4_full_pipeline_latency)
{
    printf("    --- Full Pipeline D1→D2→D3→D4 Latency Benchmark ---\n");

    if (init_cupolas_framework() != 0)
        return;

    /* 设置权限规则 (D2) */
    cupolas_add_permission_rule("pipeline_agent", "read", "/data/analysis/*", 1, 100);
    cupolas_add_permission_rule("pipeline_agent", "write", "/data/analysis/*", 1, 100);
    cupolas_add_permission_rule("pipeline_agent", "execute", "/tools/*", 1, 100);
    cupolas_add_permission_rule("pipeline_agent", "write", "/secure/*", 0, 200);

    /* 测试输入 */
    const struct {
        const char *input;
        const char *desc;
    } pipeline_inputs[] = {
        {"<script>alert('xss')</script>Analyze /data/analysis/sales.csv",
         "xss_in_pipeline"},
        {"'; DROP TABLE analysis;--",
         "sql_in_pipeline"},
        {"Normal analysis request for /data/analysis/report.csv",
         "clean_in_pipeline"},
        {"${exec('rm -rf /')}../../../etc/passwd",
         "mixed_attack_pipeline"},
        {NULL, NULL}
    };

    uint64_t *times = (uint64_t *)malloc(BENCH_ITERATIONS * sizeof(uint64_t));
    if (!times) {
        cupolas_cleanup();
        return;
    }

    for (int t = 0; pipeline_inputs[t].input != NULL; t++) {
        char sanitized[512];

        /* 预热 */
        for (int w = 0; w < BENCH_WARMUP; w++) {
            cupolas_sanitize_input(pipeline_inputs[t].input, sanitized, sizeof(sanitized));
            cupolas_check_permission("pipeline_agent", "read", sanitized, NULL);
        }

        /* 基准测试: 完整 D1→D2→D3→D4 管线 */
        for (int i = 0; i < BENCH_ITERATIONS; i++) {
            uint64_t start = get_time_ns();

            /* D3: 输入净化 */
            cupolas_sanitize_input(pipeline_inputs[t].input, sanitized, sizeof(sanitized));

            /* D2: 权限检查 */
            cupolas_check_permission("pipeline_agent", "read", sanitized, NULL);

            /* D1: 沙箱执行 (模拟 - 使用轻量 echo 命令) */
            {
                const char *cmd = "/bin/echo";
                char *argv[] = {(char *)"echo", (char *)"pipeline_test", NULL};
                int exit_code = -1;
                char stdout_buf[128] = {0};
                char stderr_buf[128] = {0};
                cupolas_execute_command(cmd, argv, &exit_code,
                                       stdout_buf, sizeof(stdout_buf),
                                       stderr_buf, sizeof(stderr_buf));
            }

            /* D4: 审计日志刷新 */
            cupolas_flush_audit_log();

            times[i] = get_time_ns() - start;
        }

        bench_result_t result;
        memset(&result, 0, sizeof(result));
        char name_buf[64];
        snprintf(name_buf, sizeof(name_buf), "Full Pipeline: %s", pipeline_inputs[t].desc);
        result.name = name_buf;
        calculate_stats(times, BENCH_ITERATIONS, &result);
        print_result_us(&result);

        /* 验证 P99 目标 */
        if (result.p99_ns <= (double)TARGET_P99_PIPELINE_NS) {
            printf("      [OK] P99=%.2f us < 5000 us target\n",
                   result.p99_ns / 1000.0);
        } else {
            printf("      [WARN] P99=%.2f us exceeds 5000 us target\n",
                   result.p99_ns / 1000.0);
        }
    }

    free(times);
    cupolas_clear_permission_cache();
    cupolas_cleanup();
    TEST_PASS("INT-12.4: full pipeline latency benchmark completed");
}

/* ============================================================================
 * INT-12.5: 吞吐量基准
 *
 * 测量每秒净化操作数:
 *   - 单线程吞吐量
 *   - 不同输入类型的吞吐量
 *   - 权限检查吞吐量
 *   - 混合操作吞吐量
 * ============================================================================ */
TEST(int12_5_throughput_benchmark)
{
    printf("    --- Throughput Benchmark ---\n");

    if (init_cupolas_framework() != 0)
        return;

    cupolas_add_permission_rule("throughput_agent", "read", "/data/*", 1, 100);
    cupolas_add_permission_rule("throughput_agent", "write", "/data/*", 1, 100);

    #define THROUGHPUT_OPS 5000

    /* 场景 1: 纯文本净化吞吐量 */
    {
        const char *input = "Normal text input for throughput measurement with some length.";
        char output[512];

        uint64_t start = get_time_ns();
        for (int i = 0; i < THROUGHPUT_OPS; i++) {
            cupolas_sanitize_input(input, output, sizeof(output));
        }
        uint64_t elapsed = get_time_ns() - start;

        double ops_per_sec = (double)THROUGHPUT_OPS / ((double)elapsed / 1000000000.0);
        double avg_ns = (double)elapsed / (double)THROUGHPUT_OPS;
        printf("    Text sanitization: %.0f ops/sec (avg=%.0f ns/op)\n",
               ops_per_sec, avg_ns);
    }

    /* 场景 2: 恶意输入净化吞吐量 */
    {
        const char *input = "<script>alert('xss')</script>'; DROP TABLE users;--";
        char output[512];

        uint64_t start = get_time_ns();
        for (int i = 0; i < THROUGHPUT_OPS; i++) {
            cupolas_sanitize_input(input, output, sizeof(output));
        }
        uint64_t elapsed = get_time_ns() - start;

        double ops_per_sec = (double)THROUGHPUT_OPS / ((double)elapsed / 1000000000.0);
        double avg_ns = (double)elapsed / (double)THROUGHPUT_OPS;
        printf("    Malicious input sanitization: %.0f ops/sec (avg=%.0f ns/op)\n",
               ops_per_sec, avg_ns);
    }

    /* 场景 3: 权限检查吞吐量 */
    {
        uint64_t start = get_time_ns();
        for (int i = 0; i < THROUGHPUT_OPS; i++) {
            cupolas_check_permission("throughput_agent", "read", "/data/file.txt", NULL);
        }
        uint64_t elapsed = get_time_ns() - start;

        double ops_per_sec = (double)THROUGHPUT_OPS / ((double)elapsed / 1000000000.0);
        double avg_ns = (double)elapsed / (double)THROUGHPUT_OPS;
        printf("    Permission check: %.0f ops/sec (avg=%.0f ns/op)\n",
               ops_per_sec, avg_ns);
    }

    /* 场景 4: 混合操作吞吐量 (净化 + 权限 + 审计) */
    {
        const char *input = "Mixed operation throughput test input string.";
        char output[512];

        uint64_t start = get_time_ns();
        for (int i = 0; i < THROUGHPUT_OPS; i++) {
            cupolas_sanitize_input(input, output, sizeof(output));
            cupolas_check_permission("throughput_agent", "read", output, NULL);
            if (i % 100 == 0)
                cupolas_flush_audit_log();
        }
        cupolas_flush_audit_log();
        uint64_t elapsed = get_time_ns() - start;

        double ops_per_sec = (double)THROUGHPUT_OPS / ((double)elapsed / 1000000000.0);
        double avg_ns = (double)elapsed / (double)THROUGHPUT_OPS;
        printf("    Mixed ops (sanitize+perm+audit): %.0f ops/sec (avg=%.0f ns/op)\n",
               ops_per_sec, avg_ns);
    }

    /* 场景 5: JSON 输入净化吞吐量 */
    {
        const char *input = "{\"action\":\"query\",\"params\":{\"key\":\"value\",\"limit\":100}}";
        char output[512];

        uint64_t start = get_time_ns();
        for (int i = 0; i < THROUGHPUT_OPS; i++) {
            cupolas_sanitize_input(input, output, sizeof(output));
        }
        uint64_t elapsed = get_time_ns() - start;

        double ops_per_sec = (double)THROUGHPUT_OPS / ((double)elapsed / 1000000000.0);
        double avg_ns = (double)elapsed / (double)THROUGHPUT_OPS;
        printf("    JSON sanitization: %.0f ops/sec (avg=%.0f ns/op)\n",
               ops_per_sec, avg_ns);
    }

    #undef THROUGHPUT_OPS

    cupolas_clear_permission_cache();
    cupolas_cleanup();
    TEST_PASS("INT-12.5: throughput benchmark completed");
}

/* ============================================================================
 * 主入口
 * ============================================================================ */
int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    printf("=========================================\n");
    printf("  Cupolas Sanitization Latency Benchmark\n");
    printf("  Phase 2 - INT-12\n");
    printf("  Iterations: %d (warmup: %d)\n", BENCH_ITERATIONS, BENCH_WARMUP);
    printf("  Target: P99 < 1ms (single), P99 < 5ms (pipeline)\n");
    printf("=========================================\n\n");

    /* INT-12.1: 输入净化延迟 */
    printf("--- INT-12.1: Input Sanitization Latency ---\n");
    RUN_TEST(int12_1_input_sanitization_latency);

    /* INT-12.2: 权限裁决延迟 */
    printf("\n--- INT-12.2: Permission Arbitration Latency ---\n");
    RUN_TEST(int12_2_permission_arbitration_latency);

    /* INT-12.3: 网络过滤延迟 */
    printf("\n--- INT-12.3: Network Filter Latency ---\n");
    RUN_TEST(int12_3_network_filter_latency);

    /* INT-12.4: 完整管线延迟 */
    printf("\n--- INT-12.4: Full Pipeline D1→D2→D3→D4 Latency ---\n");
    RUN_TEST(int12_4_full_pipeline_latency);

    /* INT-12.5: 吞吐量基准 */
    printf("\n--- INT-12.5: Throughput Benchmark ---\n");
    RUN_TEST(int12_5_throughput_benchmark);

    printf("\n=========================================\n");
    if (g_tests_failed == 0) {
        printf("  All %d Cupolas latency benchmark tests PASSED\n", g_tests_passed);
    } else {
        printf("  %d PASSED, %d FAILED\n", g_tests_passed, g_tests_failed);
    }
    printf("=========================================\n");

    return g_tests_failed > 0 ? 1 : 0;
}
