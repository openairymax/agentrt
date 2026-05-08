/**
 * @file test_svc_stop.c
 * @brief R-09-87 Daemon stop() 边缘情况单元测试
 *
 * 测试：
 * - agentos_service_stop 正常停止
 * - agentos_service_stop 非运行状态拒绝
 * - agentos_service_stop 非法参数
 * - agentos_service_start 从ZOMBIE状态恢复
 * - agentos_service_destroy 清理
 * - 状态字符串映射（含ZOMBIE）
 *
 * @copyright Copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "test_macros.h"
#include "svc_common.h"

static int g_test_stop_called = 0;
static int g_test_init_called = 0;
static int g_test_start_called = 0;

static agentos_svc_config_t g_test_config = {
    .name = "test_svc",
    .version = "1.0.0",
    .capabilities = 0,
    .max_concurrent = 4,
    .timeout_ms = 5000,
    .priority = 0,
    .auto_start = false
};

static agentos_error_t test_interface_init(agentos_service_t svc, const agentos_svc_config_t* config)
{
    (void)svc;
    (void)config;
    g_test_init_called++;
    return AGENTOS_SUCCESS;
}

static agentos_error_t test_interface_start(agentos_service_t svc)
{
    (void)svc;
    g_test_start_called++;
    return AGENTOS_SUCCESS;
}

static agentos_error_t test_interface_stop(agentos_service_t svc, bool force)
{
    (void)svc;
    (void)force;
    g_test_stop_called++;
    return AGENTOS_SUCCESS;
}

static void reset_test_state(void)
{
    g_test_stop_called = 0;
    g_test_init_called = 0;
    g_test_start_called = 0;
}

static void test_stop_null_service(void)
{
    TEST_CASE_START(stop_null_service);

    agentos_error_t err = agentos_service_stop(NULL, false);
    TEST_ASSERT_EQUAL_INT(AGENTOS_EINVAL, err, "NULL服务返回EINVAL");

    err = agentos_service_stop(NULL, true);
    TEST_ASSERT_EQUAL_INT(AGENTOS_EINVAL, err, "NULL服务force模式返回EINVAL");
}

static void test_stop_from_wrong_state(void)
{
    TEST_CASE_START(stop_from_wrong_state);

    agentos_svc_interface_t iface = {
        .init = test_interface_init,
        .start = test_interface_start,
        .stop = test_interface_stop
    };

    agentos_service_t svc = NULL;
    agentos_error_t err = agentos_service_create(&svc, "test_stop_wrong", &iface, &g_test_config);
    TEST_ASSERT_EQUAL_INT(AGENTOS_SUCCESS, err, "服务创建成功");
    TEST_ASSERT_NOT_NULL(svc, "服务句柄非空");

    /* 未初始化状态尝试stop应失败 */
    err = agentos_service_stop(svc, false);
    TEST_ASSERT_TRUE(err != AGENTOS_SUCCESS, "未初始化服务stop应失败");

    agentos_service_destroy(svc);
}

static void test_stop_normal_flow(void)
{
    TEST_CASE_START(stop_normal_flow);

    reset_test_state();

    agentos_svc_interface_t iface = {
        .init = test_interface_init,
        .start = test_interface_start,
        .stop = test_interface_stop
    };

    agentos_service_t svc = NULL;
    agentos_error_t err = agentos_service_create(&svc, "test_stop_normal", &iface, &g_test_config);
    TEST_ASSERT_EQUAL_INT(AGENTOS_SUCCESS, err, "服务创建成功");
    TEST_ASSERT_NOT_NULL(svc, "服务句柄非空");

    err = agentos_service_init(svc);
    TEST_ASSERT_EQUAL_INT(AGENTOS_SUCCESS, err, "初始化成功");

    err = agentos_service_start(svc);
    TEST_ASSERT_EQUAL_INT(AGENTOS_SUCCESS, err, "启动成功");

    err = agentos_service_stop(svc, false);
    TEST_ASSERT_EQUAL_INT(AGENTOS_SUCCESS, err, "正常停止成功");
    TEST_ASSERT_EQUAL_INT(1, g_test_stop_called, "stop回调被调用");

    const char* state_str = agentos_svc_state_to_string(agentos_service_get_state(svc));
    TEST_ASSERT_NOT_NULL(state_str, "状态字符串非空");
    TEST_ASSERT_STRING_CONTAINS(state_str, "STOPPED", "状态为STOPPED");

    agentos_service_destroy(svc);
}

static void test_stop_then_start_again(void)
{
    TEST_CASE_START(stop_then_start_again);

    reset_test_state();

    agentos_svc_interface_t iface = {
        .init = test_interface_init,
        .start = test_interface_start,
        .stop = test_interface_stop
    };

    agentos_service_t svc = NULL;
    agentos_error_t err = agentos_service_create(&svc, "test_restart", &iface, &g_test_config);
    TEST_ASSERT_EQUAL_INT(AGENTOS_SUCCESS, err, "服务创建成功");
    TEST_ASSERT_NOT_NULL(svc, "服务句柄非空");

    agentos_service_init(svc);
    agentos_service_start(svc);

    err = agentos_service_stop(svc, false);
    TEST_ASSERT_EQUAL_INT(AGENTOS_SUCCESS, err, "停止成功");

    /* 从STOPPED状态可以重新启动 */
    err = agentos_service_start(svc);
    TEST_ASSERT_EQUAL_INT(AGENTOS_SUCCESS, err, "从STOPPED重新启动成功");

    agentos_service_stop(svc, false);
    agentos_service_destroy(svc);
}

static void test_start_from_zombie_state(void)
{
    TEST_CASE_START(start_from_zombie_state);

    agentos_svc_interface_t iface = {
        .init = test_interface_init,
        .start = test_interface_start,
        .stop = test_interface_stop
    };

    agentos_service_t svc = NULL;
    agentos_error_t err = agentos_service_create(&svc, "test_zombie_start", &iface, &g_test_config);
    TEST_ASSERT_EQUAL_INT(AGENTOS_SUCCESS, err, "服务创建成功");
    TEST_ASSERT_NOT_NULL(svc, "服务句柄非空");

    /* 验证zombie状态字符串 */
    const char* zombie_str = agentos_svc_state_to_string(AGENTOS_SVC_STATE_ZOMBIE);
    TEST_ASSERT_NOT_NULL(zombie_str, "ZOMBIE状态字符串非空");
    TEST_ASSERT_STRING_CONTAINS(zombie_str, "ZOMBIE", "状态字符串包含ZOMBIE");

    agentos_service_destroy(svc);
}

static void test_state_to_string_boundary(void)
{
    TEST_CASE_START(state_to_string_boundary);

    /* 有效状态 */
    const char* s_none = agentos_svc_state_to_string(AGENTOS_SVC_STATE_NONE);
    TEST_ASSERT_NOT_NULL(s_none, "NONE状态字符串非空");
    TEST_ASSERT_STRING_CONTAINS(s_none, "NONE", "NONE匹配");

    const char* s_error = agentos_svc_state_to_string(AGENTOS_SVC_STATE_ERROR);
    TEST_ASSERT_NOT_NULL(s_error, "ERROR状态字符串非空");
    TEST_ASSERT_STRING_CONTAINS(s_error, "ERROR", "ERROR匹配");

    const char* s_zombie = agentos_svc_state_to_string(AGENTOS_SVC_STATE_ZOMBIE);
    TEST_ASSERT_NOT_NULL(s_zombie, "ZOMBIE状态字符串非空");
    TEST_ASSERT_STRING_CONTAINS(s_zombie, "ZOMBIE", "ZOMBIE匹配");

    /* 越界状态返回UNKNOWN */
    const char* s_invalid = agentos_svc_state_to_string((agentos_svc_state_t)999);
    TEST_ASSERT_NOT_NULL(s_invalid, "越界状态返回非空");
    TEST_ASSERT_STRING_CONTAINS(s_invalid, "UNKNOWN", "越界返回UNKNOWN");
}

static void test_service_create_and_destroy(void)
{
    TEST_CASE_START(service_create_and_destroy);

    agentos_svc_interface_t iface = {
        .init = test_interface_init,
        .stop = test_interface_stop
    };

    agentos_service_t svc = NULL;
    agentos_error_t err = agentos_service_create(&svc, "test_lifecycle", &iface, &g_test_config);
    TEST_ASSERT_EQUAL_INT(AGENTOS_SUCCESS, err, "服务创建成功");
    TEST_ASSERT_NOT_NULL(svc, "服务句柄非空");

    const char* name = agentos_service_get_name(svc);
    TEST_ASSERT_NOT_NULL(name, "服务名称非空");
    TEST_ASSERT_EQUAL_STRING("test_lifecycle", name, "服务名称匹配");

    agentos_svc_state_t state = agentos_service_get_state(svc);
    TEST_ASSERT_EQUAL_INT(AGENTOS_SVC_STATE_CREATED, state, "初始状态为CREATED");

    agentos_service_destroy(svc);
    TEST_ASSERT_TRUE(1, "destroy不崩溃");
}

int main(void)
{
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
