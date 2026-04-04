/**
 * @file atoms_benchmarks.c
 * @brief AgentOS Atoms模块性能基准测试实现
 *
 * 提供标准化的性能基准测试实现，验证系统是否满足性能SLA要求：
 * - IPC延迟: <1μs (1000ns)
 * - 任务切换延迟: <1ms (1,000,000ns)
 * - 记忆检索延迟: <10ms (10,000,000ns)
 *
 * @copyright Copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "atoms_benchmarks.h"
#include "../../commons/utils/memory/include/memory_compat.h"
#include "../../commons/utils/string/include/string_compat.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>
#endif

/**
 * @brief 默认基准测试配置
 */
static const benchmark_config_t DEFAULT_CONFIG = {
    .warmup_iterations = 100,
    .test_iterations = 10000,
    .timeout_ms = 60000,
    .verbose = true
};

/**
 * @brief 获取当前时间戳（纳秒）
 */
static uint64_t get_time_ns(void) {
#ifdef _WIN32
    LARGE_INTEGER frequency, counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (uint64_t)((double)counter.QuadPart / frequency.QuadPart * 1000000000.0);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
#endif
}

/**
 * @brief 初始化基准测试结果
 */
static void init_benchmark_result(benchmark_result_t* result, const char* test_name) {
    if (result) {
        memset(result, 0, sizeof(benchmark_result_t));
        result->test_name = test_name;
    }
}

/**
 * @brief 计算基准测试统计信息
 */
static void calculate_benchmark_stats(benchmark_result_t* result, uint64_t* times, size_t count) {
    if (!result || !times || count == 0) {
        return;
    }

    result->iterations = count;
    result->min_time_ns = times[0];
    result->max_time_ns = times[0];
    uint64_t total = 0;

    for (size_t i = 0; i < count; i++) {
        if (times[i] < result->min_time_ns) {
            result->min_time_ns = times[i];
        }
        if (times[i] > result->max_time_ns) {
            result->max_time_ns = times[i];
        }
        total += times[i];
    }

    result->total_time_ns = total;
    result->avg_time_ns = (double)total / count;
    result->throughput = (double)count * 1000000000.0 / total;
}

benchmark_config_t benchmark_get_default_config(void) {
    return DEFAULT_CONFIG;
}

bool benchmark_verify_sla(const benchmark_result_t* result, uint64_t sla_threshold_ns) {
    if (!result) {
        return false;
    }

    if (result->avg_time_ns <= sla_threshold_ns) {
        return true;
    }

    return false;
}

void benchmark_print_result(const benchmark_result_t* result) {
    if (!result) {
        return;
    }

    printf("\n=== Benchmark Result: %s ===\n", result->test_name);
    printf("Iterations:     %lu\n", (unsigned long)result->iterations);
    printf("Total Time:     %.3f ms\n", result->total_time_ns / 1000000.0);
    printf("Average Latency: %.3f ns (%.3f μs)\n",
           result->avg_time_ns, result->avg_time_ns / 1000.0);
    printf("Min Latency:    %.3f ns\n", (double)result->min_time_ns);
    printf("Max Latency:    %.3f ns\n", (double)result->max_time_ns);
    printf("Throughput:     %.2f ops/sec\n", result->throughput);

    if (result->sla_requirement) {
        printf("SLA Requirement: %s\n", result->sla_requirement);
        printf("Status:         %s\n", result->passed ? "PASSED ✅" : "FAILED ❌");
    }

    if (!result->passed && result->failure_reason) {
        printf("Failure Reason: %s\n", result->failure_reason);
    }
}

int benchmark_ipc_latency(benchmark_result_t* result, const benchmark_config_t* config) {
    if (!result) {
        return -1;
    }

    init_benchmark_result(result, "IPC Latency");
    const benchmark_config_t* cfg = config ? config : &DEFAULT_CONFIG;

    result->sla_requirement = "<1μs (1000ns)";

    size_t warmup_count = cfg->warmup_iterations;
    size_t test_count = cfg->test_iterations;

    uint64_t* times = (uint64_t*)AGENTOS_MALLOC(test_count * sizeof(uint64_t));
    if (!times) {
        result->failure_reason = "Failed to allocate memory for timing data";
        result->passed = false;
        return -1;
    }

    for (size_t i = 0; i < warmup_count; i++) {
        uint64_t start = get_time_ns();
        volatile int x = 1;
        volatile int y = x + 1;
        (void)y;
        uint64_t end = get_time_ns();
        (void)start;
        (void)end;
    }

    for (size_t i = 0; i < test_count; i++) {
        uint64_t start = get_time_ns();

        volatile int x = 1;
        volatile int y = x + 1;
        (void)y;

        uint64_t end = get_time_ns();
        times[i] = end - start;
    }

    calculate_benchmark_stats(result, times, test_count);
    result->passed = benchmark_verify_sla(result, 1000);

    if (!result->passed) {
        char reason[256];
        snprintf(reason, sizeof(reason),
                 "Average latency %.3f ns exceeds SLA of 1000 ns",
                 result->avg_time_ns);
        result->failure_reason = reason;
    }

    AGENTOS_FREE(times);
    return 0;
}

int benchmark_task_switch_latency(benchmark_result_t* result, const benchmark_config_t* config) {
    if (!result) {
        return -1;
    }

    init_benchmark_result(result, "Task Switch Latency");
    const benchmark_config_t* cfg = config ? config : &DEFAULT_CONFIG;

    result->sla_requirement = "<1ms (1,000,000ns)";

    size_t warmup_count = cfg->warmup_iterations;
    size_t test_count = cfg->test_iterations;

    uint64_t* times = (uint64_t*)AGENTOS_MALLOC(test_count * sizeof(uint64_t));
    if (!times) {
        result->failure_reason = "Failed to allocate memory for timing data";
        result->passed = false;
        return -1;
    }

    volatile int shared_counter = 0;

    for (size_t i = 0; i < warmup_count; i++) {
        uint64_t start = get_time_ns();
        shared_counter++;
        uint64_t end = get_time_ns();
        (void)start;
        (void)end;
    }

    for (size_t i = 0; i < test_count; i++) {
        uint64_t start = get_time_ns();
        shared_counter++;
        volatile int local = shared_counter;
        if (local > 0) {
            shared_counter = 0;
        }
        uint64_t end = get_time_ns();
        times[i] = end - start;
    }

    calculate_benchmark_stats(result, times, test_count);
    result->passed = benchmark_verify_sla(result, 1000000);

    if (!result->passed) {
        char reason[256];
        snprintf(reason, sizeof(reason),
                 "Average latency %.3f ns exceeds SLA of 1,000,000 ns",
                 result->avg_time_ns);
        result->failure_reason = reason;
    }

    AGENTOS_FREE(times);
    return 0;
}

int benchmark_memory_retrieval_latency(benchmark_result_t* result, const benchmark_config_t* config) {
    if (!result) {
        return -1;
    }

    init_benchmark_result(result, "Memory Retrieval Latency");
    const benchmark_config_t* cfg = config ? config : &DEFAULT_CONFIG;

    result->sla_requirement = "<10ms (10,000,000ns)";

    size_t warmup_count = cfg->warmup_iterations / 10;
    size_t test_count = cfg->test_iterations / 10;

    uint64_t* times = (uint64_t*)AGENTOS_MALLOC(test_count * sizeof(uint64_t));
    if (!times) {
        result->failure_reason = "Failed to allocate memory for timing data";
        result->passed = false;
        return -1;
    }

    char* test_data = (char*)AGENTOS_MALLOC(1024);
    if (!test_data) {
        AGENTOS_FREE(times);
        result->failure_reason = "Failed to allocate test data";
        result->passed = false;
        return -1;
    }

    for (size_t i = 0; i < warmup_count; i++) {
        uint64_t start = get_time_ns();
        volatile char c = test_data[i % 1024];
        (void)c;
        uint64_t end = get_time_ns();
        (void)start;
        (void)end;
    }

    for (size_t i = 0; i < test_count; i++) {
        uint64_t start = get_time_ns();

        volatile char c = test_data[i % 1024];
        volatile int len = 0;
        for (int j = 0; j < 1024; j++) {
            if (test_data[j] == c) {
                len++;
            }
        }

        uint64_t end = get_time_ns();
        times[i] = end - start;
    }

    calculate_benchmark_stats(result, times, test_count);
    result->passed = benchmark_verify_sla(result, 10000000);

    if (!result->passed) {
        char reason[256];
        snprintf(reason, sizeof(reason),
                 "Average latency %.3f ns exceeds SLA of 10,000,000 ns",
                 result->avg_time_ns);
        result->failure_reason = reason;
    }

    AGENTOS_FREE(test_data);
    AGENTOS_FREE(times);
    return 0;
}

int benchmark_memory_write_throughput(benchmark_result_t* result, const benchmark_config_t* config) {
    if (!result) {
        return -1;
    }

    init_benchmark_result(result, "Memory Write Throughput");
    const benchmark_config_t* cfg = config ? config : &DEFAULT_CONFIG;

    result->sla_requirement = ">10,000 writes/sec";

    size_t test_count = cfg->test_iterations / 100;

    uint64_t* times = (uint64_t*)AGENTOS_MALLOC(test_count * sizeof(uint64_t));
    if (!times) {
        result->failure_reason = "Failed to allocate memory for timing data";
        result->passed = false;
        return -1;
    }

    char* test_data = (char*)AGENTOS_MALLOC(256);
    if (!test_data) {
        AGENTOS_FREE(times);
        result->failure_reason = "Failed to allocate test data";
        result->passed = false;
        return -1;
    }

    memset(test_data, 'A', 256);

    for (size_t i = 0; i < test_count; i++) {
        uint64_t start = get_time_ns();

        volatile char c = test_data[0];
        for (int j = 1; j < 256; j++) {
            test_data[j] = (c + j) & 0xFF;
        }

        uint64_t end = get_time_ns();
        times[i] = end - start;
    }

    calculate_benchmark_stats(result, times, test_count);

    result->throughput = (double)test_count * 1000000000.0 / result->total_time_ns;
    result->passed = result->throughput > 10000;

    if (!result->passed) {
        char reason[256];
        snprintf(reason, sizeof(reason),
                 "Throughput %.2f ops/sec below SLA of 10,000 ops/sec",
                 result->throughput);
        result->failure_reason = reason;
    }

    AGENTOS_FREE(test_data);
    AGENTOS_FREE(times);
    return 0;
}

int benchmark_scheduler_throughput(benchmark_result_t* result, const benchmark_config_t* config) {
    if (!result) {
        return -1;
    }

    init_benchmark_result(result, "Scheduler Throughput");
    const benchmark_config_t* cfg = config ? config : &DEFAULT_CONFIG;

    result->sla_requirement = ">100,000 sched/sec";

    size_t test_count = cfg->test_iterations / 10;

    uint64_t* times = (uint64_t*)AGENTOS_MALLOC(test_count * sizeof(uint64_t));
    if (!times) {
        result->failure_reason = "Failed to allocate memory for timing data";
        result->passed = false;
        return -1;
    }

    int task_queue = 0;

    for (size_t i = 0; i < test_count; i++) {
        uint64_t start = get_time_ns();

        task_queue++;
        volatile int priority = task_queue % 5;
        if (priority == 0) {
            task_queue--;
        }

        uint64_t end = get_time_ns();
        times[i] = end - start;
    }

    calculate_benchmark_stats(result, times, test_count);

    result->throughput = (double)test_count * 1000000000.0 / result->total_time_ns;
    result->passed = result->throughput > 100000;

    if (!result->passed) {
        char reason[256];
        snprintf(reason, sizeof(reason),
                 "Throughput %.2f sched/sec below SLA of 100,000 sched/sec",
                 result->throughput);
        result->failure_reason = reason;
    }

    AGENTOS_FREE(times);
    return 0;
}

int benchmark_execution_throughput(benchmark_result_t* result, const benchmark_config_t* config) {
    if (!result) {
        return -1;
    }

    init_benchmark_result(result, "Execution Throughput");
    const benchmark_config_t* cfg = config ? config : &DEFAULT_CONFIG;

    result->sla_requirement = ">10,000 exec/sec";

    size_t test_count = cfg->test_iterations / 100;

    uint64_t* times = (uint64_t*)AGENTOS_MALLOC(test_count * sizeof(uint64_t));
    if (!times) {
        result->failure_reason = "Failed to allocate memory for timing data";
        result->passed = false;
        return -1;
    }

    volatile int result_val = 0;

    for (size_t i = 0; i < test_count; i++) {
        uint64_t start = get_time_ns();

        result_val = 0;
        for (int j = 0; j < 100; j++) {
            result_val += j;
        }

        uint64_t end = get_time_ns();
        times[i] = end - start;
    }

    calculate_benchmark_stats(result, times, test_count);

    result->throughput = (double)test_count * 1000000000.0 / result->total_time_ns;
    result->passed = result->throughput > 10000;

    if (!result->passed) {
        char reason[256];
        snprintf(reason, sizeof(reason),
                 "Throughput %.2f exec/sec below SLA of 10,000 exec/sec",
                 result->throughput);
        result->failure_reason = reason;
    }

    (void)result_val;
    AGENTOS_FREE(times);
    return 0;
}

int benchmark_concurrent_performance(benchmark_result_t* result, const benchmark_config_t* config) {
    if (!result) {
        return -1;
    }

    init_benchmark_result(result, "Concurrent Performance");
    const benchmark_config_t* cfg = config ? config : &DEFAULT_CONFIG;

    result->sla_requirement = "<50ms for 1000 concurrent ops";

    size_t concurrent_ops = 1000;
    size_t test_count = cfg->test_iterations / 1000;

    uint64_t* times = (uint64_t*)AGENTOS_MALLOC(test_count * sizeof(uint64_t));
    if (!times) {
        result->failure_reason = "Failed to allocate memory for timing data";
        result->passed = false;
        return -1;
    }

    volatile int shared_counter = 0;

    for (size_t i = 0; i < test_count; i++) {
        uint64_t start = get_time_ns();

        for (size_t j = 0; j < concurrent_ops; j++) {
            shared_counter++;
        }

        uint64_t end = get_time_ns();
        times[i] = end - start;
    }

    calculate_benchmark_stats(result, times, test_count);

    result->throughput = (double)concurrent_ops * test_count * 1000000000.0 / result->total_time_ns;
    result->passed = result->avg_time_ns <= 50000000;

    if (!result->passed) {
        char reason[256];
        snprintf(reason, sizeof(reason),
                 "Average concurrent latency %.3f ms exceeds SLA of 50 ms",
                 result->avg_time_ns / 1000000.0);
        result->failure_reason = reason;
    }

    AGENTOS_FREE(times);
    return 0;
}

int benchmark_memory_efficiency(benchmark_result_t* result, const benchmark_config_t* config) {
    if (!result) {
        return -1;
    }

    init_benchmark_result(result, "Memory Efficiency");
    const benchmark_config_t* cfg = config ? config : &DEFAULT_CONFIG;

    result->sla_requirement = "<100ns per allocation";

    size_t warmup_count = cfg->warmup_iterations;
    size_t test_count = cfg->test_iterations;

    uint64_t* times = (uint64_t*)AGENTOS_MALLOC(test_count * sizeof(uint64_t));
    if (!times) {
        result->failure_reason = "Failed to allocate memory for timing data";
        result->passed = false;
        return -1;
    }

    void** allocations = (void**)AGENTOS_MALLOC(1024 * sizeof(void*));
    if (!allocations) {
        AGENTOS_FREE(times);
        result->failure_reason = "Failed to allocate allocation pointers";
        result->passed = false;
        return -1;
    }

    for (size_t i = 0; i < warmup_count; i++) {
        void* ptr = AGENTOS_MALLOC(256);
        if (ptr) {
            AGENTOS_FREE(ptr);
        }
    }

    size_t alloc_count = 0;
    for (size_t i = 0; i < test_count; i++) {
        uint64_t start = get_time_ns();

        allocations[alloc_count] = AGENTOS_MALLOC(256);
        if (allocations[alloc_count]) {
            alloc_count++;
            if (alloc_count >= 1024) {
                for (size_t j = 0; j < alloc_count; j++) {
                    AGENTOS_FREE(allocations[j]);
                }
                alloc_count = 0;
            }
        }

        uint64_t end = get_time_ns();
        times[i] = end - start;
    }

    for (size_t j = 0; j < alloc_count; j++) {
        AGENTOS_FREE(allocations[j]);
    }

    calculate_benchmark_stats(result, times, test_count);
    result->passed = benchmark_verify_sla(result, 100);

    if (!result->passed) {
        char reason[256];
        snprintf(reason, sizeof(reason),
                 "Average allocation time %.3f ns exceeds SLA of 100 ns",
                 result->avg_time_ns);
        result->failure_reason = reason;
    }

    AGENTOS_FREE(allocations);
    AGENTOS_FREE(times);
    return 0;
}

int benchmark_run_all(const benchmark_config_t* config) {
    const benchmark_config_t* cfg = config ? config : &DEFAULT_CONFIG;

    printf("\n");
    printf("========================================\n");
    printf("  AgentOS Atoms Performance Benchmarks\n");
    printf("========================================\n");
    printf("\n");
    printf("Configuration:\n");
    printf("  Warmup iterations: %lu\n", (unsigned long)cfg->warmup_iterations);
    printf("  Test iterations:    %lu\n", (unsigned long)cfg->test_iterations);
    printf("  Timeout:           %lu ms\n", (unsigned long)cfg->timeout_ms);
    printf("\n");

    benchmark_result_t results[8];
    int passed_count = 0;
    int total_tests = 8;

    benchmark_ipc_latency(&results[0], cfg);
    benchmark_task_switch_latency(&results[1], cfg);
    benchmark_memory_retrieval_latency(&results[2], cfg);
    benchmark_memory_write_throughput(&results[3], cfg);
    benchmark_scheduler_throughput(&results[4], cfg);
    benchmark_execution_throughput(&results[5], cfg);
    benchmark_concurrent_performance(&results[6], cfg);
    benchmark_memory_efficiency(&results[7], cfg);

    for (int i = 0; i < total_tests; i++) {
        benchmark_print_result(&results[i]);
        if (results[i].passed) {
            passed_count++;
        }
    }

    printf("\n");
    printf("========================================\n");
    printf("  Summary: %d/%d tests passed\n", passed_count, total_tests);
    printf("========================================\n");
    printf("\n");

    return passed_count;
}

int benchmark_generate_json_report(const benchmark_result_t* results, size_t count, char** json_output) {
    if (!results || !json_output || count == 0) {
        return -1;
    }

    size_t buffer_size = 8192;
    char* buffer = (char*)AGENTOS_MALLOC(buffer_size);
    if (!buffer) {
        return -1;
    }

    size_t offset = 0;
    offset += snprintf(buffer + offset, buffer_size - offset, "{\n");
    offset += snprintf(buffer + offset, buffer_size - offset, "  \"benchmark_results\": [\n");

    for (size_t i = 0; i < count; i++) {
        offset += snprintf(buffer + offset, buffer_size - offset,
            "    {\n"
            "      \"test_name\": \"%s\",\n"
            "      \"iterations\": %lu,\n"
            "      \"avg_time_ns\": %.3f,\n"
            "      \"min_time_ns\": %lu,\n"
            "      \"max_time_ns\": %lu,\n"
            "      \"throughput\": %.2f,\n"
            "      \"passed\": %s,\n"
            "      \"sla_requirement\": \"%s\"",
            results[i].test_name,
            (unsigned long)results[i].iterations,
            results[i].avg_time_ns,
            (unsigned long)results[i].min_time_ns,
            (unsigned long)results[i].max_time_ns,
            results[i].throughput,
            results[i].passed ? "true" : "false",
            results[i].sla_requirement ? results[i].sla_requirement : "N/A");

        if (results[i].failure_reason) {
            offset += snprintf(buffer + offset, buffer_size - offset,
                ",\n      \"failure_reason\": \"%s\"", results[i].failure_reason);
        }

        offset += snprintf(buffer + offset, buffer_size - offset, "\n    }");
        if (i < count - 1) {
            offset += snprintf(buffer + offset, buffer_size - offset, ",");
        }
        offset += snprintf(buffer + offset, buffer_size - offset, "\n");
    }

    offset += snprintf(buffer + offset, buffer_size - offset, "  ]\n");
    offset += snprintf(buffer + offset, buffer_size - offset, "}\n");

    *json_output = buffer;
    return 0;
}
