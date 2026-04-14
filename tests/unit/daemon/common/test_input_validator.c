/**
 * @file test_input_validator.c
 * @brief 输入验证器单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "input_validator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define TEST_PASS(name) printf("✓ %s\n", name)
#define TEST_FAIL(name, reason) do { \
    printf("✗ %s: %s\n", name, reason); \
    return -1; \
} while(0)

static int test_validator_create_destroy(void) {
    validation_result_t* v = validator_create();
    if (!v) {
        TEST_FAIL("create_destroy", "Failed to create validator");
    }
    
    if (v->valid != 1) {
        validator_destroy(v);
        TEST_FAIL("create_destroy", "Initial state should be valid");
    }
    
    validator_destroy(v);
    TEST_PASS("create_destroy");
    return 0;
}

static int test_validate_required(void) {
    validation_result_t* v = validator_create();
    
    cJSON* data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "name", "Test");
    
    // 添加必填规则
    validation_rule_t rule = { .type = VALIDATE_REQUIRED, .field_name = "name" };
    validator_add_rule(v, &rule);
    
    v = validator_validate(v, data);
    if (!v || !v->valid) {
        char* err = v ? v->error_message : NULL;
        validator_destroy(v);
        cJSON_Delete(data);
        TEST_FAIL("validate_required", "Valid data rejected");
    }
    
    validator_destroy(v);
    
    // 测试缺少字段
    v = validator_create();
    cJSON_Delete(data);
    data = cJSON_CreateObject(); // 没有 name 字段
    
    validator_add_rule(v, &rule);
    v = validator_validate(v, data);
    
    if (v && v->valid) {
        validator_destroy(v);
        cJSON_Delete(data);
        TEST_FAIL("validate_required", "Missing field accepted");
    }
    
    validator_destroy(v);
    cJSON_Delete(data);
    TEST_PASS("validate_required");
    return 0;
}

static int test_validate_string_type(void) {
    validation_result_t* v = validator_create();
    
    cJSON* data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "text", "Hello");
    
    validation_rule_t rule = { .type = VALIDATE_STRING, .field_name = "text" };
    validator_add_rule(v, &rule);
    
    v = validator_validate(v, data);
    if (!v || !v->valid) {
        validator_destroy(v);
        cJSON_Delete(data);
        TEST_FAIL("validate_string_type", "String type rejected");
    }
    
    validator_destroy(v);
    
    // 测试非字符串类型
    v = validator_create();
    cJSON_Delete(data);
    data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "text", 123); // 不是字符串
    
    validator_add_rule(v, &rule);
    v = validator_validate(v, data);
    
    if (v && v->valid) {
        validator_destroy(v);
        cJSON_Delete(data);
        TEST_FAIL("validate_string_type", "Non-string accepted");
    }
    
    validator_destroy(v);
    cJSON_Delete(data);
    TEST_PASS("validate_string_type");
    return 0;
}

static int test_validate_length_limits(void) {
    validation_result_t* v = validator_create();
    
    cJSON* data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "password", "abc123"); // 长度=6
    
    // 最小长度规则
    validation_rule_t min_rule = { 
        .type = VALIDATE_MIN_LENGTH, 
        .field_name = "password",
        .length_value = 4
    };
    validator_add_rule(v, &min_rule);
    
    v = validator_validate(v, data);
    if (!v || !v->valid) {
        validator_destroy(v);
        cJSON_Delete(data);
        TEST_FAIL("validate_length_limits", "Valid length rejected");
    }
    
    validator_destroy(v);
    
    // 测试过短
    v = validator_create();
    cJSON_Delete(data);
    data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "password", "ab"); // 长度=2
    
    validator_add_rule(v, &min_rule);
    v = validator_validate(v, data);
    
    if (v && v->valid) {
        validator_destroy(v);
        cJSON_Delete(data);
        TEST_FAIL("validate_length_limits", "Too short accepted");
    }
    
    validator_destroy(v);
    cJSON_Delete(data);
    TEST_PASS("validate_length_limits");
    return 0;
}

static int test_convenience_functions(void) {
    cJSON* obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "name", "Test User");
    cJSON_AddNumberToObject(obj, "age", 25);
    
    char* error = NULL;
    
    // 测试 validate_string_field
    int ret = validate_string_field(obj, "name", 2, 50, &error);
    if (ret != 0) {
        free(error);
        cJSON_Delete(obj);
        TEST_FAIL("convenience_functions", "Valid string field rejected");
    }
    
    ret = validate_string_field(obj, "nonexistent", 2, 50, &error);
    if (ret == 0 || !error) {
        free(error);
        cJSON_Delete(obj);
        TEST_FAIL("convenience_functions", "Missing field accepted");
    }
    free(error);
    error = NULL;
    
    // 测试 validate_number_field
    ret = validate_number_field(obj, "age", 0, 150, &error);
    if (ret != 0) {
        free(error);
        cJSON_Delete(obj);
        TEST_FAIL("convenience_functions", "Valid number field rejected");
    }
    
    ret = validate_number_field(obj, "age", 30, 150, &error);
    if (ret == 0 || !error) {
        free(error);
        cJSON_Delete(obj);
        TEST_FAIL("convenience_functions", "Out of range number accepted");
    }
    free(error);
    
    // 测试 validate_required_field
    ret = validate_required_field(obj, "name", &error);
    if (ret != 0) {
        free(error);
        cJSON_Delete(obj);
        TEST_FAIL("convenience_functions", "Existing required field rejected");
    }
    
    ret = validate_required_field(obj, "missing", &error);
    if (ret == 0 || !error) {
        free(error);
        cJSON_Delete(obj);
        TEST_FAIL("convenience_functions", "Missing required field accepted");
    }
    free(error);
    
    cJSON_Delete(obj);
    TEST_PASS("convenience_functions");
    return 0;
}

static int test_multiple_rules(void) {
    validation_result_t* v = validator_create();
    
    cJSON* data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "username", "testuser");
    cJSON_AddStringToObject(data, "email", "test@example.com");
    cJSON_AddNumberToObject(data, "score", 85.5);
    
    // 添加多个规则
    validation_rule_t rules[] = {
        { .type = VALIDATE_REQUIRED, .field_name = "username" },
        { .type = VALIDATE_STRING, .field_name = "username" },
        { .type = VALIDATE_MIN_LENGTH, .field_name = "username", .length_value = 3 },
        { .type = VALIDATE_MAX_LENGTH, .field_name = "username", .length_value = 32 },
        { .type = VALIDATE_REQUIRED, .field_name = "email" },
        { .type = VALIDATE_STRING, .field_name = "email" },
        { .type = VALIDATE_REQUIRED, .field_name = "score" },
        { .type = VALIDATE_NUMBER, .field_name = "score" },
        { .type = VALIDATE_MIN_VALUE, .field_name = "score", .number_value = 0 },
        { .type = VALIDATE_MAX_VALUE, .field_name = "score", .number_value = 100 }
    };
    
    for (size_t i = 0; i < sizeof(rules)/sizeof(rules[0]); i++) {
        validator_add_rule(v, &rules[i]);
    }
    
    v = validator_validate(v, data);
    if (!v || !v->valid) {
        char* err = v ? v->error_message : NULL;
        validator_destroy(v);
        cJSON_Delete(data);
        printf("Error: %s\n", err ? err : "(null)");
        TEST_FAIL("multiple_rules", "Valid multi-field data rejected");
    }
    
    validator_destroy(v);
    cJSON_Delete(data);
    TEST_PASS("multiple_rules");
    return 0;
}

int main(void) {
    int failed = 0;

    printf("\n=== Input Validator Unit Tests ===\n\n");

    if (test_validator_create_destroy() != 0) failed++;
    if (test_validate_required() != 0) failed++;
    if (test_validate_string_type() != 0) failed++;
    if (test_validate_length_limits() != 0) failed++;
    if (test_convenience_functions() != 0) failed++;
    if (test_multiple_rules() != 0) failed++;

    printf("\n=== Test Summary ===\n");
    printf("Total: 6 tests\n");
    printf("Passed: %d\n", 6 - failed);
    printf("Failed: %d\n", failed);

    return (failed == 0) ? 0 : 1;
}
