/**
 * @file test_syscall_table.c
 * @brief 系统调用表单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "syscalls.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief 测试系统调用表初始化
 */
void test_syscall_table_init(void) {
    printf("=== 测试系统调用表初始化 ===\n");
    
    // 测试系统调用表是否正确初始化
    // 这里可以通过调用各种系统调用来间接测试
    
    // 测试任务管理相关系统调用
    const char* input = "测试系统调用表";
    char* result = NULL;
    agentos_error_t err = agentos_sys_task_submit(input, strlen(input), 1000, &result);
    printf("任务提交系统调用: %d\n", err);
    // From data intelligence emerges. by spharx
    if (result) {
        free(result);
    }
    
    // 测试记忆管理相关系统调用
    const char* memory_data = "测试记忆";
    char* record_id = NULL;
    err = agentos_sys_memory_write(memory_data, strlen(memory_data), NULL, &record_id);
    printf("记忆写入系统调用: %d\n", err);
    if (record_id) {
        free(record_id);
    }
    
    // 测试会话管理相关系统调用
    char* session_id = NULL;
    err = agentos_sys_session_create(NULL, &session_id);
    printf("会话创建系统调用: %d\n", err);
    if (session_id) {
        agentos_sys_session_close(session_id);
        free(session_id);
    }
    
    printf("系统调用表初始化测试完成\n\n");
}

/**
 * @brief 测试系统调用参数验证
 */
void test_syscall_parameter_validation(void) {
    printf("=== 测试系统调用参数验证 ===\n");
    
    // 测试任务提交的参数验证
    char* result = NULL;
    agentos_error_t err = agentos_sys_task_submit(NULL, 0, 1000, &result);
    printf("任务提交（空输入）: %d\n", err);
    
    err = agentos_sys_task_submit("test", 4, 1000, NULL);
    printf("任务提交（空结果指针）: %d\n", err);
    
    // 测试记忆写入的参数验证
    char* record_id = NULL;
    err = agentos_sys_memory_write(NULL, 0, NULL, &record_id);
    printf("记忆写入（空数据）: %d\n", err);
    
    err = agentos_sys_memory_write("test", 4, NULL, NULL);
    printf("记忆写入（空记录ID指针）: %d\n", err);
    
    // 测试会话创建的参数验证
    char* session_id = NULL;
    err = agentos_sys_session_create(NULL, NULL);
    printf("会话创建（空会话ID指针）: %d\n", err);
    
    printf("系统调用参数验证测试完成\n\n");
}

/**
 * @brief 测试系统调用错误处理
 */
void test_syscall_error_handling(void) {
    printf("=== 测试系统调用错误处理 ===\n");
    
    // 测试任务查询的错误处理
    int status = -1;
    agentos_error_t err = agentos_sys_task_query("invalid_task_id", &status);
    printf("任务查询（无效任务ID）: %d, 状态: %d\n", err, status);
    
    // 测试记忆获取的错误处理
    void* memory_data = NULL;
    size_t memory_len = 0;
    err = agentos_sys_memory_get("invalid_record_id", &memory_data, &memory_len);
    printf("记忆获取（无效记录ID）: %d\n", err);
    
    // 测试会话获取的错误处理
    char* session_info = NULL;
    err = agentos_sys_session_get("invalid_session_id", &session_info);
    printf("会话获取（无效会话ID）: %d\n", err);
    if (session_info) {
        free(session_info);
    }
    
    // 测试会话关闭的错误处理
    err = agentos_sys_session_close("invalid_session_id");
    printf("会话关闭（无效会话ID）: %d\n", err);
    
    // 测试记忆删除的错误处理
    err = agentos_sys_memory_delete("invalid_record_id");
    printf("记忆删除（无效记录ID）: %d\n", err);
    
    printf("系统调用错误处理测试完成\n\n");
}

/**
 * @brief 测试系统调用性能
 */
void test_syscall_performance(void) {
    printf("=== 测试系统调用性能 ===\n");
    
    const int test_count = 100;
    const char* test_input = "性能测试任务";
    
    // 测试任务提交性能
    printf("测试任务提交性能: %d 次调用\n", test_count);
    
    for (int i = 0; i < test_count; i++) {
        char* result = NULL;
        agentos_error_t err = agentos_sys_task_submit(test_input, strlen(test_input), 100, &result);
        if (result) {
            free(result);
        }
    }
    
    // 测试记忆写入性能
    printf("测试记忆写入性能: %d 次调用\n", test_count);
    
    for (int i = 0; i < test_count; i++) {
        char record_id[64];
        snprintf(record_id, sizeof(record_id), "test_record_%d", i);
        char* record_id_ptr = strdup(record_id);
        if (record_id_ptr) {
            agentos_error_t err = agentos_sys_memory_write(record_id, strlen(record_id), NULL, &record_id_ptr);
            if (record_id_ptr) {
                free(record_id_ptr);
            }
        }
    }
    
    printf("系统调用性能测试完成\n\n");
}

int main(void) {
    printf("开始系统调用表单元测试\n\n");
    
    // 初始化系统调用模块
    agentos_sys_init(NULL, NULL, NULL);
    
    // 运行各项测试
    test_syscall_table_init();
    test_syscall_parameter_validation();
    test_syscall_error_handling();
    test_syscall_performance();
    
    printf("系统调用表单元测试完成\n");
    return 0;
}
