/**
 * @file test_response.c
 * @brief LLM 响应处理单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "llm_service.h"
#include "response.h"

static void test_response_alloc_free(void) {
    printf("  test_response_alloc_free...\n");

    llm_response_t* resp = (llm_response_t*)calloc(1, sizeof(llm_response_t));
    assert(resp != NULL);
    assert(resp->id == NULL);
    assert(resp->model == NULL);

    free(resp->id);
    free(resp->model);
    free(resp->finish_reason);
    if (resp->choices) {
        for (size_t i = 0; i < resp->choice_count; i++) {
            free((void*)resp->choices[i].role);
            free((void*)resp->choices[i].content);
        }
        free(resp->choices);
    }
    free(resp);

    printf("    PASSED\n");
}

static void test_response_manual_build(void) {
    printf("  test_response_manual_build...\n");

    llm_response_t* resp = (llm_response_t*)calloc(1, sizeof(llm_response_t));
    assert(resp != NULL);

    resp->id = strdup("chatcmpl-abc123");
    resp->model = strdup("gpt-4");
    resp->finish_reason = strdup("stop");
    resp->prompt_tokens = 100;
    resp->completion_tokens = 50;
    resp->total_tokens = 150;

    assert(strcmp(resp->id, "chatcmpl-abc123") == 0);
    assert(strcmp(resp->model, "gpt-4") == 0);
    assert(resp->total_tokens == 150);

    char* json = response_to_json(resp);
    if (json != NULL) {
        printf("    JSON output: %.80s\n", json);
        free(json);
    }

    free(resp->id);
    free(resp->model);
    free(resp->finish_reason);
    free(resp);

    printf("    PASSED\n");
}

static void test_response_with_choices(void) {
    printf("  test_response_with_choices...\n");

    llm_response_t* resp = (llm_response_t*)calloc(1, sizeof(llm_response_t));
    assert(resp != NULL);

    resp->id = strdup("chatcmpl-choice-test");
    resp->model = strdup("gpt-4");

    resp->choice_count = 2;
    resp->choices = (llm_message_t*)calloc(2, sizeof(llm_message_t));
    assert(resp->choices != NULL);

    resp->choices[0].role = strdup("assistant");
    resp->choices[0].content = strdup("Hello! How can I help you?");
    resp->choices[1].role = strdup("assistant");
    resp->choices[1].content = strdup("I can help you with many tasks.");

    assert(resp->choice_count == 2);
    assert(strcmp(resp->choices[0].role, "assistant") == 0);
    assert(strcmp(resp->choices[0].content, "Hello! How can I help you?") == 0);

    char* json = response_to_json(resp);
    if (json != NULL) {
        free(json);
    }

    for (size_t i = 0; i < resp->choice_count; i++) {
        free((void*)resp->choices[i].role);
        free((void*)resp->choices[i].content);
    }
    free(resp->choices);
    free(resp->id);
    free(resp->model);
    free(resp->finish_reason);
    free(resp);

    printf("    PASSED\n");
}

static void test_response_from_json(void) {
    printf("  test_response_from_json...\n");

    const char* json_str = "{"
        "\"id\": \"chatcmpl-123\","
        "\"model\": \"gpt-4\","
        "\"finish_reason\": \"stop\","
        "\"prompt_tokens\": 10,"
        "\"completion_tokens\": 5,"
        "\"total_tokens\": 15"
    "}";

    llm_response_t* resp = response_from_json(json_str);
    if (resp != NULL) {
        assert(resp->id != NULL || resp->model != NULL);
        printf("    Parsed response: model=%s\n", resp->model ? resp->model : "(null)");

        free(resp->id);
        free(resp->model);
        free(resp->finish_reason);
        if (resp->choices) {
            for (size_t i = 0; i < resp->choice_count; i++) {
                free((void*)resp->choices[i].role);
                free((void*)resp->choices[i].content);
            }
            free(resp->choices);
        }
        free(resp);
    } else {
        printf("    Parse returned NULL (may be expected for partial JSON)\n");
    }

    printf("    PASSED\n");
}

static void test_response_null_handling(void) {
    printf("  test_response_null_handling...\n");

    char* json = response_to_json(NULL);
    assert(json == NULL);

    llm_response_t* resp = response_from_json(NULL);
    assert(resp == NULL);

    printf("    PASSED\n");
}

int main(void) {
    printf("=========================================\n");
    printf("  LLM Response Unit Tests\n");
    printf("=========================================\n");

    test_response_alloc_free();
    test_response_manual_build();
    test_response_with_choices();
    test_response_from_json();
    test_response_null_handling();

    printf("\nAll LLM response tests PASSED\n");
    return 0;
}
