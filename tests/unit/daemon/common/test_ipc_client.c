/**
 * @file test_ipc_client.c
 * @brief IPC客户端模块单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "svc_common.h"

/**
 * @brief 测试IPC初始化空参数
 */
static void test_ipc_init_null_param(void) {
    printf("  test_ipc_init_null_param...\n");

    int ret = svc_ipc_init(NULL);
    assert(ret == SVC_ERR_INVALID_PARAM);

    printf("    PASSED\n");
}

/**
 * @brief 测试IPC初始化和清理
 */
static void test_ipc_init_cleanup(void) {
    printf("  test_ipc_init_cleanup...\n");

    int ret = svc_ipc_init("http://localhost:8080/rpc");
    assert(ret == SVC_OK);

    svc_ipc_cleanup();

    printf("    PASSED\n");
}

/**
 * @brief 测试IPC重复初始化
 */
static void test_ipc_double_init(void) {
    printf("  test_ipc_double_init...\n");

    int ret1 = svc_ipc_init("http://localhost:8080/rpc");
    int ret2 = svc_ipc_init("http://localhost:8081/rpc");
    
    assert(ret1 == SVC_OK);
    assert(ret2 == SVC_OK);

    svc_ipc_cleanup();

    printf("    PASSED\n");
}

/**
 * @brief 测试RPC调用空参数
 */
static void test_rpc_call_null_param(void) {
    printf("  test_rpc_call_null_param...\n");

    svc_ipc_init("http://localhost:8080/rpc");

    char* result = NULL;
    int ret = svc_rpc_call(NULL, NULL, &result, 0);
    assert(ret == SVC_ERR_INVALID_PARAM);

    ret = svc_rpc_call("test_method", NULL, NULL, 0);
    assert(ret == SVC_ERR_INVALID_PARAM);

    svc_ipc_cleanup();

    printf("    PASSED\n");
}

/**
 * @brief 测试IPC未初始化时调用
 */
static void test_rpc_call_without_init(void) {
    printf("  test_rpc_call_without_init...\n");

    char* result = NULL;
    int ret = svc_rpc_call("test_method", NULL, &result, 0);
    assert(ret == SVC_ERR_RPC);

    printf("    PASSED\n");
}

/**
 * @brief 测试设置超时时间
 */
static void test_ipc_set_timeout(void) {
    printf("  test_ipc_set_timeout...\n");

    svc_ipc_init("http://localhost:8080/rpc");

    int ret = svc_ipc_set_timeout(5000);
    assert(ret == SVC_OK);

    ret = svc_ipc_set_timeout(60000);
    assert(ret == SVC_OK);

    svc_ipc_cleanup();

    printf("    PASSED\n");
}

/**
 * @brief 测试获取连接池状态
 */
static void test_ipc_get_pool_status(void) {
    printf("  test_ipc_get_pool_status...\n");

    svc_ipc_init("http://localhost:8080/rpc");

    int total = 0;
    int available = 0;
    int ret = svc_ipc_get_pool_status(&total, &available);
    assert(ret == SVC_OK);
    assert(total > 0);
    assert(available >= 0);
    assert(available <= total);

    svc_ipc_cleanup();

    printf("    PASSED\n");
}

/**
 * @brief 测试IPC清理后调用
 */
static void test_ipc_after_cleanup(void) {
    printf("  test_ipc_after_cleanup...\n");

    svc_ipc_init("http://localhost:8080/rpc");
    svc_ipc_cleanup();

    int ret = svc_ipc_set_timeout(5000);
    assert(ret == SVC_ERR_RPC);

    printf("    PASSED\n");
}

int main(void) {
    printf("=========================================\n");
    printf("  IPC Client Module Unit Tests\n");
    printf("=========================================\n");

    test_ipc_init_null_param();
    test_ipc_init_cleanup();
    test_ipc_double_init();
    test_rpc_call_null_param();
    test_rpc_call_without_init();
    test_ipc_set_timeout();
    test_ipc_get_pool_status();
    test_ipc_after_cleanup();

    printf("\n✅ All IPC client module tests PASSED\n");
    return 0;
}
