/**
 * @file test_dag_integration.c
 * @brief W18.3 (ACC-FOUND03): CoreLoopThree ↔ taskflow_advanced DAG 集成测试
 * @copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * 验证 W18 验收标准：coreloopthree 提交 DAG → taskflow_advanced 执行 → 结果回传。
 *
 * 测试内容：
 *   1. test_dag_basic — 基础 DAG 提交：创建 loop → 注册 handler → 提交单节点工作流
 *      → 等待完成 → 验证状态=completed + 结果 JSON 非空
 *   2. test_dag_status_query — 状态查询：提交后查询 status，验证状态字符串和进度
 *   3. test_dag_param_validation — 参数验证：NULL 参数返回 AGENTOS_EINVAL
 *   4. test_dag_handler_output — handler 输出回传：handler 生成 JSON → wait 返回相同 JSON
 *
 * 所有权说明（重要）：
 *   - taskflow_engine_register_workflow 是浅拷贝（复制 workflow 结构体值，包括 nodes/edges
 *     指针），taskflow_engine_destroy 会释放 nodes[j] 各字段 + nodes/edges 数组 +
 *     initial_node_id。因此 submit_dag 成功后，测试不应释放这些资源（由 loop destroy 间接
 *     通过 engine destroy 释放）。
 *   - out_execution_id / out_result_json / out_state 由调用者释放。
 *
 * @note 不使用 assert() 执行副作用操作（NDEBUG heisenbug 教训）。
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199309L
#endif

#include "agentos.h"
#include "error.h"
#include "loop.h"
#include "memory_compat.h"
#include "taskflow_advanced.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========== 测试统计 ========== */

static int tests_run = 0;
static int tests_passed = 0;

#define TEST_PASS(name)                        \
    do {                                       \
        printf("  [PASS] %s\n", name);         \
        tests_run++;                           \
        tests_passed++;                        \
    } while (0)

#define TEST_FAIL(name, msg)                                       \
    do {                                                           \
        printf("  [FAIL] %s: %s\n", name, msg);                   \
        tests_run++;                                               \
    } while (0)

#define RUN_TEST(func)                                             \
    do {                                                           \
        printf("[TEST] %s\n", #func);                              \
        func();                                                    \
    } while (0)

/* ========== mock handler ========== */

typedef struct {
    int call_count;
    char last_node_id[64];
    char last_input[256];
} handler_state_t;

static int mock_handler_success(taskflow_engine_t *engine, const char *node_id,
                                const char *input_json, char **output_json, void *user_data)
{
    (void)engine;
    handler_state_t *st = (handler_state_t *)user_data;
    if (st) {
        st->call_count++;
        AGENTOS_STRNCPY_TERM(st->last_node_id, node_id ? node_id : "", sizeof(st->last_node_id));
        AGENTOS_STRNCPY_TERM(st->last_input, input_json ? input_json : "", sizeof(st->last_input));
    }
    if (output_json) {
        char buf[256];
        snprintf(buf, sizeof(buf), "{\"node\":\"%s\",\"status\":\"ok\"}", node_id ? node_id : "");
        *output_json = AGENTOS_STRDUP(buf);
    }
    return 0;  /* 0 = 成功 */
}

/* ========== 辅助函数：构造单节点工作流 ========== *
 *
 * 注意：taskflow_engine_register_workflow 是浅拷贝，engine destroy 释放 nodes +
 * initial_node_id。因此此函数返回的 workflow 在 submit_dag 成功后不应被调用者释放
 * （由 engine destroy 负责）。仅在 submit_dag 失败时调用 dag_workflow_free。
 */
static void dag_workflow_init_single(taskflow_workflow_t *wf, const char *id,
                                     const char *name, const char *handler_name)
{
    AGENTOS_MEMSET(wf, 0, sizeof(*wf));
    AGENTOS_STRNCPY_TERM(wf->id, id, sizeof(wf->id));
    AGENTOS_STRNCPY_TERM(wf->name, name, sizeof(wf->name));
    AGENTOS_STRNCPY_TERM(wf->description, "W18 integration test workflow",
                         sizeof(wf->description));
    AGENTOS_STRNCPY_TERM(wf->version, "1.0.0", sizeof(wf->version));

    /* 分配单节点数组 — engine destroy 负责释放 */
    wf->nodes = (taskflow_node_t *)AGENTOS_CALLOC(1, sizeof(taskflow_node_t));
    if (!wf->nodes) return;
    wf->node_count = 1;

    AGENTOS_STRNCPY_TERM(wf->nodes[0].id, "node_start", sizeof(wf->nodes[0].id));
    AGENTOS_STRNCPY_TERM(wf->nodes[0].name, "Start Node", sizeof(wf->nodes[0].name));
    wf->nodes[0].type = TASKFLOW_NODE_TASK;
    wf->nodes[0].state = TASKFLOW_STATE_PENDING;
    wf->nodes[0].task_handler_name = AGENTOS_STRDUP(handler_name);
    wf->nodes[0].timeout_ms = 5000;
    wf->nodes[0].error_strategy = TASKFLOW_ERROR_STRATEGY_NONE;

    wf->initial_node_id = AGENTOS_STRDUP("node_start");
    wf->default_timeout_ms = 5000;
    wf->default_error_strategy = TASKFLOW_ERROR_STRATEGY_NONE;
}

/* ========== TEST 1: 基础 DAG 提交 ========== */

static void test_dag_basic(void)
{
    const char *test_name = "test_dag_basic";
    agentos_loop_config_t config;
    agentos_core_loop_t *loop = NULL;
    agentos_error_t err;
    handler_state_t hstate;

    AGENTOS_MEMSET(&config, 0, sizeof(config));
    config.loop_config_cognition_threads = 1;
    config.loop_config_execution_threads = 1;
    config.loop_config_memory_threads = 1;
    config.loop_config_max_queued_tasks = 10;

    err = agentos_loop_create(&config, &loop);
    if (err != AGENTOS_SUCCESS || !loop) {
        TEST_FAIL(test_name, "agentos_loop_create failed");
        return;
    }

    AGENTOS_MEMSET(&hstate, 0, sizeof(hstate));
    err = agentos_loop_dag_register_handler(loop, "h_basic", mock_handler_success, &hstate);
    if (err != AGENTOS_SUCCESS) {
        TEST_FAIL(test_name, "dag_register_handler failed");
        agentos_loop_destroy(loop);
        return;
    }

    taskflow_workflow_t wf;
    dag_workflow_init_single(&wf, "wf_basic_001", "Basic DAG Test", "h_basic");

    char *exec_id = NULL;
    err = agentos_loop_submit_dag(loop, &wf, "{\"input\":\"test_data\"}", &exec_id);
    if (err != AGENTOS_SUCCESS || !exec_id) {
        TEST_FAIL(test_name, "submit_dag failed");
        /* submit 失败，需自己释放 workflow 资源 */
        if (wf.nodes) {
            AGENTOS_FREE(wf.nodes[0].task_handler_name);
            AGENTOS_FREE(wf.nodes);
        }
        AGENTOS_FREE(wf.initial_node_id);
        agentos_loop_destroy(loop);
        return;
    }

    /* 等待执行完成 */
    char *result_json = NULL;
    err = agentos_loop_dag_wait(loop, exec_id, 0, &result_json);
    if (err != AGENTOS_SUCCESS) {
        TEST_FAIL(test_name, "dag_wait failed");
        AGENTOS_FREE(exec_id);
        AGENTOS_FREE(result_json);
        agentos_loop_destroy(loop);
        return;
    }

    /* 验证 handler 被调用 */
    if (hstate.call_count != 1) {
        char msg[128];
        snprintf(msg, sizeof(msg), "handler call_count=%d (expected 1)", hstate.call_count);
        TEST_FAIL(test_name, msg);
    } else if (!result_json || strstr(result_json, "node_start") == NULL) {
        TEST_FAIL(test_name, "result_json missing node_start");
    } else {
        TEST_PASS(test_name);
    }

    AGENTOS_FREE(exec_id);
    AGENTOS_FREE(result_json);
    agentos_loop_destroy(loop);
}

/* ========== TEST 2: 状态查询 ========== */

static void test_dag_status_query(void)
{
    const char *test_name = "test_dag_status_query";
    agentos_loop_config_t config;
    agentos_core_loop_t *loop = NULL;
    agentos_error_t err;

    AGENTOS_MEMSET(&config, 0, sizeof(config));
    config.loop_config_cognition_threads = 1;
    config.loop_config_execution_threads = 1;
    config.loop_config_memory_threads = 1;
    config.loop_config_max_queued_tasks = 10;

    err = agentos_loop_create(&config, &loop);
    if (err != AGENTOS_SUCCESS || !loop) {
        TEST_FAIL(test_name, "agentos_loop_create failed");
        return;
    }

    agentos_loop_dag_register_handler(loop, "h_status", mock_handler_success, NULL);

    taskflow_workflow_t wf;
    dag_workflow_init_single(&wf, "wf_status_001", "Status Query Test", "h_status");

    char *exec_id = NULL;
    err = agentos_loop_submit_dag(loop, &wf, NULL, &exec_id);
    if (err != AGENTOS_SUCCESS || !exec_id) {
        TEST_FAIL(test_name, "submit_dag failed");
        if (wf.nodes) {
            AGENTOS_FREE(wf.nodes[0].task_handler_name);
            AGENTOS_FREE(wf.nodes);
        }
        AGENTOS_FREE(wf.initial_node_id);
        agentos_loop_destroy(loop);
        return;
    }

    /* 等待完成 */
    char *result = NULL;
    agentos_loop_dag_wait(loop, exec_id, 0, &result);
    AGENTOS_FREE(result);

    /* 查询状态 */
    char *state = NULL;
    double progress = -1.0;
    err = agentos_loop_dag_status(loop, exec_id, &state, &progress);
    if (err != AGENTOS_SUCCESS) {
        TEST_FAIL(test_name, "dag_status failed");
    } else if (!state) {
        TEST_FAIL(test_name, "state is NULL");
    } else if (strcmp(state, "completed") != 0) {
        char msg[128];
        snprintf(msg, sizeof(msg), "state=%s (expected completed)", state);
        TEST_FAIL(test_name, msg);
    } else if (progress < 0.99 || progress > 1.01) {
        char msg[128];
        snprintf(msg, sizeof(msg), "progress=%.2f (expected ~1.0)", progress);
        TEST_FAIL(test_name, msg);
    } else {
        TEST_PASS(test_name);
    }

    AGENTOS_FREE(state);
    AGENTOS_FREE(exec_id);
    agentos_loop_destroy(loop);
}

/* ========== TEST 3: 参数验证 ========== */

static void test_dag_param_validation(void)
{
    const char *test_name = "test_dag_param_validation";
    agentos_loop_config_t config;
    agentos_core_loop_t *loop = NULL;
    agentos_error_t err;
    int failures = 0;

    AGENTOS_MEMSET(&config, 0, sizeof(config));
    config.loop_config_cognition_threads = 1;
    config.loop_config_execution_threads = 1;
    config.loop_config_memory_threads = 1;
    config.loop_config_max_queued_tasks = 10;

    err = agentos_loop_create(&config, &loop);
    if (err != AGENTOS_SUCCESS || !loop) {
        TEST_FAIL(test_name, "agentos_loop_create failed");
        return;
    }

    /* submit_dag NULL loop */
    char *exec_id = NULL;
    err = agentos_loop_submit_dag(NULL, NULL, NULL, &exec_id);
    if (err != AGENTOS_EINVAL) {
        failures++;
        printf("    expected EINV for NULL loop, got %d\n", (int)err);
    }

    /* submit_dag NULL workflow */
    err = agentos_loop_submit_dag(loop, NULL, NULL, &exec_id);
    if (err != AGENTOS_EINVAL) {
        failures++;
        printf("    expected EINV for NULL workflow, got %d\n", (int)err);
    }

    /* register_handler NULL loop */
    err = agentos_loop_dag_register_handler(NULL, "x", mock_handler_success, NULL);
    if (err != AGENTOS_EINVAL) {
        failures++;
        printf("    expected EINV for NULL loop in register_handler, got %d\n", (int)err);
    }

    /* register_handler NULL name */
    err = agentos_loop_dag_register_handler(loop, NULL, mock_handler_success, NULL);
    if (err != AGENTOS_EINVAL) {
        failures++;
        printf("    expected EINV for NULL name, got %d\n", (int)err);
    }

    /* dag_wait NULL loop */
    err = agentos_loop_dag_wait(NULL, "x", 0, NULL);
    if (err != AGENTOS_EINVAL) {
        failures++;
        printf("    expected EINV for NULL loop in wait, got %d\n", (int)err);
    }

    /* dag_status NULL loop */
    err = agentos_loop_dag_status(NULL, "x", NULL, NULL);
    if (err != AGENTOS_EINVAL) {
        failures++;
        printf("    expected EINV for NULL loop in status, got %d\n", (int)err);
    }

    if (failures == 0) {
        TEST_PASS(test_name);
    } else {
        char msg[128];
        snprintf(msg, sizeof(msg), "%d parameter validation failures", failures);
        TEST_FAIL(test_name, msg);
    }

    agentos_loop_destroy(loop);
}

/* ========== TEST 4: handler 输出回传 ========== */

static void test_dag_handler_output(void)
{
    const char *test_name = "test_dag_handler_output";
    agentos_loop_config_t config;
    agentos_core_loop_t *loop = NULL;
    agentos_error_t err;

    AGENTOS_MEMSET(&config, 0, sizeof(config));
    config.loop_config_cognition_threads = 1;
    config.loop_config_execution_threads = 1;
    config.loop_config_memory_threads = 1;
    config.loop_config_max_queued_tasks = 10;

    err = agentos_loop_create(&config, &loop);
    if (err != AGENTOS_SUCCESS || !loop) {
        TEST_FAIL(test_name, "agentos_loop_create failed");
        return;
    }

    agentos_loop_dag_register_handler(loop, "h_output", mock_handler_success, NULL);

    taskflow_workflow_t wf;
    dag_workflow_init_single(&wf, "wf_output_001", "Handler Output Test", "h_output");

    char *exec_id = NULL;
    err = agentos_loop_submit_dag(loop, &wf, "{\"task\":\"generate_output\"}", &exec_id);
    if (err != AGENTOS_SUCCESS || !exec_id) {
        TEST_FAIL(test_name, "submit_dag failed");
        if (wf.nodes) {
            AGENTOS_FREE(wf.nodes[0].task_handler_name);
            AGENTOS_FREE(wf.nodes);
        }
        AGENTOS_FREE(wf.initial_node_id);
        agentos_loop_destroy(loop);
        return;
    }

    char *result_json = NULL;
    err = agentos_loop_dag_wait(loop, exec_id, 0, &result_json);
    if (err != AGENTOS_SUCCESS) {
        TEST_FAIL(test_name, "dag_wait failed");
        AGENTOS_FREE(exec_id);
        agentos_loop_destroy(loop);
        return;
    }

    /* 验证 handler 输出通过 wait 回传：
     * mock_handler_success 生成 {"node":"node_start","status":"ok"} */
    if (!result_json) {
        TEST_FAIL(test_name, "result_json is NULL");
    } else if (strstr(result_json, "node_start") == NULL) {
        char msg[256];
        snprintf(msg, sizeof(msg), "result_json missing node_start: %.128s", result_json);
        TEST_FAIL(test_name, msg);
    } else if (strstr(result_json, "ok") == NULL) {
        char msg[256];
        snprintf(msg, sizeof(msg), "result_json missing status ok: %.128s", result_json);
        TEST_FAIL(test_name, msg);
    } else {
        TEST_PASS(test_name);
    }

    AGENTOS_FREE(exec_id);
    AGENTOS_FREE(result_json);
    agentos_loop_destroy(loop);
}

/* ========== main ========== */

int main(void)
{
    printf("=== W18.3 CoreLoopThree DAG Integration Tests ===\n");

    RUN_TEST(test_dag_basic);
    RUN_TEST(test_dag_status_query);
    RUN_TEST(test_dag_param_validation);
    RUN_TEST(test_dag_handler_output);

    printf("\n=== Summary: %d/%d passed ===\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
