/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * benchmark_cupolas.c - Performance Benchmark Suite
 *
 * Usage: ./benchmark_cupolas [iterations]
 */

#include "cupolas.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define DEFAULT_ITERATIONS 10000
#define BENCHMARK_WARMUP 1000

typedef struct benchmark_result {
    const char* name;
    double avg_us;
    double min_us;
    double max_us;
    double p50_us;
    double p99_us;
    uint64_t total_ops;
} benchmark_result_t;

static uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000 + (uint64_t)ts.tv_nsec / 1000;
}

static void print_result(const benchmark_result_t* result) {
    printf("  %-40s %8.2f us (avg) | min=%8.2f | max=%8.2f | p50=%8.2f | p99=%8.2f\n",
           result->name,
           result->avg_us,
           result->min_us,
           result->max_us,
           result->p50_us,
           result->p99_us);
}

static int compare_uint64(const void* a, const void* b) {
    return (*(uint64_t*)a > *(uint64_t*)b) - (*(uint64_t*)a < *(uint64_t*)b);
}

static void calculate_stats(uint64_t* times, size_t count, benchmark_result_t* result) {
    qsort(times, count, sizeof(uint64_t), compare_uint64);
    
    double sum = 0;
    for (size_t i = 0; i < count; i++) {
        sum += times[i];
    }
    
    result->avg_us = sum / count;
    result->min_us = (double)times[0];
    result->max_us = (double)times[count - 1];
    result->p50_us = (double)times[count / 2];
    result->p99_us = (double)times[count * 99 / 100];
}

void benchmark_sanitizer_cache(int iterations) {
    printf("\n[Sanitizer Cache Benchmark] (%d iterations)\n", iterations);
    
    sanitizer_cache_t* cache = sanitizer_cache_create(1024);
    if (!cache) { printf("  FAILED to create cache\n"); return; }
    
    uint64_t* write_times = malloc(iterations * sizeof(uint64_t));
    uint64_t* read_times = malloc(iterations * sizeof(uint64_t));
    
    for (int i = 0; i < iterations; i++) {
        char input[64], output[64];
        snprintf(input, sizeof(input), "test_input_%d", i);
        snprintf(output, sizeof(output), "test_output_%d", i);
        
        uint64_t start = get_time_us();
        sanitizer_cache_put(cache, input, output, SANITIZE_LEVEL_NORMAL);
        write_times[i] = get_time_us() - start;
        
        start = get_time_us();
        char* result = sanitizer_cache_get(cache, input, SANITIZE_LEVEL_NORMAL);
        read_times[i] = get_time_us() - start;
        
        if (result) free(result);
    }
    
    benchmark_result_t write_result = {"Cache PUT", 0};
    calculate_stats(write_times, iterations, &write_result);
    write_result.total_ops = iterations;
    print_result(&write_result);
    
    benchmark_result_t read_result = {"Cache GET (hit)", 0};
    calculate_stats(read_times, iterations, &read_result);
    read_result.total_ops = iterations;
    print_result(&read_result);
    
    free(write_times);
    free(read_times);
    sanitizer_cache_destroy(cache);
}

void benchmark_permission_check(int iterations) {
    printf("\n[Permission Check Benchmark] (%d iterations)\n", iterations);
    
    permission_context_t ctx;
    permission_context_init(&ctx);
    
    permission_engine_t* engine = permission_engine_create(1024);
    if (!engine) { printf("  FAILED to create engine\n"); return; }
    
    uint64_t* check_times = malloc(iterations * sizeof(uint64_t));
    
    for (int i = 0; i < iterations; i++) {
        char agent_id[32], resource[32];
        snprintf(agent_id, sizeof(agent_id), "agent_%d", i % 10);
        snprintf(resource, sizeof(resource), "resource_%d", i % 5);
        
        uint64_t start = get_time_us();
        permission_check(engine, agent_id, resource, PERMISSION_ACTION_READ, &ctx);
        check_times[i] = get_time_us() - start;
    }
    
    benchmark_result_t check_result = {"Permission Check", 0};
    calculate_stats(check_times, iterations, &check_result);
    check_result.total_ops = iterations;
    print_result(&check_result);
    
    free(check_times);
    permission_engine_destroy(engine);
}

void benchmark_audit_queue(int iterations) {
    printf("\n[Audit Queue Benchmark] (%d iterations)\n", iterations);
    
    audit_queue_t* queue = audit_queue_create(10000);
    if (!queue) { printf("  FAILED to create queue\n"); return; }
    
    uint64_t* push_times = malloc(iterations * sizeof(uint64_t));
    uint64_t* pop_times = malloc(iterations * sizeof(uint64_t));
    
    for (int i = 0; i < iterations; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "entry_%d", i);
        
        audit_entry_t* entry = audit_entry_create(
            AUDIT_EVENT_PERMISSION,
            buf,
            "action",
            "resource",
            "detail",
            1
        );
        
        if (!entry) continue;
        
        uint64_t start = get_time_us();
        audit_queue_push(queue, entry);
        push_times[i] = get_time_us() - start;
    }
    
    for (int i = 0; i < iterations; i++) {
        audit_entry_t* entry;
        uint64_t start = get_time_us();
        if (audit_queue_try_pop(queue, &entry) == 0) {
            pop_times[i] = get_time_us() - start;
            audit_entry_destroy(entry);
        } else {
            pop_times[i] = 0;
        }
    }
    
    benchmark_result_t push_result = {"Queue PUSH", 0};
    calculate_stats(push_times, iterations, &push_result);
    push_result.total_ops = iterations;
    print_result(&push_result);
    
    benchmark_result_t pop_result = {"Queue POP", 0};
    calculate_stats(pop_times, iterations, &pop_result);
    pop_result.total_ops = iterations;
    print_result(&pop_result);
    
    free(push_times);
    free(pop_times);
    audit_queue_destroy(queue);
}

void benchmark_circuit_breaker(int iterations) {
    printf("\n[Circuit Breaker Benchmark] (%d iterations)\n", iterations);
    
    circuit_breaker_config_t config = {
        .failure_threshold = 5,
        .success_threshold = 3,
        .timeout_ms = 60000,
        .half_open_max_calls = 3,
        .failure_rate_threshold = 0.5
    };
    
    circuit_breaker_t* breaker = circuit_breaker_create(&config);
    if (!breaker) { printf("  FAILED to create breaker\n"); return; }
    
    uint64_t* record_times = malloc(iterations * sizeof(uint64_t));
    
    static int dummy_func(void* arg) {
        (void)arg;
        return 0;
    }
    
    for (int i = 0; i < iterations; i++) {
        uint64_t start = get_time_us();
        
        if (i % 20 == 0) {
            circuit_breaker_record_failure(breaker);
        } else {
            circuit_breaker_record_success(breaker);
        }
        
        record_times[i] = get_time_us() - start;
    }
    
    benchmark_result_t record_result = {"Record Success/Failure", 0};
    calculate_stats(record_times, iterations, &record_result);
    record_result.total_ops = iterations;
    print_result(&record_result);
    
    free(record_times);
    circuit_breaker_destroy(breaker);
}

void benchmark_memory_usage(void) {
    printf("\n[Memory Usage]\n");
    
    sanitizer_cache_t* caches[100];
    size_t total_memory = 0;
    
    for (int i = 0; i < 100; i++) {
        caches[i] = sanitizer_cache_create(1024);
    }
    
    FILE* meminfo = fopen("/proc/self/status", "r");
    if (meminfo) {
        char line[256];
        while (fgets(line, sizeof(line), meminfo)) {
            if (strncmp(line, "VmRSS:", 6) == 0) {
                printf("  RSS Memory after 100 caches: %s", line + 7);
                break;
            }
        }
        fclose(meminfo);
    }
    
    for (int i = 0; i < 100; i++) {
        sanitizer_cache_destroy(caches[i]);
    }
}

int main(int argc, char* argv[]) {
    int iterations = DEFAULT_ITERATIONS;
    if (argc > 1) {
        iterations = atoi(argv[1]);
        if (iterations < 100) iterations = 100;
    }
    
    printf("========================================\n");
    printf("  AgentOS Cupolas Performance Benchmark\n");
    printf("  Iterations: %d\n", iterations);
    printf("========================================\n");
    
    benchmark_sanitizer_cache(iterations);
    benchmark_permission_check(iterations);
    benchmark_audit_queue(iterations);
    benchmark_circuit_breaker(iterations);
    benchmark_memory_usage();
    
    printf("\n========================================\n");
    printf("  Benchmark Complete\n");
    printf("========================================\n");
    
    return 0;
}
