/**
 * @file test_syscall_entry.c
 * @brief 系统调用入口单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "syscalls.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief 测试系统调用入口函数
 */
void test_syscall_invoke(void) {
    printf("=== 测试系统调用入口 ===\n");
    
    // 测试参数数量为0的情况
    void* args[] = {};
    void* result = agentos_syscall_invoke(0, args, 0);
    printf("测试参数数量为0: %p\n", result);
    
    // 测试无效的系统调用号
    result = agentos_syscall_invoke(999, args, 0);
    printf("测试无效系统调用号: %p\n", result);
    // From data intelligence emerges. by spharx
    
    printf("系统调用入口测试完成\n\n");
}

/**
 * @brief 测试任务管理系统调用
 */
void test_task_syscalls(void) {
    printf("=== 测试任务管理系统调用 ===\n");
    
    // 测试任务提交
    const char* input = "测试任务"; 
    char* result = NULL;
    agentos_error_t err = agentos_sys_task_submit(input, strlen(input), 1000, &result);
    printf("任务提交结果: %d\n", err);
    if (result) {
        printf("任务结果: %s\n", result);
        free(result);
    }
    
    printf("任务管理测试完成\n\n");
}

/**
 * @brief 测试记忆管理系统调用
 */
void test_memory_syscalls(void) {
    printf("=== 测试记忆管理系统调用 ===\n");
    
    // 测试写入记忆
    const char* data = "测试记忆数据";
    char* record_id = NULL;
    agentos_error_t err = agentos_sys_memory_write(data, strlen(data), NULL, &record_id);
    printf("写入记忆结果: %d\n", err);
    if (record_id) {
        printf("记忆记录ID: %s\n", record_id);
        
        // 测试搜索记忆
        char** record_ids = NULL;
        float* scores = NULL;
        size_t count = 0;
        err = agentos_sys_memory_search("测试", 10, &record_ids, &scores, &count);
        printf("搜索记忆结果: %d, 找到 %zu 条记录\n", err, count);
        
        // 测试获取记忆
        void* memory_data = NULL;
        size_t memory_len = 0;
        err = agentos_sys_memory_get(record_id, &memory_data, &memory_len);
        printf("获取记忆结果: %d, 数据长度: %zu\n", err, memory_len);
        if (memory_data) {
            free(memory_data);
        }
        
        // 测试删除记忆
        err = agentos_sys_memory_delete(record_id);
        printf("删除记忆结果: %d\n", err);
        
        // 释放资源
        free(record_id);
        if (record_ids) {
            for (size_t i = 0; i < count; i++) {
                free(record_ids[i]);
            }
            free(record_ids);
        }
        if (scores) {
            free(scores);
        }
    }
    
    printf("记忆管理测试完成\n\n");
}

/**
 * @brief 测试会话管理系统调用
 */
void test_session_syscalls(void) {
    printf("=== 测试会话管理系统调用 ===\n");
    
    // 测试创建会话
    char* session_id = NULL;
    agentos_error_t err = agentos_sys_session_create(NULL, &session_id);
    printf("创建会话结果: %d\n", err);
    if (session_id) {
        printf("会话ID: %s\n", session_id);
        
        // 测试获取会话信息
        char* session_info = NULL;
        err = agentos_sys_session_get(session_id, &session_info);
        printf("获取会话信息结果: %d\n", err);
        if (session_info) {
            printf("会话信息: %s\n", session_info);
            free(session_info);
        }
        
        // 测试列出会话
        char** sessions = NULL;
        size_t count = 0;
        err = agentos_sys_session_list(&sessions, &count);
        printf("列出会话结果: %d, 会话数量: %zu\n", err, count);
        if (sessions) {
            for (size_t i = 0; i < count; i++) {
                printf("会话 %zu: %s\n", i+1, sessions[i]);
                free(sessions[i]);
            }
            free(sessions);
        }
        
        // 测试关闭会话
        err = agentos_sys_session_close(session_id);
        printf("关闭会话结果: %d\n", err);
        
        free(session_id);
    }
    
    printf("会话管理测试完成\n\n");
}

/**
 * @brief 测试可观测性系统调用
 */
void test_telemetry_syscalls(void) {
    printf("=== 测试可观测性系统调用 ===\n");
    
    // 测试获取指标
    char* metrics = NULL;
    agentos_error_t err = agentos_sys_telemetry_metrics(&metrics);
    printf("获取指标结果: %d\n", err);
    if (metrics) {
        printf("指标数据: %s\n", metrics);
        free(metrics);
    }
    
    // 测试获取追踪数据
    char* traces = NULL;
    err = agentos_sys_telemetry_traces(&traces);
    printf("获取追踪数据结果: %d\n", err);
    if (traces) {
        printf("追踪数据: %s\n", traces);
        free(traces);
    }
    
    printf("可观测性测试完成\n\n");
}

int main(void) {
    printf("开始系统调用单元测试\n\n");
    
    // 初始化系统调用模块（实际测试中需要传入真实的引擎句柄）
    agentos_sys_init(NULL, NULL, NULL);
    
    // 运行各项测试
    test_syscall_invoke();
    test_task_syscalls();
    test_memory_syscalls();
    test_session_syscalls();
    test_telemetry_syscalls();
    
    printf("系统调用单元测试完成\n");
    return 0;
}
