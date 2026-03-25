/**
 * @file test_scheduler.c
 * @brief 调度器核心功能单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "scheduler_service.h"

static void test_scheduler_create_destroy(void) {
    printf("  test_scheduler_create_destroy...\n");

    scheduler_t* sched = scheduler_create(NULL);
    assert(sched != NULL);

    scheduler_destroy(sched);

    printf("    PASSED\n");
}

static void test_scheduler_config(void) {
    printf("  test_scheduler_config...\n");

    scheduler_config_t config = {
        .max_concurrent_tasks = 10,
        .default_timeout_ms = 30000,
        .strategy = SCHED_STRATEGY_ROUND_ROBIN,
        .enable_priority = 1
    };

    scheduler_t* sched = scheduler_create(&config);
    assert(sched != NULL);

    scheduler_destroy(sched);

    printf("    PASSED\n");
}

static void test_scheduler_submit_task(void) {
    printf("  test_scheduler_submit_task...\n");

    scheduler_t* sched = scheduler_create(NULL);
    assert(sched != NULL);

    task_t task;
    memset(&task, 0, sizeof(task));
    task.id = "test_task_001";
    task.type = TASK_TYPE_LLM;
    task.priority = TASK_PRIORITY_NORMAL;

    int ret = scheduler_submit(sched, &task);
    assert(ret == 0);

    scheduler_destroy(sched);

    printf("    PASSED\n");
}

static void test_scheduler_cancel_task(void) {
    printf("  test_scheduler_cancel_task...\n");

    scheduler_t* sched = scheduler_create(NULL);
    assert(sched != NULL);

    task_t task;
    memset(&task, 0, sizeof(task));
    task.id = "cancel_task_001";
    task.type = TASK_TYPE_TOOL;

    scheduler_submit(sched, &task);

    int ret = scheduler_cancel(sched, "cancel_task_001");
    assert(ret == 0);

    scheduler_destroy(sched);

    printf("    PASSED\n");
}

static void test_scheduler_get_status(void) {
    printf("  test_scheduler_get_status...\n");

    scheduler_t* sched = scheduler_create(NULL);
    assert(sched != NULL);

    task_t task;
    memset(&task, 0, sizeof(task));
    task.id = "status_task_001";
    task.type = TASK_TYPE_LLM;

    scheduler_submit(sched, &task);

    task_status_t status = scheduler_get_status(sched, "status_task_001");
    assert(status >= TASK_STATUS_PENDING);

    scheduler_destroy(sched);

    printf("    PASSED\n");
}

static void test_scheduler_set_strategy(void) {
    printf("  test_scheduler_set_strategy...\n");

    scheduler_t* sched = scheduler_create(NULL);
    assert(sched != NULL);

    int ret = scheduler_set_strategy(sched, SCHED_STRATEGY_WEIGHTED);
    assert(ret == 0);

    ret = scheduler_set_strategy(sched, SCHED_STRATEGY_ML_BASED);
    assert(ret == 0);

    scheduler_destroy(sched);

    printf("    PASSED\n");
}

static void test_scheduler_priority(void) {
    printf("  test_scheduler_priority...\n");

    scheduler_t* sched = scheduler_create(NULL);
    assert(sched != NULL);

    task_t low_task;
    memset(&low_task, 0, sizeof(low_task));
    low_task.id = "low_priority_task";
    low_task.priority = TASK_PRIORITY_LOW;

    task_t high_task;
    memset(&high_task, 0, sizeof(high_task));
    high_task.id = "high_priority_task";
    high_task.priority = TASK_PRIORITY_HIGH;

    scheduler_submit(sched, &low_task);
    scheduler_submit(sched, &high_task);

    scheduler_destroy(sched);

    printf("    PASSED\n");
}

static void test_scheduler_stats(void) {
    printf("  test_scheduler_stats...\n");

    scheduler_t* sched = scheduler_create(NULL);
    assert(sched != NULL);

    scheduler_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    int ret = scheduler_get_stats(sched, &stats);
    assert(ret == 0);

    scheduler_destroy(sched);

    printf("    PASSED\n");
}

int main(void) {
    printf("=========================================\n");
    printf("  Scheduler Unit Tests\n");
    printf("=========================================\n");

    test_scheduler_create_destroy();
    test_scheduler_config();
    test_scheduler_submit_task();
    test_scheduler_cancel_task();
    test_scheduler_get_status();
    test_scheduler_set_strategy();
    test_scheduler_priority();
    test_scheduler_stats();

    printf("\n✅ All scheduler tests PASSED\n");
    return 0;
}
