/**
 * @file test_performance_benchmarks.c
 * @brief AgentOS 性能基准测试框架 (P2-C02)
 *
 * 覆盖关键性能场景：
 * - 内存分配/释放延迟与吞吐量
 * - IPC消息传递延迟
 * - 任务调度吞吐量
 * - 配置读写性能
 * - 熔断器操作延迟
 * - 多线程并发性能
 *
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>

/* corekern */
#include "agentos.h"
#include "mem.h"
#include "task.h"
#include "ipc.h"
#include "error.h"
#include "agentos_time.h"
#include "observability.h"

/* daemon/common */
#include "svc_common.h"
#include "circuit_breaker.h"
#include "config_manager.h"
#include "method_dispatcher.h"
#include "alert_manager.h"

/* syscall */
#include "syscalls.h"

/* ==================== 性能测量工具 ==================== */

static inline uint64_t get_time_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

typedef struct {
    const char* name;
    uint64_t total_ns;
    uint64_t min_ns;
    uint64_t max_ns;
    uint32_t iterations;
} benchmark_result_t;

static void benchmark_run(const char* name, void (*func)(void* arg), void* arg,
                          uint32_t iterations, benchmark_result_t* out)
{
    out->name = name;
    out->total_ns = 0;
    out->min_ns = UINT64_MAX;
    out->max_ns = 0;
    out->iterations = iterations;

    uint64_t sum = 0;

    for (uint32_t i = 0; i < iterations; i++) {
        uint64_t start = get_time_ns();
        func(arg);
        uint64_t end = get_time_ns();
        uint64_t elapsed = end - start;

        sum += elapsed;
        if (elapsed < out->min_ns) out->min_ns = elapsed;
        if (elapsed > out->max_ns) out->max_ns = elapsed;
    }

    out->total_ns = sum;
}

static void print_result(const benchmark_result_t* r)
{
    double avg_us = (double)r->total_ns / r->iterations / 1000.0;
    double min_us = (double)r->min_ns / 1000.0;
    double max_us = (double)r->max_ns / 1000.0;
    double throughput = (double)r->iterations / ((double)r->total_ns / 1e9);

    printf("  %-30s | avg: %8.2f us | min: %8.2f us | max: %8.2f us | throughput: %8.0f ops/s\n",
           r->name, avg_us, min_us, max_us, throughput);
}

/* ==================== 基准测试场景 ==================== */

/* --- BM-01: 内存分配延迟 --- */

static void bm_mem_alloc_small(void* arg)
{
    (void)arg;
    void* p = agentos_mem_alloc(64);
    if (p) agentos_mem_free(p);
}

static void bm_mem_alloc_medium(void* arg)
{
    (void)arg;
    void* p = agentos_mem_alloc(4096);
    if (p) agentos_mem_free(p);
}

static void bm_mem_alloc_large(void* arg)
{
    (void)arg;
    void* p = agentos_mem_alloc(1024 * 1024);
    if (p) agentos_mem_free(p);
}

static void bm_mem_alloc_ex(void* arg)
{
    (void)arg;
    void* p = agentos_mem_alloc_ex(256, __FILE__, __LINE__);
    if (p) agentos_mem_free(p);
}

static void bm_mem_aligned_alloc(void* arg)
{
    (void)arg;
    void* p = agentos_mem_aligned_alloc(512, 64);
    if (p) agentos_mem_aligned_free(p);
}

static void test_benchmark_memory(void)
{
    printf("\n--- [BM-01] 内存分配基准测试 ---\n");
    printf("  %-30s | %-14s | %-14s | %-14s | %-14s\n",
           "Test", "Avg", "Min", "Max", "Throughput");
    printf("  %-30s-+-%-14s-+-%-14s-+-%-14s-+-%-14s\n",
           "------------------------------", "--------------", "--------------", "--------------", "--------------");

    agentos_mem_init(0);

    benchmark_result_t r;

    benchmark_run("mem_alloc(64B)", bm_mem_alloc_small, NULL, 50000, &r);
    print_result(&r);

    benchmark_run("mem_alloc(4KB)", bm_mem_alloc_medium, NULL, 50000, &r);
    print_result(&r);

    benchmark_run("mem_alloc(1MB)", bm_mem_alloc_large, NULL, 1000, &r);
    print_result(&r);

    benchmark_run("mem_alloc_ex(256B)", bm_mem_alloc_ex, NULL, 50000, &r);
    print_result(&r);

    benchmark_run("mem_aligned_alloc(512B, 64)", bm_mem_aligned_alloc, NULL, 50000, &r);
    print_result(&r);

    size_t leaks = agentos_mem_check_leaks();
    printf("\n  [INFO] 内存泄漏检查: %zu blocks\n", leaks);

    agentos_mem_cleanup();
}

/* --- BM-02: 任务调度基准 --- */

static void bm_task_self(void* arg)
{
    (void)arg;
    agentos_task_self();
}

static void bm_task_yield(void* arg)
{
    (void)arg;
    agentos_task_yield();
}

typedef struct {
    agentos_task_id_t ids[100];
    int index;
} task_ids_collector_t;

static void* task_self_thread_fn(void* arg)
{
    task_ids_collector_t* col = (task_ids_collector_t*)arg;
    int idx = __atomic_fetch_add(&col->index, 1, __ATOMIC_SEQ_CST);
    if (idx < 100) {
        col->ids[idx] = agentos_task_self();
    }
    return NULL;
}

static void bm_task_self_multi_thread(void* arg)
{
    task_ids_collector_t* col = (task_ids_collector_t*)arg;
    task_self_thread_fn(arg);
}

static void test_benchmark_task(void)
{
    printf("\n--- [BM-02] 任务调度基准测试 ---\n");
    printf("  %-30s | %-14s | %-14s | %-14s | %-14s\n",
           "Test", "Avg", "Min", "Max", "Throughput");
    printf("  %-30s-+-%-14s-+-%-14s-+-%-14s-+-%-14s\n",
           "------------------------------", "--------------", "--------------", "--------------", "--------------");

    agentos_task_init();

    benchmark_result_t r;

    benchmark_run("task_self()", bm_task_self, NULL, 100000, &r);
    print_result(&r);

    benchmark_run("task_yield()", bm_task_yield, NULL, 50000, &r);
    print_result(&r);

    task_ids_collector_t col = { .index = 0 };
    benchmark_run("task_self() 多线程", bm_task_self_multi_thread, &col, 10000, &r);
    print_result(&r);

    agentos_task_cleanup();
}

/* --- BM-03: 配置读写基准 --- */

static void bm_config_set(void* arg)
{
    static int key_counter = 0;
    char key[64], val[64];
    snprintf(key, sizeof(key), "bm.key.%d", key_counter++);
    snprintf(val, sizeof(val), "value_%d", key_counter);
    cm_set(key, val, "benchmark");
}

static void bm_config_get(void* arg)
{
    const char* v = cm_get("bm.cache_key", "default");
    (void)v;
}

static void bm_config_get_int(void* arg)
{
    int64_t v = cm_get_int("bm.int_key", 0);
    (void)v;
}

static void test_benchmark_config(void)
{
    printf("\n--- [BM-03] 配置读写基准测试 ---\n");
    printf("  %-30s | %-14s | %-14s | %-14s | %-14s\n",
           "Test", "Avg", "Min", "Max", "Throughput");
    printf("  %-30s-+-%-14s-+-%-14s-+-%-14s-+-%-14s\n",
           "------------------------------", "--------------", "--------------", "--------------", "--------------");

    cm_init(NULL);
    cm_set("bm.cache_key", "cached_value", "bm");
    cm_set("bm.int_key", "42", "bm");

    benchmark_result_t r;

    benchmark_run("config_set()", bm_config_set, NULL, 20000, &r);
    print_result(&r);

    benchmark_run("config_get()", bm_config_get, NULL, 50000, &r);
    print_result(&r);

    benchmark_run("config_get_int()", bm_config_get_int, NULL, 50000, &r);
    print_result(&r);

    cm_shutdown();
}

/* --- BM-04: 熔断器操作基准 --- */

static void bm_cb_allow_request(void* arg)
{
    circuit_breaker_t cb = (circuit_breaker_t)arg;
    cb_allow_request(cb);
}

static void bm_cb_record_success(void* arg)
{
    circuit_breaker_t cb = (circuit_breaker_t)arg;
    cb_record_success(cb, 100);
}

static void bm_cb_record_failure(void* arg)
{
    circuit_breaker_t cb = (circuit_breaker_t)arg;
    cb_record_failure(cb, 50);
}

static void test_benchmark_circuit_breaker(void)
{
    printf("\n--- [BM-04] 熔断器基准测试 ---\n");
    printf("  %-30s | %-14s | %-14s | %-14s | %-14s\n",
           "Test", "Avg", "Min", "Max", "Throughput");
    printf("  %-30s-+-%-14s-+-%-14s-+-%-14s-+-%-14s\n",
           "------------------------------", "--------------", "--------------", "--------------", "--------------");

    cb_manager_t mgr = cb_manager_create();
    circuit_breaker_t cb = cb_create(mgr, "bm_test", NULL);

    benchmark_result_t r;

    benchmark_run("cb_allow_request()", bm_cb_allow_request, cb, 100000, &r);
    print_result(&r);

    benchmark_run("cb_record_success()", bm_cb_record_success, cb, 100000, &r);
    print_result(&r);

    benchmark_run("cb_record_failure()", bm_cb_record_failure, cb, 100000, &r);
    print_result(&r);

    cb_manager_destroy(mgr);
}

/* --- BM-05: 方法分发器基准 --- */

static void bm_dispatcher_handler(cJSON* params, int id, void* user_data)
{
    (void)params; (void)id; (void)user_data;
}

typedef struct {
    method_dispatcher_t* disp;
    cJSON* request;
} bm_disp_ctx_t;

static void bm_method_dispatch(void* arg)
{
    bm_disp_ctx_t* ctx = (bm_disp_ctx_t*)arg;
    method_dispatcher_dispatch(ctx->disp, ctx->request, NULL, NULL);
}

static void test_benchmark_dispatcher(void)
{
    printf("\n--- [BM-05] 方法分发器基准测试 ---\n");
    printf("  %-30s | %-14s | %-14s | %-14s | %-14s\n",
           "Test", "Avg", "Min", "Max", "Throughput");
    printf("  %-30s-+-%-14s-+-%-14s-+-%-14s-+-%-14s\n",
           "------------------------------", "--------------", "--------------", "--------------", "--------------");

    method_dispatcher_t* disp = method_dispatcher_create(64);
    method_dispatcher_register(disp, "bm.method", bm_dispatcher_handler, NULL);

    cJSON* req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "method", "bm.method");
    cJSON_AddNumberToObject(req, "id", 1);
    cJSON_AddObjectToObject(req, "params");

    bm_disp_ctx_t ctx = { .disp = disp, .request = req };

    benchmark_result_t r;
    benchmark_run("method_dispatcher_dispatch()", bm_method_dispatch, &ctx, 50000, &r);
    print_result(&r);

    cJSON_Delete(req);
    method_dispatcher_destroy(disp);
}

/* --- BM-06: 可观测性基准 --- */

static void bm_metric_counter_inc(void* arg)
{
    (void)arg;
    agentos_metric_counter_inc("bm_counter", "tag1", 1.0);
}

static void bm_trace_span_start(void* arg)
{
    (void)arg;
    agentos_trace_context_t ctx;
    agentos_trace_span_start(&ctx, "bm_service", "bm_op");
}

static void test_benchmark_observability(void)
{
    printf("\n--- [BM-06] 可观测性基准测试 ---\n");
    printf("  %-30s | %-14s | %-14s | %-14s | %-14s\n",
           "Test", "Avg", "Min", "Max", "Throughput");
    printf("  %-30s-+-%-14s-+-%-14s-+-%-14s-+-%-14s\n",
           "------------------------------", "--------------", "--------------", "--------------", "--------------");

    agentos_observability_config_t cfg = {
        .enable_metrics = 1, .enable_tracing = 1, .enable_health_check = 1
    };
    agentos_observability_init(&cfg);

    benchmark_result_t r;

    benchmark_run("metric_counter_inc()", bm_metric_counter_inc, NULL, 50000, &r);
    print_result(&r);

    benchmark_run("trace_span_start()", bm_trace_span_start, NULL, 50000, &r);
    print_result(&r);

    agentos_observability_shutdown();
}

/* --- BM-07: 系统调用基准 --- */

static void bm_sys_memory_write(void* arg)
{
    (void)arg;
    char* record_id = NULL;
    agentos_sys_memory_write("bm_data", 7, "{\"type\":\"bm\"}", &record_id);
    if (record_id) agentos_sys_free(record_id);
}

static void test_benchmark_syscall(void)
{
    printf("\n--- [BM-07] 系统调用基准测试 ---\n");
    printf("  %-30s | %-14s | %-14s | %-14s | %-14s\n",
           "Test", "Avg", "Min", "Max", "Throughput");
    printf("  %-30s-+-%-14s-+-%-14s-+-%-14s-+-%-14s\n",
           "------------------------------", "--------------", "--------------", "--------------", "--------------");

    benchmark_result_t r;

    benchmark_run("sys_memory_write()", bm_sys_memory_write, NULL, 10000, &r);
    print_result(&r);
}

/* --- BM-08: 多线程并发基准 --- */

typedef struct {
    int thread_id;
    uint64_t total_alloc_time_ns;
    uint32_t alloc_count;
} bm_thread_ctx_t;

static void* bm_concurrent_thread_fn(void* arg)
{
    bm_thread_ctx_t* ctx = (bm_thread_ctx_t*)arg;

    uint64_t start = get_time_ns();
    for (int i = 0; i < 10000; i++) {
        void* p = agentos_mem_alloc(128 + (size_t)(ctx->thread_id % 8) * 16);
        if (p) {
            memset(p, (unsigned char)(ctx->thread_id + i), 128 + (size_t)(ctx->thread_id % 8) * 16);
            agentos_mem_free(p);
            ctx->alloc_count++;
        }
    }
    uint64_t end = get_time_ns();
    ctx->total_alloc_time_ns = end - start;

    return NULL;
}

static void test_benchmark_concurrent(void)
{
    printf("\n--- [BM-08] 多线程并发基准测试 ---\n");

    agentos_mem_init(0);

    #define BM_THREAD_COUNT 8
    agentos_thread_t threads[BM_THREAD_COUNT];
    bm_thread_ctx_t ctxs[BM_THREAD_COUNT];

    uint64_t wall_start = get_time_ns();

    for (int i = 0; i < BM_THREAD_COUNT; i++) {
        ctxs[i].thread_id = i;
        ctxs[i].total_alloc_time_ns = 0;
        ctxs[i].alloc_count = 0;
        agentos_thread_create(&threads[i], bm_concurrent_thread_fn, &ctxs[i]);
    }

    uint64_t total_ops = 0;
    for (int i = 0; i < BM_THREAD_COUNT; i++) {
        agentos_thread_join(threads[i], NULL);
        total_ops += ctxs[i].alloc_count;
    }

    uint64_t wall_end = get_time_ns();
    double elapsed_s = (double)(wall_end - wall_start) / 1e9;
    double throughput = (double)total_ops / elapsed_s;

    printf("  %-30s %d threads x 10,000 allocs = %u total ops\n",
           "Configuration:", BM_THREAD_COUNT, (uint32_t)total_ops);
    printf("  %-30s %.3f seconds\n", "Wall time:", elapsed_s);
    printf("  %-30s %.0f ops/sec\n", "Throughput:", throughput);

    size_t leaks = agentos_mem_check_leaks();
    printf("  %-30s %zu blocks\n", "Memory leaks:", leaks);

    agentos_mem_cleanup();
}

/* ==================== main 入口 ==================== */

int main(void)
{
    printf("========================================\n");
    printf("  AgentOS 性能基准测试框架 (P2-C02)\n");
    printf("========================================\n");

    test_benchmark_memory();
    test_benchmark_task();
    test_benchmark_config();
    test_benchmark_circuit_breaker();
    test_benchmark_dispatcher();
    test_benchmark_observability();
    test_benchmark_syscall();
    test_benchmark_concurrent();

    printf("\n========================================\n");
    printf("  P2-C02 基准测试完成 ✅\n");
    printf("========================================\n");

    return 0;
}
