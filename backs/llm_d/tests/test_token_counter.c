/**
 * @file test_token_counter.c
 * @brief Token 计数器单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
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
    uint32_t count = token_counter_count(counter, text);
    assert(count > 0);

    token_counter_destroy(counter);

    printf("    PASSED\n");
}

static void test_token_counter_empty_string(void) {
    printf("  test_token_counter_empty_string...\n");

    token_counter_t* counter = token_counter_create("gpt-4");
    assert(counter != NULL);

    uint32_t count = token_counter_count(counter, "");
    assert(count == 0);

    token_counter_destroy(counter);

    printf("    PASSED\n");
}

static void test_token_counter_null_input(void) {
    printf("  test_token_counter_null_input...\n");

    token_counter_t* counter = token_counter_create("gpt-4");
    assert(counter != NULL);

    uint32_t count = token_counter_count(counter, NULL);
    assert(count == 0);

    token_counter_destroy(counter);

    printf("    PASSED\n");
}

static void test_token_counter_estimate_tokens(void) {
    printf("  test_token_counter_estimate_tokens...\n");

    token_counter_t* counter = token_counter_create("gpt-4");
    assert(counter != NULL);

    const char* text = "The quick brown fox jumps over the lazy dog.";
    uint32_t estimated = token_counter_estimate(text);
    assert(estimated > 0);

    token_counter_destroy(counter);

    printf("    PASSED\n");
}

static void test_token_counter_messages(void) {
    printf("  test_token_counter_messages...\n");

    token_counter_t* counter = token_counter_create("gpt-4");
    assert(counter != NULL);

    llm_message_t messages[3];
    memset(messages, 0, sizeof(messages));

    messages[0].role = "system";
    messages[0].content = "You are a helpful assistant.";

    messages[1].role = "user";
    messages[1].content = "Hello!";

    messages[2].role = "assistant";
    messages[2].content = "Hi there! How can I help you today?";

    uint32_t total = token_counter_count_messages(counter, messages, 3);
    assert(total > 0);

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
    test_token_counter_estimate_tokens();
    test_token_counter_messages();

    printf("\n✅ All token counter tests PASSED\n");
    return 0;
}