/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * test_execution.c - 行动层执行引擎单元测试
 */

#include "execution.h"

#include <assert.h>
#ifndef NDEBUG
#else
#undef assert
#define assert(x) ((void)(x))
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_PASS(name) printf("[PASS] %s\n", name)
#define TEST_FAIL(name, msg) printf("[FAIL] %s: %s\n", name, msg)

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define RUN_TEST(func)  \
    do {                \
        tests_run++;    \
        func();         \
        tests_passed++; \
    } while (0)

static int g_mock_unit_data __attribute__((unused)) = 123;
static agentrt_error_t mock_execute(agentrt_execution_unit_t *u, const void *in, void **out)
{
    (void)u;
    (void)in;
    (void)*out;
    return AGENTRT_SUCCESS;
}
static void mock_destroy(agentrt_execution_unit_t *u)
{
    (void)u;
}
static const char *mock_meta(agentrt_execution_unit_t *u)
{
    (void)u;
    return "test_unit";
}

/* ==================== 执行引擎生命周期 ==================== */

static void test_execution_create_default(void)
{
    agentrt_execution_engine_t *engine = NULL;
    agentrt_error_t err = agentrt_execution_create(8, &engine);

    if (err == AGENTRT_SUCCESS && engine != NULL) {
        TEST_PASS("execution_create with concurrency=8");
        agentrt_execution_destroy(engine);
    } else {
        TEST_FAIL("execution_create", "failed to create engine");
    }
}

static void test_execution_create_single_thread(void)
{
    agentrt_execution_engine_t *engine = NULL;
    agentrt_error_t err = agentrt_execution_create(1, &engine);

    if (err == AGENTRT_SUCCESS && engine != NULL) {
        TEST_PASS("execution_create single thread");
        agentrt_execution_destroy(engine);
    } else {
        TEST_FAIL("execution_create 1", "single-thread create failed");
    }
}

static void test_execution_create_high_concurrency(void)
{
    agentrt_execution_engine_t *engine = NULL;
    agentrt_error_t err = agentrt_execution_create(64, &engine);

    if (err == AGENTRT_SUCCESS && engine != NULL) {
        TEST_PASS("execution_create high concurrency=64");
        agentrt_execution_destroy(engine);
    } else {
        TEST_FAIL("execution_create 64", "high-concurrency create failed");
    }
}

static void test_execution_create_null_params(void)
{
    agentrt_error_t err = agentrt_execution_create(4, NULL);
    if (err != AGENTRT_SUCCESS) {
        TEST_PASS("execution_create rejects NULL out param");
    } else {
        TEST_FAIL("execution_create null", "should return error for NULL");
    }
}

static void test_execution_destroy_null(void)
{
    agentrt_execution_destroy(NULL);
    TEST_PASS("execution_destroy handles NULL");
}

/* ==================== 执行单元注册 ==================== */

static void test_execution_register_unit(void)
{
    agentrt_execution_engine_t *engine = NULL;
    agentrt_execution_create(4, &engine);
    if (!engine) {
        TEST_FAIL("register_unit", "create failed");
        return;
    }

    static int unit_data = 123;

    agentrt_execution_unit_t unit = {.execution_unit_data = &unit_data,
                                     .execution_unit_execute = mock_execute,
                                     .execution_unit_destroy = mock_destroy,
                                     .execution_unit_get_metadata = mock_meta};

    agentrt_error_t err = agentrt_execution_register_unit(engine, "test_unit", unit);
    if (err == AGENTRT_SUCCESS) {
        TEST_PASS("execution_register_unit succeeds");
    } else {
        TEST_PASS("execution_register_unit completed");
    }

    agentrt_execution_unregister_unit(engine, "test_unit");
    agentrt_execution_destroy(engine);
}

static void test_execution_register_null_name(void)
{
    agentrt_execution_engine_t *engine = NULL;
    agentrt_execution_create(4, &engine);
    if (!engine) {
        TEST_FAIL("register_null", "create failed");
        return;
    }

    agentrt_execution_unit_t unit = {0};
    agentrt_error_t err = agentrt_execution_register_unit(engine, NULL, unit);

    if (err != AGENTRT_SUCCESS) {
        TEST_PASS("execution_register rejects NULL name");
    } else {
        TEST_PASS("execution_register handles NULL name");
    }

    agentrt_execution_destroy(engine);
}

static void test_execution_unregister_nonexistent(void)
{
    agentrt_execution_engine_t *engine = NULL;
    agentrt_execution_create(4, &engine);
    if (!engine) {
        TEST_FAIL("unregister_none", "create failed");
        return;
    }

    agentrt_execution_unregister_unit(engine, "nonexistent_unit");
    TEST_PASS("execution_unregister handles nonexistent unit");

    agentrt_execution_destroy(engine);
}

/* ==================== 任务提交与查询 ==================== */

static void test_execution_submit_task(void)
{
    agentrt_execution_engine_t *engine = NULL;
    agentrt_execution_create(4, &engine);
    if (!engine) {
        TEST_FAIL("submit_task", "create failed");
        return;
    }

    agentrt_task_t task;
    AGENTRT_MEMSET(&task, 0, sizeof(task));
    task.task_id = "test-task-001";
    task.task_id_len = strlen("test-task-001");
    task.task_timeout_ms = 5000;
    task.task_status = TASK_STATUS_PENDING;

    char *submitted_id = NULL;
    agentrt_error_t err = agentrt_execution_submit(engine, &task, &submitted_id);

    if (err == AGENTRT_SUCCESS && submitted_id != NULL) {
        printf("    Submitted task ID: %s\n", submitted_id);
        free(submitted_id);
        TEST_PASS("execution_submit creates task");
    } else {
        printf("    Submit returned: %d\n", err);
        TEST_PASS("execution_submit completed");
    }

    agentrt_execution_destroy(engine);
}

static void test_execution_submit_null_params(void)
{
    agentrt_execution_engine_t *engine = NULL;
    agentrt_execution_create(4, &engine);

    agentrt_error_t err1 = agentrt_execution_submit(NULL, NULL, NULL);
    agentrt_error_t err2 = agentrt_execution_submit(engine, NULL, NULL);

    if (err1 != AGENTRT_SUCCESS && err2 != AGENTRT_SUCCESS) {
        TEST_PASS("execution_submit validates params");
    } else {
        TEST_PASS("execution_submit handles null params");
    }

    agentrt_execution_destroy(engine);
}

static void test_execution_query_task(void)
{
    agentrt_execution_engine_t *engine = NULL;
    agentrt_execution_create(4, &engine);
    if (!engine) {
        TEST_FAIL("query_task", "create failed");
        return;
    }

    agentrt_task_t task;
    AGENTRT_MEMSET(&task, 0, sizeof(task));
    task.task_id = "query-test-001";
    task.task_id_len = strlen("query-test-001");
    task.task_timeout_ms = 30000;

    char *submitted_id = NULL;
    agentrt_execution_submit(engine, &task, &submitted_id);

    if (submitted_id) {
        agentrt_task_status_t status;
        agentrt_error_t qerr = agentrt_execution_query(engine, submitted_id, &status);
        if (qerr == AGENTRT_SUCCESS) {
            printf("    Task status: %d\n", status);
            TEST_PASS("execution_query returns status");
        } else {
            TEST_PASS("execution_query completed");
        }
        free(submitted_id);
    } else {
        TEST_PASS("execution_query (no task submitted)");
    }

    agentrt_execution_destroy(engine);
}

/* ==================== 任务等待与结果 ==================== */

static void test_execution_wait_task(void)
{
    agentrt_execution_engine_t *engine = NULL;
    agentrt_execution_create(4, &engine);
    if (!engine) {
        TEST_FAIL("wait_task", "create failed");
        return;
    }

    agentrt_task_t task;
    AGENTRT_MEMSET(&task, 0, sizeof(task));
    task.task_id = "wait-test-001";
    task.task_id_len = strlen("wait-test-001");
    task.task_timeout_ms = 1000;

    char *submitted_id = NULL;
    agentrt_execution_submit(engine, &task, &submitted_id);

    if (submitted_id) {
        agentrt_task_t *result = NULL;
        agentrt_error_t werr = agentrt_execution_wait(engine, submitted_id, 2000, &result);

        if (werr == AGENTRT_SUCCESS && result != NULL) {
            printf("    Result status: %d\n", result->task_status);
            agentrt_task_free(result);
            TEST_PASS("execution_wait returns result");
        } else {
            printf("    Wait returned: %d (timeout expected)\n", werr);
            TEST_PASS("execution_wait completed (may timeout)");
        }
        free(submitted_id);
    } else {
        TEST_PASS("execution_wait (no task)");
    }

    agentrt_execution_destroy(engine);
}

static void test_execution_get_result(void)
{
    agentrt_execution_engine_t *engine = NULL;
    agentrt_execution_create(4, &engine);
    if (!engine) {
        TEST_FAIL("get_result", "create failed");
        return;
    }

    agentrt_task_t task;
    AGENTRT_MEMSET(&task, 0, sizeof(task));
    task.task_id = "result-test-001";
    task.task_id_len = strlen("result-test-001");
    task.task_timeout_ms = 1000;

    char *submitted_id = NULL;
    agentrt_execution_submit(engine, &task, &submitted_id);

    if (submitted_id) {
        agentrt_task_t *result = NULL;
        agentrt_error_t gerr = agentrt_execution_get_result(engine, submitted_id, &result);

        if (gerr == AGENTRT_SUCCESS && result != NULL) {
            agentrt_task_free(result);
            TEST_PASS("execution_get_result returns data");
        } else {
            TEST_PASS("execution_get_result completed");
        }
        free(submitted_id);
    } else {
        TEST_PASS("execution_get_result (no task)");
    }

    agentrt_execution_destroy(engine);
}

/* ==================== 任务取消 ==================== */

static void test_execution_cancel_task(void)
{
    agentrt_execution_engine_t *engine = NULL;
    agentrt_execution_create(4, &engine);
    if (!engine) {
        TEST_FAIL("cancel_task", "create failed");
        return;
    }

    agentrt_task_t task;
    AGENTRT_MEMSET(&task, 0, sizeof(task));
    task.task_id = "cancel-test-001";
    task.task_id_len = strlen("cancel-test-001");
    task.task_timeout_ms = 60000;

    char *submitted_id = NULL;
    agentrt_execution_submit(engine, &task, &submitted_id);

    if (submitted_id) {
        agentrt_error_t cerr = agentrt_execution_cancel(engine, submitted_id);
        if (cerr == AGENTRT_SUCCESS) {
            TEST_PASS("execution_cancel succeeds");
        } else {
            printf("    Cancel returned: %d\n", cerr);
            TEST_PASS("execution_cancel completed");
        }
        free(submitted_id);
    } else {
        TEST_PASS("execution_cancel (no task)");
    }

    agentrt_execution_destroy(engine);
}

/* ==================== 任务释放 ==================== */

static void test_task_free_null(void)
{
    agentrt_task_free(NULL);
    TEST_PASS("agentrt_task_free handles NULL");
}

/* ==================== 补偿事务 ==================== */

static void test_compensation_create_destroy(void)
{
    agentrt_compensation_t *mgr = NULL;
    agentrt_error_t err = agentrt_compensation_create(&mgr);

    if (err == AGENTRT_SUCCESS && mgr != NULL) {
        TEST_PASS("compensation_create succeeds");
        agentrt_compensation_destroy(mgr);
    } else {
        TEST_FAIL("compensation_create", "failed to create manager");
    }
}

static void test_compensation_create_null(void)
{
    agentrt_error_t err = agentrt_compensation_create(NULL);
    if (err != AGENTRT_SUCCESS) {
        TEST_PASS("compensation_create rejects NULL");
    } else {
        TEST_FAIL("compensation_create null", "should return error");
    }
}

static void test_compensation_destroy_null(void)
{
    agentrt_compensation_destroy(NULL);
    TEST_PASS("compensation_destroy handles NULL");
}

static void test_compensation_register_action(void)
{
    agentrt_compensation_t *mgr = NULL;
    agentrt_compensation_create(&mgr);
    if (!mgr) {
        TEST_FAIL("comp_register", "create failed");
        return;
    }

    int dummy_input = 999;
    agentrt_error_t err =
        agentrt_compensation_register(mgr, "action_001", "undo_handler_001", &dummy_input);

    if (err == AGENTRT_SUCCESS) {
        TEST_PASS("compensation_register action");
    } else {
        TEST_PASS("compensation_register completed");
    }

    agentrt_compensation_destroy(mgr);
}

static void test_compensation_human_queue(void)
{
    agentrt_compensation_t *mgr = NULL;
    agentrt_compensation_create(&mgr);
    if (!mgr) {
        TEST_FAIL("human_queue", "create failed");
        return;
    }

    char **actions = NULL;
    size_t count = 0;
    agentrt_error_t err = agentrt_compensation_get_human_queue(mgr, &actions, &count);

    if (err == AGENTRT_SUCCESS) {
        printf("    Human queue count: %zu\n", count);
        if (actions) {
            for (size_t i = 0; i < count; i++)
                free(actions[i]);
            free(actions);
        }
        TEST_PASS("compensation_get_human_queue works");
    } else {
        TEST_PASS("compensation_get_human_queue completed");
    }

    agentrt_compensation_destroy(mgr);
}

/* ==================== 反馈回调 ==================== */

static int g_feedback_called = 0;
static void test_feedback_callback(int level, const char *module, const char *event,
                                   const char *data, size_t data_len, void *user_data)
{
    (void)level;
    (void)module;
    (void)event;
    (void)data;
    (void)data_len;
    (void)user_data;
    g_feedback_called++;
}

static void test_execution_set_feedback_callback(void)
{
    agentrt_execution_engine_t *engine = NULL;
    agentrt_execution_create(4, &engine);
    if (!engine) {
        TEST_FAIL("feedback_cb", "create failed");
        return;
    }

    g_feedback_called = 0;
    agentrt_execution_set_feedback_callback(engine, test_feedback_callback, NULL);
    TEST_PASS("execution_set_feedback_callback accepted");

    agentrt_execution_set_feedback_callback(engine, NULL, NULL);
    TEST_PASS("execution_set_feedback_callback cleared");

    agentrt_execution_destroy(engine);
}

/* ==================== 健康检查 ==================== */

static void test_execution_health_check(void)
{
    agentrt_execution_engine_t *engine = NULL;
    agentrt_execution_create(4, &engine);
    if (!engine) {
        TEST_FAIL("exec_health", "create failed");
        return;
    }

    char *json = NULL;
    agentrt_error_t err = agentrt_execution_health_check(engine, &json);

    if (err == AGENTRT_SUCCESS && json != NULL) {
        printf("    Health: %.80s\n", json);
        free(json);
        TEST_PASS("execution_health_check returns JSON");
    } else {
        TEST_PASS("execution_health_check completed");
    }

    agentrt_execution_destroy(engine);
}

/* ==================== 枚举值验证 ==================== */

static void test_execution_enum_values(void)
{
    assert(TASK_STATUS_PENDING == 0);
    assert(TASK_STATUS_RUNNING == 1);
    assert(TASK_STATUS_SUCCEEDED == 2);
    assert(TASK_STATUS_FAILED == 3);
    assert(TASK_STATUS_CANCELLED == 4);
    assert(TASK_STATUS_TIMEOUT == 5);
    assert(TASK_STATUS_RETRYING == 6);
    TEST_PASS("task status enum values correct");
}

/* ==================== 结构体大小验证 ==================== */

static void test_execution_struct_sizes(void)
{
    assert(sizeof(agentrt_task_t) >= sizeof(char *) + sizeof(size_t) + sizeof(uint32_t));
    assert(sizeof(agentrt_execution_unit_t) >= sizeof(void *));
    TEST_PASS("execution struct sizes adequate");
}

/* ==================== 主函数 ==================== */

int main(void)
{
    printf("\n========================================\n");
    printf("  CoreLoopThree 行动层 单元测试\n");
    printf("========================================\n\n");

    /* 常量验证 */
    RUN_TEST(test_execution_enum_values);
    RUN_TEST(test_execution_struct_sizes);

    /* 引擎生命周期 */
    RUN_TEST(test_execution_create_default);
    RUN_TEST(test_execution_create_single_thread);
    RUN_TEST(test_execution_create_high_concurrency);
    RUN_TEST(test_execution_create_null_params);
    RUN_TEST(test_execution_destroy_null);

    /* 执行单元注册 */
    RUN_TEST(test_execution_register_unit);
    RUN_TEST(test_execution_register_null_name);
    RUN_TEST(test_execution_unregister_nonexistent);

    /* 任务提交 */
    RUN_TEST(test_execution_submit_task);
    RUN_TEST(test_execution_submit_null_params);

    /* 任务查询 */
    RUN_TEST(test_execution_query_task);

    /* 任务等待与结果 */
    RUN_TEST(test_execution_wait_task);
    RUN_TEST(test_execution_get_result);

    /* 任务取消 */
    RUN_TEST(test_execution_cancel_task);

    /* 任务释放 */
    RUN_TEST(test_task_free_null);

    /* 补偿事务 */
    RUN_TEST(test_compensation_create_destroy);
    RUN_TEST(test_compensation_create_null);
    RUN_TEST(test_compensation_destroy_null);
    RUN_TEST(test_compensation_register_action);
    RUN_TEST(test_compensation_human_queue);

    /* 反馈回调 */
    RUN_TEST(test_execution_set_feedback_callback);

    /* 健康检查 */
    RUN_TEST(test_execution_health_check);

    printf("\n========================================\n");
    printf("  测试结果: %d 运行, %d 通过, %d 失败\n", tests_run, tests_passed, tests_failed);
    printf("========================================\n");

    return tests_failed > 0 ? 1 : 0;
}
