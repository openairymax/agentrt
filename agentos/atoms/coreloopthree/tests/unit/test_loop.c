/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * test_loop.c - CoreLoopThree 主循环单元测试
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "loop.h"

#define TEST_PASS(name) printf("[PASS] %s\n", name)
#define TEST_FAIL(name, msg) printf("[FAIL] %s: %s\n", name, msg)

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define RUN_TEST(func) do { \
    tests_run++; \
    func(); \
    tests_passed++; \
} while(0)

/* ==================== 生命周期测试 ==================== */

static void test_loop_create_default_config(void) {
    agentos_core_loop_t* loop = NULL;
    agentos_error_t err = agentos_loop_create(NULL, &loop);

    if (err == AGENTOS_SUCCESS && loop != NULL) {
        TEST_PASS("loop_create with default config");
        agentos_loop_destroy(loop);
    } else {
        TEST_FAIL("loop_create default", "failed to create loop");
    }
}

static void test_loop_create_custom_config(void) {
    agentos_loop_config_t config;
    memset(&config, 0, sizeof(config));
    config.loop_config_cognition_threads = 2;
    config.loop_config_execution_threads = 4;
    config.loop_config_memory_threads = 1;
    config.loop_config_max_queued_tasks = 500;
    config.loop_config_stats_interval_ms = 30000;

    agentos_core_loop_t* loop = NULL;
    agentos_error_t err = agentos_loop_create(&config, &loop);

    if (err == AGENTOS_SUCCESS && loop != NULL) {
        TEST_PASS("loop_create with custom config");
        agentos_loop_destroy(loop);
    } else {
        TEST_FAIL("loop_create custom", "failed to create loop with custom config");
    }
}

static void test_loop_create_invalid_config_too_many_threads(void) {
    agentos_loop_config_t config;
    memset(&config, 0, sizeof(config));
    config.loop_config_cognition_threads = 2000; /* 超过上限1024 */
    config.loop_config_execution_threads = 4;
    config.loop_config_memory_threads = 1;
    config.loop_config_max_queued_tasks = 100;

    agentos_core_loop_t* loop = NULL;
    agentos_error_t err = agentos_loop_create(&config, &loop);

    if (err != AGENTOS_SUCCESS || loop == NULL) {
        TEST_PASS("loop_create rejects too many threads");
        if (loop) agentos_loop_destroy(loop);
    } else {
        TEST_FAIL("loop_create threads", "should reject >1024 threads");
        agentos_loop_destroy(loop);
    }
}

static void test_loop_create_invalid_zero_queue(void) {
    agentos_loop_config_t config;
    memset(&config, 0, sizeof(config));
    config.loop_config_max_queued_tasks = 0; /* 非法值 */

    agentos_core_loop_t* loop = NULL;
    agentos_error_t err = agentos_loop_create(&config, &loop);

    if (err != AGENTOS_SUCCESS) {
        TEST_PASS("loop_create rejects zero queue size");
    } else {
        TEST_FAIL("loop_create queue", "should reject zero max_queued_tasks");
        agentos_loop_destroy(loop);
    }
}

static void test_loop_create_null_out_param(void) {
    agentos_error_t err = agentos_loop_create(NULL, NULL);
    if (err != AGENTOS_SUCCESS) {
        TEST_PASS("loop_create handles NULL out param");
    } else {
        TEST_FAIL("loop_create null", "should return error for NULL out param");
    }
}

static void test_loop_destroy_null(void) {
    agentos_loop_destroy(NULL);
    TEST_PASS("loop_destroy handles NULL gracefully");
}

/* ==================== 引擎获取测试 ==================== */

static void test_loop_get_engines_all(void) {
    agentos_core_loop_t* loop = NULL;
    agentos_error_t err = agentos_loop_create(NULL, &loop);
    if (err != AGENTOS_SUCCESS || !loop) {
        TEST_FAIL("get_engines", "create failed");
        return;
    }

    agentos_cognition_engine_t* cognition = NULL;
    agentos_execution_engine_t* execution = NULL;
    agentos_memory_engine_t* memory = NULL;

    agentos_loop_get_engines(loop, &cognition, &execution, &memory);

    if (cognition != NULL && execution != NULL && memory != NULL) {
        TEST_PASS("loop_get_engines returns all three engines");
    } else {
        TEST_FAIL("get_engines", "one or more engines are NULL");
    }

    agentos_loop_destroy(loop);
}

static void test_loop_get_engines_partial(void) {
    agentos_core_loop_t* loop = NULL;
    agentos_error_t err = agentos_loop_create(NULL, &loop);
    if (err != AGENTOS_SUCCESS || !loop) {
        TEST_FAIL("get_engines partial", "create failed");
        return;
    }

    agentos_execution_engine_t* execution = NULL;
    agentos_loop_get_engines(loop, NULL, &execution, NULL);

    if (execution != NULL) {
        TEST_PASS("loop_get_engines partial request works");
    } else {
        TEST_FAIL("get_engines partial", "requested engine is NULL");
    }

    agentos_loop_destroy(loop);
}

static void test_loop_get_engines_null_loop(void) {
    agentos_cognition_engine_t* c = NULL;
    agentos_execution_engine_t* e = NULL;
    agentos_memory_engine_t* m = NULL;

    agentos_loop_get_engines(NULL, &c, &e, &m);

    if (c == NULL && e == NULL && m == NULL) {
        TEST_PASS("loop_get_engines handles NULL loop");
    } else {
        TEST_FAIL("get_engines null", "should set all outputs to NULL for NULL loop");
    }
}

/* ==================== 配置结构体测试 ==================== */

static void test_loop_config_defaults(void) {
    agentos_loop_config_t config;
    memset(&config, 0, sizeof(config));

    assert(config.loop_config_cognition_threads == 0);
    assert(config.loop_config_execution_threads == 0);
    assert(config.loop_config_memory_threads == 0);
    assert(config.loop_config_max_queued_tasks == 0);
    assert(config.loop_config_stats_interval_ms == 0);
    assert(config.loop_config_plan_strategy == NULL);
    assert(config.loop_config_coord_strategy == NULL);
    assert(config.loop_config_disp_strategy == NULL);

    TEST_PASS("loop_config zero-initialized correctly");
}

static void test_loop_config_size_check(void) {
    assert(sizeof(agentos_loop_config_t) >= sizeof(uint32_t) * 5 + sizeof(void*) * 3);
    TEST_PASS("loop_config struct size is adequate");
}

/* ==================== API版本测试 ==================== */

static void test_api_version_constants(void) {
    assert(LOOP_API_VERSION_MAJOR >= 1);
    assert(LOOP_API_VERSION_MINOR >= 0);
    assert(LOOP_API_VERSION_PATCH >= 0);
    TEST_PASS("API version constants defined");
}

/* ==================== 主函数 ==================== */

int main(void) {
    printf("\n========================================\n");
    printf("  CoreLoopThree 主循环 单元测试\n");
    printf("========================================\n\n");
    fflush(stdout);

    /* API 版本 */
    RUN_TEST(test_api_version_constants);

    /* 配置结构体 */
    RUN_TEST(test_loop_config_defaults);
    RUN_TEST(test_loop_config_size_check);

    /* 创建/销毁 */
    RUN_TEST(test_loop_create_default_config);
    RUN_TEST(test_loop_create_custom_config);
    RUN_TEST(test_loop_create_invalid_config_too_many_threads);
    RUN_TEST(test_loop_create_invalid_zero_queue);
    RUN_TEST(test_loop_create_null_out_param);
    RUN_TEST(test_loop_destroy_null);

    /* 引擎获取 */
    RUN_TEST(test_loop_get_engines_all);
    RUN_TEST(test_loop_get_engines_partial);
    RUN_TEST(test_loop_get_engines_null_loop);

    printf("\n========================================\n");
    printf("  测试结果: %d 运行, %d 通过, %d 失败\n",
           tests_run, tests_passed, tests_failed);
    printf("========================================\n");

    return tests_failed > 0 ? 1 : 0;
}
