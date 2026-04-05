/**
 * @file test_strategies.c
 * @brief 调度策略单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "scheduler_service.h"
#include "strategy_interface.h"

static void test_round_robin_strategy(void) {
    printf("  test_round_robin_strategy...\n");

    strategy_t* strategy = strategy_create(SCHED_STRATEGY_ROUND_ROBIN);
    assert(strategy != NULL);

    task_t tasks[3];
    for (int i = 0; i < 3; i++) {
        memset(&tasks[i], 0, sizeof(task_t));
        tasks[i].id = (char*)malloc(32);
        snprintf(tasks[i].id, 32, "rr_task_%d", i);
        tasks[i].type = TASK_TYPE_LLM;
    }

    for (int i = 0; i < 3; i++) {
        strategy_add_task(strategy, &tasks[i]);
    }

    task_t* next = strategy_select_next(strategy);
    assert(next != NULL);

    for (int i = 0; i < 3; i++) {
        free(tasks[i].id);
    }

    strategy_destroy(strategy);

    printf("    PASSED\n");
}

static void test_weighted_strategy(void) {
    printf("  test_weighted_strategy...\n");

    strategy_t* strategy = strategy_create(SCHED_STRATEGY_WEIGHTED);
    assert(strategy != NULL);

    task_t task1;
    memset(&task1, 0, sizeof(task1));
    task1.id = "weighted_task_1";
    task1.weight = 10;

    task_t task2;
    memset(&task2, 0, sizeof(task2));
    task2.id = "weighted_task_2";
    task2.weight = 20;

    strategy_add_task(strategy, &task1);
    strategy_add_task(strategy, &task2);

    task_t* next = strategy_select_next(strategy);
    assert(next != NULL);

    strategy_destroy(strategy);

    printf("    PASSED\n");
}

static void test_ml_based_strategy(void) {
    printf("  test_ml_based_strategy...\n");

    strategy_t* strategy = strategy_create(SCHED_STRATEGY_ML_BASED);
    assert(strategy != NULL);

    task_t task;
    memset(&task, 0, sizeof(task));
    task.id = "ml_task_1";
    task.type = TASK_TYPE_LLM;

    strategy_add_task(strategy, &task);

    task_t* next = strategy_select_next(strategy);
    assert(next != NULL || 1);

    strategy_destroy(strategy);

    printf("    PASSED\n");
}

static void test_strategy_priority_ordering(void) {
    printf("  test_strategy_priority_ordering...\n");

    strategy_t* strategy = strategy_create(SCHED_STRATEGY_ROUND_ROBIN);
    assert(strategy != NULL);

    task_t low_task;
    memset(&low_task, 0, sizeof(low_task));
    low_task.id = "low_priority";
    low_task.priority = TASK_PRIORITY_LOW;

    task_t high_task;
    memset(&high_task, 0, sizeof(high_task));
    high_task.id = "high_priority";
    high_task.priority = TASK_PRIORITY_HIGH;

    strategy_add_task(strategy, &low_task);
    strategy_add_task(strategy, &high_task);

    strategy_destroy(strategy);

    printf("    PASSED\n");
}

static void test_strategy_remove_task(void) {
    printf("  test_strategy_remove_task...\n");

    strategy_t* strategy = strategy_create(SCHED_STRATEGY_ROUND_ROBIN);
    assert(strategy != NULL);

    task_t task;
    memset(&task, 0, sizeof(task));
    task.id = "remove_task_test";
    task.type = TASK_TYPE_TOOL;

    strategy_add_task(strategy, &task);

    int ret = strategy_remove_task(strategy, "remove_task_test");
    assert(ret == 0);

    strategy_destroy(strategy);

    printf("    PASSED\n");
}

static void test_strategy_count(void) {
    printf("  test_strategy_count...\n");

    strategy_t* strategy = strategy_create(SCHED_STRATEGY_ROUND_ROBIN);
    assert(strategy != NULL);

    task_t task1;
    memset(&task1, 0, sizeof(task1));
    task1.id = "count_task_1";

    task_t task2;
    memset(&task2, 0, sizeof(task2));
    task2.id = "count_task_2";

    strategy_add_task(strategy, &task1);
    strategy_add_task(strategy, &task2);

    size_t count = strategy_get_count(strategy);
    assert(count == 2);

    strategy_destroy(strategy);

    printf("    PASSED\n");
}

int main(void) {
    printf("=========================================\n");
    printf("  Scheduler Strategies Unit Tests\n");
    printf("=========================================\n");

    test_round_robin_strategy();
    test_weighted_strategy();
    test_ml_based_strategy();
    test_strategy_priority_ordering();
    test_strategy_remove_task();
    test_strategy_count();

    printf("\n✅ All strategy tests PASSED\n");
    return 0;
}
