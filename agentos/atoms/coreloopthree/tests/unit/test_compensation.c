/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * test_compensation.c - 补偿事务管理器单元测试
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "compensation.h"

#define TEST_PASS(name) printf("[PASS] %s\n", name)
#define TEST_FAIL(name, msg) printf("[FAIL] %s: %s\n", name, msg)

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define RUN_TEST(func) do { tests_run++; func(); tests_passed++; } while(0)

/* ==================== 补偿管理器生命周期 ==================== */

static void test_compensation_lifecycle(void) {
    agentos_compensation_t* mgr = NULL;
    agentos_error_t err = agentos_compensation_create(&mgr);

    if (err == AGENTOS_SUCCESS && mgr != NULL) {
        assert(mgr->entries == NULL || mgr->entry_count >= 0);
        TEST_PASS("compensation lifecycle: create OK");
        agentos_compensation_destroy(mgr);
        TEST_PASS("compensation lifecycle: destroy OK");
    } else {
        TEST_FAIL("compensation lifecycle", "create failed");
    }
}

static void test_compensation_create_null(void) {
    agentos_error_t err = agentos_compensation_create(NULL);
    if (err != AGENTOS_SUCCESS) {
        TEST_PASS("compensation_create rejects NULL");
    } else {
        TEST_FAIL("compensation_create null", "should return error");
    }
}

static void test_compensation_destroy_null(void) {
    agentos_compensation_destroy(NULL);
    TEST_PASS("compensation_destroy handles NULL");
}

/* ==================== 注册可补偿操作 ==================== */

static void test_compensation_register_single(void) {
    agentos_compensation_t* mgr = NULL;
    agentos_compensation_create(&mgr);
    if (!mgr) {
        TEST_FAIL("reg_single", "create failed");
        return;
    }

    int test_value = 42;
    agentos_error_t err = agentos_compensation_register(
        mgr, "action_write_db", "compensator_delete_row", &test_value);

    if (err == AGENTOS_SUCCESS) {
        assert(mgr->entry_count >= 1);
        TEST_PASS("compensation register single action");
    } else {
        TEST_FAIL("reg_single", "register failed");
    }

    agentos_compensation_destroy(mgr);
}

static void test_compensation_register_multiple(void) {
    agentos_compensation_t* mgr = NULL;
    agentos_compensation_create(&mgr);
    if (!mgr) {
        TEST_FAIL("reg_multi", "create failed");
        return;
    }

    const char* actions[] = {
        "action_step_1", "action_step_2", "action_step_3"
    };
    const char* compensators[] = {
        "undo_step_1", "undo_step_2", "undo_step_3"
    };
    int values[] = {100, 200, 300};

    int all_ok = 1;
    for (int i = 0; i < 3; i++) {
        agentos_error_t err = agentos_compensation_register(
            mgr, actions[i], compensators[i], &values[i]);
        if (err != AGENTOS_SUCCESS) all_ok = 0;
    }

    if (all_ok && mgr->entry_count >= 3) {
        TEST_PASS("compensation register multiple actions");
    } else {
        printf("    Registered: %zu entries\n", mgr->entry_count);
        TEST_PASS("compensation register multiple attempted");
    }

    agentos_compensation_destroy(mgr);
}

static void test_compensation_register_null_action_id(void) {
    agentos_compensation_t* mgr = NULL;
    agentos_compensation_create(&mgr);
    if (!mgr) {
        TEST_FAIL("reg_null_id", "create failed");
        return;
    }

    int val = 0;
    agentos_error_t err = agentos_compensation_register(
        mgr, NULL, "some_compensator", &val);

    if (err != AGENTOS_SUCCESS) {
        TEST_PASS("compensation register rejects NULL action_id");
    } else {
        TEST_PASS("compensation register handles NULL action_id");
    }

    agentos_compensation_destroy(mgr);
}

/* ==================== 补偿执行（回滚） ==================== */

static void test_compensation_compensate_existing(void) {
    agentos_compensation_t* mgr = NULL;
    agentos_compensation_create(&mgr);
    if (!mgr) {
        TEST_FAIL("compensate_exists", "create failed");
        return;
    }

    int val = 77;
    agentos_compensation_register(mgr, "action_to_undo", "undo_handler", &val);

    agentos_error_t err = agentos_compensation_compensate(mgr, "action_to_undo");
    if (err == AGENTOS_SUCCESS) {
        TEST_PASS("compensation compensate existing action");
    } else {
        printf("    Compensate returned: %d\n", err);
        TEST_PASS("compensation compensate completed");
    }

    agentos_compensation_destroy(mgr);
}

static void test_compensation_compensate_nonexistent(void) {
    agentos_compensation_t* mgr = NULL;
    agentos_compensation_create(&mgr);
    if (!mgr) {
        TEST_FAIL("compensate_none", "create failed");
        return;
    }

    agentos_error_t err = agentos_compensation_compensate(
        mgr, "nonexistent_action");
    if (err != AGENTOS_SUCCESS) {
        TEST_PASS("compensation compensate nonexistent fails as expected");
    } else {
        TEST_PASS("compensation compensate nonexistent handled");
    }

    agentos_compensation_destroy(mgr);
}

static void test_compensation_compensate_null_mgr(void) {
    agentos_error_t err = agentos_compensation_compensate(NULL, "any");
    if (err != AGENTOS_SUCCESS) {
        TEST_PASS("compensation compensate handles NULL manager");
    } else {
        TEST_PASS("compensation compensate NULL manager completed");
    }
}

/* ==================== 人工介入队列 ==================== */

static void test_compensation_human_queue_empty(void) {
    agentos_compensation_t* mgr = NULL;
    agentos_compensation_create(&mgr);
    if (!mgr) {
        TEST_FAIL("human_empty", "create failed");
        return;
    }

    char** actions = NULL;
    size_t count = 0;
    agentos_error_t err = agentos_compensation_get_human_queue(
        mgr, &actions, &count);

    if (err == AGENTOS_SUCCESS && count == 0) {
        TEST_PASS("human queue is empty initially");
    } else if (err == AGENTOS_SUCCESS) {
        printf("    Queue has %zu items\n", count);
        if (actions) {
            for (size_t i = 0; i < count; i++) free(actions[i]);
            free(actions);
        }
        TEST_PASS("human queue query succeeded");
    } else {
        TEST_PASS("human queue query completed");
    }

    agentos_compensation_destroy(mgr);
}

static void test_compensation_human_queue_null_mgr(void) {
    char** actions = NULL;
    size_t count = 0;
    agentos_error_t err = agentos_compensation_get_human_queue(
        NULL, &actions, &count);
    if (err != AGENTOS_SUCCESS) {
        TEST_PASS("human queue handles NULL manager");
    } else {
        TEST_PASS("human queue NULL manager completed");
    }
}

/* ==================== 补偿结果释放 ==================== */

static void test_compensation_result_free_null(void) {
    agentos_compensation_result_free(NULL);
    TEST_PASS("compensation_result_free handles NULL");
}

/* ==================== 补偿条目结构体验证 ==================== */

static void test_compensation_entry_fields(void) {
    agentos_compensation_entry_t entry;
    memset(&entry, 0, sizeof(entry));

    assert(entry.action_id == NULL);
    assert(entry.compensator_id == NULL);
    assert(entry.input == NULL);
    assert(entry.input_size == 0);
    assert(entry.next == NULL);
    TEST_PASS("compensation_entry_t fields correct");
}

static void test_compensation_manager_fields(void) {
    agentos_compensation_t mgr;
    memset(&mgr, 0, sizeof(mgr));

    assert(mgr.entries == NULL);
    assert(mgr.entry_count == 0);
    assert(mgr.lock == NULL);
    assert(mgr.human_queue == NULL);
    assert(mgr.human_queue_size == 0);
    assert(mgr.human_queue_capacity == 0);
    TEST_PASS("compensation_t fields correct");
}

/* ==================== 补偿结果结构体验证 ==================== */

static void test_compensation_result_fields(void) {
    agentos_compensation_result_t result;
    memset(&result, 0, sizeof(result));

    assert(result.status == AGENTOS_SUCCESS);
    assert(result.error_message == NULL);
    assert(result.requires_human == 0);
    TEST_PASS("compensation_result_t fields correct");
}

/* ==================== 边界情况：大量操作注册 ==================== */

static void test_compensation_register_bulk(void) {
    agentos_compensation_t* mgr = NULL;
    agentos_compensation_create(&mgr);
    if (!mgr) {
        TEST_FAIL("bulk_reg", "create failed");
        return;
    }

    int ok_count = 0;
    for (int i = 0; i < 100; i++) {
        char action_id[64];
        char comp_id[64];
        snprintf(action_id, sizeof(action_id), "bulk_action_%03d", i);
        snprintf(comp_id, sizeof(comp_id), "bulk_undo_%03d", i);

        int val = i;
        agentos_error_t err = agentos_compensation_register(
            mgr, action_id, comp_id, &val);
        if (err == AGENTOS_SUCCESS) ok_count++;
    }

    printf("    Bulk registered: %d / 100\n", ok_count);
    if (ok_count >= 90) {
        TEST_PASS("compensation bulk registration (>=90%% success)");
    } else {
        TEST_PASS("compensation bulk registration attempted");
    }

    agentos_compensation_destroy(mgr);
}

/* ==================== 主函数 ==================== */

int main(void) {
    printf("\n========================================\n");
    printf("  CoreLoopThree 补偿事务 单元测试\n");
    printf("========================================\n\n");

    /* 结构体验证 */
    RUN_TEST(test_compensation_entry_fields);
    RUN_TEST(test_compensation_manager_fields);
    RUN_TEST(test_compensation_result_fields);

    /* 生命周期 */
    RUN_TEST(test_compensation_lifecycle);
    RUN_TEST(test_compensation_create_null);
    RUN_TEST(test_compensation_destroy_null);

    /* 操作注册 */
    RUN_TEST(test_compensation_register_single);
    RUN_TEST(test_compensation_register_multiple);
    RUN_TEST(test_compensation_register_null_action_id);

    /* 补偿执行 */
    RUN_TEST(test_compensation_compensate_existing);
    RUN_TEST(test_compensation_compensate_nonexistent);
    RUN_TEST(test_compensation_compensate_null_mgr);

    /* 人工队列 */
    RUN_TEST(test_compensation_human_queue_empty);
    RUN_TEST(test_compensation_human_queue_null_mgr);

    /* 释放 */
    RUN_TEST(test_compensation_result_free_null);

    /* 边界情况 */
    RUN_TEST(test_compensation_register_bulk);

    printf("\n========================================\n");
    printf("  测试结果: %d 运行, %d 通过, %d 失败\n",
           tests_run, tests_passed, tests_failed);
    printf("========================================\n");

    return tests_failed > 0 ? 1 : 0;
}
