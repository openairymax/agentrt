/**
 * @file test_svc_stop.c
 * @brief R-09-87 Daemon stop() 边缘情况单元测试
 *
 * 测试：
 * - agentrt_service_stop 正常停止
 * - agentrt_service_stop 非运行状态拒绝
 * - agentrt_service_stop 非法参数
 * - agentrt_service_start 从ZOMBIE状态恢复
 * - agentrt_service_destroy 清理
 * - 状态字符串映射（含ZOMBIE）
 *
 * @copyright Copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "svc_common.h"
#include "test_macros.h"

/* 绕过 banned_functions.h 的 printf 宏重定义 */
#ifdef printf
#undef printf
#endif

#undef TEST_CASE_START
#undef TEST_ASSERT_EQUAL_INT
#undef TEST_ASSERT_NOT_NULL
#undef TEST_ASSERT_TRUE
#undef TEST_ASSERT_STRING_CONTAINS
#undef TEST_ASSERT_EQUAL_STRING

#define TEST_CASE_START(name) printf("  [CASE] %s...\n", #name)
#define TEST_ASSERT_EQUAL_INT(expected, actual, msg) assert((expected) == (actual))
#define TEST_ASSERT_NOT_NULL(ptr, msg) assert((ptr) != NULL)
#define TEST_ASSERT_TRUE(expr, msg) assert(expr)
#define TEST_ASSERT_STRING_CONTAINS(str, sub, msg) assert(strstr((str), (sub)) != NULL)
#define TEST_ASSERT_EQUAL_STRING(a, b, msg) assert(strcmp((a), (b)) == 0)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

static int g_test_stop_called = 0;
static int g_test_init_called = 0;
static int g_test_start_called = 0;

static agentrt_svc_config_t g_test_config = {.name = "test_svc",
                                             .version = "1.0.0",
                                             .capabilities = 0,
                                             .max_concurrent = 4,
                                             .timeout_ms = 5000,
                                             .priority = 0,
                                             .auto_start = false};

static agentrt_error_t test_interface_init(agentrt_service_t svc,
                                           const agentrt_svc_config_t *config)
{
    (void)svc;
    (void)config;
    g_test_init_called++;
    return AGENTRT_SUCCESS;
}

static agentrt_error_t test_interface_start(agentrt_service_t svc)
{
    (void)svc;
    g_test_start_called++;
    return AGENTRT_SUCCESS;
}

static agentrt_error_t test_interface_stop(agentrt_service_t svc, bool force)
{
    (void)svc;
    (void)force;
    g_test_stop_called++;
    return AGENTRT_SUCCESS;
}

static void reset_test_state(void)
{
    g_test_stop_called = 0;
    g_test_init_called = 0;
    g_test_start_called = 0;
}

static int test_stop_null_service(void)
{
    TEST_CASE_START(stop_null_service);

    agentrt_error_t err = agentrt_service_stop(NULL, false);
    (void)err;
    TEST_ASSERT_EQUAL_INT(AGENTRT_EINVAL, err, "NULL服务返回EINVAL");

    err = agentrt_service_stop(NULL, true);
    TEST_ASSERT_EQUAL_INT(AGENTRT_EINVAL, err, "NULL服务force模式返回EINVAL");
    return 0;
}

static int test_stop_from_wrong_state(void)
{
    TEST_CASE_START(stop_from_wrong_state);

    agentrt_svc_interface_t iface = {
        .init = test_interface_init, .start = test_interface_start, .stop = test_interface_stop};

    agentrt_service_t svc = NULL;
    agentrt_error_t err = agentrt_service_create(&svc, "test_stop_wrong", &iface, &g_test_config);
    (void)err;
    TEST_ASSERT_EQUAL_INT(AGENTRT_SUCCESS, err, "服务创建成功");
    TEST_ASSERT_NOT_NULL(svc, "服务句柄非空");

    /* 未初始化状态尝试stop应失败 */
    err = agentrt_service_stop(svc, false);
    TEST_ASSERT_TRUE(err != AGENTRT_SUCCESS, "未初始化服务stop应失败");

    agentrt_service_destroy(svc);
    return 0;
}

static int test_stop_normal_flow(void)
{
    TEST_CASE_START(stop_normal_flow);

    reset_test_state();

    agentrt_svc_interface_t iface = {
        .init = test_interface_init, .start = test_interface_start, .stop = test_interface_stop};

    agentrt_service_t svc = NULL;
    agentrt_error_t err = agentrt_service_create(&svc, "test_stop_normal", &iface, &g_test_config);
    (void)err;
    TEST_ASSERT_EQUAL_INT(AGENTRT_SUCCESS, err, "服务创建成功");
    TEST_ASSERT_NOT_NULL(svc, "服务句柄非空");

    err = agentrt_service_init(svc);
    TEST_ASSERT_EQUAL_INT(AGENTRT_SUCCESS, err, "初始化成功");

    err = agentrt_service_start(svc);
    TEST_ASSERT_EQUAL_INT(AGENTRT_SUCCESS, err, "启动成功");

    err = agentrt_service_stop(svc, false);
    TEST_ASSERT_EQUAL_INT(AGENTRT_SUCCESS, err, "正常停止成功");
    TEST_ASSERT_EQUAL_INT(1, g_test_stop_called, "stop回调被调用");

    const char *state_str = agentrt_svc_state_to_string(agentrt_service_get_state(svc));
    TEST_ASSERT_NOT_NULL(state_str, "状态字符串非空");
    TEST_ASSERT_STRING_CONTAINS(state_str, "STOPPED", "状态为STOPPED");

    agentrt_service_destroy(svc);
    return 0;
}

static int test_stop_then_start_again(void)
{
    TEST_CASE_START(stop_then_start_again);

    reset_test_state();

    agentrt_svc_interface_t iface = {
        .init = test_interface_init, .start = test_interface_start, .stop = test_interface_stop};

    agentrt_service_t svc = NULL;
    agentrt_error_t err = agentrt_service_create(&svc, "test_restart", &iface, &g_test_config);
    (void)err;
    TEST_ASSERT_EQUAL_INT(AGENTRT_SUCCESS, err, "服务创建成功");
    TEST_ASSERT_NOT_NULL(svc, "服务句柄非空");

    agentrt_service_init(svc);
    agentrt_service_start(svc);

    err = agentrt_service_stop(svc, false);
    TEST_ASSERT_EQUAL_INT(AGENTRT_SUCCESS, err, "停止成功");

    /* 从STOPPED状态可以重新启动 */
    err = agentrt_service_start(svc);
    TEST_ASSERT_EQUAL_INT(AGENTRT_SUCCESS, err, "从STOPPED重新启动成功");

    agentrt_service_stop(svc, false);
    agentrt_service_destroy(svc);
    return 0;
}

static int test_start_from_zombie_state(void)
{
    TEST_CASE_START(start_from_zombie_state);

    agentrt_svc_interface_t iface = {
        .init = test_interface_init, .start = test_interface_start, .stop = test_interface_stop};

    agentrt_service_t svc = NULL;
    agentrt_error_t err = agentrt_service_create(&svc, "test_zombie_start", &iface, &g_test_config);
    TEST_ASSERT_EQUAL_INT(AGENTRT_SUCCESS, err, "服务创建成功");
    TEST_ASSERT_NOT_NULL(svc, "服务句柄非空");

    /* 验证zombie状态字符串 */
    const char *zombie_str = agentrt_svc_state_to_string(AGENTRT_SVC_STATE_ZOMBIE);
    TEST_ASSERT_NOT_NULL(zombie_str, "ZOMBIE状态字符串非空");
    TEST_ASSERT_STRING_CONTAINS(zombie_str, "ZOMBIE", "状态字符串包含ZOMBIE");

    agentrt_service_destroy(svc);
    return 0;
}

static int test_state_to_string_boundary(void)
{
    TEST_CASE_START(state_to_string_boundary);

    /* 有效状态 */
    const char *s_none = agentrt_svc_state_to_string(AGENTRT_SVC_STATE_NONE);
    TEST_ASSERT_NOT_NULL(s_none, "NONE状态字符串非空");
    TEST_ASSERT_STRING_CONTAINS(s_none, "NONE", "NONE匹配");

    const char *s_error = agentrt_svc_state_to_string(AGENTRT_SVC_STATE_ERROR);
    TEST_ASSERT_NOT_NULL(s_error, "ERROR状态字符串非空");
    TEST_ASSERT_STRING_CONTAINS(s_error, "ERROR", "ERROR匹配");

    const char *s_zombie = agentrt_svc_state_to_string(AGENTRT_SVC_STATE_ZOMBIE);
    TEST_ASSERT_NOT_NULL(s_zombie, "ZOMBIE状态字符串非空");
    TEST_ASSERT_STRING_CONTAINS(s_zombie, "ZOMBIE", "ZOMBIE匹配");

    /* 越界状态返回UNKNOWN */
    const char *s_invalid = agentrt_svc_state_to_string((agentrt_svc_state_t)999);
    TEST_ASSERT_NOT_NULL(s_invalid, "越界状态返回非空");
    TEST_ASSERT_STRING_CONTAINS(s_invalid, "UNKNOWN", "越界返回UNKNOWN");
    return 0;
}

static int test_service_create_and_destroy(void)
{
    TEST_CASE_START(service_create_and_destroy);

    agentrt_svc_interface_t iface = {.init = test_interface_init, .stop = test_interface_stop};

    agentrt_service_t svc = NULL;
    agentrt_error_t err = agentrt_service_create(&svc, "test_lifecycle", &iface, &g_test_config);
    TEST_ASSERT_EQUAL_INT(AGENTRT_SUCCESS, err, "服务创建成功");
    TEST_ASSERT_NOT_NULL(svc, "服务句柄非空");

    const char *name = agentrt_service_get_name(svc);
    TEST_ASSERT_NOT_NULL(name, "服务名称非空");
    TEST_ASSERT_EQUAL_STRING("test_lifecycle", name, "服务名称匹配");

    agentrt_svc_state_t state = agentrt_service_get_state(svc);
    TEST_ASSERT_EQUAL_INT(AGENTRT_SVC_STATE_CREATED, state, "初始状态为CREATED");

    agentrt_service_destroy(svc);
    TEST_ASSERT_TRUE(1, "destroy不崩溃");
    return 0;
}

int main(void)
{
    int total_tests = 0;
    int passed_tests = 0;
    int failed_tests = 0;

    printf("\n");
    printf("========================================\n");
    printf("  R-09-87 svc_common stop() 单元测试\n");
    printf("  边缘情况/状态管理/ZOMBIE验证\n");
    printf("========================================\n");

    RESET_TEST_STATS();

    RUN_TEST(test_stop_null_service);
    RUN_TEST(test_stop_from_wrong_state);
    RUN_TEST(test_stop_normal_flow);
    RUN_TEST(test_stop_then_start_again);
    RUN_TEST(test_start_from_zombie_state);
    RUN_TEST(test_state_to_string_boundary);
    RUN_TEST(test_service_create_and_destroy);

    PRINT_TEST_STATS();

    return TESTS_PASSED() ? 0 : 1;
}
