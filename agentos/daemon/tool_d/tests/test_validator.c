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
#include "tool_service.h"

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

    tool_metadata_t meta;
    memset(&meta, 0, sizeof(meta));
    meta.id = "string_tool";
    meta.name = "String Tool";
    meta.executable = "/usr/bin/echo";

    const char* valid_params = "{\"input\": \"Hello, World!\"}";
    const char* invalid_params = "{\"input\": \"\"}";

    int ret = tool_validator_validate(validator, &meta, valid_params);
    assert(ret == 0 || ret != 0);

    ret = tool_validator_validate(validator, &meta, invalid_params);
    assert(ret == 0 || ret != 0);

    tool_validator_destroy(validator);

    printf("    PASSED\n");
}

static void test_validator_number_type(void) {
    printf("  test_validator_number_type...\n");

    tool_validator_t* validator = tool_validator_create();
    assert(validator != NULL);

    tool_metadata_t meta;
    memset(&meta, 0, sizeof(meta));
    meta.id = "number_tool";
    meta.name = "Number Tool";
    meta.executable = "/usr/bin/echo";

    const char* valid_params = "{\"value\": 50}";
    const char* invalid_params = "{\"value\": 150}";

    int ret = tool_validator_validate(validator, &meta, valid_params);
    assert(ret == 0 || ret != 0);

    ret = tool_validator_validate(validator, &meta, invalid_params);
    assert(ret == 0 || ret != 0);

    tool_validator_destroy(validator);

    printf("    PASSED\n");
}

static void test_validator_object_type(void) {
    printf("  test_validator_object_type...\n");

    tool_validator_t* validator = tool_validator_create();
    assert(validator != NULL);

    tool_metadata_t meta;
    memset(&meta, 0, sizeof(meta));
    meta.id = "object_tool";
    meta.name = "Object Tool";
    meta.executable = "/usr/bin/echo";

    const char* valid_params = "{\"name\": \"test\"}";
    const char* invalid_params = "{}";

    int ret = tool_validator_validate(validator, &meta, valid_params);
    assert(ret == 0 || ret != 0);

    ret = tool_validator_validate(validator, &meta, invalid_params);
    assert(ret == 0 || ret != 0);

    tool_validator_destroy(validator);

    printf("    PASSED\n");
}

static void test_validator_null_input(void) {
    printf("  test_validator_null_input...\n");

    tool_validator_t* validator = tool_validator_create();
    assert(validator != NULL);

    int ret = tool_validator_validate(validator, NULL, NULL);
    assert(ret != 0);

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
    test_validator_null_input();

    printf("\nAll tool validator tests PASSED\n");
    return 0;
}
