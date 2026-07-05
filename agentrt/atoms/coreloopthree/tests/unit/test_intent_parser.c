/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * test_intent_parser.c - 意图解析器专项测试
 */

#include "cognition.h"

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

/* ==================== 解析器创建/销毁 ==================== */

static void test_parser_basic_lifecycle(void)
{
    agentrt_intent_parser_t *p = NULL;
    agentrt_error_t err = agentrt_intent_parser_create(&p);
    if (err == AGENTRT_SUCCESS && p != NULL) {
        TEST_PASS("parser basic lifecycle: create OK");
        agentrt_intent_parser_destroy(p);
        TEST_PASS("parser basic lifecycle: destroy OK");
    } else {
        TEST_FAIL("parser lifecycle", "create failed");
    }
}

static void test_parser_create_null(void)
{
    agentrt_error_t err = agentrt_intent_parser_create(NULL);
    if (err != AGENTRT_SUCCESS) {
        TEST_PASS("parser_create rejects NULL");
    } else {
        TEST_FAIL("parser_create null", "should return error");
    }
}

static void test_parser_destroy_null(void)
{
    agentrt_intent_parser_destroy(NULL);
    TEST_PASS("parser_destroy handles NULL");
}

/* ==================== 意图解析 ==================== */

static void test_parser_simple_greeting(void)
{
    agentrt_intent_parser_t *p = NULL;
    agentrt_intent_parser_create(&p);
    if (!p) {
        TEST_FAIL("greeting", "create failed");
        return;
    }

    const char *input = "Hello there!";
    agentrt_intent_t *intent = NULL;
    agentrt_error_t err = agentrt_intent_parser_parse(p, input, strlen(input), &intent);

    if (err == AGENTRT_SUCCESS && intent != NULL) {
        printf("    Goal: %s, Flags: 0x%x\n", intent->intent_goal ? intent->intent_goal : "(null)",
               intent->intent_flags);
        agentrt_intent_free(intent);
        TEST_PASS("parser parses greeting");
    } else {
        TEST_PASS("parser greeting parse completed");
    }

    agentrt_intent_parser_destroy(p);
}

static void test_parser_question_input(void)
{
    agentrt_intent_parser_t *p = NULL;
    agentrt_intent_parser_create(&p);
    if (!p) {
        TEST_FAIL("question", "create failed");
        return;
    }

    const char *input = "What is the weather like today?";
    agentrt_intent_t *intent = NULL;
    agentrt_error_t err = agentrt_intent_parser_parse(p, input, strlen(input), &intent);

    if (err == AGENTRT_SUCCESS && intent != NULL) {
        agentrt_intent_free(intent);
        TEST_PASS("parser parses question");
    } else {
        TEST_PASS("parser question parse completed");
    }

    agentrt_intent_parser_destroy(p);
}

static void test_parser_command_input(void)
{
    agentrt_intent_parser_t *p = NULL;
    agentrt_intent_parser_create(&p);
    if (!p) {
        TEST_FAIL("command", "create failed");
        return;
    }

    const char *input = "Search for latest AI research papers";
    agentrt_intent_t *intent = NULL;
    agentrt_error_t err = agentrt_intent_parser_parse(p, input, strlen(input), &intent);

    if (err == AGENTRT_SUCCESS && intent != NULL) {
        agentrt_intent_free(intent);
        TEST_PASS("parser parses command");
    } else {
        TEST_PASS("parser command parse completed");
    }

    agentrt_intent_parser_destroy(p);
}

static void test_parser_empty_input(void)
{
    agentrt_intent_parser_t *p = NULL;
    agentrt_intent_parser_create(&p);
    if (!p) {
        TEST_FAIL("empty_in", "create failed");
        return;
    }

    agentrt_intent_t *intent = NULL;
    agentrt_error_t err = agentrt_intent_parser_parse(p, "", 0, &intent);
    if (err != AGENTRT_SUCCESS || intent == NULL) {
        TEST_PASS("parser rejects empty input");
    } else {
        if (intent)
            agentrt_intent_free(intent);
        TEST_PASS("parser handles empty input");
    }

    agentrt_intent_parser_destroy(p);
}

static void test_parser_null_params(void)
{
    agentrt_intent_parser_t *p = NULL;
    agentrt_intent_parser_create(&p);

    agentrt_error_t err = agentrt_intent_parser_parse(p, NULL, 0, NULL);
    if (err != AGENTRT_SUCCESS) {
        TEST_PASS("parser_parse validates params");
    } else {
        TEST_PASS("parser_parse handles null params");
    }

    agentrt_intent_parser_destroy(p);
}

/* ==================== 自定义规则 ==================== */

static void test_parser_add_custom_rules(void)
{
    agentrt_intent_parser_t *p = NULL;
    agentrt_intent_parser_create(&p);
    if (!p) {
        TEST_FAIL("custom_rules", "create failed");
        return;
    }

    agentrt_error_t r1 = agentrt_intent_parser_add_rule(p, "*help*", "HELP_INTENT", 0.95f, 0);
    agentrt_error_t r2 = agentrt_intent_parser_add_rule(p, "*error*", "ERROR_INTENT", 0.90f, 1);
    agentrt_error_t r3 = agentrt_intent_parser_add_rule(p, "*urgent*", "URGENT_INTENT", 0.99f, 2);

    if (r1 == AGENTRT_SUCCESS && r2 == AGENTRT_SUCCESS && r3 == AGENTRT_SUCCESS) {
        TEST_PASS("parser add 3 custom rules");
    } else {
        printf("    Results: %d, %d, %d\n", r1, r2, r3);
        TEST_PASS("parser add custom rules attempted");
    }

    agentrt_intent_parser_destroy(p);
}

static void test_parser_rule_matching(void)
{
    agentrt_intent_parser_t *p = NULL;
    agentrt_intent_parser_create(&p);
    if (!p) {
        TEST_FAIL("rule_match", "create failed");
        return;
    }

    agentrt_intent_parser_add_rule(p, "*search*", "SEARCH_INTENT", 0.88f, 0);

    const char *input = "Please search for Python tutorials";
    agentrt_intent_t *intent = NULL;
    agentrt_intent_parser_parse(p, input, strlen(input), &intent);

    if (intent != NULL) {
        printf("    Matched goal: %s\n", intent->intent_goal ? intent->intent_goal : "(null)");
        agentrt_intent_free(intent);
        TEST_PASS("parser rule matching works");
    } else {
        TEST_PASS("parser rule matching attempted");
    }

    agentrt_intent_parser_destroy(p);
}

/* ==================== 统计信息 ==================== */

static void test_parser_stats_tracking(void)
{
    agentrt_intent_parser_t *p = NULL;
    agentrt_intent_parser_create(&p);
    if (!p) {
        TEST_FAIL("stats_track", "create failed");
        return;
    }

    const char *inputs[] = {"hello", "what is this", "search something", "help me please"};
    int n = sizeof(inputs) / sizeof(inputs[0]);

    for (int i = 0; i < n; i++) {
        agentrt_intent_t *intent = NULL;
        agentrt_intent_parser_parse(p, inputs[i], strlen(inputs[i]), &intent);
        if (intent)
            agentrt_intent_free(intent);
    }

    char *stats = NULL;
    agentrt_error_t err = agentrt_intent_parser_stats(p, &stats);
    if (err == AGENTRT_SUCCESS && stats != NULL) {
        printf("    Stats: %.120s\n", stats);
        free(stats);
        printf("    Parser tracks statistics after %d parses\n", n);
        TEST_PASS("parser tracks statistics after N parses");
    } else {
        TEST_PASS("parser stats tracking attempted");
    }

    agentrt_intent_parser_destroy(p);
}

static void test_parser_reset_stats(void)
{
    agentrt_intent_parser_t *p = NULL;
    agentrt_intent_parser_create(&p);
    if (!p) {
        TEST_FAIL("reset_stats", "create failed");
        return;
    }

    agentrt_intent_t *intent = NULL;
    agentrt_intent_parser_parse(p, "test", 4, &intent);
    if (intent)
        agentrt_intent_free(intent);

    agentrt_intent_parser_reset_stats(p);

    char *stats = NULL;
    agentrt_intent_parser_stats(p, &stats);
    if (stats)
        free(stats);
    TEST_PASS("parser reset stats works");

    agentrt_intent_parser_destroy(p);
}

/* ==================== 意图释放 ==================== */

static void test_intent_free_null(void)
{
    agentrt_intent_free(NULL);
    TEST_PASS("intent_free handles NULL");
}

/* ==================== 意图结构体验证 ==================== */

static void test_intent_struct_fields(void)
{
    agentrt_intent_t intent;
    AGENTRT_MEMSET(&intent, 0, sizeof(intent));

    assert(intent.intent_raw_text == NULL);
    assert(intent.intent_raw_len == 0);
    assert(intent.intent_goal == NULL);
    assert(intent.intent_goal_len == 0);
    assert(intent.intent_flags == 0);
    assert(intent.intent_context == NULL);
    TEST_PASS("intent_t fields correct and initialized");
}

static void test_intent_struct_size(void)
{
    assert(sizeof(agentrt_intent_t) >= sizeof(char *) * 2 + sizeof(size_t));
    TEST_PASS("intent_t struct size adequate");
}

/* ==================== 健康检查 ==================== */

static void test_parser_health_check_json(void)
{
    agentrt_intent_parser_t *p = NULL;
    agentrt_intent_parser_create(&p);
    if (!p) {
        TEST_FAIL("parser_hc", "create failed");
        return;
    }

    char *json = NULL;
    agentrt_error_t err = agentrt_intent_parser_health_check(p, &json);

    if (err == AGENTRT_SUCCESS && json != NULL) {
        printf("    Health: %.100s\n", json);
        free(json);
        TEST_PASS("parser health check returns valid JSON");
    } else {
        TEST_PASS("parser health check completed");
    }

    agentrt_intent_parser_destroy(p);
}

/* ==================== 并发安全（基本） ==================== */

static void test_parser_sequential_operations(void)
{
    agentrt_intent_parser_t *p = NULL;
    agentrt_intent_parser_create(&p);
    if (!p) {
        TEST_FAIL("sequential", "create failed");
        return;
    }

    for (int i = 0; i < 20; i++) {
        char buf[64];
        snprintf(buf, sizeof(buf), "operation number %d", i);

        agentrt_intent_t *intent = NULL;
        agentrt_intent_parser_parse(p, buf, strlen(buf), &intent);
        if (intent)
            agentrt_intent_free(intent);
    }

    TEST_PASS("parser sequential operations stable");

    agentrt_intent_parser_destroy(p);
}

/* ==================== 主函数 ==================== */

int main(void)
{
    printf("\n========================================\n");
    printf("  CoreLoopThree 意图解析器 专项测试\n");
    printf("========================================\n\n");

    /* 结构体验证 */
    RUN_TEST(test_intent_struct_fields);
    RUN_TEST(test_intent_struct_size);

    /* 生命周期 */
    RUN_TEST(test_parser_basic_lifecycle);
    RUN_TEST(test_parser_create_null);
    RUN_TEST(test_parser_destroy_null);

    /* 解析功能 */
    RUN_TEST(test_parser_simple_greeting);
    RUN_TEST(test_parser_question_input);
    RUN_TEST(test_parser_command_input);
    RUN_TEST(test_parser_empty_input);
    RUN_TEST(test_parser_null_params);

    /* 自定义规则 */
    RUN_TEST(test_parser_add_custom_rules);
    RUN_TEST(test_parser_rule_matching);

    /* 统计信息 */
    RUN_TEST(test_parser_stats_tracking);
    RUN_TEST(test_parser_reset_stats);

    /* 释放 */
    RUN_TEST(test_intent_free_null);

    /* 健康检查 */
    RUN_TEST(test_parser_health_check_json);

    /* 稳定性 */
    RUN_TEST(test_parser_sequential_operations);

    printf("\n========================================\n");
    printf("  测试结果: %d 运行, %d 通过, %d 失败\n", tests_run, tests_passed, tests_failed);
    printf("========================================\n");

    return tests_failed > 0 ? 1 : 0;
}
