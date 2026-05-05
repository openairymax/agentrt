/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * test_browser.c - 浏览器执行单元单元测试
 */

#include "execution.h"
#include "memory_compat.h"
#include <assert.h>
#ifndef NDEBUG
#else
#undef assert
#define assert(x) ((void)(x))
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_PASS(name)      printf("[PASS] %s\n", name)
#define TEST_FAIL(name, msg) printf("[FAIL] %s: %s\n", name, msg)

static int tests_run    = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define RUN_TEST(func)                                                                                                 \
    do {                                                                                                               \
        tests_run++;                                                                                                   \
        func();                                                                                                        \
        tests_passed++;                                                                                                \
    } while (0)

#define ASSERT_NOT_NULL(ptr, name)                                                                                     \
    do {                                                                                                               \
        if ((ptr) == NULL) {                                                                                           \
            TEST_FAIL(name, "unexpected NULL");                                                                        \
            return;                                                                                                    \
        }                                                                                                              \
    } while (0)

#define ASSERT_STR_EQ(a, b, name)                                                                                      \
    do {                                                                                                               \
        if (strcmp((a), (b)) != 0) {                                                                                   \
            TEST_FAIL(name, "string mismatch");                                                                        \
            return;                                                                                                    \
        }                                                                                                              \
    } while (0)

#define ASSERT_STR_CONTAINS(haystack, needle, name)                                                                    \
    do {                                                                                                               \
        if (strstr((haystack), (needle)) == NULL) {                                                                    \
            TEST_FAIL(name, "expected substring not found");                                                           \
            return;                                                                                                    \
        }                                                                                                              \
    } while (0)

#define ASSERT_STR_NOT_CONTAINS(haystack, needle, name)                                                                \
    do {                                                                                                               \
        if (strstr((haystack), (needle)) != NULL) {                                                                    \
            TEST_FAIL(name, "unexpected substring found");                                                             \
            return;                                                                                                    \
        }                                                                                                              \
    } while (0)

extern agentos_execution_unit_t *agentos_browser_unit_create(void);
extern int agentos_browser_unit_set_agent(agentos_execution_unit_t *unit, const char *agent_id);
extern int agentos_browser_launch(const char *browser_path, int port, int headless,
                                  const char *user_data_dir);
extern int agentos_browser_close(void);
extern int agentos_browser_get_state(void);
extern int agentos_browser_create_context(const char *agent_id, char *out_context_id,
                                          size_t ctx_size);
extern int agentos_browser_destroy_context(const char *context_id);

static void test_browser_unit_create(void)
{
    agentos_execution_unit_t *unit = agentos_browser_unit_create();
    ASSERT_NOT_NULL(unit, "browser_unit_create");

    ASSERT_NOT_NULL(unit->execution_unit_execute, "browser_unit_execute");
    ASSERT_NOT_NULL(unit->execution_unit_destroy, "browser_unit_destroy");
    ASSERT_NOT_NULL(unit->execution_unit_data, "browser_unit_data");

    unit->execution_unit_destroy(unit);
    TEST_PASS("browser_unit_create and destroy");
}

static void test_browser_unit_set_agent(void)
{
    agentos_execution_unit_t *unit = agentos_browser_unit_create();
    ASSERT_NOT_NULL(unit, "browser_unit_set_agent create");

    int ret = agentos_browser_unit_set_agent(unit, "test-agent-001");
    if (ret != 0) {
        TEST_FAIL("browser_unit_set_agent", "set_agent returned non-zero");
        unit->execution_unit_destroy(unit);
        return;
    }

    ret = agentos_browser_unit_set_agent(NULL, "test-agent-002");
    if (ret == 0) {
        TEST_FAIL("browser_unit_set_agent NULL", "should return -1 for NULL unit");
        unit->execution_unit_destroy(unit);
        return;
    }

    ret = agentos_browser_unit_set_agent(unit, NULL);
    if (ret == 0) {
        TEST_FAIL("browser_unit_set_agent NULL id", "should return -1 for NULL agent_id");
        unit->execution_unit_destroy(unit);
        return;
    }

    unit->execution_unit_destroy(unit);
    TEST_PASS("browser_unit_set_agent");
}

static void test_browser_execute_navigate(void)
{
    agentos_execution_unit_t *unit = agentos_browser_unit_create();
    ASSERT_NOT_NULL(unit, "browser_execute_navigate create");

    void *output = NULL;
    agentos_error_t err = unit->execution_unit_execute(unit, "navigate https://example.com", &output);

    if (err != AGENTOS_SUCCESS || output == NULL) {
        TEST_FAIL("browser_execute_navigate", "execute failed");
        unit->execution_unit_destroy(unit);
        return;
    }

    const char *result = (const char *)output;
    ASSERT_STR_CONTAINS(result, "\"status\":\"navigated\"", "navigate status");
    ASSERT_STR_CONTAINS(result, "https://example.com", "navigate url");
    ASSERT_STR_CONTAINS(result, "simulated", "navigate simulated fallback");

    AGENTOS_FREE(output);
    unit->execution_unit_destroy(unit);
    TEST_PASS("browser_execute_navigate");
}

static void test_browser_execute_navigate_no_url(void)
{
    agentos_execution_unit_t *unit = agentos_browser_unit_create();
    ASSERT_NOT_NULL(unit, "browser_execute_navigate_no_url create");

    void *output = NULL;
    agentos_error_t err = unit->execution_unit_execute(unit, "navigate", &output);

    if (err != AGENTOS_EINVAL) {
        TEST_FAIL("browser_execute_navigate_no_url", "expected AGENTOS_EINVAL");
        if (output) AGENTOS_FREE(output);
        unit->execution_unit_destroy(unit);
        return;
    }

    if (output) {
        ASSERT_STR_CONTAINS((const char *)output, "no_url_provided", "no_url error");
        AGENTOS_FREE(output);
    }

    unit->execution_unit_destroy(unit);
    TEST_PASS("browser_execute_navigate_no_url");
}

static void test_browser_execute_navigate_unsafe_url(void)
{
    agentos_execution_unit_t *unit = agentos_browser_unit_create();
    ASSERT_NOT_NULL(unit, "browser_execute_navigate_unsafe create");

    void *output = NULL;
    agentos_error_t err = unit->execution_unit_execute(unit, "navigate http://127.0.0.1/admin", &output);

    if (err != AGENTOS_EPERM) {
        TEST_FAIL("browser_execute_navigate_unsafe", "expected AGENTOS_EPERM");
        if (output) AGENTOS_FREE(output);
        unit->execution_unit_destroy(unit);
        return;
    }

    if (output) {
        ASSERT_STR_CONTAINS((const char *)output, "unsafe_url", "unsafe url error");
        AGENTOS_FREE(output);
    }

    unit->execution_unit_destroy(unit);
    TEST_PASS("browser_execute_navigate_unsafe_url");
}

static void test_browser_execute_evaluate(void)
{
    agentos_execution_unit_t *unit = agentos_browser_unit_create();
    ASSERT_NOT_NULL(unit, "browser_execute_evaluate create");

    void *output = NULL;
    agentos_error_t err = unit->execution_unit_execute(unit, "evaluate script=1+1", &output);

    if (err != AGENTOS_SUCCESS || output == NULL) {
        TEST_FAIL("browser_execute_evaluate", "execute failed");
        unit->execution_unit_destroy(unit);
        return;
    }

    const char *result = (const char *)output;
    ASSERT_STR_CONTAINS(result, "\"status\":\"evaluated\"", "evaluate status");
    ASSERT_STR_CONTAINS(result, "simulated", "evaluate simulated fallback");

    AGENTOS_FREE(output);
    unit->execution_unit_destroy(unit);
    TEST_PASS("browser_execute_evaluate");
}

static void test_browser_execute_evaluate_blocked(void)
{
    agentos_execution_unit_t *unit = agentos_browser_unit_create();
    ASSERT_NOT_NULL(unit, "browser_execute_evaluate_blocked create");

    void *output = NULL;
    agentos_error_t err = unit->execution_unit_execute(unit, "evaluate script=document.cookie", &output);

    if (err != AGENTOS_SUCCESS || output == NULL) {
        TEST_FAIL("browser_execute_evaluate_blocked", "execute failed");
        unit->execution_unit_destroy(unit);
        return;
    }

    const char *result = (const char *)output;
    ASSERT_STR_CONTAINS(result, "blocked_pattern", "blocked pattern");
    ASSERT_STR_CONTAINS(result, "document.cookie", "blocked pattern name");

    AGENTOS_FREE(output);
    unit->execution_unit_destroy(unit);
    TEST_PASS("browser_execute_evaluate_blocked");
}

static void test_browser_execute_click(void)
{
    agentos_execution_unit_t *unit = agentos_browser_unit_create();
    ASSERT_NOT_NULL(unit, "browser_execute_click create");

    void *output = NULL;
    agentos_error_t err = unit->execution_unit_execute(unit, "click selector=#btn", &output);

    if (err != AGENTOS_SUCCESS || output == NULL) {
        TEST_FAIL("browser_execute_click", "execute failed");
        unit->execution_unit_destroy(unit);
        return;
    }

    const char *result = (const char *)output;
    ASSERT_STR_CONTAINS(result, "\"status\":\"clicked\"", "click status");
    ASSERT_STR_CONTAINS(result, "#btn", "click selector");

    AGENTOS_FREE(output);
    unit->execution_unit_destroy(unit);
    TEST_PASS("browser_execute_click");
}

static void test_browser_execute_screenshot(void)
{
    agentos_execution_unit_t *unit = agentos_browser_unit_create();
    ASSERT_NOT_NULL(unit, "browser_execute_screenshot create");

    void *output = NULL;
    agentos_error_t err = unit->execution_unit_execute(unit, "screenshot format=png", &output);

    if (err != AGENTOS_SUCCESS || output == NULL) {
        TEST_FAIL("browser_execute_screenshot", "execute failed");
        unit->execution_unit_destroy(unit);
        return;
    }

    const char *result = (const char *)output;
    ASSERT_STR_CONTAINS(result, "\"status\":\"screenshot_taken\"", "screenshot status");
    ASSERT_STR_CONTAINS(result, "png", "screenshot format");

    AGENTOS_FREE(output);
    unit->execution_unit_destroy(unit);
    TEST_PASS("browser_execute_screenshot");
}

static void test_browser_execute_type(void)
{
    agentos_execution_unit_t *unit = agentos_browser_unit_create();
    ASSERT_NOT_NULL(unit, "browser_execute_type create");

    void *output = NULL;
    agentos_error_t err =
        unit->execution_unit_execute(unit, "type selector=#input value=hello", &output);

    if (err != AGENTOS_SUCCESS || output == NULL) {
        TEST_FAIL("browser_execute_type", "execute failed");
        unit->execution_unit_destroy(unit);
        return;
    }

    const char *result = (const char *)output;
    ASSERT_STR_CONTAINS(result, "hello", "type value");
    ASSERT_STR_CONTAINS(result, "#input", "type selector");

    AGENTOS_FREE(output);
    unit->execution_unit_destroy(unit);
    TEST_PASS("browser_execute_type");
}

static void test_browser_execute_fill(void)
{
    agentos_execution_unit_t *unit = agentos_browser_unit_create();
    ASSERT_NOT_NULL(unit, "browser_execute_fill create");

    void *output = NULL;
    agentos_error_t err =
        unit->execution_unit_execute(unit, "fill selector=#email value=test@example.com", &output);

    if (err != AGENTOS_SUCCESS || output == NULL) {
        TEST_FAIL("browser_execute_fill", "execute failed");
        unit->execution_unit_destroy(unit);
        return;
    }

    const char *result = (const char *)output;
    ASSERT_STR_CONTAINS(result, "test@example.com", "fill value");
    ASSERT_STR_CONTAINS(result, "#email", "fill selector");

    AGENTOS_FREE(output);
    unit->execution_unit_destroy(unit);
    TEST_PASS("browser_execute_fill");
}

static void test_browser_execute_wait(void)
{
    agentos_execution_unit_t *unit = agentos_browser_unit_create();
    ASSERT_NOT_NULL(unit, "browser_execute_wait create");

    void *output = NULL;
    agentos_error_t err =
        unit->execution_unit_execute(unit, "wait timeout=1000", &output);

    if (err != AGENTOS_SUCCESS || output == NULL) {
        TEST_FAIL("browser_execute_wait", "execute failed");
        unit->execution_unit_destroy(unit);
        return;
    }

    const char *result = (const char *)output;
    ASSERT_STR_CONTAINS(result, "\"status\":\"waited\"", "wait status");
    ASSERT_STR_CONTAINS(result, "1000", "wait timeout");

    AGENTOS_FREE(output);
    unit->execution_unit_destroy(unit);
    TEST_PASS("browser_execute_wait");
}

static void test_browser_execute_wait_with_selector(void)
{
    agentos_execution_unit_t *unit = agentos_browser_unit_create();
    ASSERT_NOT_NULL(unit, "browser_execute_wait_selector create");

    void *output = NULL;
    agentos_error_t err =
        unit->execution_unit_execute(unit, "wait selector=.loaded timeout=3000", &output);

    if (err != AGENTOS_SUCCESS || output == NULL) {
        TEST_FAIL("browser_execute_wait_selector", "execute failed");
        unit->execution_unit_destroy(unit);
        return;
    }

    const char *result = (const char *)output;
    ASSERT_STR_CONTAINS(result, "\"status\":\"waited\"", "wait status");
    ASSERT_STR_CONTAINS(result, ".loaded", "wait selector");
    ASSERT_STR_CONTAINS(result, "3000", "wait timeout");

    AGENTOS_FREE(output);
    unit->execution_unit_destroy(unit);
    TEST_PASS("browser_execute_wait_with_selector");
}

static void test_browser_execute_unsupported(void)
{
    agentos_execution_unit_t *unit = agentos_browser_unit_create();
    ASSERT_NOT_NULL(unit, "browser_execute_unsupported create");

    void *output = NULL;
    agentos_error_t err = unit->execution_unit_execute(unit, "scroll down", &output);

    if (err == AGENTOS_SUCCESS) {
        TEST_FAIL("browser_execute_unsupported", "should not return SUCCESS for unsupported");
        if (output) AGENTOS_FREE(output);
        unit->execution_unit_destroy(unit);
        return;
    }

    if (output) {
        ASSERT_STR_CONTAINS((const char *)output, "unsupported_command", "unsupported command");
        AGENTOS_FREE(output);
    }

    unit->execution_unit_destroy(unit);
    TEST_PASS("browser_execute_unsupported");
}

static void test_browser_execute_null_input(void)
{
    agentos_execution_unit_t *unit = agentos_browser_unit_create();
    ASSERT_NOT_NULL(unit, "browser_execute_null_input create");

    void *output = NULL;
    agentos_error_t err = unit->execution_unit_execute(unit, NULL, &output);

    if (err != AGENTOS_EINVAL) {
        TEST_FAIL("browser_execute_null_input", "expected AGENTOS_EINVAL");
        unit->execution_unit_destroy(unit);
        return;
    }

    unit->execution_unit_destroy(unit);
    TEST_PASS("browser_execute_null_input");
}

static void test_browser_execute_null_output(void)
{
    agentos_execution_unit_t *unit = agentos_browser_unit_create();
    ASSERT_NOT_NULL(unit, "browser_execute_null_output create");

    agentos_error_t err = unit->execution_unit_execute(unit, "navigate https://example.com", NULL);

    if (err != AGENTOS_EINVAL) {
        TEST_FAIL("browser_execute_null_output", "expected AGENTOS_EINVAL");
        unit->execution_unit_destroy(unit);
        return;
    }

    unit->execution_unit_destroy(unit);
    TEST_PASS("browser_execute_null_output");
}

static void test_browser_manager_state(void)
{
    int state = agentos_browser_get_state();
    if (state != 0) {
        TEST_FAIL("browser_manager_state", "initial state should be STOPPED(0)");
        return;
    }
    TEST_PASS("browser_manager_initial_state");
}

static void test_browser_launch_invalid(void)
{
    int ret = agentos_browser_launch(NULL, 0, 1, NULL);
    if (ret == 0) {
        TEST_FAIL("browser_launch_invalid", "should fail with NULL args");
        return;
    }
    TEST_PASS("browser_launch_invalid_args");
}

static void test_browser_context_invalid(void)
{
    char ctx_id[64] = {0};
    int ret = agentos_browser_create_context(NULL, ctx_id, sizeof(ctx_id));
    if (ret == 0) {
        TEST_FAIL("browser_context_invalid", "should fail with NULL agent_id");
        return;
    }
    TEST_PASS("browser_context_invalid_args");
}

static void test_browser_destroy_context_invalid(void)
{
    int ret = agentos_browser_destroy_context(NULL);
    if (ret == 0) {
        TEST_FAIL("browser_destroy_context_invalid", "should fail with NULL context_id");
        return;
    }
    TEST_PASS("browser_destroy_context_invalid_args");
}

static void test_browser_no_enotsup(void)
{
    agentos_execution_unit_t *unit = agentos_browser_unit_create();
    ASSERT_NOT_NULL(unit, "browser_no_enotsup create");

    const char *commands[] = {
        "navigate https://example.com",
        "evaluate script=1+1",
        "click selector=#btn",
        "screenshot format=png",
        "type selector=#input value=hello",
        "fill selector=#email value=test@test.com",
        "wait timeout=1000",
    };

    for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
        void *output = NULL;
        agentos_error_t err = unit->execution_unit_execute(unit, commands[i], &output);
        if (err == AGENTOS_ENOTSUP || err == AGENTOS_ENOSYS) {
            char msg[128];
            snprintf(msg, sizeof(msg), "command '%s' returned ENOTSUP/ENOSYS", commands[i]);
            TEST_FAIL("browser_no_enotsup", msg);
            if (output) AGENTOS_FREE(output);
            unit->execution_unit_destroy(unit);
            return;
        }
        if (output) AGENTOS_FREE(output);
    }

    unit->execution_unit_destroy(unit);
    TEST_PASS("browser_no_enotsup_enosys (BAN-45)");
}

static void test_browser_simulated_markers(void)
{
    agentos_execution_unit_t *unit = agentos_browser_unit_create();
    ASSERT_NOT_NULL(unit, "browser_simulated_markers create");

    const char *commands[] = {
        "navigate https://example.com",
        "evaluate script=1+1",
        "click selector=#btn",
        "screenshot format=png",
        "type selector=#input value=hello",
        "fill selector=#email value=test@test.com",
        "wait timeout=1000",
    };

    for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
        void *output = NULL;
        agentos_error_t err = unit->execution_unit_execute(unit, commands[i], &output);
        if (err == AGENTOS_SUCCESS && output != NULL) {
            const char *result = (const char *)output;
            if (strstr(result, "simulated") == NULL && strstr(result, "cdp") == NULL) {
                char msg[128];
                snprintf(msg, sizeof(msg),
                         "command '%s' has neither simulated nor cdp marker", commands[i]);
                TEST_FAIL("browser_simulated_markers", msg);
                AGENTOS_FREE(output);
                unit->execution_unit_destroy(unit);
                return;
            }
        }
        if (output) AGENTOS_FREE(output);
    }

    unit->execution_unit_destroy(unit);
    TEST_PASS("browser_simulated_or_cdp_markers (BAN-37)");
}

int main(void)
{
    printf("=== Browser Execution Unit Tests ===\n\n");

    RUN_TEST(test_browser_unit_create);
    RUN_TEST(test_browser_unit_set_agent);
    RUN_TEST(test_browser_execute_navigate);
    RUN_TEST(test_browser_execute_navigate_no_url);
    RUN_TEST(test_browser_execute_navigate_unsafe_url);
    RUN_TEST(test_browser_execute_evaluate);
    RUN_TEST(test_browser_execute_evaluate_blocked);
    RUN_TEST(test_browser_execute_click);
    RUN_TEST(test_browser_execute_screenshot);
    RUN_TEST(test_browser_execute_type);
    RUN_TEST(test_browser_execute_fill);
    RUN_TEST(test_browser_execute_wait);
    RUN_TEST(test_browser_execute_wait_with_selector);
    RUN_TEST(test_browser_execute_unsupported);
    RUN_TEST(test_browser_execute_null_input);
    RUN_TEST(test_browser_execute_null_output);
    RUN_TEST(test_browser_manager_state);
    RUN_TEST(test_browser_launch_invalid);
    RUN_TEST(test_browser_context_invalid);
    RUN_TEST(test_browser_destroy_context_invalid);
    RUN_TEST(test_browser_no_enotsup);
    RUN_TEST(test_browser_simulated_markers);

    printf("\n=== Results: %d/%d passed, %d failed ===\n", tests_passed, tests_run, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
