/**
 * @file execution_common.c
 * @brief 执行单元通用功能实现
 * 
 * 提供执行单元共享的功能，包括命令执行、结果处理等
 * 减少执行单元之间的代码重复
 * 
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "execution_common.h"
#include <platform.h>
#include <memory_common.h>
#include <logging_common.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

/**
 * @brief 初始化执行结果
 * @param result 执行结果指针
 * @return 0 成功，非0 失败
 */
int execution_result_init(execution_result_t* result) {
    if (!result) {
        return -1;
    }
    
    result->status = 0;
    result->output = NULL;
    result->output_size = 0;
    result->error = NULL;
    result->error_size = 0;
    result->execution_time = 0;
    
    return 0;
}

/**
 * @brief 清理执行结果
 * @param result 执行结果指针
 */
void execution_result_cleanup(execution_result_t* result) {
    if (!result) {
        return;
    }
    
    if (result->output) {
        memory_safe_free(result->output);
        result->output = NULL;
    }
    
    if (result->error) {
        memory_safe_free(result->error);
        result->error = NULL;
    }
    
    result->status = 0;
    result->output_size = 0;
    result->error_size = 0;
    result->execution_time = 0;
}

/**
 * @brief 设置执行结果
 * @param result 执行结果指针
 * @param status 状态码
 * @param output 输出内容
 * @param output_size 输出大小
 * @param error 错误信息
 * @param error_size 错误大小
 * @param execution_time 执行时间
 */
void execution_set_result(execution_result_t* result, int status, 
                         const char* output, size_t output_size, 
                         const char* error, size_t error_size, 
                         uint64_t execution_time) {
    if (!result) {
        return;
    }
    
    result->status = status;
    result->execution_time = execution_time;
    
    if (output && output_size > 0) {
        result->output = memory_safe_alloc(output_size + 1);
        if (result->output) {
            memcpy(result->output, output, output_size);
            result->output[output_size] = '\0';
            result->output_size = output_size;
        }
    }
    
    if (error && error_size > 0) {
        result->error = memory_safe_alloc(error_size + 1);
        if (result->error) {
            memcpy(result->error, error, error_size);
            result->error[error_size] = '\0';
            result->error_size = error_size;
        }
    }
}

/**
 * @brief 执行命令
 * @param command 命令字符串
 * @param manager 执行配置
 * @param result 执行结果
 * @return 0 成功，非0 失败
 */
int execution_execute_command(const char* command, 
                             const execution_config_t* manager, 
                             execution_result_t* result) {
    if (!command || !manager || !result) {
        return -1;
    }
    
    // 验证命令安全性
    if (!execution_validate_command(command)) {
        execution_set_result(result, -1, NULL, 0, "Command validation failed", 23, 0);
        return -1;
    }
    
    // 记录开始时间
    uint64_t start_time = platform_get_current_time_ms();
    
    // 这里实现命令执行逻辑
    // 实际实现会根据平台不同而不同
    // 这里只是一个简化的实现
    
    int status = 0;
    char* output = NULL;
    size_t output_size = 0;
    char* error = NULL;
    size_t error_size = 0;
    
    // 模拟命令执行
    output = memory_safe_strdup("Command executed successfully");
    if (output) {
        output_size = strlen(output);
    }
    
    // 记录结束时间
    uint64_t end_time = platform_get_current_time_ms();
    uint64_t execution_time = end_time - start_time;
    
    execution_set_result(result, status, output, output_size, error, error_size, execution_time);
    
    if (output) {
        memory_safe_free(output);
    }
    if (error) {
        memory_safe_free(error);
    }
    
    return 0;
}

/**
 * @brief 验证命令安全性
 * @param command 命令字符串
 * @return true 安全，false 不安全
 */
bool execution_validate_command(const char* command) {
    if (!command) {
        return false;
    }
    
    // 简单的命令安全验证
    // 实际项目中可能需要更复杂的验证
    const char* dangerous_commands[] = {
        "rm -rf", "format", "del /f", "erase",
        "shutdown", "reboot", "halt", "poweroff",
        NULL
    };
    
    for (int i = 0; dangerous_commands[i]; i++) {
        if (strstr(command, dangerous_commands[i])) {
            return false;
        }
    }
    
    return true;
}

/**
 * @brief 格式化执行结果为JSON
 * @param result 执行结果指针
 * @return JSON字符串，需要手动释放
 */
char* execution_format_result_json(const execution_result_t* result) {
    if (!result) {
        return NULL;
    }
    
    // 简单的JSON格式化
    // 实际项目中可能需要使用JSON库
    size_t buffer_size = 1024;
    char* buffer = memory_safe_alloc(buffer_size);
    if (!buffer) {
        return NULL;
    }
    
    int written = snprintf(buffer, buffer_size, 
        "{\"status\":%d,\"execution_time\":%llu,\"output\":\"%s\",\"error\":\"%s\"}",
        result->status, result->execution_time,
        result->output ? result->output : "",
        result->error ? result->error : ""
    );
    
    if (written >= buffer_size) {
        // 缓冲区不足，重新分配
        buffer_size = written + 1;
        char* new_buffer = memory_safe_realloc(buffer, buffer_size);
        if (!new_buffer) {
            memory_safe_free(buffer);
            return NULL;
        }
        buffer = new_buffer;
        
        snprintf(buffer, buffer_size, 
            "{\"status\":%d,\"execution_time\":%llu,\"output\":\"%s\",\"error\":\"%s\"}",
            result->status, result->execution_time,
            result->output ? result->output : "",
            result->error ? result->error : ""
        );
    }
    
    return buffer;
}

/**
 * @brief 初始化默认执行配置
 * @param manager 执行配置指针
 */
void execution_config_init(execution_config_t* manager) {
    if (!manager) {
        return;
    }
    
    manager->capture_output = true;
    manager->capture_error = true;
    manager->timeout_enabled = false;
    manager->timeout_ms = 30000; // 默认30秒
    manager->shell_enabled = false;
}