/**
 * @file test_tool.c
 * @brief 工具服务单元测试
 * @details 测试工具服务的各个功能模块
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../../../backs/tool_d/include/tool_service.h"

/**
 * @brief 测试创建和销毁工具服务
 * @return 0 表示成功，非 0 表示失败
 */
int test_create_destroy() {
    printf("=== Testing create and destroy ===\n");
    
    const char* config_path = "config/services/tool.yaml";
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
 * @brief 测试注册工具
 * @return 0 表示成功，非 0 表示失败
 */
int test_register_tool() {
    printf("=== Testing register tool ===\n");
    
    const char* config_path = "config/services/tool.yaml";
    tool_service_t* service = tool_service_create(config_path);
    if (!service) {
        printf("Failed to create tool service\n");
        return -1;
    }

    // 构建工具元数据
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

    // 注册工具
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
 * @brief 测试列出工具
 * @return 0 表示成功，非 0 表示失败
 */
int test_list_tools() {
    printf("=== Testing list tools ===\n");
    
    const char* config_path = "config/services/tool.yaml";
    tool_service_t* service = tool_service_create(config_path);
    if (!service) {
        printf("Failed to create tool service\n");
        return -1;
    }

    // 构建工具元数据
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

    // 注册工具
    int ret = tool_service_register(service, &meta);
    if (ret != 0) {
        printf("Failed to register tool\n");
        tool_service_destroy(service);
        return ret;
    }

    // 列出工具
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
 * @brief 测试获取工具
 * @return 0 表示成功，非 0 表示失败
 */
int test_get_tool() {
    printf("=== Testing get tool ===\n");
    
    const char* config_path = "config/services/tool.yaml";
    tool_service_t* service = tool_service_create(config_path);
    if (!service) {
        printf("Failed to create tool service\n");
        return -1;
    }

    // 构建工具元数据
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

    // 注册工具
    int ret = tool_service_register(service, &meta);
    if (ret != 0) {
        printf("Failed to register tool\n");
        tool_service_destroy(service);
        return ret;
    }

    // 获取工具
    tool_metadata_t* tool = tool_service_get(service, "test-tool");
    if (!tool) {
        printf("Failed to get tool\n");
        tool_service_destroy(service);
        return -1;
    }

    printf("Tool retrieved: %s\n", tool->name);
    tool_metadata_free(tool);

    ret = tool_service_destroy(service);
    if (ret != 0) {
        printf("Failed to destroy tool service\n");
        return ret;
    }

    printf("Get tool test passed\n\n");
    return 0;
}

/**
 * @brief 测试执行工具
 * @return 0 表示成功，非 0 表示失败
 */
int test_execute_tool() {
    printf("=== Testing execute tool ===\n");
    
    const char* config_path = "config/services/tool.yaml";
    tool_service_t* service = tool_service_create(config_path);
    if (!service) {
        printf("Failed to create tool service\n");
        return -1;
    }

    // 构建工具元数据
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

    // 注册工具
    int ret = tool_service_register(service, &meta);
    if (ret != 0) {
        printf("Failed to register tool\n");
        tool_service_destroy(service);
        return ret;
    }

    // 构建执行请求
    tool_execute_request_t req = {
        .tool_id = "test-tool",
        .params_json = "{\"message\": \"Hello, World!\"}",
        .stream = false
    };

    // 执行工具
    tool_result_t* result = NULL;
    ret = tool_service_execute(service, &req, &result);
    if (ret != 0) {
        printf("Failed to execute tool\n");
        tool_service_destroy(service);
        return ret;
    }

    if (result) {
        printf("Tool execution result: %s\n", result->success ? "Success" : "Failure");
        if (result->output) {
            printf("Output: %s\n", result->output);
        }
        tool_result_free(result);
    }

    ret = tool_service_destroy(service);
    if (ret != 0) {
        printf("Failed to destroy tool service\n");
        return ret;
    }

    printf("Execute tool test passed\n\n");
    return 0;
}

/**
 * @brief 主测试函数
 * @return 0 表示所有测试通过，非 0 表示有测试失败
 */
int main() {
    int ret = 0;

    ret |= test_create_destroy();
    ret |= test_register_tool();
    ret |= test_list_tools();
    ret |= test_get_tool();
    ret |= test_execute_tool();

    if (ret == 0) {
        printf("All tests passed!\n");
    } else {
        printf("Some tests failed!\n");
    }

    return ret;
}
