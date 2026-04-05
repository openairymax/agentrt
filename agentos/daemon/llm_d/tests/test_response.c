/**
 * @file test_response.c
 * @brief LLM 响应处理单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "response.h"

static void test_response_create(void) {
    printf("  test_response_create...\n");

    llm_response_t* resp = llm_response_create();
    assert(resp != NULL);
    assert(resp->id == NULL);
    assert(resp->model == NULL);
    assert(resp->choices == NULL);
    assert(resp->choice_count == 0);

    llm_response_free(resp);

    printf("    PASSED\n");
}

static void test_response_set_id(void) {
    printf("  test_response_set_id...\n");

    llm_response_t* resp = llm_response_create();
    assert(resp != NULL);

    int ret = llm_response_set_id(resp, "chatcmpl-abc123");
    assert(ret == 0);
    assert(strcmp(resp->id, "chatcmpl-abc123") == 0);

    llm_response_free(resp);

    printf("    PASSED\n");
}

static void test_response_set_model(void) {
    printf("  test_response_set_model...\n");

    llm_response_t* resp = llm_response_create();
    assert(resp != NULL);

    int ret = llm_response_set_model(resp, "gpt-4");
    assert(ret == 0);
    assert(strcmp(resp->model, "gpt-4") == 0);

    llm_response_free(resp);

    printf("    PASSED\n");
}

static void test_response_add_choice(void) {
    printf("  test_response_add_choice...\n");

    llm_response_t* resp = llm_response_create();
    assert(resp != NULL);

    llm_message_t choice;
    memset(&choice, 0, sizeof(choice));
    choice.role = strdup("assistant");
    choice.content = strdup("Hello! How can I help you?");

    int ret = llm_response_add_choice(resp, &choice);
    assert(ret == 0);
    assert(resp->choice_count == 1);
    assert(strcmp(resp->choices[0].role, "assistant") == 0);
    assert(strcmp(resp->choices[0].content, "Hello! How can I help you?") == 0);

    free((void*)choice.role);
    free((void*)choice.content);
    llm_response_free(resp);

    printf("    PASSED\n");
}

static void test_response_set_usage(void) {
    printf("  test_response_set_usage...\n");

    llm_response_t* resp = llm_response_create();
    assert(resp != NULL);

    int ret = llm_response_set_usage(resp, 100, 50, 150);
    assert(ret == 0);
    assert(resp->prompt_tokens == 100);
    assert(resp->completion_tokens == 50);
    assert(resp->total_tokens == 150);

    llm_response_free(resp);

    printf("    PASSED\n");
}

static void test_response_set_finish_reason(void) {
    printf("  test_response_set_finish_reason...\n");

    llm_response_t* resp = llm_response_create();
    assert(resp != NULL);

    int ret = llm_response_set_finish_reason(resp, "stop");
    assert(ret == 0);
    assert(strcmp(resp->finish_reason, "stop") == 0);

    llm_response_free(resp);

    printf("    PASSED\n");
}

static void test_response_get_content(void) {
    printf("  test_response_get_content...\n");

    llm_response_t* resp = llm_response_create();
    assert(resp != NULL);

    llm_message_t choice;
    memset(&choice, 0, sizeof(choice));
    choice.role = strdup("assistant");
    choice.content = strdup("This is the response content.");

    llm_response_add_choice(resp, &choice);

    const char* content = llm_response_get_content(resp);
    assert(content != NULL);
    assert(strcmp(content, "This is the response content.") == 0);

    free((void*)choice.role);
    free((void*)choice.content);
    llm_response_free(resp);

    printf("    PASSED\n");
}

static void test_response_parse_json(void) {
    printf("  test_response_parse_json...\n");

    const char* json_response = "{"
        "\"id\": \"chatcmpl-123\","
        "\"object\": \"chat.completion\","
        "\"created\": 1677652288,"
        "\"model\": \"gpt-4\","
        "\"choices\": [{"
            "\"index\": 0,"
            "\"message\": {"
                "\"role\": \"assistant\","
                "\"content\": \"Hello!\""
            "},"
            "\"finish_reason\": \"stop\""
        "}],"
        "\"usage\": {"
            "\"prompt_tokens\": 10,"
            "\"completion_tokens\": 5,"
            "\"total_tokens\": 15"
        "}"
    "}";

    llm_response_t* resp = llm_response_parse(json_response);
    assert(resp != NULL);
    assert(strcmp(resp->id, "chatcmpl-123") == 0);
    assert(strcmp(resp->model, "gpt-4") == 0);
    assert(resp->choice_count == 1);
    assert(resp->total_tokens == 15);

    llm_response_free(resp);

    printf("    PASSED\n");
}

int main(void) {
    printf("=========================================\n");
    printf("  LLM Response Unit Tests\n");
    printf("=========================================\n");

    test_response_create();
    test_response_set_id();
    test_response_set_model();
    test_response_add_choice();
    test_response_set_usage();
    test_response_set_finish_reason();
    test_response_get_content();
    test_response_parse_json();

    printf("\n✅ All LLM response tests PASSED\n");
    return 0;
}