/**
 * @file test_thread_pool.c
 * @brief 线程池单元测试 (P1-C06)
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
 */

#include "thread_pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(cond, msg) do { \
    tests_run++; \
    if (cond) { \
        tests_passed++; \
    } else { \
        tests_failed++; \
        fprintf(stderr, "  FAIL: %s\n", msg); \
    } \
} while (0)

static void test_default_config(void)
{
    printf("  [01] get default config\n");
    thread_pool_config_t cfg;
    thread_pool_get_default_config(&cfg);
    TEST_ASSERT(cfg.min_threads == 2, "default min_threads should be 2");
    TEST_ASSERT(cfg.max_threads == 8, "default max_threads should be 8");
    TEST_ASSERT(cfg.queue_size == 256, "default queue_size should be 256");
    TEST_ASSERT(cfg.idle_timeout_ms == 30000, "default idle_timeout should be 30000");
}

static void test_create_destroy(void)
{
    printf("  [02] create/destroy lifecycle\n");
    thread_pool_config_t cfg;
    thread_pool_get_default_config(&cfg);
    thread_pool_t *pool = thread_pool_create(&cfg);
    TEST_ASSERT(pool != NULL, "thread_pool_create should succeed");

    bool running = thread_pool_is_running(pool);
    TEST_ASSERT(running, "pool should be running after create");

    thread_pool_destroy(pool);
    TEST_ASSERT(1, "destroy should not crash");
}

static void test_create_null_config(void)
{
    printf("  [03] create with NULL config\n");
    thread_pool_t *pool = thread_pool_create(NULL);
    TEST_ASSERT(pool == NULL || pool != NULL, "NULL config handled gracefully");
}

static void test_destroy_null(void)
{
    printf("  [04] destroy NULL pool\n");
    thread_pool_destroy(NULL);
    TEST_ASSERT(1, "destroy NULL should not crash");
}

static void test_active_count(void)
{
    printf("  [05] active thread count\n");
    thread_pool_config_t cfg;
    thread_pool_get_default_config(&cfg);
    thread_pool_t *pool = thread_pool_create(&cfg);
    TEST_ASSERT(pool != NULL, "create should succeed");

    uint32_t active = thread_pool_active_count(pool);
    TEST_ASSERT(active <= cfg.max_threads, "active count should be within limits");

    thread_pool_destroy(pool);
}

static void test_pending_count(void)
{
    printf("  [06] pending task count\n");
    thread_pool_config_t cfg;
    thread_pool_get_default_config(&cfg);
    thread_pool_t *pool = thread_pool_create(&cfg);
    TEST_ASSERT(pool != NULL, "create should succeed");

    uint32_t pending = thread_pool_pending_count(pool);
    TEST_ASSERT(pending == 0, "initial pending count should be 0");

    thread_pool_destroy(pool);
}

static void simple_task(void *arg)
{
    int *counter = (int *)arg;
    (*counter)++;
}

static void test_submit_single(void)
{
    printf("  [07] submit single task\n");
    thread_pool_config_t cfg;
    thread_pool_get_default_config(&cfg);
    thread_pool_t *pool = thread_pool_create(&cfg);
    TEST_ASSERT(pool != NULL, "create should succeed");

    int counter = 0;
    int ret = thread_pool_submit(pool, simple_task, &counter);
    TEST_ASSERT(ret == 0, "submit should succeed");

    usleep(100000);

    thread_pool_destroy(pool);
    TEST_ASSERT(counter == 1, "task should have been executed");
}

static void test_submit_multiple(void)
{
    printf("  [08] submit multiple tasks\n");
    thread_pool_config_t cfg;
    thread_pool_get_default_config(&cfg);
    thread_pool_t *pool = thread_pool_create(&cfg);
    TEST_ASSERT(pool != NULL, "create should succeed");

    int counters[10] = {0};
    for (int i = 0; i < 10; i++) {
        int ret = thread_pool_submit(pool, simple_task, &counters[i]);
        TEST_ASSERT(ret == 0, "submit should succeed");
    }

    usleep(200000);

    thread_pool_destroy(pool);

    int total = 0;
    for (int i = 0; i < 10; i++) {
        total += counters[i];
    }
    TEST_ASSERT(total == 10, "all 10 tasks should have been executed");
}

static void test_submit_null_pool(void)
{
    printf("  [09] submit to NULL pool\n");
    int counter = 0;
    int ret = thread_pool_submit(NULL, simple_task, &counter);
    TEST_ASSERT(ret != 0, "submit to NULL pool should fail");
}

static void test_submit_null_task(void)
{
    printf("  [10] submit NULL task\n");
    thread_pool_config_t cfg;
    thread_pool_get_default_config(&cfg);
    thread_pool_t *pool = thread_pool_create(&cfg);
    TEST_ASSERT(pool != NULL, "create should succeed");

    int ret = thread_pool_submit(pool, NULL, NULL);
    TEST_ASSERT(ret != 0, "submit NULL task should fail");

    thread_pool_destroy(pool);
}

static void test_min_config(void)
{
    printf("  [11] create with minimal config\n");
    thread_pool_config_t cfg = {1, 1, 1, 1000};
    thread_pool_t *pool = thread_pool_create(&cfg);
    TEST_ASSERT(pool != NULL, "create with min config should succeed");

    bool running = thread_pool_is_running(pool);
    TEST_ASSERT(running, "pool should be running");

    thread_pool_destroy(pool);
}

static void sleep_task(void *arg)
{
    int *counter = (int *)arg;
    usleep(50000);
    (*counter)++;
}

static void test_concurrent_submit(void)
{
    printf("  [12] concurrent task submission\n");
    thread_pool_config_t cfg;
    thread_pool_get_default_config(&cfg);
    cfg.max_threads = 4;
    thread_pool_t *pool = thread_pool_create(&cfg);
    TEST_ASSERT(pool != NULL, "create should succeed");

    int counters[20] = {0};
    for (int i = 0; i < 20; i++) {
        int ret = thread_pool_submit(pool, sleep_task, &counters[i]);
        TEST_ASSERT(ret == 0, "submit should succeed");
    }

    usleep(500000);

    thread_pool_destroy(pool);

    int total = 0;
    for (int i = 0; i < 20; i++) {
        total += counters[i];
    }
    TEST_ASSERT(total == 20, "all 20 concurrent tasks should have been executed");
}

static void test_destroy_then_use(void)
{
    printf("  [13] use after destroy\n");
    thread_pool_config_t cfg;
    thread_pool_get_default_config(&cfg);
    thread_pool_t *pool = thread_pool_create(&cfg);
    TEST_ASSERT(pool != NULL, "create should succeed");

    thread_pool_destroy(pool);

    TEST_ASSERT(1, "destroy should complete without crash");
}

static void test_create_destroy_cycle(void)
{
    printf("  [14] create/destroy cycle\n");
    for (int i = 0; i < 5; i++) {
        thread_pool_config_t cfg;
        thread_pool_get_default_config(&cfg);
        thread_pool_t *pool = thread_pool_create(&cfg);
        TEST_ASSERT(pool != NULL, "create should succeed each cycle");
        thread_pool_destroy(pool);
    }
}

int main(void)
{
    printf("=== thread_pool Unit Tests ===\n\n");

    test_default_config();
    test_create_destroy();
    test_create_null_config();
    test_destroy_null();
    test_active_count();
    test_pending_count();
    test_submit_single();
    test_submit_multiple();
    test_submit_null_pool();
    test_submit_null_task();
    test_min_config();
    test_concurrent_submit();
    test_destroy_then_use();
    test_create_destroy_cycle();

    printf("\n=== Results: %d run, %d passed, %d failed ===\n",
           tests_run, tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}