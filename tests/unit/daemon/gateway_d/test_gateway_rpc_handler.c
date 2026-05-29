/**
 * @file test_gateway_rpc_handler.c
 * @brief 统一RPC处理模块单元测试
 *
 * 测试 gateway_rpc_handler 模块的核心功能：
 * - 请求验证和格式检查
 * - 字段提取和处理
 * - 错误处理机制
 * - 内存管理安全性
 *
 * 设计原则：
 *   E-8 可测试性：覆盖率目标≥95%
 *   K-2 接口契约化：验证所有接口契约
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "gateway_rpc_handler.h"
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
#define ASSERT_NEQ(a, b) ASSERT_TRUE((a) != (b))
#define ASSERT_STR_EQ(a, b) ASSERT_TRUE(strcmp((a), (b)) == 0)

/* ========== 辅助函数 ========== */

/**
 * @brief 创建标准JSON-RPC请求
 */
static cJSON* create_valid_request(const char* method, int id_val) {
    cJSON* request = cJSON_CreateObject();
    if (!request) return NULL;

    cJSON_AddStringToObject(request, "jsonrpc", "2.0");
    cJSON_AddStringToObject(request, "method", method);
    cJSON_AddNumberToObject(request, "id", id_val);

    return request;
}

/**
 * @brief 创建带参数的JSON-RPC请求
 */
static cJSON* create_request_with_params(const char* method, cJSON* params, int id_val) {
    cJSON* request = cJSON_CreateObject();
    if (!request) return NULL;

    cJSON_AddStringToObject(request, "jsonrpc", "2.0");
    cJSON_AddStringToObject(request, "method", method);
    if (params) {
        cJSON_AddItemToObject(request, "params", params);
    }
    cJSON_AddNumberToObject(request, "id", id_val);

    return request;
}

/* ========== 测试用例组1: 请求验证 ========== */

/**
 * @brief 测试有效请求处理
 */
static void test_handle_valid_request(void) {
    TEST_BEGIN("handle_valid_request");

    cJSON* request = create_valid_request("agentos_sys_task_submit", 1);
    ASSERT_NOT_NULL(request);

    rpc_result_t result = gateway_rpc_handle_request(request, NULL, NULL);

    /* 应该返回成功结果 */
    ASSERT_NOT_NULL(result.response_json);
    ASSERT_EQ(result.error_code, 0);

    /* 验证响应是有效的JSON */
    cJSON* resp = cJSON_Parse(result.response_json);
    ASSERT_NOT_NULL(resp);

    cJSON* jsonrpc = cJSON_GetObjectItem(resp, "jsonrpc");
    ASSERT_NOT_NULL(jsonrpc);
    ASSERT_STR_EQ(jsonrpc->valuestring, "2.0");

    cJSON_Delete(resp);
    gateway_rpc_free(&result);
    cJSON_Delete(request);

    TEST_PASS();
}

/**
 * @brief 测试带参数的有效请求
 */
static void test_handle_request_with_params(void) {
    TEST_BEGIN("handle_request_with_params");

    cJSON* params = cJSON_CreateObject();
    cJSON_AddStringToObject(params, "name", "test_task");

    cJSON* request = create_request_with_params("agentos_sys_memory_write", params, 2);
    ASSERT_NOT_NULL(request);

    rpc_result_t result = gateway_rpc_handle_request(request, NULL, NULL);

    ASSERT_NOT_NULL(result.response_json);
    ASSERT_EQ(result.error_code, 0);

    gateway_rpc_free(&result);
    cJSON_Delete(request);

    TEST_PASS();
}

/**
 * @brief 测试NULL请求
 */
static void test_handle_null_request(void) {
    TEST_BEGIN("handle_null_request");

    rpc_result_t result = gateway_rpc_handle_request(NULL, NULL, NULL);

    ASSERT_NOT_NULL(result.response_json);
    ASSERT_NEQ(result.error_code, 0);

    /* 验证错误响应格式 */
    cJSON* resp = cJSON_Parse(result.response_json);
    ASSERT_NOT_NULL(resp);

    cJSON* error = cJSON_GetObjectItem(resp, "error");
    ASSERT_NOT_NULL(error);

    cJSON* code = cJSON_GetObjectItem(error, "code");
    ASSERT_NOT_NULL(code);
    ASSERT_EQ(code->valueint, -32600);

    cJSON_Delete(resp);
    gateway_rpc_free(&result);

    TEST_PASS();
}

/**
 * @brief 测试无效请求格式
 */
static void test_handle_invalid_format(void) {
    TEST_BEGIN("handle_invalid_format");

    /* 缺少必需字段 */
    cJSON* bad_request = cJSON_CreateObject();
    cJSON_AddStringToObject(bad_request, "method", "test");  /* 缺少jsonrpc和id */

    rpc_result_t result = gateway_rpc_handle_request(bad_request, NULL, NULL);

    ASSERT_NOT_NULL(result.response_json);
    ASSERT_NEQ(result.error_code, 0);

    gateway_rpc_free(&result);
    cJSON_Delete(bad_request);

    TEST_PASS();
}

/**
 * @brief 测试错误的jsonrpc版本
 */
static void test_handle_wrong_version(void) {
    TEST_BEGIN("handle_wrong_version");

    cJSON* request = cJSON_CreateObject();
    cJSON_AddStringToObject(request, "jsonrpc", "1.0");  /* 错误版本 */
    cJSON_AddStringToObject(request, "method", "test");
    cJSON_AddNumberToObject(request, "id", 1);

    rpc_result_t result = gateway_rpc_handle_request(request, NULL, NULL);

    ASSERT_NOT_NULL(result.response_json);
    ASSERT_NEQ(result.error_code, 0);

    gateway_rpc_free(&result);
    cJSON_Delete(request);

    TEST_PASS();
}

/* ========== 测试用例组2: 自定义Handler ========== */

/**
 * @brief 自定义Handler回调函数示例
 */
static int mock_handler(const char* request_str, char** response_str, void* user_data) {
    (void)user_data;
    
    if (!request_str || !response_str) return -1;
    
    /* 简单的echo handler */
    cJSON* request = cJSON_Parse(request_str);
    if (!request) return -1;
    
    cJSON* response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "jsonrpc", "2.0");
    cJSON_AddNumberToObject(response, "id", 999);
    cJSON_AddStringToObject(response, "result", "mock_handler_success");
    
    *response_str = cJSON_PrintUnformatted(response);
    cJSON_Delete(response);
    
    return 0;
}

/**
 * @brief 测试自定义Handler调用
 */
static void test_custom_handler_invocation(void) {
    TEST_BEGIN("custom_handler_invocation");

    cJSON* request = create_valid_request("custom_method", 10);
    ASSERT_NOT_NULL(request);

    rpc_result_t result = gateway_rpc_handle_request(
        request, mock_handler, NULL);

    ASSERT_NOT_NULL(result.response_json);
    ASSERT_EQ(result.error_code, 0);

    /* 验证handler被调用并返回了正确响应 */
    cJSON* resp = cJSON_Parse(result.response_json);
    ASSERT_NOT_NULL(resp);

    cJSON* result_field = cJSON_GetObjectItem(resp, "result");
    ASSERT_NOT_NULL(result_field);
    ASSERT_STR_EQ(result_field->valuestring, "mock_handler_success");

    cJSON_Delete(resp);
    gateway_rpc_free(&result);
    cJSON_Delete(request);

    TEST_PASS();
}

/**
 * @brief 错误Handler回调函数
 */
static int error_handler_func(const char* req, char** resp, void* data) {
    (void)req; (void)resp; (void)data;
    return -1;  /* 返回错误 */
}

/**
 * @brief 测试Handler返回错误
 */
static void test_custom_handler_error(void) {
    TEST_BEGIN("custom_handler_error");

    cJSON* request = create_valid_request("test_method", 11);
    ASSERT_NOT_NULL(request);

    rpc_result_t result = gateway_rpc_handle_request(
        request, error_handler_func, NULL);

    ASSERT_NOT_NULL(result.response_json);
    ASSERT_NEQ(result.error_code, 0);

    gateway_rpc_free(&result);
    cJSON_Delete(request);

    TEST_PASS();
}

/* ========== 测试用例组3: 错误处理 ========== */

/**
 * @brief 测试创建错误结果
 */
static void test_create_error_result(void) {
    TEST_BEGIN("create_error_result");

    rpc_result_t result = gateway_rpc_create_error(-32601, "Method not found");

    ASSERT_NOT_NULL(result.response_json);
    ASSERT_EQ(result.error_code, -32601);

    /* 验证错误响应格式 */
    cJSON* resp = cJSON_Parse(result.response_json);
    ASSERT_NOT_NULL(resp);

    cJSON* error = cJSON_GetObjectItem(resp, "error");
    ASSERT_NOT_NULL(error);

    cJSON* code = cJSON_GetObjectItem(error, "code");
    ASSERT_NOT_NULL(code);
    ASSERT_EQ(code->valueint, -32601);

    cJSON* message = cJSON_GetObjectItem(error, "message");
    ASSERT_NOT_NULL(message);
    ASSERT_STR_EQ(message->valuestring, "Method not found");

    cJSON_Delete(resp);
    gateway_rpc_free(&result);

    TEST_PASS();
}

/**
 * @brief 测试NULL消息的错误创建
 */
static void test_create_error_null_message(void) {
    TEST_BEGIN("create_error_null_message");

    rpc_result_t result = gateway_rpc_create_error(-32000, NULL);

    ASSERT_NOT_NULL(result.response_json);
    ASSERT_EQ(result.error_code, -32000);

    gateway_rpc_free(&result);

    TEST_PASS();
}

/* ========== 测试用例组4: 内存管理 ========== */

/**
 * @brief 测试资源释放安全性
 */
static void test_resource_cleanup_safety(void) {
    TEST_BEGIN("resource_cleanup_safety");

    /* 测试释放NULL结果 */
    rpc_result_t null_result = {NULL, 0, NULL};
    gateway_rpc_free(&null_result);  /* 不应崩溃 */

    /* 测试重复释放 */
    rpc_result_t result = gateway_rpc_create_error(-32000, "test");
    gateway_rpc_free(&result);
    gateway_rpc_free(&result);  /* 重复释放不应崩溃 */

    TEST_PASS();
}

/**
 * @brief 测试大量请求处理的内存稳定性
 */
static void test_memory_stability_under_load(void) {
    TEST_BEGIN("memory_stability_under_load");

    for (int i = 0; i < 1000; i++) {
        char method[64];
        snprintf(method, sizeof(method), "agentos_sys_task_submit_%d", i);

        cJSON* request = create_valid_request(method, i);
        ASSERT_NOT_NULL(request);

        rpc_result_t result = gateway_rpc_handle_request(request, NULL, NULL);

        if (result.response_json) {
            free(result.response_json);
        }

        cJSON_Delete(request);
    }

    TEST_PASS();
}

/* ========== 测试用例组5: 边界条件 ========== */

/**
 * @brief 测试空方法名
 */
static void test_empty_method_name(void) {
    TEST_BEGIN("empty_method_name");

    cJSON* request = cJSON_CreateObject();
    cJSON_AddStringToObject(request, "jsonrpc", "2.0");
    cJSON_AddStringToObject(request, "method", "");  /* 空方法名 */
    cJSON_AddNumberToObject(request, "id", 100);

    rpc_result_t result = gateway_rpc_handle_request(request, NULL, NULL);

    /* 空字符串是有效的，应该能路由到未知方法 */
    ASSERT_NOT_NULL(result.response_json);

    gateway_rpc_free(&result);
    cJSON_Delete(request);

    TEST_PASS();
}

/**
 * @brief 测试超长方法名
 */
static void test_very_long_method_name(void) {
    TEST_BEGIN("very_long_method_name");

    char long_method[1024];
    memset(long_method, 'A', sizeof(long_method) - 1);
    long_method[sizeof(long_method) - 1] = '\0';

    cJSON* request = cJSON_CreateObject();
    cJSON_AddStringToObject(request, "jsonrpc", "2.0");
    cJSON_AddStringToObject(request, "method", long_method);
    cJSON_AddNumberToObject(request, "id", 101);

    rpc_result_t result = gateway_rpc_handle_request(request, NULL, NULL);

    ASSERT_NOT_NULL(result.response_json);

    gateway_rpc_free(&result);
    cJSON_Delete(request);

    TEST_PASS();
}

/**
 * @brief 测试特殊字符方法名
 */
static void test_special_characters_in_method(void) {
    TEST_BEGIN("special_characters_in_method");

    const char* special_methods[] = {
        "agentos.sys.task.submit",      /* 点号 */
        "agentos/sys/task/submit",       /* 斜杠 */
        "agentos::sys::task::submit",    /* 双冒号 */
        "agentos_sys_task_submit_中文",  /* 中文 */
        NULL
    };

    for (int i = 0; special_methods[i] != NULL; i++) {
        cJSON* request = create_valid_request(special_methods[i], 102 + i);
        ASSERT_NOT_NULL(request);

        rpc_result_t result = gateway_rpc_handle_request(request, NULL, NULL);

        ASSERT_NOT_NULL(result.response_json);

        gateway_rpc_free(&result);
        cJSON_Delete(request);
    }

    TEST_PASS();
}

/* ========== 主函数 ========== */

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    
    printf("\n======================================================\n");
    printf("  Gateway RPC Handler Unit Tests v1.0\n");
    printf("  (Testing unified RPC processing module)\n");
    printf("======================================================\n\n");
    
    /* 组1: 请求验证测试 */
    printf("[Request Validation Tests]\n");
    test_handle_valid_request();
    test_handle_request_with_params();
    test_handle_null_request();
    test_handle_invalid_format();
    test_handle_wrong_version();
    printf("\n");
    
    /* 组2: 自定义Handler测试 */
    printf("[Custom Handler Tests]\n");
    test_custom_handler_invocation();
    test_custom_handler_error();
    printf("\n");
    
    /* 组3: 错误处理测试 */
    printf("[Error Handling Tests]\n");
    test_create_error_result();
    test_create_error_null_message();
    printf("\n");
    
    /* 组4: 内存管理测试 */
    printf("[Memory Management Tests]\n");
    test_resource_cleanup_safety();
    test_memory_stability_under_load();
    printf("\n");
    
    /* 组5: 边界条件测试 */
    printf("[Boundary Condition Tests]\n");
    test_empty_method_name();
    test_very_long_method_name();
    test_special_characters_in_method();
    printf("\n");
    
    /* 输出结果 */
    printf("======================================================\n");
    printf("  Results: %d/%d passed\n", g_tests_passed, g_tests_run);
    printf("======================================================\n\n");
    
    return (g_tests_passed == g_tests_run) ? 0 : 1;
}
