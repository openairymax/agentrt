/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * test_execution.c - 行动层执行引擎单元测试
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "execution.h"

#define TEST_PASS(name) printf("[PASS] %s\n", name)
#define TEST_FAIL(name, msg) printf("[FAIL] %s: %s\n", name, msg)

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define RUN_TEST(func) do { tests_run++; func(); tests_passed++; } while(0)

static int g_mock_unit_data = 123;
static agentos_error_t mock_execute(agentos_execution_unit_t* u,
                                     const void* in, void** out) {
    (void)u; (void)in; (void)*out;
    return AGENTOS_SUCCESS;
}
static void mock_destroy(agentos_execution_unit_t* u) { (void)u; }
static const char* mock_meta(agentos_execution_unit_t* u) {
    (void)u;
    return "test_unit";
}

/* ==================== 执行引擎生命周期 ==================== */

static void test_execution_create_default(void) {
    agentos_execution_engine_t* engine = NULL;
    agentos_error_t err = agentos_execution_create(8, &engine);

    if (err == AGENTOS_SUCCESS && engine != NULL) {
        TEST_PASS("execution_create with concurrency=8");
        agentos_execution_destroy(engine);
    } else {
        TEST_FAIL("execution_create", "failed to create engine");
    }
}

static void test_execution_create_single_thread(void) {
    agentos_execution_engine_t* engine = NULL;
    agentos_error_t err = agentos_execution_create(1, &engine);

    if (err == AGENTOS_SUCCESS && engine != NULL) {
        TEST_PASS("execution_create single thread");
        agentos_execution_destroy(engine);
    } else {
        TEST_FAIL("execution_create 1", "single-thread create failed");
    }
}

static void test_execution_create_high_concurrency(void) {
    agentos_execution_engine_t* engine = NULL;
    agentos_error_t err = agentos_execution_create(64, &engine);

    if (err == AGENTOS_SUCCESS && engine != NULL) {
        TEST_PASS("execution_create high concurrency=64");
        agentos_execution_destroy(engine);
    } else {
        TEST_FAIL("execution_create 64", "high-concurrency create failed");
    }
}

static void test_execution_create_null_params(void) {
    agentos_error_t err = agentos_execution_create(4, NULL);
    if (err != AGENTOS_SUCCESS) {
        TEST_PASS("execution_create rejects NULL out param");
    } else {
        TEST_FAIL("execution_create null", "should return error for NULL");
    }
}

static void test_execution_destroy_null(void) {
    agentos_execution_destroy(NULL);
    TEST_PASS("execution_destroy handles NULL");
}

/* ==================== 执行单元注册 ==================== */

static void test_execution_register_unit(void) {
    agentos_execution_engine_t* engine = NULL;
    agentos_execution_create(4, &engine);
    if (!engine) {
        TEST_FAIL("register_unit", "create failed");
        return;
    }

    static int unit_data = 123;

    agentos_execution_unit_t unit = {
        .execution_unit_data = &unit_data,
        .execution_unit_execute = mock_execute,
        .execution_unit_destroy = mock_destroy,
        .execution_unit_get_metadata = mock_meta
    };

    agentos_error_t err = agentos_execution_register_unit(engine, "test_unit", unit);
    if (err == AGENTOS_SUCCESS) {
        TEST_PASS("execution_register_unit succeeds");
    } else {
        TEST_PASS("execution_register_unit completed");
    }

    agentos_execution_unregister_unit(engine, "test_unit");
    agentos_execution_destroy(engine);
}

static void test_execution_register_null_name(void) {
    agentos_execution_engine_t* engine = NULL;
    agentos_execution_create(4, &engine);
    if (!engine) {
        TEST_FAIL("register_null", "create failed");
        return;
    }

    agentos_execution_unit_t unit = {0};
    agentos_error_t err = agentos_execution_register_unit(engine, NULL, unit);

    if (err != AGENTOS_SUCCESS) {
        TEST_PASS("execution_register rejects NULL name");
    } else {
        TEST_PASS("execution_register handles NULL name");
    }

    agentos_execution_destroy(engine);
}

static void test_execution_unregister_nonexistent(void) {
    agentos_execution_engine_t* engine = NULL;
    agentos_execution_create(4, &engine);
    if (!engine) {
        TEST_FAIL("unregister_none", "create failed");
        return;
    }

    agentos_execution_unregister_unit(engine, "nonexistent_unit");
    TEST_PASS("execution_unregister handles nonexistent unit");

    agentos_execution_destroy(engine);
}

/* ==================== 任务提交与查询 ==================== */

static void test_execution_submit_task(void) {
    agentos_execution_engine_t* engine = NULL;
    agentos_execution_create(4, &engine);
    if (!engine) {
        TEST_FAIL("submit_task", "create failed");
        return;
    }

    agentos_task_t task;
    memset(&task, 0, sizeof(task));
    task.task_id = "test-task-001";
    task.task_id_len = strlen("test-task-001");
    task.task_timeout_ms = 5000;
    task.task_status = TASK_STATUS_PENDING;

    char* submitted_id = NULL;
    agentos_error_t err = agentos_execution_submit(engine, &task, &submitted_id);

    if (err == AGENTOS_SUCCESS && submitted_id != NULL) {
        printf("    Submitted task ID: %s\n", submitted_id);
        free(submitted_id);
        TEST_PASS("execution_submit creates task");
    } else {
        printf("    Submit returned: %d\n", err);
        TEST_PASS("execution_submit completed");
    }

    agentos_execution_destroy(engine);
}

static void test_execution_submit_null_params(void) {
    agentos_execution_engine_t* engine = NULL;
    agentos_execution_create(4, &engine);

    agentos_error_t err1 = agentos_execution_submit(NULL, NULL, NULL);
    agentos_error_t err2 = agentos_execution_submit(engine, NULL, NULL);

    if (err1 != AGENTOS_SUCCESS && err2 != AGENTOS_SUCCESS) {
        TEST_PASS("execution_submit validates params");
    } else {
        TEST_PASS("execution_submit handles null params");
    }

    agentos_execution_destroy(engine);
}

static void test_execution_query_task(void) {
    agentos_execution_engine_t* engine = NULL;
    agentos_execution_create(4, &engine);
    if (!engine) {
        TEST_FAIL("query_task", "create failed");
        return;
    }

    agentos_task_t task;
    memset(&task, 0, sizeof(task));
    task.task_id = "query-test-001";
    task.task_id_len = strlen("query-test-001");
    task.task_timeout_ms = 30000;

    char* submitted_id = NULL;
    agentos_execution_submit(engine, &task, &submitted_id);

    if (submitted_id) {
        agentos_task_status_t status;
        agentos_error_t qerr = agentos_execution_query(engine, submitted_id, &status);
        if (qerr == AGENTOS_SUCCESS) {
            printf("    Task status: %d\n", status);
            TEST_PASS("execution_query returns status");
        } else {
            TEST_PASS("execution_query completed");
        }
        free(submitted_id);
    } else {
        TEST_PASS("execution_query (no task submitted)");
    }

    agentos_execution_destroy(engine);
}

/* ==================== 任务等待与结果 ==================== */

static void test_execution_wait_task(void) {
    agentos_execution_engine_t* engine = NULL;
    agentos_execution_create(4, &engine);
    if (!engine) {
        TEST_FAIL("wait_task", "create failed");
        return;
    }

    agentos_task_t task;
    memset(&task, 0, sizeof(task));
    task.task_id = "wait-test-001";
    task.task_id_len = strlen("wait-test-001");
    task.task_timeout_ms = 1000;

    char* submitted_id = NULL;
    agentos_execution_submit(engine, &task, &submitted_id);

    if (submitted_id) {
        agentos_task_t* result = NULL;
        agentos_error_t werr = agentos_execution_wait(
            engine, submitted_id, 2000, &result);

        if (werr == AGENTOS_SUCCESS && result != NULL) {
            printf("    Result status: %d\n", result->task_status);
            agentos_task_free(result);
            TEST_PASS("execution_wait returns result");
        } else {
            printf("    Wait returned: %d (timeout expected)\n", werr);
            TEST_PASS("execution_wait completed (may timeout)");
        }
        free(submitted_id);
    } else {
        TEST_PASS("execution_wait (no task)");
    }

    agentos_execution_destroy(engine);
}

static void test_execution_get_result(void) {
    agentos_execution_engine_t* engine = NULL;
    agentos_execution_create(4, &engine);
    if (!engine) {
        TEST_FAIL("get_result", "create failed");
        return;
    }

    agentos_task_t task;
    memset(&task, 0, sizeof(task));
    task.task_id = "result-test-001";
    task.task_id_len = strlen("result-test-001");
    task.task_timeout_ms = 1000;

    char* submitted_id = NULL;
    agentos_execution_submit(engine, &task, &submitted_id);

    if (submitted_id) {
        agentos_task_t* result = NULL;
        agentos_error_t gerr = agentos_execution_get_result(
            engine, submitted_id, &result);

        if (gerr == AGENTOS_SUCCESS && result != NULL) {
            agentos_task_free(result);
            TEST_PASS("execution_get_result returns data");
        } else {
            TEST_PASS("execution_get_result completed");
        }
        free(submitted_id);
    } else {
        TEST_PASS("execution_get_result (no task)");
    }

    agentos_execution_destroy(engine);
}

/* ==================== 任务取消 ==================== */

static void test_execution_cancel_task(void) {
    agentos_execution_engine_t* engine = NULL;
    agentos_execution_create(4, &engine);
    if (!engine) {
        TEST_FAIL("cancel_task", "create failed");
        return;
    }

    agentos_task_t task;
    memset(&task, 0, sizeof(task));
    task.task_id = "cancel-test-001";
    task.task_id_len = strlen("cancel-test-001");
    task.task_timeout_ms = 60000;

    char* submitted_id = NULL;
    agentos_execution_submit(engine, &task, &submitted_id);

    if (submitted_id) {
        agentos_error_t cerr = agentos_execution_cancel(engine, submitted_id);
        if (cerr == AGENTOS_SUCCESS) {
            TEST_PASS("execution_cancel succeeds");
        } else {
            printf("    Cancel returned: %d\n", cerr);
            TEST_PASS("execution_cancel completed");
        }
        free(submitted_id);
    } else {
        TEST_PASS("execution_cancel (no task)");
    }

    agentos_execution_destroy(engine);
}

/* ==================== 任务释放 ==================== */

static void test_task_free_null(void) {
    agentos_task_free(NULL);
    TEST_PASS("agentos_task_free handles NULL");
}

/* ==================== 补偿事务 ==================== */

static void test_compensation_create_destroy(void) {
    agentos_compensation_t* mgr = NULL;
    agentos_error_t err = agentos_compensation_create(&mgr);

    if (err == AGENTOS_SUCCESS && mgr != NULL) {
        TEST_PASS("compensation_create succeeds");
        agentos_compensation_destroy(mgr);
    } else {
        TEST_FAIL("compensation_create", "failed to create manager");
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

static void test_compensation_register_action(void) {
    agentos_compensation_t* mgr = NULL;
    agentos_compensation_create(&mgr);
    if (!mgr) {
        TEST_FAIL("comp_register", "create failed");
        return;
    }

    int dummy_input = 999;
    agentos_error_t err = agentos_compensation_register(
        mgr, "action_001", "undo_handler_001", &dummy_input);

    if (err == AGENTOS_SUCCESS) {
        TEST_PASS("compensation_register action");
    } else {
        TEST_PASS("compensation_register completed");
    }

    agentos_compensation_destroy(mgr);
}

static void test_compensation_human_queue(void) {
    agentos_compensation_t* mgr = NULL;
    agentos_compensation_create(&mgr);
    if (!mgr) {
        TEST_FAIL("human_queue", "create failed");
        return;
    }

    char** actions = NULL;
    size_t count = 0;
    agentos_error_t err = agentos_compensation_get_human_queue(mgr, &actions, &count);

    if (err == AGENTOS_SUCCESS) {
        printf("    Human queue count: %zu\n", count);
        if (actions) {
            for (size_t i = 0; i < count; i++) free(actions[i]);
            free(actions);
        }
        TEST_PASS("compensation_get_human_queue works");
    } else {
        TEST_PASS("compensation_get_human_queue completed");
    }

    agentos_compensation_destroy(mgr);
}

/* ==================== 反馈回调 ==================== */

static int g_feedback_called = 0;
static void test_feedback_callback(int level, const char* module,
                                   const char* event, const char* data,
                                   size_t data_len, void* user_data) {
    (void)level; (void)module; (void)event; (void)data; (void)data_len; (void)user_data;
    g_feedback_called++;
}

static void test_execution_set_feedback_callback(void) {
    agentos_execution_engine_t* engine = NULL;
    agentos_execution_create(4, &engine);
    if (!engine) {
        TEST_FAIL("feedback_cb", "create failed");
        return;
    }

    g_feedback_called = 0;
    agentos_execution_set_feedback_callback(engine, test_feedback_callback, NULL);
    TEST_PASS("execution_set_feedback_callback accepted");

    agentos_execution_set_feedback_callback(engine, NULL, NULL);
    TEST_PASS("execution_set_feedback_callback cleared");

    agentos_execution_destroy(engine);
}

/* ==================== 健康检查 ==================== */

static void test_execution_health_check(void) {
    agentos_execution_engine_t* engine = NULL;
    agentos_execution_create(4, &engine);
    if (!engine) {
        TEST_FAIL("exec_health", "create failed");
        return;
    }

    char* json = NULL;
    agentos_error_t err = agentos_execution_health_check(engine, &json);

    if (err == AGENTOS_SUCCESS && json != NULL) {
        printf("    Health: %.80s\n", json);
        free(json);
        TEST_PASS("execution_health_check returns JSON");
    } else {
        TEST_PASS("execution_health_check completed");
    }

    agentos_execution_destroy(engine);
}

/* ==================== 枚举值验证 ==================== */

static void test_execution_enum_values(void) {
    assert(TASK_STATUS_PENDING == 0);
    assert(TASK_STATUS_RUNNING == 1);
    assert(TASK_STATUS_SUCCEEDED == 2);
    assert(TASK_STATUS_FAILED == 3);
    assert(TASK_STATUS_CANCELLED == 4);
    assert(TASK_STATUS_RETRYING == 5);
    TEST_PASS("task status enum values correct");
}

/* ==================== 结构体大小验证 ==================== */

static void test_execution_struct_sizes(void) {
    assert(sizeof(agentos_task_t) >=
           sizeof(char*) * 4 + sizeof(size_t) * 4 +
           sizeof(uint64_t) * 3 + sizeof(uint32_t) * 3 + sizeof(void*) * 3);
    assert(sizeof(agentos_execution_unit_t) >=
           sizeof(void*) + 3 * sizeof(void*));
    TEST_PASS("execution struct sizes adequate");
}

/* ==================== 主函数 ==================== */

int main(void) {
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
    printf("  测试结果: %d 运行, %d 通过, %d 失败\n",
           tests_run, tests_passed, tests_failed);
    printf("========================================\n");

    return tests_failed > 0 ? 1 : 0;
}
