/**
 * @file test_kernel_state.c
 * @brief corekern 模块系统级集成测试
 *
 * 覆盖范围：
 * - 系统初始化/清理生命周期
 * - 内存管理基础功能
 * - 任务调度基础功能
 * - 错误码体系验证
 * - 可观测性子系统初始化
 *
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "agentos.h"
#include "mem.h"
#include "task.h"
#include "error.h"
#include "agentos_time.h"
#include "observability.h"
#include <stdlib.h>

/* ==================== 前向声明（不在头文件中的函数） ==================== */
AGENTOS_API int agentos_core_init(void);
AGENTOS_API void agentos_core_shutdown(void);
AGENTOS_API agentos_error_t agentos_task_init(void);

/* ==================== 测试框架 ==================== */

static int g_tests_run = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST_ASSERT(cond, msg) do { \
    g_tests_run++; \
    if (cond) { \
        g_tests_passed++; \
        printf("  [PASS] %s\n", msg); \
    } else { \
        g_tests_failed++; \
        printf("  [FAIL] %s (line %d)\n", msg, __LINE__); \
    } \
} while(0)

#define TEST_ASSERT_EQ(a, b, msg) do { \
    g_tests_run++; \
    if ((a) == (b)) { \
        g_tests_passed++; \
        printf("  [PASS] %s\n", msg); \
    } else { \
        g_tests_failed++; \
        printf("  [FAIL] %s: expected %ld, got %ld (line %d)\n", msg, (long)(b), (long)(a), __LINE__); \
    } \
} while(0)

#define TEST_ASSERT_NEQ(a, b, msg) do { \
    g_tests_run++; \
    if ((a) != (b)) { \
        g_tests_passed++; \
        printf("  [PASS] %s\n", msg); \
    } else { \
        g_tests_failed++; \
        printf("  [FAIL] %s: values should differ (line %d)\n", msg, __LINE__); \
    } \
} while(0)

#define TEST_ASSERT_NULL(ptr, msg) TEST_ASSERT((ptr) == NULL, msg)
#define TEST_ASSERT_NOT_NULL(ptr, msg) TEST_ASSERT((ptr) != NULL, msg)

/* ==================== 1. 系统初始化与生命周期测试 ==================== */

static void test_agentos_init_cleanup(void)
{
    printf("\n--- 系统初始化与清理 ---\n");

    int ret = agentos_core_init();
    TEST_ASSERT_EQ(ret, AGENTOS_SUCCESS, "agentos_core_init() 返回 SUCCESS");
    TEST_ASSERT_EQ(ret, 0, "AGENTOS_SUCCESS 值为 0");

    agentos_core_shutdown();
    printf("  [INFO] agentos_core_shutdown() 完成\n");
}

static void test_double_init(void)
{
    printf("\n--- 双重初始化测试 ---\n");

    int err1 = agentos_core_init();
    TEST_ASSERT_EQ(err1, AGENTOS_SUCCESS, "第一次 init 成功");

    int err2 = agentos_core_init();
    TEST_ASSERT_EQ(err2, AGENTOS_SUCCESS, "第二次 init 也应成功（幂等）");

    agentos_core_shutdown();
}

/* ==================== 2. 内存管理基础测试 ==================== */

static void test_mem_alloc_free(void)
{
    printf("\n--- 内存分配与释放 ---\n");

    agentos_error_t err = agentos_mem_init(0);
    TEST_ASSERT_EQ(err, AGENTOS_SUCCESS, "agentos_mem_init() 成功");

    void* ptr1 = agentos_mem_alloc(256);
    TEST_ASSERT_NOT_NULL(ptr1, "agentos_mem_alloc(256) 返回非空指针");

    void* ptr2 = agentos_mem_alloc(1024);
    TEST_ASSERT_NOT_NULL(ptr2, "agentos_mem_alloc(1024) 返回非空指针");

    TEST_ASSERT_NEQ(ptr1, ptr2, "两次分配返回不同地址");

    memset(ptr1, 0xAA, 256);
    memset(ptr2, 0xBB, 1024);

    unsigned char* p1 = (unsigned char*)ptr1;
    unsigned char* p2 = (unsigned char*)ptr2;
    TEST_ASSERT(p1[0] == 0xAA && p1[255] == 0xAA, "ptr1 内存可读写且数据正确");
    TEST_ASSERT(p2[0] == 0xBB && p2[1023] == 0xBB, "ptr2 内存可读写且数据正确");

    agentos_mem_free(ptr1);
    agentos_mem_free(ptr2);

    size_t leaks = agentos_mem_check_leaks();
    TEST_ASSERT_EQ(leaks, 0, "释放后无内存泄漏");

    agentos_mem_cleanup();
}

static void test_mem_zero_size(void)
{
    printf("\n--- 零大小分配 ---\n");

    agentos_mem_init(0);

    void* ptr = agentos_mem_alloc(0);
    TEST_ASSERT_NOT_NULL(ptr, "agentos_mem_alloc(0) 返回有效指针（实现依赖）");
    if (ptr) {
        agentos_mem_free(ptr);
    }

    agentos_mem_cleanup();
}

static void test_mem_null_free(void)
{
    printf("\n--- NULL指针释放安全性 ---\n");

    agentos_mem_init(0);

    agentos_mem_free(NULL);
    TEST_ASSERT(1, "agentos_mem_free(NULL) 不崩溃");

    agentos_mem_aligned_free(NULL);
    TEST_ASSERT(1, "agentos_mem_aligned_free(NULL) 不崩溃");

    agentos_mem_cleanup();
}

static void test_mem_realloc(void)
{
    printf("\n--- 内存重新分配 ---\n");

    agentos_mem_init(0);

    void* ptr = agentos_mem_alloc(64);
    TEST_ASSERT_NOT_NULL(ptr, "初始分配64字节");

    const char* test_str = "Hello, AgentOS Memory System!";
    size_t test_len = strlen(test_str);
    memcpy((char*)ptr, test_str, test_len + 1);

    void* new_ptr = agentos_mem_realloc(ptr, 256);
    TEST_ASSERT_NOT_NULL(new_ptr, "realloc 到 256 字节成功");
    TEST_ASSERT(strcmp((char*)new_ptr, test_str) == 0, "realloc 后数据保持完整");

    void* shrinked = agentos_mem_realloc(new_ptr, 32);
    TEST_ASSERT_NOT_NULL(shrinked, "缩小到 32 字节成功");
    TEST_ASSERT(strncmp((char*)shrinked, test_str, 31) == 0, "缩小后数据截断正确");

    agentos_mem_free(shrinked);
    TEST_ASSERT_EQ(agentos_mem_check_leaks(), 0, "realloc 链无泄漏");

    agentos_mem_cleanup();
}

static void test_mem_realloc_null(void)
{
    printf("\n--- realloc(ptr=NULL) 行为 ---\n");

    agentos_mem_init(0);

    void* ptr = agentos_mem_realloc(NULL, 128);
    TEST_ASSERT_NOT_NULL(ptr, "realloc(NULL, 128) 等价于 malloc(128)");
    agentos_mem_free(ptr);

    void* result = agentos_mem_realloc(ptr, 0);
    TEST_ASSERT_NULL(result, "realloc(ptr, 0) 等价于 free(ptr)");

    agentos_mem_cleanup();
}

static void test_mem_aligned_alloc(void)
{
    printf("\n--- 对齐内存分配 ---\n");

    agentos_mem_init(0);

    for (size_t align = 8; align <= 4096; align *= 2) {
        void* ptr = agentos_mem_aligned_alloc(128, align);
        TEST_ASSERT_NOT_NULL(ptr, "对齐分配成功");
        if (ptr) {
            uintptr_t addr = (uintptr_t)ptr;
            TEST_ASSERT(addr % align == 0, "地址按指定对齐值对齐");
            agentos_mem_aligned_free(ptr);
        }
    }

    agentos_mem_cleanup();
}

static void test_mem_stats(void)
{
    printf("\n--- 内存统计 ---\n");

    agentos_mem_init(0);

    size_t total_before = 0, used_before = 0, peak_before = 0;
    agentos_mem_stats(&total_before, &used_before, &peak_before);

    void* p1 = agentos_mem_alloc(100);
    void* p2 = agentos_mem_alloc(200);

    size_t total_mid = 0, used_mid = 0, peak_mid = 0;
    agentos_mem_stats(&total_mid, &used_mid, &peak_mid);

    TEST_ASSERT(total_mid >= total_before + 300, "总分配量增加");
    TEST_ASSERT(used_mid >= used_before + 300, "使用量增加");
    TEST_ASSERT(peak_mid >= peak_before, "峰值不减少");

    agentos_mem_free(p1);

    size_t total_after = 0, used_after = 0, peak_after = 0;
    agentos_mem_stats(&total_after, &used_after, &peak_after);

    TEST_ASSERT(used_after < used_mid, "释放后使用量减少");
    TEST_ASSERT(peak_after >= peak_mid, "峰值保持历史最高");

    agentos_mem_free(p2);
    agentos_mem_cleanup();
}

/* ==================== 3. 内存池测试 ==================== */

static void test_pool_create_destroy(void)
{
    printf("\n--- 内存池创建与销毁 ---\n");

    agentos_mem_init(0);

    agentos_mem_pool_t* pool = agentos_mem_pool_create(64, 16);
    TEST_ASSERT_NOT_NULL(pool, "创建 64B×16 的内存池成功");

    agentos_mem_pool_destroy(pool);
    TEST_ASSERT(1, "销毁内存池不崩溃");

    agentos_mem_pool_t* null_pool = agentos_mem_pool_create(0, 10);
    TEST_ASSERT_NULL(null_pool, "block_size=0 时返回 NULL");

    null_pool = agentos_mem_pool_create(64, 0);
    TEST_ASSERT_NULL(null_pool, "block_count=0 时返回 NULL");

    agentos_mem_cleanup();
}

static void test_pool_alloc_free(void)
{
    printf("\n--- 内存池分配与归还 ---\n");

    agentos_mem_init(0);

    const uint32_t block_count = 8;
    const size_t block_size = 128;
    agentos_mem_pool_t* pool = agentos_mem_pool_create(block_size, block_count);
    TEST_ASSERT_NOT_NULL(pool, "创建 128B×8 的内存池成功");

    void* blocks[8];
    for (uint32_t i = 0; i < block_count; i++) {
        blocks[i] = agentos_mem_pool_alloc(pool);
        TEST_ASSERT_NOT_NULL(blocks[i], "从池中分配块成功");
    }

    void* overflow = agentos_mem_pool_alloc(pool);
    TEST_ASSERT_NULL(overflow, "池耗尽时返回 NULL");

    for (uint32_t i = 0; i < block_count; i++) {
        memset(blocks[i], (int)(i + 1), block_size);
    }

    for (uint32_t i = 0; i < block_count; i++) {
        agentos_mem_pool_free(pool, blocks[i]);
    }

    void* after_return = agentos_mem_pool_alloc(pool);
    TEST_ASSERT_NOT_NULL(after_return, "归还后重新分配成功");

    unsigned char* data = (unsigned char*)after_return;
    TEST_ASSERT(data[0] == 0, "归还的块被清零（或至少可用）");

    agentos_mem_pool_free(pool, after_return);
    agentos_mem_pool_destroy(pool);
    agentos_mem_cleanup();
}

static void test_pool_null_safety(void)
{
    printf("\n--- 内存池NULL安全 ---\n");

    agentos_mem_init(0);

    void* result = agentos_mem_pool_alloc(NULL);
    TEST_ASSERT_NULL(result, "pool_alloc(NULL) 安全返回 NULL");

    agentos_mem_pool_free(NULL, (void*)0x1234);
    TEST_ASSERT(1, "pool_free(NULL, ptr) 不崩溃");

    agentos_mem_pool_destroy(NULL);
    TEST_ASSERT(1, "pool_destroy(NULL) 不崩溃");

    agentos_mem_cleanup();
}

/* ==================== 4. 任务系统测试 ==================== */

static int g_task_entry_called = 0;
static void* simple_task_entry(void* arg)
{
    g_task_entry_called++;
    int* val = (int*)arg;
    if (val) (*val)++;
    return arg;
}

static void test_task_create_join(void)
{
    printf("\n--- 任务创建与等待 ---\n");

    agentos_error_t err = agentos_task_init();
    TEST_ASSERT_EQ(err, AGENTOS_SUCCESS, "task_init 成功");

    int shared_val = 0;
    agentos_thread_t thread;

    err = agentos_thread_create(&thread, simple_task_entry, &shared_val);
    TEST_ASSERT_EQ(err, AGENTOS_SUCCESS, "thread_create 成功");

    void* retval = NULL;
    err = agentos_thread_join(thread, &retval);
    TEST_ASSERT_EQ(err, AGENTOS_SUCCESS, "thread_join 成功");
    TEST_ASSERT(retval == &shared_val, "返回值为传入的参数");
    TEST_ASSERT(g_task_entry_called > 0, "任务入口函数被执行");
    TEST_ASSERT(shared_val > 0, "共享变量被修改");

    agentos_task_cleanup();
}

static void test_task_cleanup_full(void)
{
    printf("\n--- 任务系统清理 ---\n");

    agentos_error_t err = agentos_task_init();
    TEST_ASSERT_EQ(err, AGENTOS_SUCCESS, "task_init 成功");

    agentos_thread_t threads[4];
    int vals[4] = {0};

    for (int i = 0; i < 4; i++) {
        err = agentos_thread_create(&threads[i], simple_task_entry, &vals[i]);
        TEST_ASSERT_EQ(err, AGENTOS_SUCCESS, "创建多个任务成功");
    }

    for (int i = 0; i < 4; i++) {
        agentos_thread_join(threads[i], NULL);
    }

    agentos_task_cleanup();
    TEST_ASSERT(1, "agentos_task_cleanup() 执行完成");
}

/* ==================== 5. 时间系统测试 ==================== */

static void test_time_monotonic(void)
{
    printf("\n--- 单调时钟 ---\n");

    uint64_t t1 = agentos_time_monotonic_ns();
    TEST_ASSERT(t1 > 0, "单调时钟返回正值");

    volatile int sink = 0;
    for (int i = 0; i < 10000; i++) sink++;

    uint64_t t2 = agentos_time_monotonic_ns();
    TEST_ASSERT(t2 >= t1, "单调时钟单调递增");
}

static void test_time_realtime(void)
{
    printf("\n--- 实时时钟 ---\n");

    uint64_t ns = agentos_time_realtime_ns();
    TEST_ASSERT(ns > 1700000000000000ULL, "实时时间 > 2023年（纳秒）");
}

static void test_time_sleep(void)
{
    printf("\n--- 睡眠精度 ---\n");

    uint64_t before = agentos_time_monotonic_ns();

    agentos_time_sleep_ms(50);

    uint64_t after = agentos_time_monotonic_ns();
    uint64_t elapsed_us = (after - before) / 1000;

    TEST_ASSERT(elapsed_us >= 45000, "睡眠时间 >= 45ms（允许5ms误差）");
    TEST_ASSERT(elapsed_us <= 150000, "睡眠时间 <= 150ms（上限保护）");
}

/* 事件等待线程入口函数（需要顶层声明以符合C99） */
typedef struct { agentos_event_t* evt; int* result; } wait_arg_t;

static void* waiter_fn(void* arg) {
    wait_arg_t* wa = (wait_arg_t*)arg;
    *wa->result = agentos_event_wait(wa->evt, 5000);
    return NULL;
}

static void test_event_signal_wait(void)
{
    printf("\n--- 事件信号机制 ---\n");

    agentos_event_t* event = agentos_event_create();
    TEST_ASSERT_NOT_NULL(event, "event_create 成功");

    agentos_error_t err = agentos_event_reset(event);
    TEST_ASSERT_EQ(err, AGENTOS_SUCCESS, "event_reset 成功（线程安全）");

    agentos_thread_t waiter_thread;
    static int wait_result = -1;

    wait_arg_t warg = { .evt = event, .result = &wait_result };
    agentos_thread_create(&waiter_thread, waiter_fn, &warg);

    agentos_time_sleep_ms(100);
    agentos_event_signal(event);

    agentos_thread_join(waiter_thread, NULL);
    TEST_ASSERT_EQ(wait_result, AGENTOS_SUCCESS, "事件等待者收到信号并返回SUCCESS");

    agentos_event_destroy(event);
}

static void test_eventloop_lifecycle(void)
{
    printf("\n--- 事件循环生命周期 ---\n");

    agentos_error_t err = agentos_time_eventloop_init();
    TEST_ASSERT_EQ(err, AGENTOS_SUCCESS, "eventloop_init 成功");

    /* eventloop_run()是阻塞循环，测试init/stop/cleanup即可 */
    agentos_time_eventloop_stop();

    agentos_time_eventloop_cleanup();
    TEST_ASSERT(1, "eventloop_cleanup 执行完成");
}

/* ==================== 6. IPC子系统测试 ==================== */

static void test_ipc_init_cleanup(void)
{
    printf("\n--- IPC子系统生命周期 ---\n");

    agentos_error_t err = agentos_ipc_init();
    TEST_ASSERT_EQ(err, AGENTOS_SUCCESS, "ipc_init 成功");

    agentos_ipc_cleanup();
    TEST_ASSERT(1, "ipc_cleanup 执行完成");
}

static void test_ipc_channel_create_close(void)
{
    printf("\n--- IPC通道创建与关闭 ---\n");

    agentos_ipc_init();

    agentos_ipc_channel_t* channel = NULL;
    agentos_error_t err = agentos_ipc_create_channel(
        "test_channel", NULL, NULL, &channel);
    TEST_ASSERT_EQ(err, AGENTOS_SUCCESS, "ipc_create_channel 成功");
    TEST_ASSERT_NOT_NULL(channel, "通道句柄非空");

    int32_t fd = agentos_ipc_get_fd(channel);
    TEST_ASSERT(fd != 0 || fd == -1, "get_fd 返回有效值（-1表示内存队列）");

    err = agentos_ipc_close(channel);
    TEST_ASSERT_EQ(err, AGENTOS_SUCCESS, "ipc_close 成功");

    agentos_ipc_cleanup();
}

/* ==================== 7. 错误码体系测试 ==================== */

static void test_error_codes(void)
{
    printf("\n--- 错误码体系 ---\n");

    TEST_ASSERT_EQ(AGENTOS_SUCCESS, 0, "SUCCESS == 0");
    TEST_ASSERT(AGENTOS_EINVAL < 0, "EINVAL 为负数");
    TEST_ASSERT(AGENTOS_ENOMEM < 0, "ENOMEM 为负数");
    TEST_ASSERT(AGENTOS_ENOSYS < 0, "ENOSYS 为负数");
    TEST_ASSERT(AGENTOS_EINTR < 0, "EINTR 为负数");
    TEST_ASSERT(AGENTOS_EBADF < 0, "EBADF 为负数");
    TEST_ASSERT(AGENTOS_ERESOURCE < 0, "ERESOURCE 为负数");
}

/* ==================== 8. 可观测性子系统测试 ==================== */

static void test_observability_init_shutdown(void)
{
    printf("\n--- 可观测性生命周期 ---\n");

    agentos_observability_config_t config = {
        .enable_metrics = 1,
        .enable_tracing = 1,
        .enable_health_check = 1,
        .metrics_interval_ms = 5000,
        .health_check_interval_ms = 10000
    };

    agentos_error_t err = agentos_observability_init(&config);
    TEST_ASSERT_EQ(err, AGENTOS_SUCCESS, "observability_init 成功");

    agentos_observability_shutdown();
    TEST_ASSERT(1, "observability_shutdown 完成无崩溃");
}

static agentos_health_status_t mock_health_pass(void* ud)
{
    (void)ud;
    return AGENTOS_HEALTH_PASS;
}
static agentos_health_status_t mock_health_fail(void* ud)
{
    (void)ud;
    return AGENTOS_HEALTH_FAIL;
}
static agentos_health_status_t mock_health_warn(void* ud)
{
    (void)ud;
    return AGENTOS_HEALTH_WARN;
}

static void test_health_checks(void)
{
    printf("\n--- 健康检查注册与运行 ---\n");

    agentos_observability_config_t config = {
        .enable_metrics = 1, .enable_tracing = 1, .enable_health_check = 1
    };
    agentos_observability_init(&config);

    agentos_error_t err = agentos_health_check_register("cpu_ok", mock_health_pass, NULL);
    TEST_ASSERT_EQ(err, AGENTOS_SUCCESS, "注册健康检查 pass 回调成功");

    err = agentos_health_check_register("disk_fail", mock_health_fail, NULL);
    TEST_ASSERT_EQ(err, AGENTOS_SUCCESS, "注册健康检查 fail 回调成功");

    err = agentos_health_check_register("mem_warn", mock_health_warn, NULL);
    TEST_ASSERT_EQ(err, AGENTOS_SUCCESS, "注册健康检查 warn 回调成功");

    agentos_health_status_t status = agentos_health_check_run(5000);
    TEST_ASSERT_EQ(status, AGENTOS_HEALTH_FAIL, "最差状态为 FAIL（有fail回调）");

    agentos_observability_shutdown();
}

static void test_metric_counter(void)
{
    printf("\n--- 指标计数器 ---\n");

    agentos_observability_config_t config = {
        .enable_metrics = 1, .enable_tracing = 0, .enable_health_check = 0
    };
    agentos_observability_init(&config);

    agentos_error_t err = agentos_metric_counter_create("requests_total", "method=GET");
    TEST_ASSERT_EQ(err, AGENTOS_SUCCESS, "创建计数器成功");

    err = agentos_metric_counter_inc("requests_total", "method=GET", 1.0);
    TEST_ASSERT_EQ(err, AGENTOS_SUCCESS, "递增计数器成功");

    err = agentos_metric_counter_inc("requests_total", "method=GET", 9.0);
    TEST_ASSERT_EQ(err, AGENTOS_SUCCESS, "再次递增成功");

    agentos_metric_sample_t sample = {
        .name = "requests_total",
        .labels = "method=GET",
        .type = AGENTOS_METRIC_COUNTER_E,
        .value = 42.0,
        .timestamp_ns = agentos_time_monotonic_ns()
    };
    err = agentos_metric_record(&sample);
    TEST_ASSERT_EQ(err, AGENTOS_SUCCESS, "记录指标样本成功");

    char buf[4096];
    int written = agentos_metrics_export_prometheus(buf, sizeof(buf));
    TEST_ASSERT(written > 0, "Prometheus导出产生输出");
    TEST_ASSERT(strstr(buf, "requests_total") != NULL, "导出包含指标名称");

    agentos_observability_shutdown();
}

static void test_metric_gauge(void)
{
    printf("\n--- 指标仪表盘 ---\n");

    agentos_observability_config_t config = {
        .enable_metrics = 1, .enable_tracing = 0, .enable_health_check = 0
    };
    agentos_observability_init(&config);

    int gauge_ret = agentos_metric_gauge_create("temperature", "room=server", 25.0);
    TEST_ASSERT_EQ(gauge_ret, 0, "创建仪表指标成功");

    int gauge_set_ret = agentos_metric_gauge_set("temperature", "room=server", 36.5);
    TEST_ASSERT_EQ(gauge_set_ret, 0, "设置仪表值成功");

    gauge_set_ret = agentos_metric_gauge_set("temperature", "room=server", 38.2);
    TEST_ASSERT_EQ(gauge_set_ret, 0, "更新仪表值成功");

    agentos_observability_shutdown();
}

static void test_trace_span(void)
{
    printf("\n--- 分布式追踪span ---\n");

    agentos_observability_config_t config = {
        .enable_metrics = 0, .enable_tracing = 1, .enable_health_check = 0
    };
    agentos_observability_init(&config);

    agentos_trace_context_t ctx;

    agentos_error_t err = agentos_trace_span_start(&ctx, "test_service", "test_operation");
    TEST_ASSERT_EQ(err, AGENTOS_SUCCESS, "启动trace span成功");
    TEST_ASSERT(ctx.trace_id[0] != '\0', "trace_id已生成");
    TEST_ASSERT(ctx.span_id[0] != '\0', "span_id已生成");
    TEST_ASSERT(ctx.start_ns > 0, "start_ns已设置");
    TEST_ASSERT(strcmp(ctx.service_name, "test_service") == 0, "service_name正确");
    TEST_ASSERT(strcmp(ctx.operation_name, "test_operation") == 0, "operation_name正确");

    agentos_time_sleep_ms(10);

    err = agentos_trace_span_end(&ctx, 0);
    TEST_ASSERT_EQ(err, AGENTOS_SUCCESS, "结束trace span成功");
    TEST_ASSERT(ctx.end_ns > ctx.start_ns, "end_ns > start_ns");
    TEST_ASSERT_EQ(ctx.error_code, 0, "error_code正确");

    err = agentos_trace_set_tag(&ctx, "key", "value");
    TEST_ASSERT_EQ(err, AGENTOS_SUCCESS, "设置tag成功");

    err = agentos_trace_log(&ctx, "test log message");
    TEST_ASSERT_EQ(err, AGENTOS_SUCCESS, "记录log成功");

    agentos_observability_shutdown();
}

static void test_performance_metrics(void)
{
    printf("\n--- 性能指标采集 ---\n");

    double cpu = -1, mem = -1;
    int threads = -1;

    agentos_error_t err = agentos_performance_get_metrics(&cpu, &mem, &threads);
    TEST_ASSERT_EQ(err, AGENTOS_SUCCESS, "获取性能指标成功");
    TEST_ASSERT(cpu >= 0.0, "CPU使用率 >= 0");
    TEST_ASSERT(mem >= 0.0, "内存使用率 >= 0");
    TEST_ASSERT(threads > 0, "线程数 > 0");
}

static void test_health_export(void)
{
    printf("\n--- 健康状态导出 ---\n");

    agentos_observability_config_t config = {
        .enable_metrics = 0, .enable_tracing = 0, .enable_health_check = 1
    };
    agentos_observability_init(&config);

    agentos_health_check_register("check_a", mock_health_pass, NULL);

    char buf[2048];
    int written = agentos_health_export_status(buf, sizeof(buf));
    TEST_ASSERT(written > 0, "健康状态JSON导出产生输出");
    TEST_ASSERT(strstr(buf, "\"status\"") != NULL, "包含status字段");
    TEST_ASSERT(strstr(buf, "check_a") != NULL, "包含注册的检查项");

    agentos_observability_shutdown();
}

/* ==================== main 入口 ==================== */

int main(void)
{
    printf("========================================\n");
    printf("  AgentOS corekern 模块集成测试套件\n");
    printf("========================================\n");

    /* 1. 系统生命周期 */
    test_agentos_init_cleanup();
    test_double_init();

    /* 2. 内存管理 */
    test_mem_alloc_free();
    test_mem_zero_size();
    test_mem_null_free();
    test_mem_realloc();
    test_mem_realloc_null();
    test_mem_aligned_alloc();
    test_mem_stats();

    /* 3. 内存池 */
    test_pool_create_destroy();
    test_pool_alloc_free();
    test_pool_null_safety();

    /* 4. 任务系统 */
    test_task_create_join();
    test_task_cleanup_full();

    /* 5. 时间系统 */
    test_time_monotonic();
    test_time_realtime();
    test_time_sleep();
    test_event_signal_wait();
    test_eventloop_lifecycle();

    /* 6. IPC子系统 */
    test_ipc_init_cleanup();
    test_ipc_channel_create_close();

    /* 7. 错误码 */
    test_error_codes();

    /* 8. 可观测性 */
    test_observability_init_shutdown();
    test_health_checks();
    test_metric_counter();
    test_metric_gauge();
    test_trace_span();
    test_performance_metrics();
    test_health_export();

    /* 结果汇总 */
    printf("\n========================================\n");
    printf("  测试结果汇总\n");
    printf("========================================\n");
    printf("  总计:   %d\n", g_tests_run);
    printf("  通过:   %d ✅\n", g_tests_passed);
    printf("  失败:   %d ❌\n", g_tests_failed);
    printf("  通过率: %.1f%%\n",
           g_tests_run > 0 ? (double)g_tests_passed / g_tests_run * 100.0 : 0.0);
    printf("========================================\n");

    return g_tests_failed > 0 ? 1 : 0;
}
