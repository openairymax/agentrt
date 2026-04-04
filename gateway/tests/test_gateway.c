/**
 * @file test_gateway.c
 * @brief Gateway模块单元测试
 *
 * 测试网关模块的核心功能：
 * - 网关创建/销毁生命周期
 * - 公共API接口调用
 * - 类型查询和状态检查
 * - NULL安全验证
 *
 * 设计原则：
 *   E-3 资源确定性：所有分配的资源都有对应释放
 *   K-2 接口契约化：验证所有API符合声明契约
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "gateway.h"

/* ========== 测试辅助宏 ========== */

static int g_tests_run = 0;
static int g_tests_passed = 0;

#define TEST_BEGIN(name) \
    do { \
        printf("  [TEST] %s ... ", name); \
        g_tests_run++; \
    } while(0)

#define TEST_PASS() \
    do { \
        printf("PASS\n"); \
        g_tests_passed++; \
    } while(0)

#define TEST_FAIL(msg) \
    do { \
        printf("FAIL: %s\n", msg); \
    } while(0)

#define ASSERT_TRUE(cond) \
    do { \
        if (!(cond)) { \
            TEST_FAIL(#cond); \
            return; \
        } \
    } while(0)

#define ASSERT_FALSE(cond) ASSERT_TRUE(!(cond))
#define ASSERT_NULL(ptr) ASSERT_TRUE((ptr) == NULL)
#define ASSERT_NOT_NULL(ptr) ASSERT_TRUE((ptr) != NULL)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))

/* ========== 测试用例：网关类型枚举 ========== */

/**
 * @brief 验证网关类型枚举值正确性
 */
static void test_gateway_types(void) {
    TEST_BEGIN("gateway_type_enum_values");

    ASSERT_EQ(GATEWAY_TYPE_HTTP, 0);
    ASSERT_EQ(GATEWAY_TYPE_WS, 1);
    ASSERT_EQ(GATEWAY_TYPE_STDIO, 2);

    /* 验证类型数量 */
    ASSERT_TRUE(GATEWAY_TYPE_STDIO >= 2);

    TEST_PASS();
}

/* ========== 测试用例：错误码枚举 ========== */

/**
 * @brief 验证网关错误码定义
 */
static void test_error_codes(void) {
    TEST_BEGIN("gateway_error_codes");

    ASSERT_EQ(GATEWAY_SUCCESS, 0);
    ASSERT_EQ(GATEWAY_ERROR_INVALID, -1);
    ASSERT_EQ(GATEWAY_ERROR_MEMORY, -2);
    ASSERT_EQ(GATEWAY_ERROR_IO, -3);
    ASSERT_EQ(GATEWAY_ERROR_TIMEOUT, -4);
    ASSERT_EQ(GATEWAY_ERROR_CLOSED, -5);
    ASSERT_EQ(GATEWAY_ERROR_PROTOCOL, -6);

    TEST_PASS();
}

/* ========== 测试用例：NULL安全 ========== */

/**
 * @brief 验证所有公共API对NULL输入的安全性
 *
 * 符合 E-6 错误可追溯原则：无效输入不应导致崩溃
 */
static void test_null_safety(void) {
    TEST_BEGIN("null_pointer_safety");

    /* lifecycle APIs */
    ASSERT_EQ(gateway_start(NULL), AGENTOS_EINVAL);
    ASSERT_EQ(gateway_stop(NULL), AGENTOS_SUCCESS);     /* 静默忽略 */
    ASSERT_EQ(gateway_get_stats(NULL, NULL), AGENTOS_EINVAL);

    /* query APIs */
    ASSERT_FALSE(gateway_is_running(NULL));
    ASSERT_EQ(gateway_get_type(NULL), GATEWAY_TYPE_HTTP);  /* 默认值 */
    ASSERT_EQ(gateway_set_handler(NULL, NULL, NULL), AGENTOS_EINVAL);

    const char* name = gateway_get_name(NULL);
    ASSERT_NOT_NULL(name);
    ASSERT_TRUE(strcmp(name, "unknown") == 0);

    /* create APIs - NULL host 应返回 NULL */
    ASSERT_NULL(gateway_http_create(NULL, 8080));

    TEST_PASS();
}

/* ========== 测试用例：HTTP网关创建存根 ========== */

/**
 * @brief 测试 HTTP 网关创建接口存在性
 *
 * 注意：实际创建需要 libmicrohttpd 运行环境。
 * 此测试仅验证编译链接和基本参数校验。
 */
static void test_http_gateway_create(void) {
    TEST_BEGIN("http_gateway_create_interface");

    /* NULL host 参数应返回 NULL */
    gateway_t* gw_null = gateway_http_create(NULL, 8080);
    ASSERT_NULL(gw_null);

    /* 有效参数（需要运行环境才能成功创建） */
    gateway_t* gw = gateway_http_create("127.0.0.1", 18080);
    if (gw) {
        ASSERT_EQ(gateway_get_type(gw), GATEWAY_TYPE_HTTP);
        ASSERT_FALSE(gateway_is_running(gw));

        const char* name = gateway_get_name(gw);
        ASSERT_NOT_NULL(name);
        ASSERT_TRUE(strcmp(name, "HTTP Gateway") == 0);

        gateway_destroy(gw);
    }

    TEST_PASS();
}

/* ========== 测试用例：WebSocket网关创建存根 ========== */

/**
 * @brief 测试 WebSocket 网关创建接口存在性
 */
static void test_ws_gateway_create(void) {
    TEST_BEGIN("ws_gateway_create_interface");

    gateway_t* gw_null = gateway_ws_create(NULL, 8081);
    ASSERT_NULL(gw_null);

    gateway_t* gw = gateway_ws_create("127.0.0.1", 18081);
    if (gw) {
        ASSERT_EQ(gateway_get_type(gw), GATEWAY_TYPE_WS);
        ASSERT_FALSE(gateway_is_running(gw));

        const char* name = gateway_get_name(gw);
        ASSERT_TRUE(strcmp(name, "WebSocket Gateway") == 0);

        gateway_destroy(gw);
    }

    TEST_PASS();
}

/* ========== 测试用例：Stdio网关创建存根 ========== */

/**
 * @brief 测试 Stdio 网关创建接口存在性
 */
static void test_stdio_gateway_create(void) {
    TEST_BEGIN("stdio_gateway_create_interface");

    gateway_t* gw = gateway_stdio_create();
    if (gw) {
        ASSERT_EQ(gateway_get_type(gw), GATEWAY_TYPE_STDIO);
        ASSERT_FALSE(gateway_is_running(gw));

        const char* name = gateway_get_name(gw);
        ASSERT_TRUE(strcmp(name, "Stdio Gateway") == 0);

        gateway_destroy(gw);
    }

    TEST_PASS();
}

/* ========== 测试用例：destroy安全性 ========== */

/**
 * @brief 验证 gateway_destroy 对各种输入的安全性
 */
static void test_destroy_safety(void) {
    TEST_BEGIN("destroy_safety");

    /* NULL destroy 应该是安全的（不崩溃） */
    gateway_destroy(NULL);

    /* 创建后立即销毁 */
    gateway_t* gw = gateway_http_create("127.0.0.1", 19090);
    if (gw) {
        gateway_destroy(gw);
    }

    TEST_PASS();
}

/* ========== 主函数 ========== */

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    printf("\n========================================\n");
    printf("  Gateway Module Unit Tests v2.0\n");
    printf("  (Aligned with ARCHITECTURAL_PRINCIPLES)\n");
    printf("========================================\n\n");

    /* 枚举和错误码测试 */
    printf("[Definition Tests]\n");
    test_gateway_types();
    test_error_codes();
    printf("\n");

    /* 安全性测试 */
    printf("[Safety Tests]\n");
    test_null_safety();
    test_destroy_safety();
    printf("\n");

    /* 接口存在性测试 */
    printf("[Interface Tests]\n");
    test_http_gateway_create();
    test_ws_gateway_create();
    test_stdio_gateway_create();
    printf("\n");

    /* 输出结果 */
    printf("========================================\n");
    printf("  Results: %d/%d passed\n", g_tests_passed, g_tests_run);
    printf("========================================\n\n");

    return (g_tests_passed == g_tests_run) ? 0 : 1;
}
