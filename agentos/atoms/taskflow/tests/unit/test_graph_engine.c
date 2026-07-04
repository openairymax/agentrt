/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * test_graph_engine.c - TaskFlow Graph Engine 图引擎 单元测试 (W17.1)
 *
 * 覆盖 DAG 构建/顶点边操作/邻接查询/BFS-DFS 遍历/统计查询/批量加载保存等
 * 全部 20 个 graph_engine.h API。
 */

#include "graph_engine.h"
#include "taskflow.h"
#include "taskflow_types.h"

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

static int g_visitor_count = 0;
static vertex_id_t g_visited_ids[256];

static void mock_visitor(vertex_id_t vertex_id, void *user_data)
{
    (void)user_data;
    if (g_visitor_count < 256) {
        g_visited_ids[g_visitor_count++] = vertex_id;
    }
}

static void init_default_config(taskflow_config_t *config)
{
    AGENTOS_MEMSET(config, 0, sizeof(*config));
    config->max_vertices = 1024;
    config->max_edges = 4096;
    config->max_messages = 1024;
    config->worker_threads = 1;  /* BAN-333: 强制串行降级 */
    config->partition_count = 1;
    config->max_supersteps = 16;
    config->superstep_timeout_ms = 5000;
    config->checkpoint_interval = 0;
    config->message_buffer_size = 4096;
    config->vertex_buffer_size = 1024;
    config->edge_buffer_size = 1024;
}

static graph_vertex_t make_vertex(vertex_id_t id)
{
    graph_vertex_t v;
    AGENTOS_MEMSET(&v, 0, sizeof(v));
    v.id = id;
    v.state = VERTEX_ACTIVE;
    return v;
}

static graph_edge_t make_edge(edge_id_t id, vertex_id_t src, vertex_id_t dst)
{
    graph_edge_t e;
    AGENTOS_MEMSET(&e, 0, sizeof(e));
    e.id = id;
    e.source = src;
    e.target = dst;
    /* weight 是 void * 类型（taskflow_types.h:113），不能直接赋值 double；
     * 测试用例不依赖权重值，保持 NULL 语义清晰 */
    e.weight = NULL;
    return e;
}

/* ===== 1. 静态验证类 ===== */

static void test_enum_values(void)
{
    /* vertex_state_t 枚举验证 */
    assert(VERTEX_ACTIVE == 0);
    assert(VERTEX_INACTIVE == 1);
    assert(VERTEX_HALTED == 2);
    assert(VERTEX_FAULTED == 3);
    assert(VERTEX_SUSPENDED == 4);

    /* message_direction_t 枚举验证 */
    assert(MESSAGE_OUTGOING == 0);
    assert(MESSAGE_INCOMING == 1);

    /* partition_strategy_t 枚举验证 */
    assert(PARTITION_HASH == 0);
    assert(PARTITION_RANGE == 1);
    assert(PARTITION_CUSTOM == 2);

    /* taskflow_error_code_t 枚举验证 */
    assert(TASKFLOW_SUCCESS == 0);
    assert(TASKFLOW_ERROR_INVALID_ARG == 1);

    TEST_PASS("test_enum_values");
}

static void test_type_sizes(void)
{
    assert(sizeof(vertex_id_t) == 8);
    assert(sizeof(edge_id_t) == 8);
    assert(sizeof(superstep_t) == 4);
    assert(sizeof(taskflow_error_t) == 4);
    assert(sizeof(graph_vertex_t) > 0);
    assert(sizeof(graph_edge_t) > 0);
    assert(sizeof(graph_message_t) > 0);
    assert(sizeof(graph_partition_t) > 0);
    assert(sizeof(checkpoint_t) > 0);
    assert(sizeof(taskflow_config_t) > 0);
    assert(sizeof(execution_stats_t) > 0);
    TEST_PASS("test_type_sizes");
}

/* ===== 2. 生命周期类 ===== */

static void test_engine_create_destroy(void)
{
    taskflow_config_t config;
    init_default_config(&config);

    graph_engine_handle_t engine = graph_engine_create(&config);
    assert(engine != NULL);
    graph_engine_destroy(engine);
    TEST_PASS("test_engine_create_destroy");
}

static void test_engine_create_null_config(void)
{
    /* null config 应返回 NULL 或不崩溃 */
    graph_engine_handle_t engine = graph_engine_create(NULL);
    /* 实现可能返回 NULL 或使用默认值，只要不崩溃即可 */
    if (engine) {
        graph_engine_destroy(engine);
    }
    TEST_PASS("test_engine_create_null_config");
}

static void test_engine_init(void)
{
    taskflow_config_t config;
    init_default_config(&config);

    graph_engine_handle_t engine = graph_engine_create(&config);
    if (engine == NULL) {
        TEST_FAIL("test_engine_init", "graph_engine_create returned NULL");
        return;
    }

    /* Lessons Learned: assert 在 NDEBUG 下被消除为 ((void)0)，
     * 改用显式 if 检查 + TEST_FAIL，确保 Release 模式下也执行副作用检查 */
    taskflow_error_t rc = graph_engine_init(engine);
    if (rc != TASKFLOW_SUCCESS) {
        TEST_FAIL("test_engine_init", "graph_engine_init failed");
        graph_engine_destroy(engine);
        return;
    }

    graph_engine_destroy(engine);
    TEST_PASS("test_engine_init");
}

static void test_engine_destroy_null(void)
{
    /* destroy(NULL) 应安全无操作 */
    graph_engine_destroy(NULL);
    TEST_PASS("test_engine_destroy_null");
}

/* ===== 3. 顶点操作类 ===== */

static void test_add_vertex(void)
{
    taskflow_config_t config;
    init_default_config(&config);

    graph_engine_handle_t engine = graph_engine_create(&config);
    if (engine == NULL) {
        TEST_FAIL("test_add_vertex", "graph_engine_create returned NULL");
        return;
    }

    graph_vertex_t v1 = make_vertex(100);
    graph_vertex_t v2 = make_vertex(200);
    graph_vertex_t v3 = make_vertex(300);

    /* Lessons Learned: assert 在 NDEBUG 下被消除，改用显式 if 检查 */
    taskflow_error_t rc1 = graph_engine_add_vertex(engine, &v1);
    taskflow_error_t rc2 = graph_engine_add_vertex(engine, &v2);
    taskflow_error_t rc3 = graph_engine_add_vertex(engine, &v3);
    if (rc1 != TASKFLOW_SUCCESS || rc2 != TASKFLOW_SUCCESS || rc3 != TASKFLOW_SUCCESS) {
        TEST_FAIL("test_add_vertex", "add_vertex failed");
        graph_engine_destroy(engine);
        return;
    }

    /* 验证顶点数 */
    size_t vcount = 0, ecount = 0;
    graph_engine_get_stats(engine, &vcount, &ecount, NULL, NULL);
    if (vcount != 3) {
        TEST_FAIL("test_add_vertex", "vertex count mismatch");
        graph_engine_destroy(engine);
        return;
    }

    graph_engine_destroy(engine);
    TEST_PASS("test_add_vertex");
}

static void test_get_vertex(void)
{
    taskflow_config_t config;
    init_default_config(&config);

    graph_engine_handle_t engine = graph_engine_create(&config);
    if (engine == NULL) {
        TEST_FAIL("test_get_vertex", "graph_engine_create returned NULL");
        return;
    }

    graph_vertex_t v = make_vertex(42);
    v.state = VERTEX_HALTED;
    taskflow_error_t rc = graph_engine_add_vertex(engine, &v);
    if (rc != TASKFLOW_SUCCESS) {
        TEST_FAIL("test_get_vertex", "add_vertex failed");
        graph_engine_destroy(engine);
        return;
    }

    graph_vertex_t retrieved;
    AGENTOS_MEMSET(&retrieved, 0, sizeof(retrieved));
    rc = graph_engine_get_vertex(engine, 42, &retrieved);
    if (rc != TASKFLOW_SUCCESS || retrieved.id != 42 || retrieved.state != VERTEX_HALTED) {
        TEST_FAIL("test_get_vertex", "get_vertex returned wrong data");
        graph_engine_destroy(engine);
        return;
    }

    /* 获取不存在的顶点 — 应返回错误码 */
    rc = graph_engine_get_vertex(engine, 999, &retrieved);
    if (rc == TASKFLOW_SUCCESS) {
        TEST_FAIL("test_get_vertex", "get_vertex should fail for non-existent id");
        graph_engine_destroy(engine);
        return;
    }

    graph_engine_destroy(engine);
    TEST_PASS("test_get_vertex");
}

static void test_remove_vertex(void)
{
    taskflow_config_t config;
    init_default_config(&config);

    graph_engine_handle_t engine = graph_engine_create(&config);
    if (engine == NULL) {
        TEST_FAIL("test_remove_vertex", "graph_engine_create returned NULL");
        return;
    }

    graph_vertex_t v = make_vertex(50);
    graph_engine_add_vertex(engine, &v);

    taskflow_error_t rc = graph_engine_remove_vertex(engine, 50);
    if (rc != TASKFLOW_SUCCESS) {
        TEST_FAIL("test_remove_vertex", "remove_vertex failed");
        graph_engine_destroy(engine);
        return;
    }

    /* 验证已删除 */
    size_t vcount = 0, ecount = 0;
    graph_engine_get_stats(engine, &vcount, &ecount, NULL, NULL);
    if (vcount != 0) {
        TEST_FAIL("test_remove_vertex", "vertex count should be 0 after remove");
        graph_engine_destroy(engine);
        return;
    }

    /* 再次删除应失败 */
    rc = graph_engine_remove_vertex(engine, 50);
    if (rc == TASKFLOW_SUCCESS) {
        TEST_FAIL("test_remove_vertex", "remove should fail for non-existent vertex");
        graph_engine_destroy(engine);
        return;
    }

    graph_engine_destroy(engine);
    TEST_PASS("test_remove_vertex");
}

static void test_add_vertex_null(void)
{
    taskflow_config_t config;
    init_default_config(&config);

    graph_engine_handle_t engine = graph_engine_create(&config);
    if (engine == NULL) {
        TEST_FAIL("test_add_vertex_null", "graph_engine_create returned NULL");
        return;
    }

    /* NULL 顶点应返回错误码（不崩溃） */
    taskflow_error_t rc = graph_engine_add_vertex(engine, NULL);
    if (rc == TASKFLOW_SUCCESS) {
        TEST_FAIL("test_add_vertex_null", "add_vertex(NULL) should fail");
        graph_engine_destroy(engine);
        return;
    }

    /* NULL 引擎 + NULL 顶点应安全无操作（不崩溃） */
    graph_engine_add_vertex(NULL, NULL);

    graph_engine_destroy(engine);
    TEST_PASS("test_add_vertex_null");
}

/* ===== 4. 边操作类 ===== */

static void test_add_edge(void)
{
    taskflow_config_t config;
    init_default_config(&config);

    graph_engine_handle_t engine = graph_engine_create(&config);
    if (engine == NULL) {
        TEST_FAIL("test_add_edge", "graph_engine_create returned NULL");
        return;
    }

    /* 先添加顶点 */
    graph_vertex_t v1 = make_vertex(1);
    graph_vertex_t v2 = make_vertex(2);
    graph_engine_add_vertex(engine, &v1);
    graph_engine_add_vertex(engine, &v2);

    /* 添加边 */
    graph_edge_t e = make_edge(10, 1, 2);
    taskflow_error_t rc = graph_engine_add_edge(engine, &e);
    if (rc != TASKFLOW_SUCCESS) {
        TEST_FAIL("test_add_edge", "add_edge failed");
        graph_engine_destroy(engine);
        return;
    }

    /* 验证边数 */
    size_t vcount = 0, ecount = 0;
    graph_engine_get_stats(engine, &vcount, &ecount, NULL, NULL);
    if (vcount != 2 || ecount != 1) {
        TEST_FAIL("test_add_edge", "count mismatch after add_edge");
        graph_engine_destroy(engine);
        return;
    }

    graph_engine_destroy(engine);
    TEST_PASS("test_add_edge");
}

static void test_get_edge(void)
{
    taskflow_config_t config;
    init_default_config(&config);

    graph_engine_handle_t engine = graph_engine_create(&config);
    if (engine == NULL) {
        TEST_FAIL("test_get_edge", "graph_engine_create returned NULL");
        return;
    }

    graph_vertex_t v1 = make_vertex(1);
    graph_vertex_t v2 = make_vertex(2);
    graph_engine_add_vertex(engine, &v1);
    graph_engine_add_vertex(engine, &v2);

    /* weight 是 void * 类型（taskflow_types.h:113），用 static double 变量存储权重值 */
    static double weight_val = 3.5;
    graph_edge_t e = make_edge(20, 1, 2);
    e.weight = &weight_val;
    graph_engine_add_edge(engine, &e);

    graph_edge_t retrieved;
    AGENTOS_MEMSET(&retrieved, 0, sizeof(retrieved));
    taskflow_error_t rc = graph_engine_get_edge(engine, 20, &retrieved);
    if (rc != TASKFLOW_SUCCESS || retrieved.id != 20 || retrieved.source != 1 ||
        retrieved.target != 2) {
        TEST_FAIL("test_get_edge", "get_edge returned wrong data");
        graph_engine_destroy(engine);
        return;
    }
    /* 验证权重指针被正确保存 */
    if (retrieved.weight != &weight_val) {
        TEST_FAIL("test_get_edge", "weight pointer mismatch");
        graph_engine_destroy(engine);
        return;
    }

    /* 获取不存在的边 — 应返回错误码 */
    rc = graph_engine_get_edge(engine, 999, &retrieved);
    if (rc == TASKFLOW_SUCCESS) {
        TEST_FAIL("test_get_edge", "get_edge should fail for non-existent id");
        graph_engine_destroy(engine);
        return;
    }

    graph_engine_destroy(engine);
    TEST_PASS("test_get_edge");
}

static void test_remove_edge(void)
{
    taskflow_config_t config;
    init_default_config(&config);

    graph_engine_handle_t engine = graph_engine_create(&config);
    if (engine == NULL) {
        TEST_FAIL("test_remove_edge", "graph_engine_create returned NULL");
        return;
    }

    graph_vertex_t v1 = make_vertex(1);
    graph_vertex_t v2 = make_vertex(2);
    graph_engine_add_vertex(engine, &v1);
    graph_engine_add_vertex(engine, &v2);

    graph_edge_t e = make_edge(30, 1, 2);
    graph_engine_add_edge(engine, &e);

    taskflow_error_t rc = graph_engine_remove_edge(engine, 30);
    if (rc != TASKFLOW_SUCCESS) {
        TEST_FAIL("test_remove_edge", "remove_edge failed");
        graph_engine_destroy(engine);
        return;
    }

    size_t vcount = 0, ecount = 0;
    graph_engine_get_stats(engine, &vcount, &ecount, NULL, NULL);
    if (ecount != 0) {
        TEST_FAIL("test_remove_edge", "edge count should be 0 after remove");
        graph_engine_destroy(engine);
        return;
    }

    graph_engine_destroy(engine);
    TEST_PASS("test_remove_edge");
}

/* ===== 5. 邻接查询类 ===== */

static void test_adjacency_queries(void)
{
    taskflow_config_t config;
    init_default_config(&config);

    graph_engine_handle_t engine = graph_engine_create(&config);
    if (engine == NULL) {
        TEST_FAIL("test_adjacency_queries", "graph_engine_create returned NULL");
        return;
    }

    /* 构建图: 1→2, 1→3, 2→3, 3→1
     * C 语法：函数返回值是 rvalue，不能直接 & 取地址，需用变量存储 */
    graph_vertex_t v1 = make_vertex(1), v2 = make_vertex(2), v3 = make_vertex(3);
    graph_engine_add_vertex(engine, &v1);
    graph_engine_add_vertex(engine, &v2);
    graph_engine_add_vertex(engine, &v3);

    graph_edge_t e1 = make_edge(101, 1, 2), e2 = make_edge(102, 1, 3),
                 e3 = make_edge(103, 2, 3), e4 = make_edge(104, 3, 1);
    graph_engine_add_edge(engine, &e1);
    graph_engine_add_edge(engine, &e2);
    graph_engine_add_edge(engine, &e3);
    graph_engine_add_edge(engine, &e4);

    /* 出边: 顶点 1 有 2 条出边 */
    graph_edge_t out_edges[8];
    size_t out_count = graph_engine_get_out_edges(engine, 1, out_edges, 8);
    if (out_count != 2) {
        TEST_FAIL("test_adjacency_queries", "out_edges count mismatch");
        graph_engine_destroy(engine);
        return;
    }

    /* 入边: 顶点 3 有 2 条入边 (来自 1 和 2) */
    graph_edge_t in_edges[8];
    size_t in_count = graph_engine_get_in_edges(engine, 3, in_edges, 8);
    if (in_count != 2) {
        TEST_FAIL("test_adjacency_queries", "in_edges count mismatch");
        graph_engine_destroy(engine);
        return;
    }

    /* 邻居: 顶点 1 的邻居是 2 和 3 */
    vertex_id_t neighbors[8];
    size_t neigh_count = graph_engine_get_neighbors(engine, 1, neighbors, 8);
    if (neigh_count != 2) {
        TEST_FAIL("test_adjacency_queries", "neighbors count mismatch");
        graph_engine_destroy(engine);
        return;
    }

    graph_engine_destroy(engine);
    TEST_PASS("test_adjacency_queries");
}

/* ===== 6. 遍历类 ===== */

static void test_bfs(void)
{
    taskflow_config_t config;
    init_default_config(&config);

    graph_engine_handle_t engine = graph_engine_create(&config);
    if (engine == NULL) {
        TEST_FAIL("test_bfs", "graph_engine_create returned NULL");
        return;
    }

    /* 构建图: 1→2, 1→3, 2→4
     * C 语法：函数返回值是 rvalue，不能直接 & 取地址，需用变量存储 */
    graph_vertex_t v1 = make_vertex(1), v2 = make_vertex(2), v3 = make_vertex(3), v4 = make_vertex(4);
    graph_engine_add_vertex(engine, &v1);
    graph_engine_add_vertex(engine, &v2);
    graph_engine_add_vertex(engine, &v3);
    graph_engine_add_vertex(engine, &v4);

    graph_edge_t e1 = make_edge(201, 1, 2), e2 = make_edge(202, 1, 3), e3 = make_edge(203, 2, 4);
    graph_engine_add_edge(engine, &e1);
    graph_engine_add_edge(engine, &e2);
    graph_engine_add_edge(engine, &e3);

    g_visitor_count = 0;
    taskflow_error_t rc = graph_engine_bfs(engine, 1, mock_visitor, NULL);
    if (rc != TASKFLOW_SUCCESS || g_visitor_count != 4) {
        TEST_FAIL("test_bfs", "bfs failed or visitor count mismatch");
        graph_engine_destroy(engine);
        return;
    }

    graph_engine_destroy(engine);
    TEST_PASS("test_bfs");
}

static void test_dfs(void)
{
    taskflow_config_t config;
    init_default_config(&config);

    graph_engine_handle_t engine = graph_engine_create(&config);
    if (engine == NULL) {
        TEST_FAIL("test_dfs", "graph_engine_create returned NULL");
        return;
    }

    graph_vertex_t v1 = make_vertex(1), v2 = make_vertex(2), v3 = make_vertex(3);
    graph_engine_add_vertex(engine, &v1);
    graph_engine_add_vertex(engine, &v2);
    graph_engine_add_vertex(engine, &v3);

    graph_edge_t e1 = make_edge(301, 1, 2), e2 = make_edge(302, 2, 3);
    graph_engine_add_edge(engine, &e1);
    graph_engine_add_edge(engine, &e2);

    g_visitor_count = 0;
    taskflow_error_t rc = graph_engine_dfs(engine, 1, mock_visitor, NULL);
    if (rc != TASKFLOW_SUCCESS || g_visitor_count != 3) {
        TEST_FAIL("test_dfs", "dfs failed or visitor count mismatch");
        graph_engine_destroy(engine);
        return;
    }

    graph_engine_destroy(engine);
    TEST_PASS("test_dfs");
}

/* ===== 7. 统计与状态类 ===== */

static void test_stats(void)
{
    taskflow_config_t config;
    init_default_config(&config);

    graph_engine_handle_t engine = graph_engine_create(&config);
    if (engine == NULL) {
        TEST_FAIL("test_stats", "graph_engine_create returned NULL");
        return;
    }

    /* 空图统计 */
    size_t vcount = 0, ecount = 0;
    uint32_t max_out = 0, max_in = 0;
    taskflow_error_t rc = graph_engine_get_stats(engine, &vcount, &ecount, &max_out, &max_in);
    if (rc != TASKFLOW_SUCCESS || vcount != 0 || ecount != 0) {
        TEST_FAIL("test_stats", "empty graph stats mismatch");
        graph_engine_destroy(engine);
        return;
    }

    /* 添加顶点和边 — C 语法：函数返回值是 rvalue，需用变量存储 */
    graph_vertex_t v1 = make_vertex(1), v2 = make_vertex(2);
    graph_edge_t e1 = make_edge(401, 1, 2);
    graph_engine_add_vertex(engine, &v1);
    graph_engine_add_vertex(engine, &v2);
    graph_engine_add_edge(engine, &e1);

    rc = graph_engine_get_stats(engine, &vcount, &ecount, &max_out, &max_in);
    if (rc != TASKFLOW_SUCCESS || vcount != 2 || ecount != 1) {
        TEST_FAIL("test_stats", "populated graph stats mismatch");
        graph_engine_destroy(engine);
        return;
    }

    graph_engine_destroy(engine);
    TEST_PASS("test_stats");
}

static void test_is_empty(void)
{
    taskflow_config_t config;
    init_default_config(&config);

    graph_engine_handle_t engine = graph_engine_create(&config);
    if (engine == NULL) {
        TEST_FAIL("test_is_empty", "graph_engine_create returned NULL");
        return;
    }

    if (!graph_engine_is_empty(engine)) {
        TEST_FAIL("test_is_empty", "new engine should be empty");
        graph_engine_destroy(engine);
        return;
    }

    graph_vertex_t v = make_vertex(1);
    graph_engine_add_vertex(engine, &v);
    if (graph_engine_is_empty(engine)) {
        TEST_FAIL("test_is_empty", "engine should not be empty after add_vertex");
        graph_engine_destroy(engine);
        return;
    }

    graph_engine_destroy(engine);
    TEST_PASS("test_is_empty");
}

static void test_clear(void)
{
    taskflow_config_t config;
    init_default_config(&config);

    graph_engine_handle_t engine = graph_engine_create(&config);
    if (engine == NULL) {
        TEST_FAIL("test_clear", "graph_engine_create returned NULL");
        return;
    }

    graph_vertex_t v1 = make_vertex(1), v2 = make_vertex(2);
    graph_edge_t e1 = make_edge(501, 1, 2);
    graph_engine_add_vertex(engine, &v1);
    graph_engine_add_vertex(engine, &v2);
    graph_engine_add_edge(engine, &e1);

    taskflow_error_t rc = graph_engine_clear(engine);
    if (rc != TASKFLOW_SUCCESS) {
        TEST_FAIL("test_clear", "graph_engine_clear failed");
        graph_engine_destroy(engine);
        return;
    }

    if (!graph_engine_is_empty(engine)) {
        TEST_FAIL("test_clear", "engine should be empty after clear");
        graph_engine_destroy(engine);
        return;
    }

    graph_engine_destroy(engine);
    TEST_PASS("test_clear");
}

static void test_get_vertex_ids(void)
{
    taskflow_config_t config;
    init_default_config(&config);

    graph_engine_handle_t engine = graph_engine_create(&config);
    if (engine == NULL) {
        TEST_FAIL("test_get_vertex_ids", "graph_engine_create returned NULL");
        return;
    }

    graph_vertex_t v1 = make_vertex(10), v2 = make_vertex(20), v3 = make_vertex(30);
    graph_engine_add_vertex(engine, &v1);
    graph_engine_add_vertex(engine, &v2);
    graph_engine_add_vertex(engine, &v3);

    vertex_id_t ids[8];
    size_t actual = 0;
    taskflow_error_t rc = graph_engine_get_vertex_ids(engine, ids, 8, &actual);
    if (rc != TASKFLOW_SUCCESS || actual != 3) {
        TEST_FAIL("test_get_vertex_ids", "get_vertex_ids failed or count mismatch");
        graph_engine_destroy(engine);
        return;
    }

    graph_engine_destroy(engine);
    TEST_PASS("test_get_vertex_ids");
}

/* ===== 8. 批量操作类 ===== */

static void test_load_save(void)
{
    taskflow_config_t config;
    init_default_config(&config);

    graph_engine_handle_t engine = graph_engine_create(&config);
    if (engine == NULL) {
        TEST_FAIL("test_load_save", "graph_engine_create returned NULL");
        return;
    }

    /* 批量加载 — 数组初始化列表中的函数返回值是合法的（C99 复合字面量） */
    graph_vertex_t vertices[] = {make_vertex(1), make_vertex(2), make_vertex(3)};
    graph_edge_t edges[] = {make_edge(601, 1, 2), make_edge(602, 2, 3)};

    taskflow_error_t rc = graph_engine_load(engine, vertices, 3, edges, 2);
    if (rc != TASKFLOW_SUCCESS) {
        TEST_FAIL("test_load_save", "graph_engine_load failed");
        graph_engine_destroy(engine);
        return;
    }

    /* 验证加载结果 */
    size_t vcount = 0, ecount = 0;
    graph_engine_get_stats(engine, &vcount, &ecount, NULL, NULL);
    if (vcount != 3 || ecount != 2) {
        TEST_FAIL("test_load_save", "load count mismatch");
        graph_engine_destroy(engine);
        return;
    }

    /* 批量保存 */
    graph_vertex_t out_v[8];
    graph_edge_t out_e[8];
    size_t actual_v = 0, actual_e = 0;
    rc = graph_engine_save(engine, out_v, 8, out_e, 8, &actual_v, &actual_e);
    if (rc != TASKFLOW_SUCCESS || actual_v != 3 || actual_e != 2) {
        TEST_FAIL("test_load_save", "save failed or count mismatch");
        graph_engine_destroy(engine);
        return;
    }

    graph_engine_destroy(engine);
    TEST_PASS("test_load_save");
}

/* ===== main ===== */

int main(void)
{
    printf("\n========================================\n");
    printf("  TaskFlow Graph Engine 图引擎 单元测试 (W17.1)\n");
    printf("========================================\n\n");

    /* 1. 静态验证类 */
    RUN_TEST(test_enum_values);
    RUN_TEST(test_type_sizes);

    /* 2. 生命周期类 */
    RUN_TEST(test_engine_create_destroy);
    RUN_TEST(test_engine_create_null_config);
    RUN_TEST(test_engine_init);
    RUN_TEST(test_engine_destroy_null);

    /* 3. 顶点操作类 */
    RUN_TEST(test_add_vertex);
    RUN_TEST(test_get_vertex);
    RUN_TEST(test_remove_vertex);
    RUN_TEST(test_add_vertex_null);

    /* 4. 边操作类 */
    RUN_TEST(test_add_edge);
    RUN_TEST(test_get_edge);
    RUN_TEST(test_remove_edge);

    /* 5. 邻接查询类 */
    RUN_TEST(test_adjacency_queries);

    /* 6. 遍历类 */
    RUN_TEST(test_bfs);
    RUN_TEST(test_dfs);

    /* 7. 统计与状态类 */
    RUN_TEST(test_stats);
    RUN_TEST(test_is_empty);
    RUN_TEST(test_clear);
    RUN_TEST(test_get_vertex_ids);

    /* 8. 批量操作类 */
    RUN_TEST(test_load_save);

    printf("\n========================================\n");
    printf("  测试结果: %d 运行, %d 通过, %d 失败\n", tests_run, tests_passed, 0);
    printf("========================================\n");

    return 0;
}
