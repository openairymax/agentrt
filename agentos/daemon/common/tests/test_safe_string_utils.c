/**
 * @file test_safe_string_utils.c
 * @brief 安全字符串工具单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "safe_string_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define TEST_PASS(name) printf("✓ %s\n", name)
#define TEST_FAIL(name, reason) do { \
    printf("✗ %s: %s\n", name, reason); \
    return -1; \
} while(0)

static int test_safe_strcpy(void) {
    char buf[64];
    
    int ret = safe_strcpy(buf, "Hello World", sizeof(buf));
    if (ret != 0 || strcmp(buf, "Hello World") != 0) {
        TEST_FAIL("safe_strcpy", "Basic copy failed");
    }
    
    ret = safe_strcpy(buf, "This is a very long string that exceeds buffer", 10);
    if (ret != -2) {
        TEST_FAIL("safe_strcpy", "Should fail on overflow");
    }
    
    ret = safe_strcpy(NULL, "test", 10);
    if (ret != -1) {
        TEST_FAIL("safe_strcpy", "Should reject NULL dest");
    }
    
    TEST_PASS("safe_strcpy");
    return 0;
}

static int test_safe_strcat(void) {
    char buf[64] = "Hello";
    
    int ret = safe_strcat(buf, " World", sizeof(buf));
    if (ret != 0 || strcmp(buf, "Hello World") != 0) {
        TEST_FAIL("safe_strcat", "Basic concat failed");
    }
    
    char small[8] = "Hi";
    ret = safe_strcat(small, " Very Long String", sizeof(small));
    if (ret != -2) {
        TEST_FAIL("safe_strcat", "Should fail on overflow");
    }
    
    TEST_PASS("safe_strcat");
    return 0;
}

static int test_safe_sprintf(void) {
    char buf[64];
    
    int written = safe_sprintf(buf, sizeof(buf), "%d + %d = %d", 2, 3, 5);
    if (written <= 0 || strcmp(buf, "2 + 3 = 5") != 0) {
        TEST_FAIL("safe_sprintf", "Basic format failed");
    }
    
    written = safe_sprintf(buf, 5, "Long string %s", "that overflows");
    if (written >= 5) {
        TEST_FAIL("safe_sprintf", "Should truncate");
    }
    
    TEST_PASS("safe_sprintf");
    return 0;
}

static int test_safe_strlen(void) {
    const char* str = "Hello";
    
    size_t len = safe_strlen(str, 100);
    if (len != 5) {
        TEST_FAIL("safe_strlen", "Wrong length for short string");
    }
    
    len = safe_strlen(str, 3);
    if (len != 3) {
        TEST_FAIL("safe_strlen", "Wrong length with limit");
    }
    
    len = safe_strlen(NULL, 10);
    if (len != 0) {
        TEST_FAIL("safe_strlen", "NULL should return 0");
    }
    
    TEST_PASS("safe_strlen");
    return 0;
}

static int test_safe_strcmp(void) {
    int cmp = safe_strcmp("abc", "abc", 10);
    if (cmp != 0) {
        TEST_FAIL("safe_strcmp", "Equal strings should return 0");
    }
    
    cmp = safe_strcmp("abc", "abd", 10);
    if (cmp >= 0) {
        TEST_FAIL("safe_strcmp", "First should be less than second");
    }
    
    cmp = safe_strcmp(NULL, NULL, 10);
    if (cmp != 0) {
        TEST_FAIL("safe_strcmp", "Both NULL should be equal");
    }
    
    TEST_PASS("safe_strcmp");
    return 0;
}

static int test_safe_strdup_with_limit(void) {
    char* copy = safe_strdup_with_limit("Hello World", 5);
    if (!copy || strcmp(copy, "Hello") != 0) {
        free(copy);
        TEST_FAIL("safe_strdup_with_limit", "Copy with limit failed");
    }
    free(copy);
    
    copy = safe_strdup_with_limit("Test", 0);
    if (!copy || strcmp(copy, "Test") != 0) {
        free(copy);
        TEST_FAIL("safe_strdup_with_limit", "No limit failed");
    }
    free(copy);
    
    copy = safe_strdup_with_limit(NULL, 10);
    if (copy != NULL) {
        free(copy);
        TEST_FAIL("safe_strdup_with_limit", "NULL should return NULL");
    }
    
    TEST_PASS("safe_strdup_with_limit");
    return 0;
}

static int test_is_valid_ascii(void) {
    if (!is_valid_ascii("Hello World!", 12)) {
        TEST_FAIL("is_valid_ascii", "Valid ASCII rejected");
    }
    
    if (is_valid_ascii("Invalid \x80\x81", 11)) {
        TEST_FAIL("is_valid_ascii", "Non-ASCII accepted");
    }
    
    if (!is_valid_ascii("", 0)) {
        TEST_FAIL("is_valid_ascii", "Empty string should be valid");
    }
    
    TEST_PASS("is_valid_ascii");
    return 0;
}

static int test_secure_clear(void) {
    char secret[] = "password123";
    secure_clear(secret, sizeof(secret));
    
    for (size_t i = 0; i < sizeof(secret); i++) {
        if (secret[i] != '\0') {
            TEST_FAIL("secure_clear", "Data not cleared");
        }
    }
    
    secure_clear(NULL, 10); // Should not crash
    
    TEST_PASS("secure_clear");
    return 0;
}

int main(void) {
    int failed = 0;

    printf("\n=== Safe String Utils Unit Tests ===\n\n");

    if (test_safe_strcpy() != 0) failed++;
    if (test_safe_strcat() != 0) failed++;
    if (test_safe_sprintf() != 0) failed++;
    if (test_safe_strlen() != 0) failed++;
    if (test_safe_strcmp() != 0) failed++;
    if (test_safe_strdup_with_limit() != 0) failed++;
    if (test_is_valid_ascii() != 0) failed++;
    if (test_secure_clear() != 0) failed++;

    printf("\n=== Test Summary ===\n");
    printf("Total: 8 tests\n");
    printf("Passed: %d\n", 8 - failed);
    printf("Failed: %d\n", failed);

    return (failed == 0) ? 0 : 1;
}
