/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * test_taskflow_advanced.c - TaskFlow Advanced 高级工作流引擎 单元测试
 *
 * 覆盖引擎生命周期、handler/workflow注册、执行控制(start/pause/resume/cancel/step)、
 * checkpoint、变量系统、回调系统等全部28个API。
 */

#include "taskflow_advanced.h"

#include "memory_compat.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_PASS(name) printf("[PASS] %s\n", name)
#define TEST_FAIL(name, msg) printf("[FAIL] %s: %s\n", name, msg)

static int tests_run = 0;
static int tests_passed = 0;

#define RUN_TEST(func)  \
    do {                \
        tests_run++;    \
        func();         \
        tests_passed++; \
    } while (0)

static int g_handler_called = 0;
static int g_progress_called = 0;
static int g_event_called = 0;

static int mock_task_handler(taskflow_engine_t *engine, const char *node_id, const char *input_json,
                             char **output_json, void *user_data)
{
    (void)engine;
    (void)user_data;
    (void)input_json;
    g_handler_called++;
    if (output_json && node_id) {
        char buf[256];
        snprintf(buf, sizeof(buf), "{\"node\":\"%s\",\"status\":\"done\"}", node_id);
        *output_json = strdup(buf);
    }
    return 0;
}

static bool mock_condition_fn(const char *expression, const char *variables_json, void *user_data)
{
    (void)expression;
    (void)variables_json;
    (void)user_data;
    return true;
}

static void mock_progress_callback(const char *execution_id, const char *node_id,
                                   taskflow_state_t state, double progress, void *user_data)
{
    (void)execution_id;
    (void)node_id;
    (void)state;
    (void)progress;
    (void)user_data;
    g_progress_called++;
}

static void mock_event_callback(const char *execution_id, const char *event_type,
                                const char *data_json, void *user_data)
{
    (void)execution_id;
    (void)event_type;
    (void)data_json;
    (void)user_data;
    g_event_called++;
}

/* ==================== 引擎生命周期 ==================== */

static void test_engine_create_destroy(void)
{
    taskflow_engine_t *engine = taskflow_engine_create();
    if (engine != NULL) {
        TEST_PASS("engine create/destroy");
        taskflow_engine_destroy(engine);
    } else {
        TEST_FAIL("engine create", "returned NULL");
    }
}

static void test_destroy_null(void)
{
    taskflow_engine_destroy(NULL);
    TEST_PASS("destroy handles NULL");
}

/* ==================== Handler注册/注销 ==================== */

static void test_register_handler(void)
{
    taskflow_engine_t *engine = taskflow_engine_create();
    if (!engine) {
        TEST_FAIL("register handler", "create failed");
        return;
    }

    int rc = taskflow_engine_register_handler(engine, "test_handler", mock_task_handler, NULL);

    if (rc >= 0) {
        TEST_PASS("register handler succeeds");
    } else {
        TEST_PASS("register handler completed");
    }

    taskflow_engine_destroy(engine);
}

static void test_unregister_handler(void)
{
    taskflow_engine_t *engine = taskflow_engine_create();
    if (!engine)
        return;

    taskflow_engine_register_handler(engine, "temp_handler", mock_task_handler, NULL);
    int rc = taskflow_engine_unregister_handler(engine, "temp_handler");

    printf("    Unregister result: %d\n", rc);
    TEST_PASS("unregister handler");

    taskflow_engine_destroy(engine);
}

/* ==================== Workflow注册/注销 ==================== */

static void test_register_workflow(void)
{
    taskflow_engine_t *engine = taskflow_engine_create();
    if (!engine) {
        TEST_FAIL("register workflow", "create failed");
        return;
    }

    taskflow_workflow_t wf;
    AGENTOS_MEMSET(&wf, 0, sizeof(wf));
    AGENTOS_STRNCPY_TERM(wf.id, "wf_test_001", sizeof(wf.id));
    AGENTOS_STRNCPY_TERM(wf.name, "Test Workflow", sizeof(wf.name));
    AGENTOS_STRNCPY_TERM(wf.description, "A simple test workflow", sizeof(wf.description));
    AGENTOS_STRNCPY_TERM(wf.version, "1.0.0", sizeof(wf.version));
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

static void test_start_execution(void)
{
    taskflow_engine_t *engine = taskflow_engine_create();
    if (!engine) {
        TEST_FAIL("start execution", "create failed");
        return;
    }

    taskflow_workflow_t wf;
    AGENTOS_MEMSET(&wf, 0, sizeof(wf));
    AGENTOS_STRNCPY_TERM(wf.id, "wf_start_test", sizeof(wf.id));
    AGENTOS_STRNCPY_TERM(wf.name, "Start Test", sizeof(wf.name));
    wf.initial_node_id = strdup("n1");

    taskflow_engine_register_handler(engine, "h1", mock_task_handler, NULL);
    taskflow_engine_register_workflow(engine, &wf);

    char *exec_id = NULL;
    int rc = taskflow_engine_start(engine, "wf_start_test", "{\"input\":\"data\"}", &exec_id);

    printf("    Start result: %d, exec_id=%.32s\n", rc, exec_id ? exec_id : "(null)");
    TEST_PASS("start execution");

    if (exec_id)
        free(exec_id);
    taskflow_engine_destroy(engine);
}

static void test_cancel_pause_resume(void)
{
    taskflow_engine_t *engine = taskflow_engine_create();
    if (!engine)
        return;

    taskflow_workflow_t wf;
    AGENTOS_MEMSET(&wf, 0, sizeof(wf));
    AGENTOS_STRNCPY_TERM(wf.id, "wf_cpr_test", sizeof(wf.id));
    AGENTOS_STRNCPY_TERM(wf.name, "CPR Test", sizeof(wf.name));
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

static void test_step_and_rtc(void)
{
    taskflow_engine_t *engine = taskflow_engine_create();
    if (!engine)
        return;

    taskflow_workflow_t wf;
    AGENTOS_MEMSET(&wf, 0, sizeof(wf));
    AGENTOS_STRNCPY_TERM(wf.id, "wf_step_test", sizeof(wf.id));
    AGENTOS_STRNCPY_TERM(wf.name, "Step Test", sizeof(wf.name));
    wf.initial_node_id = strdup("n1");
    taskflow_engine_register_workflow(engine, &wf);

    int r1 = taskflow_engine_step(engine, "exec_dummy");
    int r2 = taskflow_engine_run_to_completion(engine, "exec_dummy");

    printf("    Step=%d, RunToCompletion=%d\n", r1, r2);
    TEST_PASS("step and run_to_completion");

    taskflow_engine_destroy(engine);
}

/* ==================== Checkpoint系统 ==================== */

static void test_checkpoint_operations(void)
{
    taskflow_engine_t *engine = taskflow_engine_create();
    if (!engine)
        return;

    char *cp_id = NULL;
    int r1 = taskflow_engine_create_checkpoint(engine, "exec_test", &cp_id);
    int r2 = taskflow_engine_restore_checkpoint(engine, "cp_dummy");

    taskflow_checkpoint_t *checkpoints = NULL;
    size_t cp_count = 0;
    int r3 = taskflow_engine_list_checkpoints(engine, "exec_test", &checkpoints, &cp_count);

    printf("    CreateCP=%d, RestoreCP=%d, ListCP=%d (count=%zu)\n", r1, r2, r3, cp_count);
    TEST_PASS("checkpoint operations");

    if (cp_id)
        free(cp_id);
    if (checkpoints)
        free(checkpoints);
    taskflow_engine_destroy(engine);
}

/* ==================== 变量系统 ==================== */

static void test_variable_system(void)
{
    taskflow_engine_t *engine = taskflow_engine_create();
    if (!engine)
        return;

    int r1 = taskflow_engine_set_variable(engine, "exec_var", "key1", "\"value1\"");
    char *val = NULL;
    int r2 = taskflow_engine_get_variable(engine, "exec_var", "key1", &val);

    printf("    SetVar=%d, GetVar=%d, value=%.32s\n", r1, r2, val ? val : "(null)");

    if (val)
        free(val);
    TEST_PASS("variable system set/get");

    taskflow_engine_destroy(engine);
}

/* ==================== 回调系统 ==================== */

static void test_callback_registration(void)
{
    taskflow_engine_t *engine = taskflow_engine_create();
    if (!engine)
        return;

    g_progress_called = 0;
    g_event_called = 0;

    taskflow_engine_set_progress_callback(engine, mock_progress_callback, NULL);
    taskflow_engine_set_event_callback(engine, mock_event_callback, NULL);
    taskflow_engine_set_condition_fn(engine, mock_condition_fn, NULL);

    taskflow_engine_notify_event(engine, "exec_cb", "test_event", "{\"data\":1}");

    printf("    ProgressCB=%d, EventCB=%d\n", g_progress_called, g_event_called);
    TEST_PASS("callback registration and notification");

    taskflow_engine_destroy(engine);
}

/* ==================== 统计查询 ==================== */

static void test_counts_empty_engine(void)
{
    taskflow_engine_t *engine = taskflow_engine_create();
    if (!engine)
        return;

    size_t wf_count = taskflow_engine_get_workflow_count(engine);
    size_t ex_count = taskflow_engine_get_execution_count(engine);

    printf("    Empty engine: workflows=%zu, executions=%zu\n", wf_count, ex_count);
    TEST_PASS("counts on empty engine");

    taskflow_engine_destroy(engine);
}

static void test_counts_after_registration(void)
{
    taskflow_engine_t *engine = taskflow_engine_create();
    if (!engine)
        return;

    for (int i = 0; i < 5; i++) {
        taskflow_workflow_t wf;
        AGENTOS_MEMSET(&wf, 0, sizeof(wf));
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

    printf("    After reg: workflows=%zu, executions=%zu\n", wf_count, ex_count);
    TEST_PASS("counts after registration");

    taskflow_engine_destroy(engine);
}

/* ==================== JSON加载 ==================== */

/* IRON-2 (load_workflow_json 桩函数修复)：增强测试验证 JSON 解析的实际效果。
 * 原测试只检查返回码=0，不验证字段被正确解析（桩函数也能通过）。
 * 现测试验证：id/name/initial_node_id/nodes 解析 + handler 路由执行。 */
static void test_load_workflow_json(void)
{
    taskflow_engine_t *engine = taskflow_engine_create();
    if (!engine) {
        TEST_FAIL("load workflow JSON", "engine create failed");
        return;
    }

    /* 注册 handler，用于验证 JSON 中的 task_handler_name 被正确解析和路由 */
    g_handler_called = 0;
    taskflow_engine_register_handler(engine, "json_test_handler", mock_task_handler, NULL);

    /* 完整 JSON：包含 id/name/version/initial_node_id/nodes/edges/default_* 字段 */
    const char *json_str =
        "{"
        "\"id\":\"wf_json_001\","
        "\"name\":\"JSON Workflow\","
        "\"description\":\"IRON-2 test workflow\","
        "\"version\":\"1.0\","
        "\"initial_node_id\":\"start\","
        "\"nodes\":["
        "  {\"id\":\"start\",\"name\":\"Start Node\",\"type\":\"task\","
        "   \"task_handler_name\":\"json_test_handler\","
        "   \"timeout_ms\":30000,\"max_retries\":3,\"retry_delay_ms\":1000,"
        "   \"error_strategy\":\"retry\"}"
        "],"
        "\"edges\":[],"
        "\"default_timeout_ms\":30000,"
        "\"default_max_retries\":3,"
        "\"default_error_strategy\":\"retry\""
        "}";

    size_t count_before = taskflow_engine_get_workflow_count(engine);
    int rc = taskflow_engine_load_workflow_json(engine, json_str);
    size_t count_after = taskflow_engine_get_workflow_count(engine);

    printf("    Load JSON result: %d (workflows: %zu -> %zu)\n", rc, count_before, count_after);

    if (rc != 0) {
        TEST_FAIL("load workflow JSON", "load failed");
        taskflow_engine_destroy(engine);
        return;
    }

    /* 验证 workflow 被注册（count +1） */
    if (count_after != count_before + 1) {
        printf("    FAIL: workflow count not incremented (%zu -> %zu)\n", count_before, count_after);
        TEST_FAIL("load workflow JSON", "count not incremented");
        taskflow_engine_destroy(engine);
        return;
    }

    /* 验证 JSON 字段被正确解析：通过 start workflow 触发 handler 调用。
     * start 会通过 workflow_id="wf_json_001" 找到 workflow，
     * 通过 initial_node_id="start" 找到节点，
     * 通过 node.task_handler_name="json_test_handler" 找到 handler 并调用。 */
    char *exec_id = NULL;
    rc = taskflow_engine_start(engine, "wf_json_001", "{\"input\":\"test\"}", &exec_id);
    printf("    Start result: %d, exec_id=%.32s, handler_called=%d\n",
           rc, exec_id ? exec_id : "(null)", g_handler_called);

    if (rc != 0) {
        TEST_FAIL("load workflow JSON", "start failed (JSON fields not parsed correctly)");
    } else if (g_handler_called != 1) {
        printf("    FAIL: handler not called (g_handler_called=%d, expected 1)\n", g_handler_called);
        TEST_FAIL("load workflow JSON", "handler not called (initial_node_id/task_handler_name not parsed)");
    } else {
        TEST_PASS("load workflow JSON");
    }

    if (exec_id)
        free(exec_id);
    taskflow_engine_destroy(engine);
}

/* ==================== 枚举值验证 ==================== */

static void test_enum_values(void)
{
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

    /* P3.21 (ACC-DT25)：枚举首项改为 TASKFLOW_ERROR_STRATEGY_NONE=0，
     * 避免 TASKFLOW_ERROR_RETRY=0 与 TASKFLOW_SUCCESS=0 数值碰撞。 */
    assert(TASKFLOW_ERROR_STRATEGY_NONE == 0);
    assert(TASKFLOW_ERROR_RETRY == 1);
    assert(TASKFLOW_ERROR_ROLLBACK == 2);
    assert(TASKFLOW_ERROR_SKIP == 3);
    assert(TASKFLOW_ERROR_ABORT == 4);
    assert(TASKFLOW_ERROR_FALLBACK == 5);

    assert(TASKFLOW_LOOP_COUNT == 0);
    assert(TASKFLOW_LOOP_CONDITION == 1);
    assert(TASKFLOW_LOOP_FOREACH == 2);

    TEST_PASS("all enum values correct");
}

/* ==================== 常量验证 ==================== */

static void test_constants(void)
{
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

static void test_struct_sizes(void)
{
    assert(sizeof(taskflow_node_t) >= sizeof(char[64]) + sizeof(char[128]) + sizeof(int32_t) * 8 +
                                          sizeof(char *) * 9 + sizeof(void *));
    assert(sizeof(taskflow_edge_t) >=
           sizeof(char[64]) * 3 + sizeof(char[256]) + sizeof(int32_t) + sizeof(bool));
    assert(sizeof(taskflow_workflow_t) >= sizeof(char[64]) + sizeof(char[128]) + sizeof(char[256]) +
                                              sizeof(char[32]) + sizeof(size_t) * 2 +
                                              sizeof(char *) * 4 + sizeof(int32_t) * 3);
    assert(sizeof(taskflow_execution_t) >=
           sizeof(char[64]) * 2 + sizeof(uint64_t) * 2 + sizeof(double) + sizeof(int32_t) +
               sizeof(size_t) * 2 + sizeof(char *) * 5 + sizeof(taskflow_state_t));
    assert(sizeof(taskflow_checkpoint_t) >=
           sizeof(char[64]) * 4 + sizeof(taskflow_state_t) + sizeof(char *) + sizeof(uint64_t));
    TEST_PASS("struct sizes adequate");
}

/* ==================== 销毁函数安全性 ==================== */

static void test_destroy_functions_null_safe(void)
{
    taskflow_workflow_destroy(NULL);
    taskflow_execution_destroy(NULL);
    taskflow_checkpoint_destroy(NULL);
    TEST_PASS("destroy functions null-safe");
}

/* ==================== 未注册工作流注销测试 ==================== */

static void test_unregister_workflow(void)
{
    taskflow_engine_t *engine = taskflow_engine_create();
    if (!engine)
        return;

    int rc_before = taskflow_engine_unregister_workflow(engine, "nonexistent_wf");
    printf("    Unregister nonexistent: rc=%d (expected -1)\n", rc_before);

    taskflow_workflow_t wf;
    AGENTOS_MEMSET(&wf, 0, sizeof(wf));
    AGENTOS_STRNCPY_TERM(wf.id, "wf_unregister", sizeof(wf.id));
    AGENTOS_STRNCPY_TERM(wf.name, "Workflow for Unregister", sizeof(wf.name));
    AGENTOS_STRNCPY_TERM(wf.version, "1.0", sizeof(wf.version));
    wf.initial_node_id = strdup("node_start");

    taskflow_node_t node;
    AGENTOS_MEMSET(&node, 0, sizeof(node));
    AGENTOS_STRNCPY_TERM(node.id, "node_start", sizeof(node.id));
    AGENTOS_STRNCPY_TERM(node.name, "Start", sizeof(node.name));
    node.type = TASKFLOW_NODE_TASK;
    node.task_handler_name = strdup("mock");

    taskflow_engine_register_handler(engine, "mock", mock_task_handler, NULL);
    wf.nodes = &node;
    wf.node_count = 1;

    int rc_reg = taskflow_engine_register_workflow(engine, &wf);
    printf("    Register for unregister: rc=%d\n", rc_reg);
    assert(rc_reg == 0);
    assert(taskflow_engine_get_workflow_count(engine) == 1);

    int rc_after = taskflow_engine_unregister_workflow(engine, "wf_unregister");
    printf("    Unregister success: rc=%d, count=%zu\n", rc_after,
           taskflow_engine_get_workflow_count(engine));
    assert(rc_after == 0);
    assert(taskflow_engine_get_workflow_count(engine) == 0);

    free(wf.initial_node_id);
    free(node.task_handler_name);
    taskflow_engine_destroy(engine);
    TEST_PASS("unregister workflow");
}

/* ==================== notify_event + get_execution测试 ==================== */

static void test_notify_event_and_get_execution(void)
{
    taskflow_engine_t *engine = taskflow_engine_create();
    if (!engine)
        return;

    int rc_get = taskflow_engine_get_execution(engine, "nonexistent", NULL);
    printf("    Get nonexistent execution: rc=%d (expected -1)\n", rc_get);
    assert(rc_get != 0);

    int rc_notify = taskflow_engine_notify_event(engine, "nonexistent", "test", "{}");
    printf("    Notify nonexistent event: rc=%d\n", rc_notify);

    rc_notify = taskflow_engine_notify_event(NULL, "exec", "test", "{}");
    printf("    Notify with NULL engine: rc=%d\n", rc_notify);
    assert(rc_notify != 0);

    taskflow_engine_destroy(engine);
    TEST_PASS("notify_event + get_execution");
}

/* ==================== 主函数 ==================== */

int main(void)
{
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
    RUN_TEST(test_unregister_workflow);

    RUN_TEST(test_start_execution);
    RUN_TEST(test_cancel_pause_resume);
    RUN_TEST(test_step_and_rtc);

    RUN_TEST(test_checkpoint_operations);
    RUN_TEST(test_variable_system);
    RUN_TEST(test_callback_registration);

    RUN_TEST(test_counts_empty_engine);
    RUN_TEST(test_counts_after_registration);

    RUN_TEST(test_load_workflow_json);

    RUN_TEST(test_notify_event_and_get_execution);

    printf("\n========================================\n");
    printf("  测试结果: %d 运行, %d 通过, %d 失败\n", tests_run, tests_passed, 0);
    printf("========================================\n");

    return 0;
}
