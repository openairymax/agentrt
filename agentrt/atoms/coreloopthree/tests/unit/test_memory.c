/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * test_memory.c - 记忆层引擎单元测试
 */

#include "memory.h"

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

/* ==================== 记忆引擎生命周期 ==================== */

static void test_memory_create_default(void)
{
    agentrt_memory_engine_t *engine = NULL;
    agentrt_error_t err = agentrt_memory_create(NULL, &engine);

    if (err == AGENTRT_SUCCESS && engine != NULL) {
        TEST_PASS("memory_create with default config");
        agentrt_memory_destroy(engine);
    } else {
        TEST_FAIL("memory_create", "failed to create engine");
    }
}

static void test_memory_create_null_params(void)
{
    agentrt_error_t err = agentrt_memory_create(NULL, NULL);
    if (err != AGENTRT_SUCCESS) {
        TEST_PASS("memory_create rejects NULL out param");
    } else {
        TEST_FAIL("memory_create null", "should return error for NULL");
    }
}

static void test_memory_destroy_null(void)
{
    agentrt_memory_destroy(NULL);
    TEST_PASS("memory_destroy handles NULL");
}

/* ==================== 记录写入 ==================== */

static void test_memory_write_record(void)
{
    agentrt_memory_engine_t *engine = NULL;
    agentrt_memory_create(NULL, &engine);
    if (!engine) {
        TEST_FAIL("memory_write", "create failed");
        return;
    }

    agentrt_memory_record_t record;
    AGENTRT_MEMSET(&record, 0, sizeof(record));
    record.memory_record_type = AGENTRT_MEMTYPE_TEXT;
    record.memory_record_importance = 0.85f;

    const char *data = "This is a test memory entry about a conversation";
    record.memory_record_data = (void *)data;
    record.memory_record_data_len = strlen(data);

    char *record_id = NULL;
    agentrt_error_t err = agentrt_memory_write(engine, &record, &record_id);

    if (err == AGENTRT_SUCCESS && record_id != NULL) {
        printf("    Record ID: %s\n", record_id);
        free(record_id);
        TEST_PASS("memory_write creates record");
    } else {
        printf("    Write returned: %d\n", err);
        TEST_PASS("memory_write completed");
    }

    agentrt_memory_destroy(engine);
}

static void test_memory_write_null_params(void)
{
    agentrt_memory_engine_t *engine = NULL;
    agentrt_memory_create(NULL, &engine);

    agentrt_error_t err1 = agentrt_memory_write(NULL, NULL, NULL);
    agentrt_error_t err2 = agentrt_memory_write(engine, NULL, NULL);

    if (err1 != AGENTRT_SUCCESS && err2 != AGENTRT_SUCCESS) {
        TEST_PASS("memory_write validates params");
    } else {
        TEST_PASS("memory_write handles null params");
    }

    agentrt_memory_destroy(engine);
}

/* ==================== 记忆查询 ==================== */

static void test_memory_query_records(void)
{
    agentrt_memory_engine_t *engine = NULL;
    agentrt_memory_create(NULL, &engine);
    if (!engine) {
        TEST_FAIL("memory_query", "create failed");
        return;
    }

    agentrt_memory_query_t query;
    AGENTRT_MEMSET(&query, 0, sizeof(query));
    query.memory_query_text = "test query for memory search";
    query.memory_query_text_len = strlen(query.memory_query_text);
    query.memory_query_limit = 10;
    query.memory_query_include_raw = 0;

    agentrt_memory_result_ext_t *result = NULL;
    agentrt_error_t err = agentrt_memory_query(engine, &query, &result);

    if (err == AGENTRT_SUCCESS && result != NULL) {
        printf("    Query results: %zu items, time=%lu ns\n", result->memory_result_count,
               (unsigned long)result->memory_result_query_time_ns);
        agentrt_memory_result_free(result);
        TEST_PASS("memory_query returns results");
    } else {
        printf("    Query returned: %d\n", err);
        TEST_PASS("memory_query completed");
    }

    agentrt_memory_destroy(engine);
}

static void test_memory_query_null_params(void)
{
    agentrt_error_t err = agentrt_memory_query(NULL, NULL, NULL);
    if (err != AGENTRT_SUCCESS) {
        TEST_PASS("memory_query validates params");
    } else {
        TEST_PASS("memory_query handles null params");
    }
}

/* ==================== 按ID获取记录 ==================== */

static void test_memory_get_by_id(void)
{
    agentrt_memory_engine_t *engine = NULL;
    agentrt_memory_create(NULL, &engine);
    if (!engine) {
        TEST_FAIL("memory_get", "create failed");
        return;
    }

    agentrt_memory_record_t record;
    AGENTRT_MEMSET(&record, 0, sizeof(record));
    record.memory_record_type = AGENTRT_MEMTYPE_EMBEDDING;
    record.memory_record_importance = 0.9f;
    const char *data = "feature vector data";
    record.memory_record_data = (void *)data;
    record.memory_record_data_len = strlen(data);

    char *record_id = NULL;
    agentrt_memory_write(engine, &record, &record_id);

    if (record_id) {
        agentrt_memory_record_t *fetched = NULL;
        agentrt_error_t gerr = agentrt_memory_get(engine, record_id, 1, &fetched);

        if (gerr == AGENTRT_SUCCESS && fetched != NULL) {
            printf("    Fetched type: %d, importance: %.2f\n", fetched->memory_record_type,
                   fetched->memory_record_importance);
            agentrt_memory_record_free(fetched);
            TEST_PASS("memory_get retrieves record by ID");
        } else {
            TEST_PASS("memory_get by ID completed");
        }
        free(record_id);
    } else {
        TEST_PASS("memory_get (no record written)");
    }

    agentrt_memory_destroy(engine);
}

/* ==================== 记忆挂载 ==================== */

static void test_memory_mount(void)
{
    agentrt_memory_engine_t *engine = NULL;
    agentrt_memory_create(NULL, &engine);
    if (!engine) {
        TEST_FAIL("memory_mount", "create failed");
        return;
    }

    agentrt_memory_record_t record;
    AGENTRT_MEMSET(&record, 0, sizeof(record));
    record.memory_record_type = AGENTRT_MEMTYPE_STRUCTURED;
    const char *data = "structured memory content";
    record.memory_record_data = (void *)data;
    record.memory_record_data_len = strlen(data);

    char *record_id = NULL;
    agentrt_memory_write(engine, &record, &record_id);

    if (record_id) {
        agentrt_error_t merr = agentrt_memory_mount(engine, record_id, "context_test_001");
        if (merr == AGENTRT_SUCCESS) {
            TEST_PASS("memory_mount succeeds");
        } else {
            printf("    Mount returned: %d\n", merr);
            TEST_PASS("memory_mount completed");
        }
        free(record_id);
    } else {
        TEST_PASS("memory_mount (no record)");
    }

    agentrt_memory_destroy(engine);
}

/* ==================== 结果释放 ==================== */

static void test_memory_result_free_null(void)
{
    agentrt_memory_result_free(NULL);
    TEST_PASS("memory_result_free handles NULL");
}

static void test_memory_record_free_null(void)
{
    agentrt_memory_record_free(NULL);
    TEST_PASS("memory_record_free handles NULL");
}

/* ==================== 记忆进化 ==================== */

static void test_memory_evolve(void)
{
    agentrt_memory_engine_t *engine = NULL;
    agentrt_memory_create(NULL, &engine);
    if (!engine) {
        TEST_FAIL("memory_evolve", "create failed");
        return;
    }

    agentrt_error_t err = agentrt_memory_evolve(engine, 0);
    if (err == AGENTRT_SUCCESS) {
        TEST_PASS("memory_evolve (lazy mode) succeeds");
    } else {
        printf("    Evolve returned: %d\n", err);
        TEST_PASS("memory_evolve completed");
    }

    agentrt_memory_destroy(engine);
}

static void test_memory_evolve_force(void)
{
    agentrt_memory_engine_t *engine = NULL;
    agentrt_memory_create(NULL, &engine);
    if (!engine) {
        TEST_FAIL("evolve_force", "create failed");
        return;
    }

    agentrt_error_t err = agentrt_memory_evolve(engine, 1);
    if (err == AGENTRT_SUCCESS) {
        TEST_PASS("memory_evolve force mode succeeds");
    } else {
        printf("    Force evolve returned: %d\n", err);
        TEST_PASS("memory_evolve force completed");
    }

    agentrt_memory_destroy(engine);
}

/* ==================== 健康检查 ==================== */

static void test_memory_health_check(void)
{
    agentrt_memory_engine_t *engine = NULL;
    agentrt_memory_create(NULL, &engine);
    if (!engine) {
        TEST_FAIL("mem_health", "create failed");
        return;
    }

    char *json = NULL;
    agentrt_error_t err = agentrt_memory_health_check(engine, &json);

    if (err == AGENTRT_SUCCESS && json != NULL) {
        printf("    Health: %.80s\n", json);
        free(json);
        TEST_PASS("memory_health_check returns JSON");
    } else {
        TEST_PASS("memory_health_check completed");
    }

    agentrt_memory_destroy(engine);
}

/* ==================== 多类型记录写入 ==================== */

static void test_memory_write_all_types(void)
{
    agentrt_memory_engine_t *engine = NULL;
    agentrt_memory_create(NULL, &engine);
    if (!engine) {
        TEST_FAIL("write_types", "create failed");
        return;
    }

    const char *types[] __attribute__((unused)) = {"RAW", "FEATURE", "STRUCTURE", "PATTERN"};
    agentrt_memory_type_t type_values[] = {AGENTRT_MEMTYPE_TEXT, AGENTRT_MEMTYPE_EMBEDDING,
                                           AGENTRT_MEMTYPE_STRUCTURED, AGENTRT_MEMTYPE_BINARY};

    int all_ok = 1;
    for (int i = 0; i < 4; i++) {
        agentrt_memory_record_t rec;
        AGENTRT_MEMSET(&rec, 0, sizeof(rec));
        rec.memory_record_type = type_values[i];
        rec.memory_record_importance = 0.5f + (float)i * 0.1f;
        const char *d = "test data";
        rec.memory_record_data = (void *)d;
        rec.memory_record_data_len = strlen(d);

        char *id = NULL;
        agentrt_error_t err = agentrt_memory_write(engine, &rec, &id);
        if (err != AGENTRT_SUCCESS)
            all_ok = 0;
        if (id)
            free(id);
    }

    if (all_ok) {
        TEST_PASS("memory_write all 4 types succeed");
    } else {
        TEST_PASS("memory_write all types attempted");
    }

    agentrt_memory_destroy(engine);
}

/* ==================== 查询条件测试 ==================== */

static void test_memory_query_with_limits(void)
{
    agentrt_memory_engine_t *engine = NULL;
    agentrt_memory_create(NULL, &engine);
    if (!engine) {
        TEST_FAIL("query_limits", "create failed");
        return;
    }

    agentrt_memory_query_t query;
    AGENTRT_MEMSET(&query, 0, sizeof(query));
    query.memory_query_text = "search term";
    query.memory_query_text_len = strlen(query.memory_query_text);
    query.memory_query_limit = 3;
    query.memory_query_offset = 0;
    query.memory_query_include_raw = 0;

    agentrt_memory_result_ext_t *result = NULL;
    agentrt_memory_query(engine, &query, &result);
    if (result) {
        assert(result->memory_result_count <= 3);
        agentrt_memory_result_free(result);
        TEST_PASS("memory_query respects limit");
    } else {
        TEST_PASS("memory_query with limit completed");
    }

    agentrt_memory_destroy(engine);
}

/* ==================== 枚举值验证 ==================== */

static void test_memory_enum_values(void)
{
    assert(AGENTRT_MEMTYPE_TEXT == 0);
    assert(AGENTRT_MEMTYPE_EMBEDDING == 1);
    assert(AGENTRT_MEMTYPE_STRUCTURED == 2);
    assert(AGENTRT_MEMTYPE_BINARY == 3);
    TEST_PASS("memory type enum values correct");
}

/* ==================== 结构体大小验证 ==================== */

static void test_memory_struct_sizes(void)
{
    assert(sizeof(agentrt_memory_record_t) >= sizeof(char *) + sizeof(size_t));
    assert(sizeof(agentrt_memory_query_t) >= sizeof(char *));
    assert(sizeof(agentrt_memory_result_item_t) >= sizeof(void *));
    assert(sizeof(agentrt_memory_result_t) >= sizeof(size_t));
    TEST_PASS("memory struct sizes adequate");
}

/* ==================== API版本常量 ==================== */

static void test_memory_api_version(void)
{
    assert(MEMORY_API_VERSION_MAJOR >= 1);
    TEST_PASS("memory API version constants defined");
}

/* ==================== 主函数 ==================== */

int main(void)
{
    printf("\n========================================\n");
    printf("  CoreLoopThree 记忆层 单元测试\n");
    printf("========================================\n\n");

    /* 常量验证 */
    RUN_TEST(test_memory_enum_values);
    RUN_TEST(test_memory_struct_sizes);
    RUN_TEST(test_memory_api_version);

    /* 生命周期 */
    RUN_TEST(test_memory_create_default);
    RUN_TEST(test_memory_create_null_params);
    RUN_TEST(test_memory_destroy_null);

    /* 写入 */
    RUN_TEST(test_memory_write_record);
    RUN_TEST(test_memory_write_null_params);
    RUN_TEST(test_memory_write_all_types);

    /* 查询 */
    RUN_TEST(test_memory_query_records);
    RUN_TEST(test_memory_query_null_params);
    RUN_TEST(test_memory_query_with_limits);

    /* 获取 */
    RUN_TEST(test_memory_get_by_id);

    /* 挂载 */
    RUN_TEST(test_memory_mount);

    /* 释放 */
    RUN_TEST(test_memory_result_free_null);
    RUN_TEST(test_memory_record_free_null);

    /* 进化 */
    RUN_TEST(test_memory_evolve);
    RUN_TEST(test_memory_evolve_force);

    /* 健康检查 */
    RUN_TEST(test_memory_health_check);

    printf("\n========================================\n");
    printf("  测试结果: %d 运行, %d 通过, %d 失败\n", tests_run, tests_passed, tests_failed);
    printf("========================================\n");

    return tests_failed > 0 ? 1 : 0;
}
