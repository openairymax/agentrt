/**
 * @file test_syscall_router.c
 * @brief syscall_router 模块单元测试
 *
 * 测试系统调用路由器的核心功能：
 * - 方法名路由到正确的 syscall
 * - 参数验证和错误处理
 * - 响应生成正确性
 *
 * 设计原则：
 *   E-8 可测试性：单元测试覆盖率≥80%
 *   K-2 接口契约化：验证路由契约
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "syscall_router.h"
#include "jsonrpc.h"

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
#define ASSERT_STR_EQ(a, b) ASSERT_TRUE(strcmp((a), (b)) == 0)

/* ========== 辅助函数：创建 JSON-RPC 请求 ========== */

static cJSON* create_jsonrpc_request(const char* method) {
    cJSON* request = cJSON_CreateObject();
    ASSERT_NOT_NULL(request);
    
    cJSON_AddStringToObject(request, "jsonrpc", "2.0");
    cJSON_AddStringToObject(request, "method", method);
    cJSON_AddNumberToObject(request, "id", 1);
    
    return request;
}

static cJSON* create_jsonrpc_request_with_params(const char* method, cJSON* params) {
    cJSON* request = cJSON_CreateObject();
    ASSERT_NOT_NULL(request);
    
    cJSON_AddStringToObject(request, "jsonrpc", "2.0");
    cJSON_AddStringToObject(request, "method", method);
    cJSON_AddItemToObject(request, "params", params);
    cJSON_AddNumberToObject(request, "id", 1);
    
    return request;
}

/* ========== 测试用例：任务管理方法路由 ========== */

/**
 * @brief 测试任务管理方法的路由
 */
static void test_route_task_methods(void) {
    TEST_BEGIN("route_task_methods");
    
    /* 测试 agentos_sys_task_submit */
    cJSON* params = cJSON_CreateObject();
    cJSON_AddStringToObject(params, "name", "test_task");
    cJSON* request = create_jsonrpc_request_with_params("agentos_sys_task_submit", params);
    
    char* response = gateway_syscall_route("agentos_sys_task_submit", params, 
                                            jsonrpc_get_id(request));
    ASSERT_NOT_NULL(response);
    /* 响应应该是有效的 JSON */
    cJSON* resp_json = cJSON_Parse(response);
    ASSERT_NOT_NULL(resp_json);
    
    /* 检查响应格式 */
    cJSON* jsonrpc_ver = cJSON_GetObjectItem(resp_json, "jsonrpc");
    ASSERT_NOT_NULL(jsonrpc_ver);
    ASSERT_STR_EQ(jsonrpc_ver->valuestring, "2.0");
    
    cJSON* id = cJSON_GetObjectItem(resp_json, "id");
    ASSERT_NOT_NULL(id);
    ASSERT_EQ(id->valueint, 1);
    
    cJSON_free(response);
    cJSON_Delete(resp_json);
    cJSON_Delete(request);
    
    /* 测试其他任务方法 */
    response = gateway_syscall_route("agentos_sys_task_query", NULL, NULL);
    ASSERT_NOT_NULL(response);
    cJSON_free(response);
    
    response = gateway_syscall_route("agentos_sys_task_wait", NULL, NULL);
    ASSERT_NOT_NULL(response);
    cJSON_free(response);
    
    response = gateway_syscall_route("agentos_sys_task_cancel", NULL, NULL);
    ASSERT_NOT_NULL(response);
    cJSON_free(response);
    
    TEST_PASS();
}

/* ========== 测试用例：记忆管理方法路由 ========== */

/**
 * @brief 测试记忆管理方法的路由
 */
static void test_route_memory_methods(void) {
    TEST_BEGIN("route_memory_methods");
    
    char* response;
    
    /* 测试 agentos_sys_memory_write */
    cJSON* params = cJSON_CreateObject();
    cJSON_AddStringToObject(params, "key", "test_key");
    cJSON_AddStringToObject(params, "value", "test_value");
    response = gateway_syscall_route("agentos_sys_memory_write", params, NULL);
    ASSERT_NOT_NULL(response);
    cJSON_free(response);
    cJSON_Delete(params);
    
    /* 测试 agentos_sys_memory_search */
    params = cJSON_CreateObject();
    cJSON_AddStringToObject(params, "query", "search_query");
    response = gateway_syscall_route("agentos_sys_memory_search", params, NULL);
    ASSERT_NOT_NULL(response);
    cJSON_free(response);
    cJSON_Delete(params);
    
    /* 测试 agentos_sys_memory_get */
    response = gateway_syscall_route("agentos_sys_memory_get", NULL, NULL);
    ASSERT_NOT_NULL(response);
    cJSON_free(response);
    
    /* 测试 agentos_sys_memory_delete */
    response = gateway_syscall_route("agentos_sys_memory_delete", NULL, NULL);
    ASSERT_NOT_NULL(response);
    cJSON_free(response);
    
    TEST_PASS();
}

/* ========== 测试用例：会话管理方法路由 ========== */

/**
 * @brief 测试会话管理方法的路由
 */
static void test_route_session_methods(void) {
    TEST_BEGIN("route_session_methods");
    
    char* response;
    
    /* 测试 agentos_sys_session_create */
    response = gateway_syscall_route("agentos_sys_session_create", NULL, NULL);
    ASSERT_NOT_NULL(response);
    cJSON_free(response);
    
    /* 测试 agentos_sys_session_get */
    response = gateway_syscall_route("agentos_sys_session_get", NULL, NULL);
    ASSERT_NOT_NULL(response);
    cJSON_free(response);
    
    /* 测试 agentos_sys_session_close */
    response = gateway_syscall_route("agentos_sys_session_close", NULL, NULL);
    ASSERT_NOT_NULL(response);
    cJSON_free(response);
    
    /* 测试 agentos_sys_session_list */
    response = gateway_syscall_route("agentos_sys_session_list", NULL, NULL);
    ASSERT_NOT_NULL(response);
    cJSON_free(response);
    
    TEST_PASS();
}

/* ========== 测试用例：可观测性方法路由 ========== */

/**
 * @brief 测试可观测性方法的路由
 */
static void test_route_telemetry_methods(void) {
    TEST_BEGIN("route_telemetry_methods");
    
    char* response;
    
    /* 测试 agentos_sys_telemetry_metrics */
    response = gateway_syscall_route("agentos_sys_telemetry_metrics", NULL, NULL);
    ASSERT_NOT_NULL(response);
    cJSON_free(response);
    
    /* 测试 agentos_sys_telemetry_traces */
    response = gateway_syscall_route("agentos_sys_telemetry_traces", NULL, NULL);
    ASSERT_NOT_NULL(response);
    cJSON_free(response);
    
    TEST_PASS();
}

/* ========== 测试用例：Agent 管理方法路由 ========== */

/**
 * @brief 测试 Agent 管理方法的路由
 */
static void test_route_agent_methods(void) {
    TEST_BEGIN("route_agent_methods");
    
    char* response;
    
    /* 测试 agentos_sys_agent_spawn */
    response = gateway_syscall_route("agentos_sys_agent_spawn", NULL, NULL);
    ASSERT_NOT_NULL(response);
    cJSON_free(response);
    
    /* 测试 agentos_sys_agent_terminate */
    response = gateway_syscall_route("agentos_sys_agent_terminate", NULL, NULL);
    ASSERT_NOT_NULL(response);
    cJSON_free(response);
    
    /* 测试 agentos_sys_agent_invoke */
    response = gateway_syscall_route("agentos_sys_agent_invoke", NULL, NULL);
    ASSERT_NOT_NULL(response);
    cJSON_free(response);
    
    /* 测试 agentos_sys_agent_list */
    response = gateway_syscall_route("agentos_sys_agent_list", NULL, NULL);
    ASSERT_NOT_NULL(response);
    cJSON_free(response);
    
    TEST_PASS();
}

/* ========== 测试用例：未知方法处理 ========== */

/**
 * @brief 测试未知方法名的错误处理
 */
static void test_route_unknown_method(void) {
    TEST_BEGIN("route_unknown_method");
    
    char* response = gateway_syscall_route("agentos_sys_unknown_method", NULL, NULL);
    ASSERT_NOT_NULL(response);
    
    /* 解析响应，检查是否为错误响应 */
    cJSON* resp_json = cJSON_Parse(response);
    ASSERT_NOT_NULL(resp_json);
    
    /* 应该包含 error 字段 */
    cJSON* error = cJSON_GetObjectItem(resp_json, "error");
    ASSERT_NOT_NULL(error);
    
    /* 错误码应该是方法未找到 (-32601) */
    cJSON* code = cJSON_GetObjectItem(error, "code");
    ASSERT_NOT_NULL(code);
    ASSERT_EQ(code->valueint, JSONRPC_METHOD_NOT_FOUND);
    
    cJSON_Delete(resp_json);
    cJSON_free(response);
    
    TEST_PASS();
}

/* ========== 测试用例：NULL 参数安全性 ========== */

/**
 * @brief 验证 syscall_router 对 NULL 参数的安全性
 */
static void test_null_safety(void) {
    TEST_BEGIN("null_parameter_safety");
    
    /* method 为 NULL 应该返回错误响应，而不是崩溃 */
    char* response = gateway_syscall_route(NULL, NULL, NULL);
    ASSERT_NOT_NULL(response);
    
    /* 响应应该是有效的 JSON */
    cJSON* resp_json = cJSON_Parse(response);
    ASSERT_NOT_NULL(resp_json);
    
    /* 应该是错误响应 */
    cJSON* error = cJSON_GetObjectItem(resp_json, "error");
    ASSERT_NOT_NULL(error);
    
    cJSON_Delete(resp_json);
    cJSON_free(response);
    
    TEST_PASS();
}

/* ========== 测试用例：方法名前缀匹配 ========== */

/**
 * @brief 测试方法名前缀匹配逻辑
 */
static void test_method_prefix_matching(void) {
    TEST_BEGIN("method_prefix_matching");
    
    /* 测试 agentos_sys_前缀的方法 */
    char* response = gateway_syscall_route("agentos_sys_task_submit", NULL, NULL);
    ASSERT_NOT_NULL(response);
    cJSON_free(response);
    
    /* 测试不带前缀的方法（应该返回方法未找到） */
    response = gateway_syscall_route("task_submit", NULL, NULL);
    ASSERT_NOT_NULL(response);
    
    cJSON* resp_json = cJSON_Parse(response);
    ASSERT_NOT_NULL(resp_json);
    
    cJSON* error = cJSON_GetObjectItem(resp_json, "error");
    ASSERT_NOT_NULL(error);
    
    cJSON_Delete(resp_json);
    cJSON_free(response);
    
    TEST_PASS();
}

/* ========== 主函数 ========== */

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    
    printf("\n========================================\n");
    printf("  Syscall Router Unit Tests v1.0\n");
    printf("  (Testing 18 syscall methods)\n");
    printf("========================================\n\n");
    
    /* 任务管理测试 */
    printf("[Task Management Tests]\n");
    test_route_task_methods();
    printf("\n");
    
    /* 记忆管理测试 */
    printf("[Memory Management Tests]\n");
    test_route_memory_methods();
    printf("\n");
    
    /* 会话管理测试 */
    printf("[Session Management Tests]\n");
    test_route_session_methods();
    printf("\n");
    
    /* 可观测性测试 */
    printf("[Telemetry Tests]\n");
    test_route_telemetry_methods();
    printf("\n");
    
    /* Agent 管理测试 */
    printf("[Agent Management Tests]\n");
    test_route_agent_methods();
    printf("\n");
    
    /* 错误处理测试 */
    printf("[Error Handling Tests]\n");
    test_route_unknown_method();
    test_null_safety();
    test_method_prefix_matching();
    printf("\n");
    
    /* 输出结果 */
    printf("========================================\n");
    printf("  Results: %d/%d passed\n", g_tests_passed, g_tests_run);
    printf("========================================\n\n");
    
    return (g_tests_passed == g_tests_run) ? 0 : 1;
}
