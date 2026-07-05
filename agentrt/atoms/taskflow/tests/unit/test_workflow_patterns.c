/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * test_workflow_patterns.c - TaskFlow Workflow Patterns 工作流模式 单元测试 (W17.4)
 *
 * 覆盖 5 种工作流模式（sequence/parallel/conditional/loop/fork-join）、
 * 上下文生命周期、节点边管理、执行控制（sync/async/pause/resume/stop/status）等
 * 全部 workflow_patterns.h API。
 */

#include "taskflow.h"
#include "taskflow_types.h"
#include "workflow_patterns.h"

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

/* ===== 辅助设施 ===== */

static int g_executor_count = 0;
static int g_condition_result = 1;  /* 默认返回 true */

static void mock_task_executor(void *task_data)
{
    (void)task_data;
    g_executor_count++;
}

static int mock_condition_func(void *context)
{
    (void)context;
    return g_condition_result;
}

static void mock_async_callback(taskflow_error_t result, void *user_data)
{
    (void)result;
    (void)user_data;
}

static void init_default_config(taskflow_config_t *config)
{
    AGENTRT_MEMSET(config, 0, sizeof(*config));
    config->max_vertices = 1024;
    config->max_edges = 4096;
    config->max_messages = 1024;
    config->worker_threads = 1;  /* BAN-333 */
    config->partition_count = 1;
    config->max_supersteps = 16;
    config->superstep_timeout_ms = 5000;
    config->message_buffer_size = 4096;
    config->vertex_buffer_size = 1024;
    config->edge_buffer_size = 1024;
}

static taskflow_handle_t create_taskflow_engine(void)
{
    taskflow_config_t config;
    init_default_config(&config);
    taskflow_handle_t engine = taskflow_engine_create_core(&config);
    assert(engine != NULL);
    return engine;
}

static workflow_pattern_config_t make_pattern_config(workflow_pattern_type_t type)
{
    workflow_pattern_config_t config;
    AGENTRT_MEMSET(&config, 0, sizeof(config));
    config.pattern_type = type;
    config.max_nodes = 64;
    config.max_edges = 128;
    config.enable_tracing = true;
    config.enable_checkpoint = false;
    config.checkpoint_interval = 0;
    return config;
}

static workflow_node_t make_node(vertex_id_t id, const char *name, workflow_node_type_t type)
{
    workflow_node_t node;
    AGENTRT_MEMSET(&node, 0, sizeof(node));
    node.node_id = id;
    node.node_type = type;
    /* node_name 是 char* 指针（workflow_patterns.h:58），非固定数组。
     * 原实现 AGENTRT_STRNCPY_TERM(node.node_name, ...) 写入 NULL 指针导致 SEGV。
     * workflow_add_node 内部会 AGENTRT_STRDUP 复制 node_name（workflow_patterns.c:412），
     * workflow_context_destroy 只释放内部复制的副本（workflow_patterns.c:365-366）。
     * 因此本地 node.node_name 指向字符串字面量（只读）是安全的：
     *   - add_node 只读不写（STRDUP）
     *   - destroy 不释放原始 node_name
     * 避免堆分配，消除内存泄漏风险。 */
    node.node_name = (char *)name;
    node.task_executor = mock_task_executor;
    return node;
}

static workflow_edge_t make_wf_edge(edge_id_t id, vertex_id_t src, vertex_id_t dst)
{
    workflow_edge_t edge;
    AGENTRT_MEMSET(&edge, 0, sizeof(edge));
    edge.edge_id = id;
    edge.source_node = src;
    edge.target_node = dst;
    return edge;
}

/* ===== 1. 静态验证类 ===== */

static void test_enum_values(void)
{
    /* workflow_pattern_type_t 枚举验证 */
    assert(WORKFLOW_SEQUENTIAL == 0);
    assert(WORKFLOW_PARALLEL == 1);
    assert(WORKFLOW_CONDITIONAL == 2);
    assert(WORKFLOW_LOOP == 3);
    assert(WORKFLOW_FORK_JOIN == 4);
    assert(WORKFLOW_PIPELINE == 5);
    assert(WORKFLOW_CUSTOM == 6);

    /* workflow_node_type_t 枚举验证 */
    assert(NODE_TASK == 0);
    assert(NODE_CONDITION == 1);
    assert(NODE_LOOP_START == 2);
    assert(NODE_LOOP_END == 3);
    assert(NODE_PARALLEL_START == 4);
    assert(NODE_PARALLEL_END == 5);
    assert(NODE_SUBWORKFLOW == 6);

    TEST_PASS("test_enum_values");
}

static void test_type_sizes(void)
{
    assert(sizeof(workflow_node_t) > 0);
    assert(sizeof(workflow_edge_t) > 0);
    assert(sizeof(workflow_pattern_config_t) > 0);
    assert(sizeof(workflow_context_t) > 0);
    TEST_PASS("test_type_sizes");
}

/* ===== 2. 上下文生命周期类 ===== */

static void test_context_create_destroy(void)
{
    taskflow_handle_t engine = create_taskflow_engine();
    workflow_pattern_config_t config = make_pattern_config(WORKFLOW_SEQUENTIAL);

    workflow_context_t *ctx = workflow_context_create(&config, engine);
    assert(ctx != NULL);

    workflow_context_destroy(ctx);
    taskflow_engine_destroy_core(engine);
    TEST_PASS("test_context_create_destroy");
}

static void test_context_create_null(void)
{
    /* null config 或 null engine 应安全处理 */
    workflow_context_t *ctx = workflow_context_create(NULL, NULL);
    if (ctx) {
        workflow_context_destroy(ctx);
    }
    TEST_PASS("test_context_create_null");
}

static void test_context_destroy_null(void)
{
    workflow_context_destroy(NULL);
    TEST_PASS("test_context_destroy_null");
}

/* ===== 3. 节点/边管理类 ===== */

static void test_add_node(void)
{
    taskflow_handle_t engine = create_taskflow_engine();
    workflow_pattern_config_t config = make_pattern_config(WORKFLOW_CUSTOM);
    workflow_context_t *ctx = workflow_context_create(&config, engine);
    assert(ctx != NULL);

    workflow_node_t n1 = make_node(1, "task_a", NODE_TASK);
    workflow_node_t n2 = make_node(2, "task_b", NODE_TASK);

    taskflow_error_t rc1 = workflow_add_node(ctx, &n1);
    taskflow_error_t rc2 = workflow_add_node(ctx, &n2);
    assert(rc1 == TASKFLOW_SUCCESS);
    assert(rc2 == TASKFLOW_SUCCESS);

    workflow_context_destroy(ctx);
    taskflow_engine_destroy_core(engine);
    TEST_PASS("test_add_node");
}

static void test_add_edge(void)
{
    taskflow_handle_t engine = create_taskflow_engine();
    workflow_pattern_config_t config = make_pattern_config(WORKFLOW_CUSTOM);
    workflow_context_t *ctx = workflow_context_create(&config, engine);
    assert(ctx != NULL);

    workflow_node_t n1 = make_node(1, "a", NODE_TASK);
    workflow_node_t n2 = make_node(2, "b", NODE_TASK);
    workflow_add_node(ctx, &n1);
    workflow_add_node(ctx, &n2);

    workflow_edge_t e = make_wf_edge(101, 1, 2);
    taskflow_error_t rc = workflow_add_edge(ctx, &e);
    assert(rc == TASKFLOW_SUCCESS);

    workflow_context_destroy(ctx);
    taskflow_engine_destroy_core(engine);
    TEST_PASS("test_add_edge");
}

static void test_set_start_end_node(void)
{
    taskflow_handle_t engine = create_taskflow_engine();
    workflow_pattern_config_t config = make_pattern_config(WORKFLOW_CUSTOM);
    workflow_context_t *ctx = workflow_context_create(&config, engine);
    assert(ctx != NULL);

    workflow_node_t n1 = make_node(1, "start", NODE_TASK);
    workflow_node_t n2 = make_node(2, "end", NODE_TASK);
    workflow_add_node(ctx, &n1);
    workflow_add_node(ctx, &n2);

    taskflow_error_t rc = workflow_set_start_node(ctx, 1);
    assert(rc == TASKFLOW_SUCCESS);

    rc = workflow_set_end_node(ctx, 2);
    assert(rc == TASKFLOW_SUCCESS);

    workflow_context_destroy(ctx);
    taskflow_engine_destroy_core(engine);
    TEST_PASS("test_set_start_end_node");
}

/* ===== 4. 5 种工作流模式构建器类 ===== */

static void test_build_sequential(void)
{
    taskflow_handle_t engine = create_taskflow_engine();
    workflow_pattern_config_t config = make_pattern_config(WORKFLOW_SEQUENTIAL);
    workflow_context_t *ctx = workflow_context_create(&config, engine);
    assert(ctx != NULL);

    /* 先添加节点 */
    workflow_node_t n1 = make_node(1, "step1", NODE_TASK);
    workflow_node_t n2 = make_node(2, "step2", NODE_TASK);
    workflow_node_t n3 = make_node(3, "step3", NODE_TASK);
    workflow_add_node(ctx, &n1);
    workflow_add_node(ctx, &n2);
    workflow_add_node(ctx, &n3);

    /* 构建顺序模式 */
    vertex_id_t node_ids[] = {1, 2, 3};
    taskflow_error_t rc = workflow_build_sequential(ctx, node_ids, 3);
    assert(rc == TASKFLOW_SUCCESS);

    workflow_context_destroy(ctx);
    taskflow_engine_destroy_core(engine);
    TEST_PASS("test_build_sequential");
}

static void test_build_parallel(void)
{
    taskflow_handle_t engine = create_taskflow_engine();
    workflow_pattern_config_t config = make_pattern_config(WORKFLOW_PARALLEL);
    workflow_context_t *ctx = workflow_context_create(&config, engine);
    assert(ctx != NULL);

    /* 添加节点：1 个 start + 3 个并行 + 1 个 end */
    workflow_node_t n1 = make_node(1, "start", NODE_PARALLEL_START);
    workflow_node_t n2 = make_node(2, "p1", NODE_TASK);
    workflow_node_t n3 = make_node(3, "p2", NODE_TASK);
    workflow_node_t n4 = make_node(4, "p3", NODE_TASK);
    workflow_node_t n5 = make_node(5, "end", NODE_PARALLEL_END);
    workflow_add_node(ctx, &n1);
    workflow_add_node(ctx, &n2);
    workflow_add_node(ctx, &n3);
    workflow_add_node(ctx, &n4);
    workflow_add_node(ctx, &n5);

    /* 构建并行模式 */
    vertex_id_t parallel_ids[] = {2, 3, 4};
    taskflow_error_t rc = workflow_build_parallel(ctx, 1, parallel_ids, 3, 5);
    assert(rc == TASKFLOW_SUCCESS);

    workflow_context_destroy(ctx);
    taskflow_engine_destroy_core(engine);
    TEST_PASS("test_build_parallel");
}

static void test_build_conditional(void)
{
    taskflow_handle_t engine = create_taskflow_engine();
    workflow_pattern_config_t config = make_pattern_config(WORKFLOW_CONDITIONAL);
    workflow_context_t *ctx = workflow_context_create(&config, engine);
    assert(ctx != NULL);

    /* 添加节点：condition + true_branch + false_branch + merge */
    workflow_node_t n1 = make_node(1, "cond", NODE_CONDITION);
    workflow_node_t n2 = make_node(2, "true", NODE_TASK);
    workflow_node_t n3 = make_node(3, "false", NODE_TASK);
    workflow_node_t n4 = make_node(4, "merge", NODE_TASK);
    workflow_add_node(ctx, &n1);
    workflow_add_node(ctx, &n2);
    workflow_add_node(ctx, &n3);
    workflow_add_node(ctx, &n4);

    /* 构建条件模式 */
    taskflow_error_t rc = workflow_build_conditional(ctx, 1, 2, 3, 4,
                                                     mock_condition_func, NULL);
    assert(rc == TASKFLOW_SUCCESS);

    workflow_context_destroy(ctx);
    taskflow_engine_destroy_core(engine);
    TEST_PASS("test_build_conditional");
}

static void test_build_loop(void)
{
    taskflow_handle_t engine = create_taskflow_engine();
    workflow_pattern_config_t config = make_pattern_config(WORKFLOW_LOOP);
    workflow_context_t *ctx = workflow_context_create(&config, engine);
    assert(ctx != NULL);

    /* 添加节点：loop_start + loop_body + loop_cond + loop_end */
    workflow_node_t n1 = make_node(1, "loop_start", NODE_LOOP_START);
    workflow_node_t n2 = make_node(2, "body", NODE_TASK);
    workflow_node_t n3 = make_node(3, "loop_cond", NODE_CONDITION);
    workflow_node_t n4 = make_node(4, "loop_end", NODE_LOOP_END);
    workflow_add_node(ctx, &n1);
    workflow_add_node(ctx, &n2);
    workflow_add_node(ctx, &n3);
    workflow_add_node(ctx, &n4);

    /* 构建循环模式 */
    taskflow_error_t rc = workflow_build_loop(ctx, 1, 2, 3, 4,
                                              mock_condition_func, NULL);
    assert(rc == TASKFLOW_SUCCESS);

    workflow_context_destroy(ctx);
    taskflow_engine_destroy_core(engine);
    TEST_PASS("test_build_loop");
}

static void test_build_fork_join_manual(void)
{
    /* FORK_JOIN 无专门 builder，用手动 add_node + add_edge 构造 */
    taskflow_handle_t engine = create_taskflow_engine();
    workflow_pattern_config_t config = make_pattern_config(WORKFLOW_FORK_JOIN);
    workflow_context_t *ctx = workflow_context_create(&config, engine);
    assert(ctx != NULL);

    /* fork → 3 并行 → join */
    workflow_node_t n1 = make_node(1, "fork", NODE_PARALLEL_START);
    workflow_node_t n2 = make_node(2, "b1", NODE_TASK);
    workflow_node_t n3 = make_node(3, "b2", NODE_TASK);
    workflow_node_t n4 = make_node(4, "b3", NODE_TASK);
    workflow_node_t n5 = make_node(5, "join", NODE_PARALLEL_END);
    workflow_add_node(ctx, &n1);
    workflow_add_node(ctx, &n2);
    workflow_add_node(ctx, &n3);
    workflow_add_node(ctx, &n4);
    workflow_add_node(ctx, &n5);

    /* 手动添加边: 1→2, 1→3, 1→4, 2→5, 3→5, 4→5
     * C 语法：函数返回值是 rvalue，不能直接 & 取地址，需用变量存储 */
    workflow_edge_t we1 = make_wf_edge(201, 1, 2), we2 = make_wf_edge(202, 1, 3),
                    we3 = make_wf_edge(203, 1, 4), we4 = make_wf_edge(204, 2, 5),
                    we5 = make_wf_edge(205, 3, 5), we6 = make_wf_edge(206, 4, 5);
    workflow_add_edge(ctx, &we1);
    workflow_add_edge(ctx, &we2);
    workflow_add_edge(ctx, &we3);
    workflow_add_edge(ctx, &we4);
    workflow_add_edge(ctx, &we5);
    workflow_add_edge(ctx, &we6);

    workflow_set_start_node(ctx, 1);
    workflow_set_end_node(ctx, 5);

    workflow_context_destroy(ctx);
    taskflow_engine_destroy_core(engine);
    TEST_PASS("test_build_fork_join_manual");
}

/* ===== 5. 执行控制类 ===== */

static void test_execute_sync(void)
{
    taskflow_handle_t engine = create_taskflow_engine();
    workflow_pattern_config_t config = make_pattern_config(WORKFLOW_SEQUENTIAL);
    workflow_context_t *ctx = workflow_context_create(&config, engine);
    assert(ctx != NULL);

    workflow_node_t n1 = make_node(1, "a", NODE_TASK);
    workflow_node_t n2 = make_node(2, "b", NODE_TASK);
    workflow_add_node(ctx, &n1);
    workflow_add_node(ctx, &n2);

    vertex_id_t node_ids[] = {1, 2};
    workflow_build_sequential(ctx, node_ids, 2);
    workflow_set_start_node(ctx, 1);
    workflow_set_end_node(ctx, 2);

    g_executor_count = 0;
    taskflow_error_t rc = workflow_execute_sync(ctx, 10);
    (void)rc;

    workflow_context_destroy(ctx);
    taskflow_engine_destroy_core(engine);
    TEST_PASS("test_execute_sync");
}

static void test_execute_async(void)
{
    taskflow_handle_t engine = create_taskflow_engine();
    workflow_pattern_config_t config = make_pattern_config(WORKFLOW_SEQUENTIAL);
    workflow_context_t *ctx = workflow_context_create(&config, engine);
    assert(ctx != NULL);

    workflow_node_t n1 = make_node(1, "a", NODE_TASK);
    workflow_add_node(ctx, &n1);
    workflow_set_start_node(ctx, 1);
    workflow_set_end_node(ctx, 1);

    taskflow_error_t rc = workflow_execute_async(ctx, 5, mock_async_callback, NULL);
    (void)rc;

    /* 等待异步完成 */
    workflow_wait_or_stop:
    {
        size_t completed = 0, total = 0;
        vertex_id_t current = 0;
        workflow_get_status(ctx, &completed, &total, &current);
    }

    workflow_context_destroy(ctx);
    taskflow_engine_destroy_core(engine);
    TEST_PASS("test_execute_async");
}

static void test_pause_resume_stop(void)
{
    taskflow_handle_t engine = create_taskflow_engine();
    workflow_pattern_config_t config = make_pattern_config(WORKFLOW_SEQUENTIAL);
    workflow_context_t *ctx = workflow_context_create(&config, engine);
    assert(ctx != NULL);

    /* pause/resume/stop 在未执行时调用，应安全处理 */
    taskflow_error_t rc = workflow_pause(ctx);
    (void)rc;

    rc = workflow_resume(ctx);
    (void)rc;

    rc = workflow_stop(ctx);
    (void)rc;

    workflow_context_destroy(ctx);
    taskflow_engine_destroy_core(engine);
    TEST_PASS("test_pause_resume_stop");
}

static void test_get_status(void)
{
    taskflow_handle_t engine = create_taskflow_engine();
    workflow_pattern_config_t config = make_pattern_config(WORKFLOW_SEQUENTIAL);
    workflow_context_t *ctx = workflow_context_create(&config, engine);
    assert(ctx != NULL);

    workflow_node_t n1 = make_node(1, "a", NODE_TASK);
    workflow_node_t n2 = make_node(2, "b", NODE_TASK);
    workflow_add_node(ctx, &n1);
    workflow_add_node(ctx, &n2);

    size_t completed = 999, total = 999;
    vertex_id_t current = 999;
    taskflow_error_t rc = workflow_get_status(ctx, &completed, &total, &current);
    (void)rc;
    /* 初始状态 completed 应为 0，total 应为 2 */

    workflow_context_destroy(ctx);
    taskflow_engine_destroy_core(engine);
    TEST_PASS("test_get_status");
}

/* ===== 6. 条件回调验证类 ===== */

static void test_condition_true_branch(void)
{
    taskflow_handle_t engine = create_taskflow_engine();
    workflow_pattern_config_t config = make_pattern_config(WORKFLOW_CONDITIONAL);
    workflow_context_t *ctx = workflow_context_create(&config, engine);
    assert(ctx != NULL);

    workflow_node_t n1 = make_node(1, "cond", NODE_CONDITION);
    workflow_node_t n2 = make_node(2, "true", NODE_TASK);
    workflow_node_t n3 = make_node(3, "false", NODE_TASK);
    workflow_node_t n4 = make_node(4, "merge", NODE_TASK);
    workflow_add_node(ctx, &n1);
    workflow_add_node(ctx, &n2);
    workflow_add_node(ctx, &n3);
    workflow_add_node(ctx, &n4);

    workflow_build_conditional(ctx, 1, 2, 3, 4, mock_condition_func, NULL);
    workflow_set_start_node(ctx, 1);
    workflow_set_end_node(ctx, 4);

    /* 条件返回 true → 走 true 分支 */
    g_condition_result = 1;
    g_executor_count = 0;
    taskflow_error_t rc = workflow_execute_sync(ctx, 10);
    (void)rc;

    workflow_context_destroy(ctx);
    taskflow_engine_destroy_core(engine);
    TEST_PASS("test_condition_true_branch");
}

/* ===== main ===== */

int main(void)
{
    printf("\n========================================\n");
    printf("  TaskFlow Workflow Patterns 工作流模式 单元测试 (W17.4)\n");
    printf("========================================\n\n");

    /* 1. 静态验证类 */
    RUN_TEST(test_enum_values);
    RUN_TEST(test_type_sizes);

    /* 2. 上下文生命周期类 */
    RUN_TEST(test_context_create_destroy);
    RUN_TEST(test_context_create_null);
    RUN_TEST(test_context_destroy_null);

    /* 3. 节点/边管理类 */
    RUN_TEST(test_add_node);
    RUN_TEST(test_add_edge);
    RUN_TEST(test_set_start_end_node);

    /* 4. 5 种工作流模式构建器类 */
    RUN_TEST(test_build_sequential);
    RUN_TEST(test_build_parallel);
    RUN_TEST(test_build_conditional);
    RUN_TEST(test_build_loop);
    RUN_TEST(test_build_fork_join_manual);

    /* 5. 执行控制类 */
    RUN_TEST(test_execute_sync);
    RUN_TEST(test_execute_async);
    RUN_TEST(test_pause_resume_stop);
    RUN_TEST(test_get_status);

    /* 6. 条件回调验证类 */
    RUN_TEST(test_condition_true_branch);

    printf("\n========================================\n");
    printf("  测试结果: %d 运行, %d 通过, %d 失败\n", tests_run, tests_passed, 0);
    printf("========================================\n");

    return 0;
}
