/**
 * @file test_token_counter.c
 * @brief Token 计数器单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include "token_counter.h"

static void test_token_counter_create_destroy(void) {
    printf("  test_token_counter_create_destroy...\n");

    token_counter_t* counter = token_counter_create("gpt-4");
    assert(counter != NULL);

    token_counter_destroy(counter);

    printf("    PASSED\n");
}

static void test_token_counter_count(void) {
    printf("  test_token_counter_count...\n");

    token_counter_t* counter = token_counter_create("gpt-4");
    assert(counter != NULL);

    const char* text = "Hello, world! This is a test.";
    size_t count = token_counter_count(counter, text);
    assert(count > 0);

    token_counter_destroy(counter);

    printf("    PASSED\n");
}

static void test_token_counter_empty_string(void) {
    printf("  test_token_counter_empty_string...\n");

    token_counter_t* counter = token_counter_create("gpt-4");
    assert(counter != NULL);

    size_t count = token_counter_count(counter, "");
    assert(count == 0);

    token_counter_destroy(counter);

    printf("    PASSED\n");
}

static void test_token_counter_null_input(void) {
    printf("  test_token_counter_null_input...\n");

    token_counter_t* counter = token_counter_create("gpt-4");
    assert(counter != NULL);

    size_t count = token_counter_count(counter, NULL);
    assert(count == 0);

    token_counter_destroy(counter);

    printf("    PASSED\n");
}

static void test_token_counter_estimate(void) {
    printf("  test_token_counter_estimate...\n");

    token_counter_t* counter = token_counter_create("gpt-4");
    assert(counter != NULL);

    const char* text = "The quick brown fox jumps over the lazy dog.";
    size_t estimated = token_counter_count(counter, text);
    assert(estimated > 0);

    token_counter_destroy(counter);

    printf("    PASSED\n");
}

int main(void) {
    printf("=========================================\n");
    printf("  Token Counter Unit Tests\n");
    printf("=========================================\n");

    test_token_counter_create_destroy();
    test_token_counter_count();
    test_token_counter_empty_string();
    test_token_counter_null_input();
    test_token_counter_estimate();

    printf("\nAll token counter tests PASSED\n");
    return 0;
}
