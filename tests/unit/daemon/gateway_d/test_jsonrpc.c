/**
 * @file test_jsonrpc.c
 * @brief JSON-RPC 2.0 协议工具单元测试
 *
 * 测试 JSON-RPC 协议工具的完整功能：
 * - 请求验证
 * - 响应生成
 * - 错误处理
 * - 边界条件
 *
 * 设计原则：
 *   E-8 可测试性：协议合规性验证
 *   K-2 接口契约化：验证 JSON-RPC 2.0 标准
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

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

/* ========== 辅助函数 ========== */

static cJSON* parse_json(const char* json_str) {
    ASSERT_NOT_NULL(json_str);
    cJSON* json = cJSON_Parse(json_str);
    ASSERT_NOT_NULL(json);
    return json;
}

/* ========== 测试用例：有效请求验证 ========== */

/**
 * @brief 测试有效的 JSON-RPC 请求验证
 */
static void test_validate_valid_request(void) {
    TEST_BEGIN("validate_valid_request");
    
    /* 标准请求 */
    const char* valid_request = "{\"jsonrpc\":\"2.0\",\"method\":\"test\",\"id\":1}";
    cJSON* json = parse_json(valid_request);
    
    int result = jsonrpc_validate_request(json);
    ASSERT_EQ(result, 0);  /* 应该返回 0 表示有效 */
    
    /* 验证能正确提取字段 */
    const char* method = jsonrpc_get_method(json);
    ASSERT_NOT_NULL(method);
    ASSERT_STR_EQ(method, "test");
    
    const cJSON* id = jsonrpc_get_id(json);
    ASSERT_NOT_NULL(id);
    ASSERT_EQ(id->valueint, 1);
    
    cJSON_Delete(json);
    
    TEST_PASS();
}

/**
 * @brief 测试带参数的有效请求
 */
static void test_validate_request_with_params(void) {
    TEST_BEGIN("validate_request_with_params");
    
    const char* valid_request = "{\"jsonrpc\":\"2.0\",\"method\":\"test\","
                                "\"params\":{\"key\":\"value\"},\"id\":\"req1\"}";
    cJSON* json = parse_json(valid_request);
    
    int result = jsonrpc_validate_request(json);
    ASSERT_EQ(result, 0);
    
    /* 验证参数提取 */
    const cJSON* params = jsonrpc_get_params(json);
    ASSERT_NOT_NULL(params);
    
    cJSON* key = cJSON_GetObjectItem(params, "key");
    ASSERT_NOT_NULL(key);
    ASSERT_STR_EQ(key->valuestring, "value");
    
    cJSON_Delete(json);
    
    TEST_PASS();
}

/* ========== 测试用例：无效请求验证 ========== */

/**
 * @brief 测试缺少必需字段的请求
 */
static void test_validate_missing_fields(void) {
    TEST_BEGIN("validate_missing_fields");
    
    /* 缺少 jsonrpc 字段 */
    const char* missing_jsonrpc = "{\"method\":\"test\",\"id\":1}";
    cJSON* json = parse_json(missing_jsonrpc);
    int result = jsonrpc_validate_request(json);
    ASSERT_TRUE(result < 0);
    cJSON_Delete(json);
    
    /* 缺少 method 字段 */
    const char* missing_method = "{\"jsonrpc\":\"2.0\",\"id\":1}";
    json = parse_json(missing_method);
    result = jsonrpc_validate_request(json);
    ASSERT_TRUE(result < 0);
    cJSON_Delete(json);
    
    /* 缺少 id 字段 */
    const char* missing_id = "{\"jsonrpc\":\"2.0\",\"method\":\"test\"}";
    json = parse_json(missing_id);
    result = jsonrpc_validate_request(json);
    ASSERT_TRUE(result < 0);
    cJSON_Delete(json);
    
    TEST_PASS();
}

/**
 * @brief 测试 jsonrpc 版本错误
 */
static void test_validate_wrong_version(void) {
    TEST_BEGIN("validate_wrong_version");
    
    /* 错误的版本号 */
    const char* wrong_version = "{\"jsonrpc\":\"1.0\",\"method\":\"test\",\"id\":1}";
    cJSON* json = parse_json(wrong_version);
    
    int result = jsonrpc_validate_request(json);
    ASSERT_TRUE(result < 0);  /* 应该返回 -3 表示版本错误 */
    
    cJSON_Delete(json);
    
    TEST_PASS();
}

/**
 * @brief 测试字段类型错误
 */
static void test_validate_wrong_field_types(void) {
    TEST_BEGIN("validate_wrong_field_types");
    
    /* method 应该是字符串 */
    const char* method_not_string = "{\"jsonrpc\":\"2.0\",\"method\":123,\"id\":1}";
    cJSON* json = parse_json(method_not_string);
    int result = jsonrpc_validate_request(json);
    ASSERT_TRUE(result < 0);
    cJSON_Delete(json);
    
    /* id 可以为字符串或数字，但不应为对象 */
    const char* id_is_object = "{\"jsonrpc\":\"2.0\",\"method\":\"test\",\"id\":{}}";
    json = parse_json(id_is_object);
    result = jsonrpc_validate_request(json);
    ASSERT_TRUE(result < 0);
    cJSON_Delete(json);
    
    TEST_PASS();
}

/* ========== 测试用例：成功响应生成 ========== */

/**
 * @brief 测试创建成功响应
 */
static void test_create_success_response(void) {
    TEST_BEGIN("create_success_response");
    
    /* 创建请求 ID */
    cJSON* id = cJSON_CreateNumber(1);
    ASSERT_NOT_NULL(id);
    
    /* 创建结果对象 */
    cJSON* result = cJSON_CreateObject();
    cJSON_AddStringToObject(result, "status", "success");
    ASSERT_NOT_NULL(result);
    
    /* 生成响应 */
    char* response = jsonrpc_create_success_response(id, result);
    ASSERT_NOT_NULL(response);
    
    /* 验证响应格式 */
    cJSON* resp_json = cJSON_Parse(response);
    ASSERT_NOT_NULL(resp_json);
    
    /* 检查 jsonrpc 版本 */
    cJSON* version = cJSON_GetObjectItem(resp_json, "jsonrpc");
    ASSERT_NOT_NULL(version);
    ASSERT_STR_EQ(version->valuestring, "2.0");
    
    /* 检查结果字段 */
    cJSON* result_field = cJSON_GetObjectItem(resp_json, "result");
    ASSERT_NOT_NULL(result_field);
    
    /* 检查 ID 字段 */
    cJSON* id_field = cJSON_GetObjectItem(resp_json, "id");
    ASSERT_NOT_NULL(id_field);
    ASSERT_EQ(id_field->valueint, 1);
    
    /* 确保没有 error 字段 */
    cJSON* error = cJSON_GetObjectItem(resp_json, "error");
    ASSERT_NULL(error);
    
    cJSON_Delete(resp_json);
    cJSON_free(response);
    cJSON_Delete(id);
    
    TEST_PASS();
}

/**
 * @brief 测试 NULL 结果的响应
 */
static void test_create_success_response_null_result(void) {
    TEST_BEGIN("create_success_response_null_result");
    
    cJSON* id = cJSON_CreateNumber(2);
    ASSERT_NOT_NULL(id);
    
    char* response = jsonrpc_create_success_response(id, NULL);
    ASSERT_NOT_NULL(response);
    
    cJSON* resp_json = cJSON_Parse(response);
    ASSERT_NOT_NULL(resp_json);
    
    cJSON* result = cJSON_GetObjectItem(resp_json, "result");
    ASSERT_NOT_NULL(result);
    ASSERT_TRUE(result->type == cJSON_NULL);
    
    cJSON_Delete(resp_json);
    cJSON_free(response);
    cJSON_Delete(id);
    
    TEST_PASS();
}

/* ========== 测试用例：错误响应生成 ========== */

/**
 * @brief 测试创建错误响应
 */
static void test_create_error_response(void) {
    TEST_BEGIN("create_error_response");
    
    cJSON* id = cJSON_CreateNumber(3);
    ASSERT_NOT_NULL(id);
    
    char* response = jsonrpc_create_error_response(id, -32601, "Method not found", NULL);
    ASSERT_NOT_NULL(response);
    
    cJSON* resp_json = cJSON_Parse(response);
    ASSERT_NOT_NULL(resp_json);
    
    /* 检查 error 字段 */
    cJSON* error = cJSON_GetObjectItem(resp_json, "error");
    ASSERT_NOT_NULL(error);
    
    /* 检查错误码 */
    cJSON* code = cJSON_GetObjectItem(error, "code");
    ASSERT_NOT_NULL(code);
    ASSERT_EQ(code->valueint, -32601);
    
    /* 检查错误消息 */
    cJSON* message = cJSON_GetObjectItem(error, "message");
    ASSERT_NOT_NULL(message);
    ASSERT_STR_EQ(message->valuestring, "Method not found");
    
    /* 检查 ID */
    cJSON* id_field = cJSON_GetObjectItem(resp_json, "id");
    ASSERT_NOT_NULL(id_field);
    ASSERT_EQ(id_field->valueint, 3);
    
    /* 确保没有 result 字段 */
    cJSON* result = cJSON_GetObjectItem(resp_json, "result");
    ASSERT_NULL(result);
    
    cJSON_Delete(resp_json);
    cJSON_free(response);
    cJSON_Delete(id);
    
    TEST_PASS();
}

/**
 * @brief 测试创建错误响应带详细数据
 */
static void test_create_error_response_with_data(void) {
    TEST_BEGIN("create_error_response_with_data");
    
    cJSON* id = cJSON_CreateNumber(4);
    cJSON* data = cJSON_CreateString("Additional error details");
    
    char* response = jsonrpc_create_error_response(id, -32000, "Server error", data);
    ASSERT_NOT_NULL(response);
    
    cJSON* resp_json = cJSON_Parse(response);
    ASSERT_NOT_NULL(resp_json);
    
    cJSON* error = cJSON_GetObjectItem(resp_json, "error");
    ASSERT_NOT_NULL(error);
    
    cJSON* data_field = cJSON_GetObjectItem(error, "data");
    ASSERT_NOT_NULL(data_field);
    ASSERT_STR_EQ(data_field->valuestring, "Additional error details");
    
    cJSON_Delete(resp_json);
    cJSON_free(response);
    cJSON_Delete(id);
    
    TEST_PASS();
}

/* ========== 测试用例：便捷错误响应函数 ========== */

/**
 * @brief 测试解析错误响应
 */
static void test_create_parse_error_response(void) {
    TEST_BEGIN("create_parse_error_response");
    
    char* response = jsonrpc_create_parse_error_response();
    ASSERT_NOT_NULL(response);
    
    cJSON* resp_json = cJSON_Parse(response);
    ASSERT_NOT_NULL(resp_json);
    
    cJSON* error = cJSON_GetObjectItem(resp_json, "error");
    ASSERT_NOT_NULL(error);
    
    cJSON* code = cJSON_GetObjectItem(error, "code");
    ASSERT_NOT_NULL(code);
    ASSERT_EQ(code->valueint, JSONRPC_PARSE_ERROR);
    
    cJSON_Delete(resp_json);
    cJSON_free(response);
    
    TEST_PASS();
}

/**
 * @brief 测试无效请求响应
 */
static void test_create_invalid_request_response(void) {
    TEST_BEGIN("create_invalid_request_response");
    
    char* response = jsonrpc_create_invalid_request_response();
    ASSERT_NOT_NULL(response);
    
    cJSON* resp_json = cJSON_Parse(response);
    cJSON* error = cJSON_GetObjectItem(resp_json, "error");
    cJSON* code = cJSON_GetObjectItem(error, "code");
    
    ASSERT_EQ(code->valueint, JSONRPC_INVALID_REQUEST);
    
    cJSON_Delete(resp_json);
    cJSON_free(response);
    
    TEST_PASS();
}

/**
 * @brief 测试方法未找到响应
 */
static void test_create_method_not_found_response(void) {
    TEST_BEGIN("create_method_not_found_response");
    
    cJSON* id = cJSON_CreateNumber(5);
    char* response = jsonrpc_create_method_not_found_response(id);
    ASSERT_NOT_NULL(response);
    
    cJSON* resp_json = cJSON_Parse(response);
    cJSON* error = cJSON_GetObjectItem(resp_json, "error");
    cJSON* code = cJSON_GetObjectItem(error, "code");
    
    ASSERT_EQ(code->valueint, JSONRPC_METHOD_NOT_FOUND);
    
    cJSON_Delete(resp_json);
    cJSON_free(response);
    cJSON_Delete(id);
    
    TEST_PASS();
}

/**
 * @brief 测试无效参数响应
 */
static void test_create_invalid_params_response(void) {
    TEST_BEGIN("create_invalid_params_response");
    
    cJSON* id = cJSON_CreateNumber(6);
    char* response = jsonrpc_create_invalid_params_response(id, "Missing required field");
    ASSERT_NOT_NULL(response);
    
    cJSON* resp_json = cJSON_Parse(response);
    cJSON* error = cJSON_GetObjectItem(resp_json, "error");
    cJSON* code = cJSON_GetObjectItem(error, "code");
    cJSON* message = cJSON_GetObjectItem(error, "message");
    
    ASSERT_EQ(code->valueint, JSONRPC_INVALID_PARAMS);
    ASSERT_STR_EQ(message->valuestring, "Missing required field");
    
    cJSON_Delete(resp_json);
    cJSON_free(response);
    cJSON_Delete(id);
    
    TEST_PASS();
}

/**
 * @brief 测试内部错误响应
 */
static void test_create_internal_error_response(void) {
    TEST_BEGIN("create_internal_error_response");
    
    cJSON* id = cJSON_CreateNumber(7);
    char* response = jsonrpc_create_internal_error_response(id, "Unexpected error");
    ASSERT_NOT_NULL(response);
    
    cJSON* resp_json = cJSON_Parse(response);
    cJSON* error = cJSON_GetObjectItem(resp_json, "error");
    cJSON* code = cJSON_GetObjectItem(error, "code");
    
    ASSERT_EQ(code->valueint, JSONRPC_INTERNAL_ERROR);
    
    cJSON_Delete(resp_json);
    cJSON_free(response);
    cJSON_Delete(id);
    
    TEST_PASS();
}

/**
 * @brief 测试限流响应
 */
static void test_create_rate_limited_response(void) {
    TEST_BEGIN("create_rate_limited_response");
    
    cJSON* id = cJSON_CreateNumber(8);
    char* response = jsonrpc_create_rate_limited_response(id);
    ASSERT_NOT_NULL(response);
    
    cJSON* resp_json = cJSON_Parse(response);
    cJSON* error = cJSON_GetObjectItem(resp_json, "error");
    cJSON* code = cJSON_GetObjectItem(error, "code");
    
    ASSERT_EQ(code->valueint, JSONRPC_RATE_LIMITED);
    
    cJSON_Delete(resp_json);
    cJSON_free(response);
    cJSON_Delete(id);
    
    TEST_PASS();
}

/**
 * @brief 测试认证失败响应
 */
static void test_create_auth_failed_response(void) {
    TEST_BEGIN("create_auth_failed_response");
    
    cJSON* id = cJSON_CreateNumber(9);
    char* response = jsonrpc_create_auth_failed_response(id);
    ASSERT_NOT_NULL(response);
    
    cJSON* resp_json = cJSON_Parse(response);
    cJSON* error = cJSON_GetObjectItem(resp_json, "error");
    cJSON* code = cJSON_GetObjectItem(error, "code");
    
    ASSERT_EQ(code->valueint, JSONRPC_AUTH_FAILED);
    
    cJSON_Delete(resp_json);
    cJSON_free(response);
    cJSON_Delete(id);
    
    TEST_PASS();
}

/* ========== 测试用例：错误消息获取 ========== */

/**
 * @brief 测试获取标准错误消息
 */
static void test_get_error_message(void) {
    TEST_BEGIN("get_error_message");
    
    const char* msg;
    
    msg = jsonrpc_get_error_message(JSONRPC_PARSE_ERROR);
    ASSERT_NOT_NULL(msg);
    ASSERT_TRUE(strlen(msg) > 0);
    
    msg = jsonrpc_get_error_message(JSONRPC_INVALID_REQUEST);
    ASSERT_NOT_NULL(msg);
    ASSERT_TRUE(strlen(msg) > 0);
    
    msg = jsonrpc_get_error_message(JSONRPC_METHOD_NOT_FOUND);
    ASSERT_NOT_NULL(msg);
    ASSERT_TRUE(strlen(msg) > 0);
    
    msg = jsonrpc_get_error_message(JSONRPC_INVALID_PARAMS);
    ASSERT_NOT_NULL(msg);
    ASSERT_TRUE(strlen(msg) > 0);
    
    msg = jsonrpc_get_error_message(JSONRPC_INTERNAL_ERROR);
    ASSERT_NOT_NULL(msg);
    ASSERT_TRUE(strlen(msg) > 0);
    
    TEST_PASS();
}

/* ========== 测试用例：边界条件 ========== */

/**
 * @brief 测试 NULL 输入的处理
 */
static void test_null_input_handling(void) {
    TEST_BEGIN("null_input_handling");
    
    /* jsonrpc_validate_request 应该能处理 NULL */
    int result = jsonrpc_validate_request(NULL);
    ASSERT_TRUE(result < 0);
    
    /* jsonrpc_get_method 应该返回 NULL */
    const char* method = jsonrpc_get_method(NULL);
    ASSERT_NULL(method);
    
    /* jsonrpc_get_params 应该返回 NULL */
    const cJSON* params = jsonrpc_get_params(NULL);
    ASSERT_NULL(params);
    
    /* jsonrpc_get_id 应该返回 NULL */
    const cJSON* id = jsonrpc_get_id(NULL);
    ASSERT_NULL(id);
    
    TEST_PASS();
}

/**
 * @brief 测试内存分配失败场景（通过 NULL 参数模拟）
 */
static void test_memory_allocation_failure(void) {
    TEST_BEGIN("memory_allocation_failure_simulation");
    
    /* 传入 NULL ID 应该仍能创建响应 */
    char* response = jsonrpc_create_success_response(NULL, NULL);
    ASSERT_NOT_NULL(response);
    cJSON_free(response);
    
    response = jsonrpc_create_error_response(NULL, -32600, "Test error", NULL);
    ASSERT_NOT_NULL(response);
    cJSON_free(response);
    
    TEST_PASS();
}

/* ========== 主函数 ========== */

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    
    printf("\n========================================\n");
    printf("  JSON-RPC 2.0 Protocol Unit Tests v1.0\n");
    printf("  (Testing protocol compliance)\n");
    printf("========================================\n\n");
    
    /* 请求验证测试 */
    printf("[Request Validation Tests]\n");
    test_validate_valid_request();
    test_validate_request_with_params();
    test_validate_missing_fields();
    test_validate_wrong_version();
    test_validate_wrong_field_types();
    printf("\n");
    
    /* 成功响应测试 */
    printf("[Success Response Tests]\n");
    test_create_success_response();
    test_create_success_response_null_result();
    printf("\n");
    
    /* 错误响应测试 */
    printf("[Error Response Tests]\n");
    test_create_error_response();
    test_create_error_response_with_data();
    test_create_parse_error_response();
    test_create_invalid_request_response();
    test_create_method_not_found_response();
    test_create_invalid_params_response();
    test_create_internal_error_response();
    test_create_rate_limited_response();
    test_create_auth_failed_response();
    printf("\n");
    
    /* 错误消息测试 */
    printf("[Error Message Tests]\n");
    test_get_error_message();
    printf("\n");
    
    /* 边界条件测试 */
    printf("[Boundary Condition Tests]\n");
    test_null_input_handling();
    test_memory_allocation_failure();
    printf("\n");
    
    /* 输出结果 */
    printf("========================================\n");
    printf("  Results: %d/%d passed\n", g_tests_passed, g_tests_run);
    printf("========================================\n\n");
    
    return (g_tests_passed == g_tests_run) ? 0 : 1;
}
