/**
 * @file test_service.c
 * @brief LLM 服务核心功能单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "llm_service.h"
#include "service.h"

static void test_service_create_destroy(void) {
    printf("  test_service_create_destroy...\n");

    llm_service_t* svc = llm_service_create(NULL);
    assert(svc != NULL);

    llm_service_destroy(svc);

    printf("    PASSED\n");
}

static void test_service_config(void) {
    printf("  test_service_config...\n");

    llm_service_config_t manager = {
        .default_model = "gpt-4",
        .timeout_ms = 30000,
        .max_retries = 3,
        .cache_enabled = 1,
        .cache_ttl_sec = 3600
    };

    llm_service_t* svc = llm_service_create(&manager);
    assert(svc != NULL);

    llm_service_destroy(svc);

    printf("    PASSED\n");
}

static void test_service_register_provider(void) {
    printf("  test_service_register_provider...\n");

    llm_service_t* svc = llm_service_create(NULL);
    assert(svc != NULL);

    int ret = llm_service_register_provider(svc, "openai", NULL, NULL, NULL, 30.0, 3);
    assert(ret == 0);

    llm_service_destroy(svc);

    printf("    PASSED\n");
}

static void test_message_build(void) {
    printf("  test_message_build...\n");

    llm_message_t messages[2];
    memset(messages, 0, sizeof(messages));

    messages[0].role = strdup("system");
    messages[0].content = strdup("You are a helpful assistant.");

    messages[1].role = strdup("user");
    messages[1].content = strdup("Hello!");

    assert(strcmp(messages[0].role, "system") == 0);
    assert(strcmp(messages[1].content, "Hello!") == 0);

    free((void*)messages[0].role);
    free((void*)messages[0].content);
    free((void*)messages[1].role);
    free((void*)messages[1].content);

    printf("    PASSED\n");
}

static void test_request_config(void) {
    printf("  test_request_config...\n");

    llm_request_config_t manager;
    memset(&manager, 0, sizeof(manager));

    manager.model = "gpt-4";
    manager.temperature = 0.7;
    manager.max_tokens = 1024;
    manager.stream = 0;

    assert(strcmp(manager.model, "gpt-4") == 0);
    assert(manager.temperature > 0.69 && manager.temperature < 0.71);
    assert(manager.max_tokens == 1024);
    assert(manager.stream == 0);

    printf("    PASSED\n");
}

static void test_response_free(void) {
    printf("  test_response_free...\n");

    llm_response_t* resp = (llm_response_t*)calloc(1, sizeof(llm_response_t));
    assert(resp != NULL);

    resp->id = strdup("chatcmpl-123");
    resp->model = strdup("gpt-4");
    resp->finish_reason = strdup("stop");

    llm_response_free(resp);

    printf("    PASSED\n");
}

int main(void) {
    printf("=========================================\n");
    printf("  LLM Service Unit Tests\n");
    printf("=========================================\n");

    test_service_create_destroy();
    test_service_config();
    test_service_register_provider();
    test_message_build();
    test_request_config();
    test_response_free();

    printf("\n✅ All LLM service tests PASSED\n");
    return 0;
}