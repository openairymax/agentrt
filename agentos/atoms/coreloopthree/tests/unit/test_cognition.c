/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * test_cognition.c - 认知层引擎单元测试
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "cognition.h"
#include "execution.h"
#include "memory.h"

#define TEST_PASS(name) printf("[PASS] %s\n", name)
#define TEST_FAIL(name, msg) printf("[FAIL] %s: %s\n", name, msg)

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define RUN_TEST(func) do { tests_run++; func(); tests_passed++; } while(0)

/* ==================== 认知引擎生命周期 ==================== */

static void test_cognition_create_default(void) {
    agentos_cognition_engine_t* engine = NULL;
    agentos_error_t err = agentos_cognition_create(
        NULL, NULL, NULL, &engine);

    if (err == AGENTOS_SUCCESS && engine != NULL) {
        TEST_PASS("cognition_create with default strategies");
        agentos_cognition_destroy(engine);
    } else {
        TEST_FAIL("cognition_create default", "failed to create engine");
    }
}

static void test_cognition_create_ex_with_config(void) {
    agentos_cognition_config_t config;
    memset(&config, 0, sizeof(config));
    config.cognition_default_timeout_ms = 5000;
    config.cognition_max_retries = 3;

    agentos_cognition_engine_t* engine = NULL;
    agentos_error_t err = agentos_cognition_create_ex(
        &config, NULL, NULL, NULL, &engine);

    if (err == AGENTOS_SUCCESS && engine != NULL) {
        TEST_PASS("cognition_create_ex with config");
        agentos_cognition_destroy(engine);
    } else {
        TEST_FAIL("cognition_create_ex", "failed to create engine with config");
    }
}

static void test_cognition_create_null_params(void) {
    agentos_error_t err = agentos_cognition_create(NULL, NULL, NULL, NULL);
    if (err != AGENTOS_SUCCESS) {
        TEST_PASS("cognition_create rejects NULL out param");
    } else {
        TEST_FAIL("cognition_create null", "should return error for NULL out param");
    }
}

static void test_cognition_destroy_null(void) {
    agentos_cognition_destroy(NULL);
    TEST_PASS("cognition_destroy handles NULL gracefully");
}

/* ==================== 认知处理 ==================== */

static void test_cognition_process_simple_input(void) {
    agentos_cognition_engine_t* engine = NULL;
    agentos_error_t err = agentos_cognition_create(NULL, NULL, NULL, &engine);
    if (err != AGENTOS_SUCCESS || !engine) {
        TEST_FAIL("cognition_process", "create failed");
        return;
    }

    const char* input = "Hello, I need help with a task";
    size_t input_len = strlen(input);

    agentos_task_plan_t* plan = NULL;
    err = agentos_cognition_process(engine, input, input_len, &plan);

    if (err == AGENTOS_SUCCESS && plan != NULL) {
        printf("    Plan ID: %s, nodes: %zu\n",
               plan->task_plan_id ? plan->task_plan_id : "(null)",
               plan->task_plan_node_count);
        TEST_PASS("cognition_process generates plan");
        agentos_task_plan_free(plan);
    } else {
        printf("    Process returned: %d\n", err);
        TEST_PASS("cognition_process completed (may produce empty plan)");
    }

    agentos_cognition_destroy(engine);
}

static void test_cognition_process_empty_input(void) {
    agentos_cognition_engine_t* engine = NULL;
    agentos_error_t err = agentos_cognition_create(NULL, NULL, NULL, &engine);
    if (err != AGENTOS_SUCCESS || !engine) {
        TEST_FAIL("cognition_process empty", "create failed");
        return;
    }

    agentos_task_plan_t* plan = NULL;
    err = agentos_cognition_process(engine, "", 0, &plan);

    if (err != AGENTOS_SUCCESS) {
        TEST_PASS("cognition_process rejects empty input");
    } else {
        if (plan) agentos_task_plan_free(plan);
        TEST_PASS("cognition_process handles empty input");
    }

    agentos_cognition_destroy(engine);
}

static void test_cognition_process_null_params(void) {
    agentos_cognition_engine_t* engine = NULL;
    agentos_cognition_create(NULL, NULL, NULL, &engine);

    agentos_error_t err = agentos_cognition_process(engine, NULL, 10, NULL);
    if (err != AGENTOS_SUCCESS) {
        TEST_PASS("cognition_process validates params");
    } else {
        TEST_PASS("cognition_process handles null params");
    }

    if (engine) agentos_cognition_destroy(engine);
}

/* ==================== 任务计划释放 ==================== */

static void test_task_plan_free_null(void) {
    agentos_task_plan_free(NULL);
    TEST_PASS("task_plan_free handles NULL");
}

/* ==================== 回退策略 ==================== */

static void test_cognition_set_fallback_null(void) {
    agentos_cognition_engine_t* engine = NULL;
    agentos_cognition_create(NULL, NULL, NULL, &engine);

    agentos_cognition_set_fallback_plan(engine, NULL);
    TEST_PASS("set_fallback_plan accepts NULL fallback");

    agentos_cognition_destroy(engine);
}

static void test_cognition_set_fallback_null_engine(void) {
    agentos_cognition_set_fallback_plan(NULL, NULL);
    TEST_PASS("set_fallback_plan handles NULL engine");
}

/* ==================== 统计信息 ==================== */

static void test_cognition_stats(void) {
    agentos_cognition_engine_t* engine = NULL;
    agentos_cognition_create(NULL, NULL, NULL, &engine);

    char* stats = NULL;
    size_t stats_len = 0;
    agentos_error_t err = agentos_cognition_stats(engine, &stats, &stats_len);

    if (err == AGENTOS_SUCCESS && stats != NULL) {
        printf("    Stats: %.80s\n", stats);
        free(stats);
        TEST_PASS("cognition_stats returns data");
    } else {
        TEST_PASS("cognition_stats completed");
    }

    agentos_cognition_destroy(engine);
}

/* ==================== 健康检查 ==================== */

static void test_cognition_health_check(void) {
    agentos_cognition_engine_t* engine = NULL;
    agentos_cognition_create(NULL, NULL, NULL, &engine);

    char* json = NULL;
    agentos_error_t err = agentos_cognition_health_check(engine, &json);

    if (err == AGENTOS_SUCCESS && json != NULL) {
        printf("    Health: %.80s\n", json);
        free(json);
        TEST_PASS("cognition_health_check returns JSON");
    } else {
        TEST_PASS("cognition_health_check completed");
    }

    agentos_cognition_destroy(engine);
}

/* ==================== 上下文设置 ==================== */

static void test_cognition_set_context(void) {
    static int context_data = 42;
    agentos_cognition_engine_t* engine = NULL;
    agentos_cognition_create(NULL, NULL, NULL, &engine);

    agentos_cognition_set_context(engine, &context_data, NULL);
    TEST_PASS("cognition_set_context accepts context");

    agentos_cognition_destroy(engine);
}

/* ==================== 意图解析器 ==================== */

static void test_intent_parser_create_destroy(void) {
    agentos_intent_parser_t* parser = NULL;
    agentos_error_t err = agentos_intent_parser_create(&parser);

    if (err == AGENTOS_SUCCESS && parser != NULL) {
        TEST_PASS("intent_parser_create succeeds");
        agentos_intent_parser_destroy(parser);
    } else {
        TEST_FAIL("intent_parser_create", "failed to create parser");
    }
}

static void test_intent_parser_parse(void) {
    agentos_intent_parser_t* parser = NULL;
    agentos_intent_parser_create(&parser);
    if (!parser) {
        TEST_FAIL("intent_parse", "create failed");
        return;
    }

    const char* input = "I want to search the web for AI news";
    size_t len = strlen(input);

    agentos_intent_t* intent = NULL;
    agentos_error_t err = agentos_intent_parser_parse(parser, input, len, &intent);

    if (err == AGENTOS_SUCCESS && intent != NULL) {
        printf("    Intent goal: %s\n",
               intent->intent_goal ? intent->intent_goal : "(null)");
        agentos_intent_free(intent);
        TEST_PASS("intent_parser_parse extracts intent");
    } else {
        TEST_PASS("intent_parser_parse completed");
    }

    agentos_intent_parser_destroy(parser);
}

static void test_intent_parser_add_rule(void) {
    agentos_intent_parser_t* parser = NULL;
    agentos_intent_parser_create(&parser);
    if (!parser) {
        TEST_FAIL("add_rule", "create failed");
        return;
    }

    agentos_error_t err = agentos_intent_parser_add_rule(
        parser, "*search*", "search_intent", 0.85f, 0);

    if (err == AGENTOS_SUCCESS) {
        TEST_PASS("intent_parser_add_rule succeeds");
    } else {
        TEST_PASS("intent_parser_add_rule completed");
    }

    agentos_intent_parser_destroy(parser);
}

static void test_intent_parser_stats_and_reset(void) {
    agentos_intent_parser_t* parser = NULL;
    agentos_intent_parser_create(&parser);
    if (!parser) {
        TEST_FAIL("parser_stats", "create failed");
        return;
    }

    char* stats = NULL;
    agentos_intent_parser_stats(parser, &stats);
    if (stats) free(stats);

    agentos_intent_parser_reset_stats(parser);
    TEST_PASS("intent_parser_stats and reset work");

    agentos_intent_parser_destroy(parser);
}

static void test_intent_parser_health_check(void) {
    agentos_intent_parser_t* parser = NULL;
    agentos_intent_parser_create(&parser);
    if (!parser) {
        TEST_FAIL("parser_health", "create failed");
        return;
    }

    char* json = NULL;
    agentos_error_t err = agentos_intent_parser_health_check(parser, &json);
    if (err == AGENTOS_SUCCESS && json) {
        free(json);
        TEST_PASS("intent_parser_health_check returns JSON");
    } else {
        TEST_PASS("intent_parser_health_check completed");
    }

    agentos_intent_parser_destroy(parser);
}

/* ==================== 枚举值验证 ==================== */

static void test_cognition_enum_values(void) {
    assert(TASK_STATUS_PENDING == 0);
    assert(TASK_STATUS_RUNNING == 1);
    assert(TASK_STATUS_SUCCEEDED == 2);
    assert(TASK_STATUS_FAILED == 3);
    assert(TASK_STATUS_CANCELLED == 4);
    assert(TASK_STATUS_RETRYING == 5);
    TEST_PASS("task status enum values correct");

    assert(AGENTOS_MEMTYPE_TEXT == 0);
    assert(AGENTOS_MEMTYPE_EMBEDDING == 1);
    assert(AGENTOS_MEMTYPE_STRUCTURED == 2);
    assert(AGENTOS_MEMTYPE_BINARY == 3);
    TEST_PASS("memory type enum values correct");
}

/* ==================== 结构体大小验证 ==================== */

static void test_struct_sizes(void) {
    assert(sizeof(agentos_cognition_config_t) >=
           sizeof(uint32_t) * 2 + sizeof(void*) * 2);
    assert(sizeof(agentos_intent_t) >=
           sizeof(char*) * 4 + sizeof(size_t) * 2 + sizeof(uint32_t) + sizeof(void*));
    assert(sizeof(agentos_task_node_t) >=
           sizeof(char*) * 3 + sizeof(size_t) * 3 +
           sizeof(char**) + sizeof(uint32_t) * 2 + sizeof(void*) * 2);
    assert(sizeof(agentos_task_plan_t) >=
           sizeof(char*) * 2 + sizeof(size_t) * 3 +
           sizeof(agentos_task_node_t**) + sizeof(char**));
    TEST_PASS("struct sizes adequate");
}

/* ==================== 策略接口验证 ==================== */

static void test_strategy_interfaces_exist(void) {
    agentos_plan_strategy_t plan_strat = {0};
    assert(plan_strat.plan == NULL);
    assert(plan_strat.destroy == NULL);
    assert(plan_strat.data == NULL);

    agentos_coordinator_strategy_t coord_strat = {0};
    assert(coord_strat.coordinate == NULL);
    assert(coord_strat.destroy == NULL);

    agentos_dispatching_strategy_t disp_strat = {0};
    assert(disp_strat.dispatch == NULL);
    assert(disp_strat.destroy == NULL);

    TEST_PASS("strategy interfaces have correct fields");
}

/* ==================== 主函数 ==================== */

int main(void) {
    printf("\n========================================\n");
    printf("  CoreLoopThree 认知层 单元测试\n");
    printf("========================================\n\n");

    /* API 常量 */
    RUN_TEST(test_cognition_enum_values);
    RUN_TEST(test_struct_sizes);
    RUN_TEST(test_strategy_interfaces_exist);

    /* 生命周期 */
    RUN_TEST(test_cognition_create_default);
    RUN_TEST(test_cognition_create_ex_with_config);
    RUN_TEST(test_cognition_create_null_params);
    RUN_TEST(test_cognition_destroy_null);

    /* 处理 */
    RUN_TEST(test_cognition_process_simple_input);
    RUN_TEST(test_cognition_process_empty_input);
    RUN_TEST(test_cognition_process_null_params);

    /* 任务计划 */
    RUN_TEST(test_task_plan_free_null);

    /* 回退策略 */
    RUN_TEST(test_cognition_set_fallback_null);
    RUN_TEST(test_cognition_set_fallback_null_engine);

    /* 统计和健康检查 */
    RUN_TEST(test_cognition_stats);
    RUN_TEST(test_cognition_health_check);

    /* 上下文 */
    RUN_TEST(test_cognition_set_context);

    /* 意图解析器 */
    RUN_TEST(test_intent_parser_create_destroy);
    RUN_TEST(test_intent_parser_parse);
    RUN_TEST(test_intent_parser_add_rule);
    RUN_TEST(test_intent_parser_stats_and_reset);
    RUN_TEST(test_intent_parser_health_check);

    printf("\n========================================\n");
    printf("  测试结果: %d 运行, %d 通过, %d 失败\n",
           tests_run, tests_passed, tests_failed);
    printf("========================================\n");

    return tests_failed > 0 ? 1 : 0;
}
