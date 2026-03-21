/**
 * @file test_llm.c
 * @brief LLM 服务单元测试
 * @details 测试 LLM 服务的各个功能模块
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../../../backs/llm_d/include/llm_service.h"
#include "../../../../backs/llm_d/include/response.h"

/**
 * @brief 测试创建和销毁 LLM 服务
 * @return 0 表示成功，非 0 表示失败
 */
int test_create_destroy() {
    printf("=== Testing create and destroy ===\n");
    
    const char* config_path = "config/services/llm.yaml";
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
 * @brief 测试 LLM 文本生成
 * @return 0 表示成功，非 0 表示失败
 */
int test_complete() {
    printf("=== Testing complete ===\n");
    
    const char* config_path = "config/services/llm.yaml";
    llm_service_t* service = llm_service_create(config_path);
    if (!service) {
        printf("Failed to create LLM service\n");
        return -1;
    }

    // 构建请求配置
    llm_request_config_t cfg = {
        .model = "gpt-3.5-turbo",
        .messages = NULL,
        .message_count = 0,
        .temperature = 0.7,
        .top_p = 0.9,
        .max_tokens = 100,
        .stream = false
    };

    // 创建测试消息
    llm_message_t messages[] = {
        {"user", "Hello, how are you?"}
    };
    cfg.messages = messages;
    cfg.message_count = 1;

    // 执行文本生成
    llm_response_t* resp = NULL;
    int ret = llm_service_complete(service, &cfg, &resp);
    if (ret != 0) {
        printf("Failed to complete LLM request\n");
        llm_service_destroy(service);
        return ret;
    }

    if (resp) {
        printf("Response received\n");
        llm_response_free(resp);
    }

    ret = llm_service_destroy(service);
    if (ret != 0) {
        printf("Failed to destroy LLM service\n");
        return ret;
    }

    printf("Complete test passed\n\n");
    return 0;
}

/**
 * @brief 测试 LLM 流式文本生成
 * @return 0 表示成功，非 0 表示失败
 */
int test_complete_stream() {
    printf("=== Testing complete stream ===\n");
    
    const char* config_path = "config/services/llm.yaml";
    llm_service_t* service = llm_service_create(config_path);
    if (!service) {
        printf("Failed to create LLM service\n");
        return -1;
    }

    // 构建请求配置
    llm_request_config_t cfg = {
        .model = "gpt-3.5-turbo",
        .messages = NULL,
        .message_count = 0,
        .temperature = 0.7,
        .top_p = 0.9,
        .max_tokens = 100,
        .stream = true
    };

    // 创建测试消息
    llm_message_t messages[] = {
        {"user", "Hello, how are you?"}
    };
    cfg.messages = messages;
    cfg.message_count = 1;

    // 执行流式文本生成
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
 * @brief 测试 LLM 响应转换为 JSON
 * @return 0 表示成功，非 0 表示失败
 */
int test_response_to_json() {
    printf("=== Testing response to JSON ===\n");
    
    // 创建测试响应
    llm_response_t resp = {
        .id = "test-id",
        .object = "chat.completion",
        .created = 1234567890,
        .model = "gpt-3.5-turbo",
        .choices = NULL,
        .choice_count = 0,
        .usage = {
            .prompt_tokens = 10,
            .completion_tokens = 20,
            .total_tokens = 30
        }
    };

    // 转换为 JSON
    char* json = response_to_json(&resp);
    if (!json) {
        printf("Failed to convert response to JSON\n");
        return -1;
    }

    printf("Response JSON generated\n");
    free(json);

    printf("Response to JSON test passed\n\n");
    return 0;
}

/**
 * @brief 主测试函数
 * @return 0 表示所有测试通过，非 0 表示有测试失败
 */
int main() {
    int ret = 0;

    ret |= test_create_destroy();
    ret |= test_complete();
    ret |= test_complete_stream();
    ret |= test_response_to_json();

    if (ret == 0) {
        printf("All tests passed!\n");
    } else {
        printf("Some tests failed!\n");
    }

    return ret;
}
