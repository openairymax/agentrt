﻿﻿﻿/**
 * @file test_llm.c
 * @brief LLM Service 单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "llm_service.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief 测试 LLM 服务创建和销毁
 * @return 0 表示成功，非 0 表示失败
 */
int test_create_destroy() {
    printf("=== Testing create and destroy ===\n");
    
    const char* config_path = "manager/service/llm_d/llm.yaml";
    llm_service_t* service = llm_service_create(config_path);
    if (!service) {
        printf("Failed to create LLM service\n");
        return -1;
    }

    int ret = llm_service_destroy(service);
    if (ret != 0) {
        printf("Failed to destroy LLM service\n");
        return ret;
    }

    printf("Create and destroy test passed\n\n");
    return 0;
}

/**
 * @brief 测试流式文本生成
 * @return 0 表示成功，非 0 表示失败
 */
int test_complete_stream() {
    printf("=== Testing complete stream ===\n");
    
    const char* config_path = "manager/service/llm_d/llm.yaml";
    llm_service_t* service = llm_service_create(config_path);
    if (!service) {
        printf("Failed to create LLM service\n");
        return -1;
    }

    llm_request_config_t cfg = {
        .model = "gpt-3.5-turbo",
        .messages = NULL,
        .message_count = 0,
        .temperature = 0.7,
        .top_p = 0.9,
        .max_tokens = 100,
        .stream = true
    };

    llm_message_t messages[] = {
        {"user", "Hello, how are you?"}
    };
    cfg.messages = messages;
    cfg.message_count = 1;

    llm_response_t* resp = NULL;
    int ret = llm_service_complete_stream(service, &cfg, NULL, &resp);
    if (ret != 0) {
        printf("Failed to complete stream LLM request\n");
        llm_service_destroy(service);
        return ret;
    }

    if (resp) {
        llm_response_free(resp);
    }

    ret = llm_service_destroy(service);
    if (ret != 0) {
        printf("Failed to destroy LLM service\n");
        return ret;
    }

    printf("Complete stream test passed\n\n");
    return 0;
}

/**
 * @brief 主函数
 */
int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    int failed = 0;

    if (test_create_destroy() != 0) failed++;
    if (test_complete_stream() != 0) failed++;

    if (failed == 0) {
        printf("All tests passed!\n");
        return 0;
    } else {
        printf("%d test(s) failed\n", failed);
        return 1;
    }
}
