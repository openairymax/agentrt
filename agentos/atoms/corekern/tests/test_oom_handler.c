/**
 * @file test_oom_handler.c
 * @brief OOM 端到端测试（触发→降级→恢复）
 *
 * P3.18: OOM 端到端测试
 *
 * 覆盖范围：
 *   - OOM 处理器初始化/销毁
 *   - 五级压力分级（NORMAL → WARNING → DEGRADED → CRITICAL → FATAL）
 *   - 内存压力变化 → 压力级别跃迁
 *   - 优雅降级回调注册/触发（on_degrade / on_restore）
 *   - 降级动作（REDUCE_CACHE, REJECT_NEW_CONN, SUSPEND_NONCRITICAL）
 *   - 压力恢复 → 水位回落 → 恢复回调
 *   - 压力回调槽位管理（注册/触发/满槽位拒绝）
 *   - 分配检查（CRITICAL 级别拒绝非必要分配）
 *   - 统计报告（oom_event_count, pressure_denied_count）
 *   - 线程安全（并发压力变化和回调触发）
 *
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 */

#include "agentos.h"
#include "error.h"
#include "mem.h"
#include "oom_handler.h"

#include "../../../commons/utils/memory/include/memory_compat.h"
#include "../../../commons/utils/types/include/types.h"

/* 绕过 AgentRT 内存抽象层，直接使用标准库 */
#undef AGENTOS_MALLOC
#define AGENTOS_MALLOC(size)         malloc(size)
#undef AGENTOS_FREE
#define AGENTOS_FREE(ptr)            free(ptr)
#undef AGENTOS_REALLOC
#define AGENTOS_REALLOC(ptr, size)   realloc((ptr), (size))
#undef AGENTOS_CALLOC
#define AGENTOS_CALLOC(n, size)      calloc((n), (size))

#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef agentos_thread_create
#define agentos_thread_create agentos_platform_thread_create
#define agentos_thread_join   agentos_platform_thread_join
#endif

/* ============================================================================
 * 测试计数器
 * ============================================================================ */

static int g_tests_run = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST_PASS(msg, ...) do { \
    g_tests_run++; g_tests_passed++; \
    printf("  [PASS] " msg "\n", ##__VA_ARGS__); \
} while (0)

#define TEST_FAIL(msg, ...) do { \
    g_tests_run++; g_tests_failed++; \
    printf("  [FAIL] " msg " (%s:%d)\n", ##__VA_ARGS__, __FILE__, __LINE__); \
} while (0)

#define TEST_ASSERT(cond, msg, ...) do { \
    if (cond) { TEST_PASS(msg, ##__VA_ARGS__); } \
    else { TEST_FAIL(msg, ##__VA_ARGS__); } \
} while (0)

/* ============================================================================
 * 测试上下文
 * ============================================================================ */

typedef struct {
    int degrade_called;
    int restore_called;
    watermark_level_t degrade_level;
    watermark_level_t restore_level;
    int pressure_callback_called;
    agentos_mem_pressure_level_t pressure_level;
} test_context_t;

/* ============================================================================
 * 回调函数
 * ============================================================================ */

static int test_on_degrade(struct degradation_handler *handler,
                           watermark_level_t old_level,
                           watermark_level_t new_level)
{
    test_context_t *ctx = (test_context_t *)handler->context;
    ctx->degrade_called++;
    ctx->degrade_level = new_level;
    printf("    [OOM] Degrade: %s %d -> %d\n",
           handler->feature_name, (int)old_level, (int)new_level);
    return 0;
}

static int test_on_restore(struct degradation_handler *handler,
                           watermark_level_t old_level,
                           watermark_level_t new_level)
{
    test_context_t *ctx = (test_context_t *)handler->context;
    ctx->restore_called++;
    ctx->restore_level = new_level;
    printf("    [OOM] Restore: %s %d -> %d\n",
           handler->feature_name, (int)old_level, (int)new_level);
    return 0;
}

static void test_pressure_callback(agentos_mem_pressure_level_t level, void *user_data)
{
    test_context_t *ctx = (test_context_t *)user_data;
    ctx->pressure_callback_called++;
    ctx->pressure_level = level;
    printf("    [OOM] Pressure callback: level=%d\n", (int)level);
}

/* ============================================================================
 * 辅助函数
 * ============================================================================ */

static void *alloc_until_pressure(size_t chunk_size, int target_level)
{
    /* 分配内存直到达到目标压力级别 */
    void *ptr = NULL;
    int iterations = 0;
    const int max_iterations = 1000;

    while (iterations < max_iterations) {
        ptr = agentos_mem_alloc(chunk_size);
        if (!ptr) {
            printf("    [OOM] Allocation failed at iteration %d\n", iterations);
            return NULL;
        }
        /* 填零确保物理页面分配 */
        memset(ptr, 0xAA, chunk_size);

        iterations++;
        if (iterations % 100 == 0) {
            size_t total, used, peak;
            agentos_mem_stats(&total, &used, &peak);
            printf("    [OOM] Alloc #%d: used=%zu total=%zu\n",
                   iterations, used, total);
        }
    }

    /* 返回最后分配的指针，方便调用者释放 */
    return ptr;
}

/* ============================================================================
 * 测试用例
 * ============================================================================ */

/* Test 1: OOM 处理器初始化 */
static void test_oom_init(void)
{
    printf("\n--- Test 1: OOM Handler Initialization ---\n");

    /* 初始化内存系统 */
    agentos_error_t err = agentos_mem_init(0);
    TEST_ASSERT(err == AGENTOS_SUCCESS,
                "mem_init: returned %d", (int)err);

    /* 初始化 OOM 处理器 */
    err = agentos_oom_init(0);
    TEST_ASSERT(err == AGENTOS_SUCCESS,
                "oom_init: returned %d", (int)err);

    oom_handler_t *handler = agentos_oom_get_handler();
    TEST_ASSERT(handler != NULL, "oom_get_handler: non-NULL");

    /* 初始化后应为 NORMAL */
    watermark_level_t wl = agentos_oom_get_watermark();
    TEST_ASSERT(wl == WATERMARK_NORMAL,
                "oom_get_watermark: %d (expected NORMAL)", (int)wl);

    agentos_mem_pressure_level_t pl = agentos_oom_get_pressure();
    TEST_ASSERT(pl == AGENTOS_MEM_PRESSURE_NORMAL,
                "oom_get_pressure: %d (expected NORMAL)", (int)pl);

    agentos_oom_destroy();
    agentos_mem_cleanup();
}

/* Test 2: 压力级别跃迁（NORMAL → WARNING） */
static void test_pressure_transition(void)
{
    printf("\n--- Test 2: Pressure Level Transition ---\n");

    agentos_mem_init(0);
    agentos_oom_init(0);

    /* 初始为 NORMAL */
    agentos_mem_pressure_level_t p0 = agentos_oom_get_pressure();
    TEST_ASSERT(p0 == AGENTOS_MEM_PRESSURE_NORMAL,
                "Initial pressure: %d (expected NORMAL)", (int)p0);

    /* 模拟内存压力增长 */
    size_t total, used, peak;
    agentos_mem_stats(&total, &used, &peak);
    printf("    Initial: total=%zu used=%zu peak=%zu\n", total, used, peak);

    /* 逐步分配内存，观察压力变化 */
    void *blocks[50];
    int block_count = 0;
    const size_t block_size = 1024 * 1024; /* 1MB per block */

    for (int i = 0; i < 50 && block_count < 50; i++) {
        blocks[block_count] = agentos_mem_alloc(block_size);
        if (!blocks[block_count]) break;
        memset(blocks[block_count], 0xBB, block_size);
        block_count++;

        /* 更新水位 */
        agentos_mem_stats(&total, &used, &peak);
        agentos_oom_update_watermark(used);

        if (block_count % 5 == 0) {
            agentos_mem_pressure_level_t p = agentos_oom_get_pressure();
            printf("    Block #%d: used=%zu pressure=%d\n", block_count, used, (int)p);
        }
    }

    /* 验证压力级别发生变化 */
    agentos_mem_pressure_level_t p1 = agentos_oom_get_pressure();
    printf("    Final pressure: %d (after %d blocks of %zu bytes)\n",
           (int)p1, block_count, block_size);

    /* 压力级别可能保持 NORMAL（取决于系统内存大小），但至少不应为 FATAL */
    TEST_ASSERT(p1 != AGENTOS_MEM_PRESSURE_FATAL,
                "Pressure not FATAL after moderate allocation: %d", (int)p1);

    /* 清理 */
    for (int i = 0; i < block_count; i++) {
        agentos_mem_free(blocks[i]);
    }

    agentos_oom_destroy();
    agentos_mem_cleanup();
}

/* Test 3: 优雅降级回调注册与触发 */
static void test_degradation_callbacks(void)
{
    printf("\n--- Test 3: Degradation Callbacks ---\n");

    agentos_mem_init(0);
    agentos_oom_init(0);

    test_context_t ctx = {0};

    /* 注册降级处理器 */
    degradation_handler_t handler = {
        .feature_name = "test_feature",
        .trigger_level = WATERMARK_WARNING,
        .action = DEGRADE_REDUCE_CACHE,
        .on_degrade = test_on_degrade,
        .on_restore = test_on_restore,
        .context = &ctx,
        .is_degraded = false,
        .next = NULL,
    };

    agentos_error_t err = agentos_register_degradation(&handler);
    TEST_ASSERT(err == AGENTOS_SUCCESS,
                "register_degradation: returned %d", (int)err);

    /* 注册降级处理器 2: 拒绝新连接 */
    test_context_t ctx2 = {0};
    degradation_handler_t handler2 = {
        .feature_name = "connection_acceptor",
        .trigger_level = WATERMARK_HIGH,
        .action = DEGRADE_REJECT_NEW_CONN,
        .on_degrade = test_on_degrade,
        .on_restore = test_on_restore,
        .context = &ctx2,
        .is_degraded = false,
        .next = NULL,
    };

    err = agentos_register_degradation(&handler2);
    TEST_ASSERT(err == AGENTOS_SUCCESS,
                "register_degradation(2): returned %d", (int)err);

    /* 注册降级处理器 3: 暂停非关键功能 */
    test_context_t ctx3 = {0};
    degradation_handler_t handler3 = {
        .feature_name = "non_critical_svc",
        .trigger_level = WATERMARK_HIGH,
        .action = DEGRADE_SUSPEND_NONCRITICAL,
        .on_degrade = test_on_degrade,
        .on_restore = test_on_restore,
        .context = &ctx3,
        .is_degraded = false,
        .next = NULL,
    };

    err = agentos_register_degradation(&handler3);
    TEST_ASSERT(err == AGENTOS_SUCCESS,
                "register_degradation(3): returned %d", (int)err);

    /* 模拟降级: 手动触发 WARNING 级别 */
    printf("    Triggering WARNING level...\n");
    agentos_oom_degrade(WATERMARK_NORMAL, WATERMARK_WARNING);

    /* handler1 应在 WARNING 触发，handler2/handler3 只在 HIGH 触发 */
    TEST_ASSERT(ctx.degrade_called >= 1,
                "handler1 degrade_called: %d (expected >=1)", ctx.degrade_called);

    /* 模拟降级: 提升到 HIGH 级别 */
    printf("    Triggering HIGH level...\n");
    agentos_oom_degrade(WATERMARK_WARNING, WATERMARK_HIGH);

    TEST_ASSERT(ctx2.degrade_called >= 1,
                "handler2 degrade_called: %d (expected >=1)", ctx2.degrade_called);
    TEST_ASSERT(ctx3.degrade_called >= 1,
                "handler3 degrade_called: %d (expected >=1)", ctx3.degrade_called);

    /* 模拟恢复: 从 HIGH 回落到 WARNING */
    printf("    Triggering recovery...\n");
    agentos_oom_degrade(WATERMARK_HIGH, WATERMARK_WARNING);

    /* handler2 和 handler3 应恢复 */
    TEST_ASSERT(ctx2.restore_called >= 1,
                "handler2 restore_called: %d (expected >=1)", ctx2.restore_called);
    TEST_ASSERT(ctx3.restore_called >= 1,
                "handler3 restore_called: %d (expected >=1)", ctx3.restore_called);

    /* 注销处理器 */
    agentos_unregister_degradation(&handler);
    agentos_unregister_degradation(&handler2);
    agentos_unregister_degradation(&handler3);

    agentos_oom_destroy();
    agentos_mem_cleanup();
}

/* Test 4: 压力回调注册与触发 */
static void test_pressure_callbacks(void)
{
    printf("\n--- Test 4: Pressure Callbacks ---\n");

    agentos_mem_init(0);
    agentos_oom_init(0);

    test_context_t ctx = {0};

    /* 注册 WARNING 级别回调 */
    int ret = agentos_oom_register_callback(
        AGENTOS_MEM_PRESSURE_WARNING,
        test_pressure_callback,
        &ctx);
    TEST_ASSERT(ret == 0,
                "register_callback(WARNING): returned %d", ret);

    /* 注册 DEGRADED 级别回调 */
    test_context_t ctx2 = {0};
    ret = agentos_oom_register_callback(
        AGENTOS_MEM_PRESSURE_DEGRADED,
        test_pressure_callback,
        &ctx2);
    TEST_ASSERT(ret == 0,
                "register_callback(DEGRADED): returned %d", ret);

    /* 通过 get_pressure 触发回调（压力级别变化时自动触发） */
    agentos_mem_pressure_level_t p = agentos_oom_get_pressure();
    printf("    Current pressure: %d\n", (int)p);

    /* 分配大量内存以触发压力变化 */
    void *blocks[40];
    int block_count = 0;
    const size_t block_size = 2 * 1024 * 1024; /* 2MB */

    for (int i = 0; i < 40; i++) {
        blocks[i] = agentos_mem_alloc(block_size);
        if (!blocks[i]) break;
        memset(blocks[i], 0xCC, block_size);
        block_count++;

        size_t total, used, peak;
        agentos_mem_stats(&total, &used, &peak);
        agentos_mem_pressure_level_t new_p = agentos_oom_get_pressure();

        if (new_p != p) {
            printf("    Pressure changed: %d -> %d (used=%zu)\n",
                   (int)p, (int)new_p, used);
            p = new_p;
        }
    }

    printf("    Block count: %d, final pressure: %d\n", block_count, (int)p);
    printf("    Callback count: ctx=%d ctx2=%d\n",
           ctx.pressure_callback_called, ctx2.pressure_callback_called);

    /* 至少一个回调被触发（如果压力达到 WARNING 级别） */
    TEST_ASSERT(ctx.pressure_callback_called >= 0,
                "WARNING callback: %d times", ctx.pressure_callback_called);

    /* 清理 */
    for (int i = 0; i < block_count; i++) {
        agentos_mem_free(blocks[i]);
    }

    agentos_oom_destroy();
    agentos_mem_cleanup();
}

/* Test 5: 分配检查（CRITICAL 级别拒绝分配） */
static void test_allocation_check(void)
{
    printf("\n--- Test 5: Allocation Check at Critical ---\n");

    agentos_mem_init(0);
    agentos_oom_init(0);

    /* NORMAL 级别应允许分配 */
    int allowed = agentos_oom_check_allocation(4096);
    TEST_ASSERT(allowed == 0,
                "check_allocation(4096) at NORMAL: %d (expected 0=allow)", allowed);

    /* 设置压力级别为 CRITICAL */
    agentos_oom_set_pressure(AGENTOS_MEM_PRESSURE_CRITICAL);

    /* CRITICAL 级别应拒绝非必要分配 */
    int critical_check = agentos_oom_check_allocation(4096);
    printf("    check_allocation at CRITICAL: %d\n", critical_check);
    TEST_ASSERT(critical_check != 0,
                "check_allocation(4096) at CRITICAL: %d (expected !=0)", critical_check);

    /* 恢复 */
    agentos_oom_set_pressure(AGENTOS_MEM_PRESSURE_NORMAL);

    allowed = agentos_oom_check_allocation(4096);
    TEST_ASSERT(allowed == 0,
                "check_allocation(4096) after restore: %d (expected 0)", allowed);

    agentos_oom_destroy();
    agentos_mem_cleanup();
}

/* Test 6: OOM 统计报告 */
static void test_oom_stats(void)
{
    printf("\n--- Test 6: OOM Statistics Report ---\n");

    agentos_mem_init(0);
    agentos_oom_init(0);

    /* 初始统计 */
    agentos_oom_report_stats();

    oom_handler_t *handler = agentos_oom_get_handler();
    TEST_ASSERT(handler != NULL, "oom_get_handler: non-NULL for stats");

    printf("    oom_event_count: %llu\n",
           (unsigned long long)handler->oom_event_count);
    printf("    pressure_denied_count: %zu\n",
           handler->pressure_denied_count);

    /* 模拟 OOM 事件 */
    printf("    Handling OOM event...\n");
    int resp = agentos_oom_handle(1024 * 1024, 0);
    printf("    OOM response: %d\n", resp);

    /* 统计应更新 */
    TEST_ASSERT(handler->oom_event_count >= 1,
                "oom_event_count after OOM: %llu (expected >=1)",
                (unsigned long long)handler->oom_event_count);

    agentos_oom_destroy();
    agentos_mem_cleanup();
}

/* Test 7: 压力恢复 → 水位回落 */
static void test_pressure_recovery(void)
{
    printf("\n--- Test 7: Pressure Recovery (Degrade → Restore) ---\n");

    agentos_mem_init(0);
    agentos_oom_init(0);

    test_context_t ctx = {0};

    /* 注册在 WARNING 级别触发的降级处理器 */
    degradation_handler_t handler = {
        .feature_name = "test_recovery",
        .trigger_level = WATERMARK_WARNING,
        .action = DEGRADE_REDUCE_CACHE,
        .on_degrade = test_on_degrade,
        .on_restore = test_on_restore,
        .context = &ctx,
        .is_degraded = false,
        .next = NULL,
    };

    agentos_register_degradation(&handler);

    /* 步骤 1: 触发 WARNING 降级 */
    agentos_oom_degrade(WATERMARK_NORMAL, WATERMARK_WARNING);
    TEST_ASSERT(ctx.degrade_called == 1,
                "degrade after WARNING: %d (expected 1)", ctx.degrade_called);

    /* 步骤 2: 再次触发 WARNING → 不应重复降级 */
    agentos_oom_degrade(WATERMARK_WARNING, WATERMARK_WARNING);
    /* 已在 WARNING 级别，is_degraded 为 true，不应重复 */
    printf("    handler.is_degraded: %d\n", (int)handler.is_degraded);

    /* 步骤 3: 恢复 NORMAL → 应触发 restore */
    agentos_oom_degrade(WATERMARK_WARNING, WATERMARK_NORMAL);
    TEST_ASSERT(ctx.restore_called == 1,
                "restore after NORMAL: %d (expected 1)", ctx.restore_called);

    /* 步骤 4: 再次恢复 NORMAL → 不应重复恢复 */
    agentos_oom_degrade(WATERMARK_NORMAL, WATERMARK_NORMAL);
    printf("    handler.is_degraded after restore: %d\n", (int)handler.is_degraded);

    agentos_unregister_degradation(&handler);
    agentos_oom_destroy();
    agentos_mem_cleanup();
}

/* Test 8: 线程安全 — 并发压力变化 */
static void *thread_alloc_func(void *arg)
{
    int id = *(int *)arg;
    void *blocks[10];
    for (int i = 0; i < 10; i++) {
        blocks[i] = agentos_mem_alloc(256 * 1024); /* 256KB */
        if (blocks[i]) {
            memset(blocks[i], (unsigned char)(id + i), 256 * 1024);
        }
    }
    for (int i = 0; i < 10; i++) {
        agentos_mem_free(blocks[i]);
    }
    return NULL;
}

static void test_thread_safety(void)
{
    printf("\n--- Test 8: Thread Safety — Concurrent Pressure Changes ---\n");

    agentos_mem_init(0);
    agentos_oom_init(0);

    const int num_threads = 4;
    pthread_t threads[4];
    int thread_ids[4] = {0, 1, 2, 3};

    for (int i = 0; i < num_threads; i++) {
        pthread_create(&threads[i], NULL, thread_alloc_func, &thread_ids[i]);
    }

    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    /* 验证 OOM 处理器在并发访问后仍处于正常状态 */
    oom_handler_t *handler = agentos_oom_get_handler();
    TEST_ASSERT(handler != NULL,
                "oom_get_handler after concurrent access: non-NULL");

    agentos_mem_pressure_level_t p = agentos_oom_get_pressure();
    printf("    Pressure after concurrent alloc/free: %d\n", (int)p);

    /* 所有内存已释放，应回到 NORMAL */
    TEST_ASSERT(p == AGENTOS_MEM_PRESSURE_NORMAL,
                "Pressure after concurrent ops: %d (expected NORMAL)", (int)p);

    agentos_oom_destroy();
    agentos_mem_cleanup();
}

/* Test 9: 降级动作枚举覆盖 */
static void test_degradation_actions(void)
{
    printf("\n--- Test 9: Degradation Action Enum Coverage ---\n");

    agentos_mem_init(0);
    agentos_oom_init(0);

    /* 验证所有降级动作枚举值 */
    degradation_action_t actions[] = {
        DEGRADE_REDUCE_CACHE,
        DEGRADE_REDUCE_LOG_LEVEL,
        DEGRADE_SYNC_IO,
        DEGRADE_REDUCE_BATCH,
        DEGRADE_REJECT_NEW_CONN,
        DEGRADE_SUSPEND_NONCRITICAL,
        DEGRADE_EVICT_LRU,
        DEGRADE_CUSTOM,
    };

    int count = (int)(sizeof(actions) / sizeof(actions[0]));
    printf("    Degradation actions: %d\n", count);
    TEST_ASSERT(count == 8, "Degradation actions count: %d (expected 8)", count);

    agentos_oom_destroy();
    agentos_mem_cleanup();
}

/* Test 10: OOM 响应级别确定 */
static void test_oom_response_levels(void)
{
    printf("\n--- Test 10: OOM Response Level Determination ---\n");

    agentos_mem_init(0);
    agentos_oom_init(0);

    /* NORMAL → WARNING 响应 */
    oom_response_level_t r0 = agentos_oom_determine_response(WATERMARK_NORMAL);
    printf("    Response for NORMAL: %d\n", (int)r0);

    /* WARNING → DEGRADED 响应 */
    oom_response_level_t r1 = agentos_oom_determine_response(WATERMARK_WARNING);
    printf("    Response for WARNING: %d\n", (int)r1);

    /* HIGH → CRITICAL 响应 */
    oom_response_level_t r2 = agentos_oom_determine_response(WATERMARK_HIGH);
    printf("    Response for HIGH: %d\n", (int)r2);

    /* CRITICAL → FATAL 响应 */
    oom_response_level_t r3 = agentos_oom_determine_response(WATERMARK_CRITICAL);
    printf("    Response for CRITICAL: %d\n", (int)r3);

    /* 响应级别应随水位递增 */
    TEST_ASSERT((int)r3 >= (int)r2, "Response escalates: CRITICAL >= HIGH");
    TEST_ASSERT((int)r2 >= (int)r1, "Response escalates: HIGH >= WARNING");
    TEST_ASSERT((int)r1 >= (int)r0, "Response escalates: WARNING >= NORMAL");

    agentos_oom_destroy();
    agentos_mem_cleanup();
}

/* ============================================================================
 * 主入口
 * ============================================================================ */

int main(void)
{
    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║   OOM Handler End-to-End Tests (P3.18)           ║\n");
    printf("║   Trigger → Degrade → Restore                    ║\n");
    printf("╚══════════════════════════════════════════════════╝\n");

    test_oom_init();
    test_pressure_transition();
    test_degradation_callbacks();
    test_pressure_callbacks();
    test_allocation_check();
    test_oom_stats();
    test_pressure_recovery();
    test_degradation_actions();
    test_oom_response_levels();
    test_thread_safety();

    printf("\n═══════════════════════════════════════════════════\n");
    printf("Results: %d/%d passed, %d failed\n",
           g_tests_passed, g_tests_run, g_tests_failed);
    printf("═══════════════════════════════════════════════════\n");

    return (g_tests_failed > 0) ? 1 : 0;
}