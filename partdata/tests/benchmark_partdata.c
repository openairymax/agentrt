/**
 * @file benchmark_partdata.c
 * @brief AgentOS 数据分区性能基准测试
 *
 * Copyright (c) 2026 SPHARX. All Rights Reserved.
 * "From data intelligence emerges."
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

#include "partdata.h"
#include "partdata_log.h"
#include "partdata_registry.h"
#include "partdata_trace.h"
#include "partdata_ipc.h"
#include "partdata_memory.h"

#define BENCHMARK_ITERATIONS 10000
#define BENCHMARK_BATCH_SIZE 100

static double get_time_ms(void) {
#ifdef _WIN32
    LARGE_INTEGER frequency;
    LARGE_INTEGER counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (double)(counter.QuadPart * 1000.0 / frequency.QuadPart);
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
#endif
}

static void benchmark_init_shutdown(void) {
    printf("\n=== Init/Shutdown Benchmark ===\n");

    double start = get_time_ms();

    for (int i = 0; i < 100; i++) {
        partdata_init(&(partdata_config_t){.root_path = "bench_partdata"});
        partdata_shutdown();
    }

    double elapsed = get_time_ms() - start;
    printf("100 init/shutdown cycles: %.2f ms (%.2f ms/op)\n",
           elapsed, elapsed / 100.0);
}

static void benchmark_logging(void) {
    printf("\n=== Logging Benchmark ===\n");

    partdata_init(&(partdata_config_t){.root_path = "bench_partdata"});
    partdata_log_set_level(PARTDATA_LOG_ERROR);

    double start = get_time_ms();

    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        PARTDATA_LOG_ERROR("bench_service", "trace_bench", "Benchmark log message %d", i);
    }

    double elapsed = get_time_ms() - start;
    printf("%d log writes: %.2f ms (%.2f ops/sec)\n",
           BENCHMARK_ITERATIONS, elapsed, BENCHMARK_ITERATIONS * 1000.0 / elapsed);

    partdata_shutdown();
}

static void benchmark_registry_insert(void) {
    printf("\n=== Registry Insert Benchmark ===\n");

    partdata_init(&(partdata_config_t){.root_path = "bench_partdata"});

    double start = get_time_ms();

    for (int i = 0; i < 1000; i++) {
        partdata_agent_record_t agent;
        memset(&agent, 0, sizeof(agent));
        snprintf(agent.id, sizeof(agent.id), "bench_agent_%d", i);
        snprintf(agent.name, sizeof(agent.name), "Benchmark Agent %d", i);
        snprintf(agent.type, sizeof(agent.type), "benchmark");
        snprintf(agent.version, sizeof(agent.version), "1.0.0");
        snprintf(agent.status, sizeof(agent.status), "active");
        agent.created_at = (uint64_t)time(NULL);
        agent.updated_at = agent.created_at;

        partdata_registry_add_agent(&agent);
    }

    double elapsed = get_time_ms() - start;
    printf("1000 agent inserts: %.2f ms (%.2f ops/sec)\n",
           elapsed, 1000 * 1000.0 / elapsed);

    partdata_shutdown();
}

static void benchmark_trace_write(void) {
    printf("\n=== Trace Write Benchmark ===\n");

    partdata_init(&(partdata_config_t){.root_path = "bench_partdata"});

    double start = get_time_ms();

    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        partdata_span_t span;
        memset(&span, 0, sizeof(span));
        snprintf(span.trace_id, sizeof(span.trace_id), "trace_bench_%d", i);
        snprintf(span.span_id, sizeof(span.span_id), "span_%d", i);
        snprintf(span.name, sizeof(span.name), "bench_operation");
        span.start_time_ns = (uint64_t)time(NULL) * 1000000000;
        span.end_time_ns = span.start_time_ns + 1000000;
        snprintf(span.service_name, sizeof(span.service_name), "bench_service");
        snprintf(span.status, sizeof(span.status), "OK");

        partdata_trace_write_span(&span);
    }

    double elapsed = get_time_ms() - start;
    printf("%d span writes: %.2f ms (%.2f ops/sec)\n",
           BENCHMARK_ITERATIONS, elapsed, BENCHMARK_ITERATIONS * 1000.0 / elapsed);

    partdata_trace_flush();
    partdata_shutdown();
}

static void benchmark_trace_batch_write(void) {
    printf("\n=== Trace Batch Write Benchmark ===\n");

    partdata_init(&(partdata_config_t){.root_path = "bench_partdata"});

    partdata_span_t spans[BENCHMARK_BATCH_SIZE];

    double start = get_time_ms();

    for (int batch = 0; batch < BENCHMARK_ITERATIONS / BENCHMARK_BATCH_SIZE; batch++) {
        for (int i = 0; i < BENCHMARK_BATCH_SIZE; i++) {
            memset(&spans[i], 0, sizeof(partdata_span_t));
            snprintf(spans[i].trace_id, sizeof(spans[i].trace_id), "trace_batch_%d_%d", batch, i);
            snprintf(spans[i].span_id, sizeof(spans[i].span_id), "span_%d_%d", batch, i);
            snprintf(spans[i].name, sizeof(spans[i].name), "batch_operation");
            spans[i].start_time_ns = (uint64_t)time(NULL) * 1000000000;
            spans[i].end_time_ns = spans[i].start_time_ns + 1000000;
            snprintf(spans[i].service_name, sizeof(spans[i].service_name), "bench_service");
            snprintf(spans[i].status, sizeof(spans[i].status), "OK");
        }

        partdata_trace_write_spans_batch(spans, BENCHMARK_BATCH_SIZE);
    }

    double elapsed = get_time_ms() - start;
    printf("%d span batch writes (%d per batch): %.2f ms (%.2f ops/sec)\n",
           BENCHMARK_ITERATIONS, BENCHMARK_BATCH_SIZE,
           elapsed, BENCHMARK_ITERATIONS * 1000.0 / elapsed);

    partdata_trace_flush();
    partdata_shutdown();
}

static void benchmark_ipc_operations(void) {
    printf("\n=== IPC Operations Benchmark ===\n");

    partdata_init(&(partdata_config_t){.root_path = "bench_partdata"});

    double start = get_time_ms();

    for (int i = 0; i < 5000; i++) {
        partdata_ipc_channel_t channel;
        memset(&channel, 0, sizeof(channel));
        snprintf(channel.channel_id, sizeof(channel.channel_id), "bench_ch_%d", i);
        snprintf(channel.name, sizeof(channel.name), "Benchmark Channel %d", i);
        snprintf(channel.type, sizeof(channel.type), "benchmark");
        channel.created_at = (uint64_t)time(NULL);
        snprintf(channel.status, sizeof(channel.status), "active");

        partdata_ipc_record_channel(&channel);
    }

    double elapsed = get_time_ms() - start;
    printf("5000 IPC channel inserts: %.2f ms (%.2f ops/sec)\n",
           elapsed, 5000 * 1000.0 / elapsed);

    start = get_time_ms();

    for (int i = 0; i < 5000; i++) {
        partdata_ipc_channel_t channel;
        memset(&channel, 0, sizeof(channel));
        snprintf(channel.channel_id, sizeof(channel.channel_id), "bench_ch_%d", i);
        partdata_ipc_get_channel(channel.channel_id, &channel);
    }

    elapsed = get_time_ms() - start;
    printf("5000 IPC channel lookups: %.2f ms (%.2f ops/sec)\n",
           elapsed, 5000 * 1000.0 / elapsed);

    partdata_shutdown();
}

static void benchmark_memory_operations(void) {
    printf("\n=== Memory Operations Benchmark ===\n");

    partdata_init(&(partdata_config_t){.root_path = "bench_partdata"});

    double start = get_time_ms();

    for (int i = 0; i < 5000; i++) {
        partdata_memory_pool_t pool;
        memset(&pool, 0, sizeof(pool));
        snprintf(pool.pool_id, sizeof(pool.pool_id), "bench_pool_%d", i);
        snprintf(pool.name, sizeof(pool.name), "Benchmark Pool %d", i);
        pool.total_size = 1024 * 1024;
        pool.used_size = 512 * 1024;
        pool.block_count = 256;
        pool.free_block_count = 128;
        pool.created_at = (uint64_t)time(NULL);
        snprintf(pool.status, sizeof(pool.status), "active");

        partdata_memory_record_pool(&pool);
    }

    double elapsed = get_time_ms() - start;
    printf("5000 memory pool inserts: %.2f ms (%.2f ops/sec)\n",
           elapsed, 5000 * 1000.0 / elapsed);

    start = get_time_ms();

    for (int i = 0; i < 5000; i++) {
        partdata_memory_pool_t pool;
        memset(&pool, 0, sizeof(pool));
        snprintf(pool.pool_id, sizeof(pool.pool_id), "bench_pool_%d", i);
        partdata_memory_get_pool(pool.pool_id, &pool);
    }

    elapsed = get_time_ms() - start;
    printf("5000 memory pool lookups: %.2f ms (%.2f ops/sec)\n",
           elapsed, 5000 * 1000.0 / elapsed);

    partdata_shutdown();
}

static void benchmark_stats_operations(void) {
    printf("\n=== Stats Operations Benchmark ===\n");

    partdata_init(&(partdata_config_t){.root_path = "bench_partdata"});

    for (int i = 0; i < 1000; i++) {
        partdata_ipc_channel_t channel;
        memset(&channel, 0, sizeof(channel));
        snprintf(channel.channel_id, sizeof(channel.channel_id), "stats_ch_%d", i);
        snprintf(channel.name, sizeof(channel.name), "Stats Channel %d", i);
        snprintf(channel.type, sizeof(channel.type), "stats");
        channel.created_at = (uint64_t)time(NULL);
        snprintf(channel.status, sizeof(channel.status), "active");
        partdata_ipc_record_channel(&channel);
    }

    double start = get_time_ms();

    for (int i = 0; i < 10000; i++) {
        uint32_t ch_count = 0, buf_count = 0;
        uint64_t total_size = 0;
        partdata_ipc_get_stats(&ch_count, &buf_count, &total_size);
    }

    double elapsed = get_time_ms() - start;
    printf("10000 stats queries: %.2f ms (%.2f ops/sec)\n",
           elapsed, 10000 * 1000.0 / elapsed);

    partdata_shutdown();
}

int main(void) {
    printf("===========================================\n");
    printf("   AgentOS Partdata Performance Benchmark\n");
    printf("===========================================\n");
    printf("Iterations per test: %d\n", BENCHMARK_ITERATIONS);
    printf("===========================================\n");

    srand((unsigned int)time(NULL));

    benchmark_init_shutdown();
    benchmark_logging();
    benchmark_registry_insert();
    benchmark_trace_write();
    benchmark_trace_batch_write();
    benchmark_ipc_operations();
    benchmark_memory_operations();
    benchmark_stats_operations();

    printf("\n===========================================\n");
    printf("           Benchmark Complete\n");
    printf("===========================================\n");

    return 0;
}
