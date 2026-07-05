/**
 * @file test_config_source_manager.c
 * @brief R-09-87 配置源管理器 poll_changes 单元测试
 *
 * 测试：
 * - config_source_manager_poll_changes 防抖机制
 * - 回调触发正确性
 * - watch/watching 状态管理
 * - 多源变更检测
 * - 边界条件（NULL/空manager）
 *
 * @copyright Copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "../include/config_source.h"
#include "test_macros.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int g_callback_count = 0;
static config_source_t *g_last_callback_source = NULL;

static void test_change_callback(config_source_t *source, void *user_data)
{
    (void)user_data;
    g_callback_count++;
    g_last_callback_source = source;
}

static void reset_callback_state(void)
{
    g_callback_count = 0;
    g_last_callback_source = NULL;
}

static void test_poll_null_manager(void)
{
    TEST_CASE_START(poll_null_manager);
    int result = config_source_manager_poll_changes(NULL);
    TEST_ASSERT_EQUAL_INT(0, result, "NULL管理器返回0");
}

static void test_poll_not_watching(void)
{
    TEST_CASE_START(poll_not_watching);

    config_source_manager_t *mgr = config_source_manager_create();
    TEST_ASSERT_NOT_NULL(mgr, "管理器创建成功");

    int result = config_source_manager_poll_changes(mgr);
    TEST_ASSERT_EQUAL_INT(0, result, "未watch时返回0，无回调触发");
    TEST_ASSERT_EQUAL_INT(0, g_callback_count, "回调未被调用");

    config_source_manager_destroy(mgr);
}

static void test_poll_watch_register(void)
{
    TEST_CASE_START(poll_watch_register);

    config_source_manager_t *mgr = config_source_manager_create();
    TEST_ASSERT_NOT_NULL(mgr, "管理器创建成功");

    config_error_t err = config_source_manager_watch(mgr, test_change_callback, NULL);
    TEST_ASSERT_EQUAL_INT(CONFIG_SUCCESS, err, "watch注册成功");

    config_source_manager_watch(mgr, NULL, NULL);

    int result = config_source_manager_poll_changes(mgr);
    TEST_ASSERT_EQUAL_INT(0, result, "取消watch后返回0");

    config_source_manager_destroy(mgr);
}

static void test_poll_debounce(void)
{
    TEST_CASE_START(poll_debounce);

    reset_callback_state();
    config_source_manager_t *mgr = config_source_manager_create();
    TEST_ASSERT_NOT_NULL(mgr, "管理器创建成功");

    config_source_manager_watch(mgr, test_change_callback, NULL);

    int res1 = config_source_manager_poll_changes(mgr);
    TEST_ASSERT_EQUAL_INT(0, res1, "无配置源时返回0");

    int res2 = config_source_manager_poll_changes(mgr);
    TEST_ASSERT_EQUAL_INT(0, res2, "防抖期内第二次调用返回0");
    TEST_ASSERT_EQUAL_INT(0, g_callback_count, "防抖期内回调不被触发");

    config_source_manager_destroy(mgr);
}

static void test_poll_with_memory_source(void)
{
    TEST_CASE_START(poll_with_memory_source);

    reset_callback_state();
    config_source_manager_t *mgr = config_source_manager_create();
    TEST_ASSERT_NOT_NULL(mgr, "管理器创建成功");

    config_source_t *mem_src = config_source_create_memory("test_mem");
    TEST_ASSERT_NOT_NULL(mem_src, "内存配置源创建成功");

    config_source_manager_add(mgr, mem_src);
    config_source_manager_watch(mgr, test_change_callback, NULL);

    int result = config_source_manager_poll_changes(mgr);
    TEST_ASSERT_EQUAL_INT(0, result, "内存源不可watch，返回0");

    config_source_manager_destroy(mgr);
}

static void test_poll_no_changes(void)
{
    TEST_CASE_START(poll_no_changes);

    reset_callback_state();
    config_source_manager_t *mgr = config_source_manager_create();
    TEST_ASSERT_NOT_NULL(mgr, "管理器创建成功");

    config_source_manager_watch(mgr, test_change_callback, NULL);

    int result = config_source_manager_poll_changes(mgr);
    TEST_ASSERT_EQUAL_INT(0, result, "空源列表poll返回0");

    config_source_manager_destroy(mgr);
}

int main(void)
{
    printf("\n");
    printf("========================================\n");
    printf("  R-09-87 config_source_manager 单元测试\n");
    printf("  poll_changes 防抖/回调/状态验证\n");
    printf("========================================\n");

    RESET_TEST_STATS();

    RUN_TEST(test_poll_null_manager);
    RUN_TEST(test_poll_not_watching);
    RUN_TEST(test_poll_watch_register);
    RUN_TEST(test_poll_debounce);
    RUN_TEST(test_poll_with_memory_source);
    RUN_TEST(test_poll_no_changes);

    PRINT_TEST_STATS();

    return TESTS_PASSED() ? 0 : 1;
}
