﻿﻿﻿﻿﻿﻿﻿/**
 * @file test_tool.c
 * @brief Tool Service 单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "tool_service.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief 测试工具服务创建和销毁
 * @return 0 表示成功，非 0 表示失败
 */
int test_create_destroy() {
    printf("=== Testing create and destroy ===\n");
    
    const char* config_path = "agentos/manager/service/tool_d/tool.yaml";
    tool_service_t* service = tool_service_create(config_path);
    if (!service) {
        printf("Failed to create tool service\n");
        return -1;
    }

    int ret = tool_service_destroy(service);
    if (ret != 0) {
        printf("Failed to destroy tool service\n");
        return ret;
    }

    printf("Create and destroy test passed\n\n");
    return 0;
}

/**
 * @brief 测试工具注册
 * @return 0 表示成功，非 0 表示失败
 */
int test_register_tool() {
    printf("=== Testing register tool ===\n");
    
    const char* config_path = "agentos/manager/service/tool_d/tool.yaml";
    tool_service_t* service = tool_service_create(config_path);
    if (!service) {
        printf("Failed to create tool service\n");
        return -1;
    }

    tool_metadata_t meta = {
        .id = "test-tool",
        .name = "Test Tool",
        .description = "A test tool",
        .executable = "/bin/echo",
        .timeout_sec = 10,
        .cacheable = false,
        .permission_rule = "all",
        .params = NULL,
        .param_count = 0
    };

    int ret = tool_service_register(service, &meta);
    if (ret != 0) {
        printf("Failed to register tool\n");
        tool_service_destroy(service);
        return ret;
    }

    ret = tool_service_destroy(service);
    if (ret != 0) {
        printf("Failed to destroy tool service\n");
        return ret;
    }

    printf("Register tool test passed\n\n");
    return 0;
}

/**
 * @brief 测试工具列表
 * @return 0 表示成功，非 0 表示失败
 */
int test_list_tools() {
    printf("=== Testing list tools ===\n");
    
    const char* config_path = "agentos/manager/service/tool_d/tool.yaml";
    tool_service_t* service = tool_service_create(config_path);
    if (!service) {
        printf("Failed to create tool service\n");
        return -1;
    }

    tool_metadata_t meta = {
        .id = "test-tool",
        .name = "Test Tool",
        .description = "A test tool",
        .executable = "/bin/echo",
        .timeout_sec = 10,
        .cacheable = false,
        .permission_rule = "all",
        .params = NULL,
        .param_count = 0
    };

    int ret = tool_service_register(service, &meta);
    if (ret != 0) {
        printf("Failed to register tool\n");
        tool_service_destroy(service);
        return ret;
    }

    char* list_json = tool_service_list(service);
    if (!list_json) {
        printf("Failed to list tools\n");
        tool_service_destroy(service);
        return -1;
    }

    printf("Tools list generated\n");
    free(list_json);

    ret = tool_service_destroy(service);
    if (ret != 0) {
        printf("Failed to destroy tool service\n");
        return ret;
    }

    printf("List tools test passed\n\n");
    return 0;
}

/**
 * @brief 测试工具执行
 * @return 0 表示成功，非 0 表示失败
 */
int test_execute_tool() {
    printf("=== Testing execute tool ===\n");
    
    const char* config_path = "agentos/manager/service/tool_d/tool.yaml";
    tool_service_t* service = tool_service_create(config_path);
    if (!service) {
        printf("Failed to create tool service\n");
        return -1;
    }

    tool_metadata_t meta = {
        .id = "echo-tool",
        .name = "Echo Tool",
        .description = "Echo input text",
        .executable = "/bin/echo",
        .timeout_sec = 10,
        .cacheable = false,
        .permission_rule = "all",
        .params = NULL,
        .param_count = 0
    };

    int ret = tool_service_register(service, &meta);
    if (ret != 0) {
        printf("Failed to register tool\n");
        tool_service_destroy(service);
        return ret;
    }

    tool_result_t* result = tool_service_execute(service, "echo-tool", "Hello World");
    if (!result) {
        printf("Failed to execute tool\n");
        tool_service_destroy(service);
        return -1;
    }

    printf("Tool output: %s\n", result->output);
    tool_result_free(result);

    ret = tool_service_destroy(service);
    if (ret != 0) {
        printf("Failed to destroy tool service\n");
        return ret;
    }

    printf("Execute tool test passed\n\n");
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
    if (test_register_tool() != 0) failed++;
    if (test_list_tools() != 0) failed++;
    if (test_execute_tool() != 0) failed++;

    if (failed == 0) {
        printf("All tests passed!\n");
        return 0;
    } else {
        printf("%d test(s) failed\n", failed);
        return 1;
    }
}
