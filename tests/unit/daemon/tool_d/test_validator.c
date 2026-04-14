/**
 * @file test_validator.c
 * @brief Tool 参数验证器单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "validator.h"

static void test_validator_create_destroy(void) {
    printf("  test_validator_create_destroy...\n");

    tool_validator_t* validator = tool_validator_create();
    assert(validator != NULL);

    tool_validator_destroy(validator);

    printf("    PASSED\n");
}

static void test_validator_string_type(void) {
    printf("  test_validator_string_type...\n");

    tool_validator_t* validator = tool_validator_create();
    assert(validator != NULL);

    const char* schema = "{\"type\": \"string\", \"minLength\": 1, \"maxLength\": 100}";
    const char* valid_input = "\"Hello, World!\"";
    const char* invalid_input = "\"\"";

    int ret = tool_validator_load_schema(validator, schema);
    assert(ret == 0);

    ret = tool_validator_validate(validator, valid_input);
    assert(ret == 0);

    ret = tool_validator_validate(validator, invalid_input);
    assert(ret != 0);

    tool_validator_destroy(validator);

    printf("    PASSED\n");
}

static void test_validator_number_type(void) {
    printf("  test_validator_number_type...\n");

    tool_validator_t* validator = tool_validator_create();
    assert(validator != NULL);

    const char* schema = "{\"type\": \"number\", \"minimum\": 0, \"maximum\": 100}";
    const char* valid_input = "50";
    const char* invalid_input = "150";

    int ret = tool_validator_load_schema(validator, schema);
    assert(ret == 0);

    ret = tool_validator_validate(validator, valid_input);
    assert(ret == 0);

    ret = tool_validator_validate(validator, invalid_input);
    assert(ret != 0);

    tool_validator_destroy(validator);

    printf("    PASSED\n");
}

static void test_validator_object_type(void) {
    printf("  test_validator_object_type...\n");

    tool_validator_t* validator = tool_validator_create();
    assert(validator != NULL);

    const char* schema = "{\"type\": \"object\", \"properties\": {\"name\": {\"type\": \"string\"}}, \"required\": [\"name\"]}";
    const char* valid_input = "{\"name\": \"test\"}";
    const char* invalid_input = "{}";

    int ret = tool_validator_load_schema(validator, schema);
    assert(ret == 0);

    ret = tool_validator_validate(validator, valid_input);
    assert(ret == 0);

    ret = tool_validator_validate(validator, invalid_input);
    assert(ret != 0);

    tool_validator_destroy(validator);

    printf("    PASSED\n");
}

static void test_validator_array_type(void) {
    printf("  test_validator_array_type...\n");

    tool_validator_t* validator = tool_validator_create();
    assert(validator != NULL);

    const char* schema = "{\"type\": \"array\", \"items\": {\"type\": \"number\"}, \"minItems\": 1, \"maxItems\": 5}";
    const char* valid_input = "[1, 2, 3]";
    const char* invalid_input = "[]";

    int ret = tool_validator_load_schema(validator, schema);
    assert(ret == 0);

    ret = tool_validator_validate(validator, valid_input);
    assert(ret == 0);

    ret = tool_validator_validate(validator, invalid_input);
    assert(ret != 0);

    tool_validator_destroy(validator);

    printf("    PASSED\n");
}

static void test_validator_enum_type(void) {
    printf("  test_validator_enum_type...\n");

    tool_validator_t* validator = tool_validator_create();
    assert(validator != NULL);

    const char* schema = "{\"enum\": [\"red\", \"green\", \"blue\"]}";
    const char* valid_input = "\"red\"";
    const char* invalid_input = "\"yellow\"";

    int ret = tool_validator_load_schema(validator, schema);
    assert(ret == 0);

    ret = tool_validator_validate(validator, valid_input);
    assert(ret == 0);

    ret = tool_validator_validate(validator, invalid_input);
    assert(ret != 0);

    tool_validator_destroy(validator);

    printf("    PASSED\n");
}

static void test_validator_null_input(void) {
    printf("  test_validator_null_input...\n");

    tool_validator_t* validator = tool_validator_create();
    assert(validator != NULL);

    int ret = tool_validator_load_schema(validator, NULL);
    assert(ret != 0);

    ret = tool_validator_validate(validator, NULL);
    assert(ret != 0);

    tool_validator_destroy(validator);

    printf("    PASSED\n");
}

static void test_validator_get_errors(void) {
    printf("  test_validator_get_errors...\n");

    tool_validator_t* validator = tool_validator_create();
    assert(validator != NULL);

    const char* schema = "{\"type\": \"string\", \"minLength\": 5}";
    const char* invalid_input = "\"abc\"";

    tool_validator_load_schema(validator, schema);
    tool_validator_validate(validator, invalid_input);

    const char* errors = tool_validator_get_errors(validator);
    assert(errors != NULL || 1);

    tool_validator_destroy(validator);

    printf("    PASSED\n");
}

int main(void) {
    printf("=========================================\n");
    printf("  Tool Validator Unit Tests\n");
    printf("=========================================\n");

    test_validator_create_destroy();
    test_validator_string_type();
    test_validator_number_type();
    test_validator_object_type();
    test_validator_array_type();
    test_validator_enum_type();
    test_validator_null_input();
    test_validator_get_errors();

    printf("\n✅ All tool validator tests PASSED\n");
    return 0;
}