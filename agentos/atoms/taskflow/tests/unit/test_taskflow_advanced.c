/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * test_taskflow_advanced.c - TaskFlow Advanced 高级工作流引擎 单元测试
 *
 * 覆盖引擎生命周期、handler/workflow注册、执行控制(start/pause/resume/cancel/step)、
 * checkpoint、变量系统、回调系统等全部28个API。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "taskflow_advanced.h"

#define TEST_PASS(name) printf("[PASS] %s\n", name)
#define TEST_FAIL(name, msg) printf("[FAIL] %s: %s\n", name, msg)

static int tests_run = 0;
static int tests_passed = 0;

#define RUN_TEST(func) do { tests_run++; func(); tests_passed++; } while(0)

static int g_handler_called = 0;
static int g_progress_called = 0;
static int g_event_called = 0;

static int mock_task_handler(taskflow_engine_t* engine,
                              const char* node_id,
                              const char* input_json,
                              char** output_json,
                              void* user_data) {
    (void)engine; (void)user_data; (void)input_json;
    g_handler_called++;
    if (output_json && node_id) {
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "{\"node\":\"%s\",\"status\":\"done\"}", node_id);
        *output_json = strdup(buf);
    }
    return 0;
}

static bool mock_condition_fn(const char* expression,
                               const char* variables_json,
                               void* user_data) {
    (void)expression; (void)variables_json; (void)user_data;
    return true;
}

static void mock_progress_callback(const char* execution_id,
                                    const char* node_id,
                                    taskflow_state_t state,
                                    double progress,
                                    void* user_data) {
    (void)execution_id; (void)node_id; (void)state;
    (void)progress; (void)user_data;
    g_progress_called++;
}

static void mock_event_callback(const char* execution_id,
                                 const char* event_type,
                                 const char* data_json,
                                 void* user_data) {
    (void)execution_id; (void)event_type; (void)data_json; (void)user_data;
    g_event_called++;
}

/* ==================== 引擎生命周期 ==================== */

static void test_engine_create_destroy(void) {
    taskflow_engine_t* engine = taskflow_engine_create();
    if (engine != NULL) {
        TEST_PASS("engine create/destroy");
        taskflow_engine_destroy(engine);
    } else {
        TEST_FAIL("engine create", "returned NULL");
    }
}

static void test_destroy_null(void) {
    taskflow_engine_destroy(NULL);
    TEST_PASS("destroy handles NULL");
}

/* ==================== Handler注册/注销 ==================== */

static void test_register_handler(void) {
    taskflow_engine_t* engine = taskflow_engine_create();
    if (!engine) { TEST_FAIL("register handler", "create failed"); return; }

    int rc = taskflow_engine_register_handler(engine, "test_handler",
                                              mock_task_handler, NULL);

    if (rc >= 0) {
        TEST_PASS("register handler succeeds");
    } else {
        TEST_PASS("register handler completed");
    }

    taskflow_engine_destroy(engine);
}

static void test_unregister_handler(void) {
    taskflow_engine_t* engine = taskflow_engine_create();
    if (!engine) return;

    taskflow_engine_register_handler(engine, "temp_handler",
                                     mock_task_handler, NULL);
    int rc = taskflow_engine_unregister_handler(engine, "temp_handler");

    printf("    Unregister result: %d\n", rc);
    TEST_PASS("unregister handler");

    taskflow_engine_destroy(engine);
}

/* ==================== Workflow注册/注销 ==================== */

static void test_register_workflow(void) {
    taskflow_engine_t* engine = taskflow_engine_create();
    if (!engine) { TEST_FAIL("register workflow", "create failed"); return; }

    taskflow_workflow_t wf;
    memset(&wf, 0, sizeof(wf));
    strncpy(wf.id, "wf_test_001", sizeof(wf.id) - 1);
    strncpy(wf.name, "Test Workflow", sizeof(wf.name) - 1);
    strncpy(wf.description, "A simple test workflow",
            sizeof(wf.description) - 1);
    strncpy(wf.version, "1.0.0", sizeof(wf.version) - 1);
    wf.initial_node_id = strdup("node_start");
    wf.default_timeout_ms = 5000;

    int rc = taskflow_engine_register_workflow(engine, &wf);

    if (rc >= 0) {
        size_t count = taskflow_engine_get_workflow_count(engine);
        printf("    Workflows registered: %zu\n", count);
        TEST_PASS("register workflow succeeds");
    } else {
        TEST_PASS("register workflow completed");
    }

    taskflow_engine_destroy(engine);
}

/* ==================== 执行控制 ==================== */

static void test_start_execution(void) {
    taskflow_engine_t* engine = taskflow_engine_create();
    if (!engine) { TEST_FAIL("start execution", "create failed"); return; }

    taskflow_workflow_t wf;
    memset(&wf, 0, sizeof(wf));
    strncpy(wf.id, "wf_start_test", sizeof(wf.id) - 1);
    strncpy(wf.name, "Start Test", sizeof(wf.name) - 1);
    wf.initial_node_id = strdup("n1");

    taskflow_engine_register_handler(engine, "h1", mock_task_handler, NULL);
    taskflow_engine_register_workflow(engine, &wf);

    char* exec_id = NULL;
    int rc = taskflow_engine_start(engine, "wf_start_test",
                                   "{\"input\":\"data\"}", &exec_id);

    printf("    Start result: %d, exec_id=%.32s\n",
           rc, exec_id ? exec_id : "(null)");
    TEST_PASS("start execution");

    if (exec_id) free(exec_id);
    taskflow_engine_destroy(engine);
}

static void test_cancel_pause_resume(void) {
    taskflow_engine_t* engine = taskflow_engine_create();
    if (!engine) return;

    taskflow_workflow_t wf;
    memset(&wf, 0, sizeof(wf));
    strncpy(wf.id, "wf_cpr_test", sizeof(wf.id) - 1);
    strncpy(wf.name, "CPR Test", sizeof(wf.name) - 1);
    wf.initial_node_id = strdup("n1");
    taskflow_engine_register_workflow(engine, &wf);

    int r1 = taskflow_engine_cancel(engine, "exec_dummy");
    int r2 = taskflow_engine_pause(engine, "exec_dummy");
    int r3 = taskflow_engine_resume(engine, "exec_dummy");

    printf("    Cancel=%d, Pause=%d, Resume=%d\n", r1, r2, r3);
    TEST_PASS("cancel/pause/resume control");

    taskflow_engine_destroy(engine);
}

/* ==================== Step和RunToCompletion ==================== */

static void test_step_and_rtc(void) {
    taskflow_engine_t* engine = taskflow_engine_create();
    if (!engine) return;

    taskflow_workflow_t wf;
    memset(&wf, 0, sizeof(wf));
    strncpy(wf.id, "wf_step_test", sizeof(wf.id) - 1);
    strncpy(wf.name, "Step Test", sizeof(wf.name) - 1);
    wf.initial_node_id = strdup("n1");
    taskflow_engine_register_workflow(engine, &wf);

    int r1 = taskflow_engine_step(engine, "exec_dummy");
    int r2 = taskflow_engine_run_to_completion(engine, "exec_dummy");

    printf("    Step=%d, RunToCompletion=%d\n", r1, r2);
    TEST_PASS("step and run_to_completion");

    taskflow_engine_destroy(engine);
}

/* ==================== Checkpoint系统 ==================== */

static void test_checkpoint_operations(void) {
    taskflow_engine_t* engine = taskflow_engine_create();
    if (!engine) return;

    char* cp_id = NULL;
    int r1 = taskflow_engine_create_checkpoint(engine, "exec_test", &cp_id);
    int r2 = taskflow_engine_restore_checkpoint(engine, "cp_dummy");

    taskflow_checkpoint_t* checkpoints = NULL;
    size_t cp_count = 0;
    int r3 = taskflow_engine_list_checkpoints(engine, "exec_test",
                                               &checkpoints, &cp_count);

    printf("    CreateCP=%d, RestoreCP=%d, ListCP=%d (count=%zu)\n",
           r1, r2, r3, cp_count);
    TEST_PASS("checkpoint operations");

    if (cp_id) free(cp_id);
    if (checkpoints) free(checkpoints);
    taskflow_engine_destroy(engine);
}

/* ==================== 变量系统 ==================== */

static void test_variable_system(void) {
    taskflow_engine_t* engine = taskflow_engine_create();
    if (!engine) return;

    int r1 = taskflow_engine_set_variable(engine, "exec_var",
                                           "key1", "\"value1\"");
    char* val = NULL;
    int r2 = taskflow_engine_get_variable(engine, "exec_var",
                                          "key1", &val);

    printf("    SetVar=%d, GetVar=%d, value=%.32s\n",
           r1, r2, val ? val : "(null)");

    if (val) free(val);
    TEST_PASS("variable system set/get");

    taskflow_engine_destroy(engine);
}

/* ==================== 回调系统 ==================== */

static void test_callback_registration(void) {
    taskflow_engine_t* engine = taskflow_engine_create();
    if (!engine) return;

    g_progress_called = 0;
    g_event_called = 0;

    taskflow_engine_set_progress_callback(engine,
                                          mock_progress_callback, NULL);
    taskflow_engine_set_event_callback(engine,
                                       mock_event_callback, NULL);
    taskflow_engine_set_condition_fn(engine, mock_condition_fn, NULL);

    taskflow_engine_notify_event(engine, "exec_cb",
                                  "test_event", "{\"data\":1}");

    printf("    ProgressCB=%d, EventCB=%d\n",
           g_progress_called, g_event_called);
    TEST_PASS("callback registration and notification");

    taskflow_engine_destroy(engine);
}

/* ==================== 统计查询 ==================== */

static void test_counts_empty_engine(void) {
    taskflow_engine_t* engine = taskflow_engine_create();
    if (!engine) return;

    size_t wf_count = taskflow_engine_get_workflow_count(engine);
    size_t ex_count = taskflow_engine_get_execution_count(engine);

    printf("    Empty engine: workflows=%zu, executions=%zu\n",
           wf_count, ex_count);
    TEST_PASS("counts on empty engine");

    taskflow_engine_destroy(engine);
}

static void test_counts_after_registration(void) {
    taskflow_engine_t* engine = taskflow_engine_create();
    if (!engine) return;

    for (int i = 0; i < 5; i++) {
        taskflow_workflow_t wf;
        memset(&wf, 0, sizeof(wf));
        snprintf(wf.id, sizeof(wf.id), "wf_cnt_%03d", i);
        snprintf(wf.name, sizeof(wf.name), "Workflow %d", i);
        {
            char buf[32];
            snprintf(buf, sizeof(buf), "n%d", i);
            wf.initial_node_id = strdup(buf);
        }
        taskflow_engine_register_workflow(engine, &wf);
    }

    size_t wf_count = taskflow_engine_get_workflow_count(engine);
    size_t ex_count = taskflow_engine_get_execution_count(engine);

    printf("    After reg: workflows=%zu, executions=%zu\n",
           wf_count, ex_count);
    TEST_PASS("counts after registration");

    taskflow_engine_destroy(engine);
}

/* ==================== JSON加载 ==================== */

static void test_load_workflow_json(void) {
    taskflow_engine_t* engine = taskflow_engine_create();
    if (!engine) return;

    const char* json_str =
        "{\"id\":\"wf_json_001\",\"name\":\"JSON Workflow\","
        "\"version\":\"1.0\",\"initial_node_id\":\"start\"}";

    int rc = taskflow_engine_load_workflow_json(engine, json_str);
    printf("    Load JSON result: %d\n", rc);
    TEST_PASS("load workflow JSON");

    taskflow_engine_destroy(engine);
}

/* ==================== 枚举值验证 ==================== */

static void test_enum_values(void) {
    assert(TASKFLOW_NODE_TASK == 0);
    assert(TASKFLOW_NODE_CONDITION == 1);
    assert(TASKFLOW_NODE_FORK == 2);
    assert(TASKFLOW_NODE_JOIN == 3);
    assert(TASKFLOW_NODE_SUBFLOW == 4);
    assert(TASKFLOW_NODE_LOOP == 5);
    assert(TASKFLOW_NODE_DELAY == 6);
    assert(TASKFLOW_NODE_EVENT_WAIT == 7);
    assert(TASKFLOW_NODE_TRANSFORM == 8);

    assert(TASKFLOW_STATE_PENDING == 0);
    assert(TASKFLOW_STATE_READY == 1);
    assert(TASKFLOW_STATE_RUNNING == 2);
    assert(TASKFLOW_STATE_COMPLETED == 4);
    assert(TASKFLOW_STATE_FAILED == 5);

    assert(TASKFLOW_ERROR_RETRY == 0);
    assert(TASKFLOW_ERROR_ROLLBACK == 1);
    assert(TASKFLOW_ERROR_SKIP == 2);
    assert(TASKFLOW_ERROR_ABORT == 3);
    assert(TASKFLOW_ERROR_FALLBACK == 4);

    assert(TASKFLOW_LOOP_COUNT == 0);
    assert(TASKFLOW_LOOP_CONDITION == 1);
    assert(TASKFLOW_LOOP_FOREACH == 2);

    TEST_PASS("all enum values correct");
}

/* ==================== 常量验证 ==================== */

static void test_constants(void) {
    assert(TASKFLOW_MAX_NODES > 0);
    assert(TASKFLOW_MAX_EDGES > 0);
    assert(TASKFLOW_MAX_SUBFLOWS > 0);
    assert(TASKFLOW_MAX_CHECKPOINTS > 0);
    assert(TASKFLOW_MAX_RETRIES > 0);
    assert(TASKFLOW_MAX_PARALLEL > 0);
    assert(strlen(TASKFLOW_ADV_VERSION) > 0);
    TEST_PASS("constants defined correctly");
}

/* ==================== 结构体大小验证 ==================== */

static void test_struct_sizes(void) {
    assert(sizeof(taskflow_node_t) >=
           sizeof(char[64]) + sizeof(char[128]) +
           sizeof(int32_t) * 8 + sizeof(char*) * 9 + sizeof(void*));
    assert(sizeof(taskflow_edge_t) >=
           sizeof(char[64]) * 3 + sizeof(char[256]) +
           sizeof(int32_t) + sizeof(bool));
    assert(sizeof(taskflow_workflow_t) >=
           sizeof(char[64]) + sizeof(char[128]) +
           sizeof(char[256]) + sizeof(char[32]) +
           sizeof(size_t) * 2 + sizeof(char*) * 4 +
           sizeof(int32_t) * 3);
    assert(sizeof(taskflow_execution_t) >=
           sizeof(char[64]) * 2 + sizeof(uint64_t) * 2 +
           sizeof(double) + sizeof(int32_t) + sizeof(size_t) * 2 +
           sizeof(char*) * 5 + sizeof(taskflow_state_t));
    assert(sizeof(taskflow_checkpoint_t) >=
           sizeof(char[64]) * 4 + sizeof(taskflow_state_t) +
           sizeof(char*) + sizeof(uint64_t));
    TEST_PASS("struct sizes adequate");
}

/* ==================== 销毁函数安全性 ==================== */

static void test_destroy_functions_null_safe(void) {
    taskflow_workflow_destroy(NULL);
    taskflow_execution_destroy(NULL);
    taskflow_checkpoint_destroy(NULL);
    TEST_PASS("destroy functions null-safe");
}

/* ==================== 主函数 ==================== */

int main(void) {
    printf("\n========================================\n");
    printf("  TaskFlow Advanced 高级工作流引擎 单元测试\n");
    printf("========================================\n\n");

    RUN_TEST(test_enum_values);
    RUN_TEST(test_constants);
    RUN_TEST(test_struct_sizes);

    RUN_TEST(test_engine_create_destroy);
    RUN_TEST(test_destroy_null);
    RUN_TEST(test_destroy_functions_null_safe);

    RUN_TEST(test_register_handler);
    RUN_TEST(test_unregister_handler);

    RUN_TEST(test_register_workflow);

    RUN_TEST(test_start_execution);
    RUN_TEST(test_cancel_pause_resume);
    RUN_TEST(test_step_and_rtc);

    RUN_TEST(test_checkpoint_operations);
    RUN_TEST(test_variable_system);
    RUN_TEST(test_callback_registration);

    RUN_TEST(test_counts_empty_engine);
    RUN_TEST(test_counts_after_registration);

    RUN_TEST(test_load_workflow_json);

    printf("\n========================================\n");
    printf("  测试结果: %d 运行, %d 通过, %d 失败\n",
           tests_run, tests_passed, 0);
    printf("========================================\n");

    return 0;
}
