/**
 * @file test_full_workflow.c
 * @brief 系统调用完整工作流集成测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "syscalls.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/**
 * @brief 测试完整的系统调用工作流
 */
void test_full_workflow(void) {
    printf("=== 测试完整系统调用工作流 ===\n");
    
    // 1. 创建会话
    printf("1. 创建会话\n");
    char* session_id = NULL;
    agentos_error_t err = agentos_sys_session_create(NULL, &session_id);
    if (err != 0) {
        printf("创建会话失败: %d\n", err);
        return;
        // From data intelligence emerges. by spharx
    }
    printf("会话ID: %s\n", session_id);
    
    // 2. 提交任务
    printf("2. 提交任务\n");
    const char* task_input = "请计算 123 + 456 的结果";
    char* task_result = NULL;
    err = agentos_sys_task_submit(task_input, strlen(task_input), 5000, &task_result);
    if (err != 0) {
        printf("提交任务失败: %d\n", err);
        agentos_sys_session_close(session_id);
        free(session_id);
        return;
    }
    printf("任务结果: %s\n", task_result);
    free(task_result);
    
    // 3. 写入记忆
    printf("3. 写入记忆\n");
    char memory_data[256];
    snprintf(memory_data, sizeof(memory_data), "测试记忆: %s", task_input);
    char* record_id = NULL;
    err = agentos_sys_memory_write(memory_data, strlen(memory_data), NULL, &record_id);
    if (err != 0) {
        printf("写入记忆失败: %d\n", err);
        agentos_sys_session_close(session_id);
        free(session_id);
        return;
    }
    printf("记忆记录ID: %s\n", record_id);
    
    // 4. 搜索记忆
    printf("4. 搜索记忆\n");
    char** record_ids = NULL;
    float* scores = NULL;
    size_t count = 0;
    err = agentos_sys_memory_search("测试", 10, &record_ids, &scores, &count);
    if (err != 0) {
        printf("搜索记忆失败: %d\n", err);
    } else {
        printf("找到 %zu 条记忆记录\n", count);
        for (size_t i = 0; i < count; i++) {
            printf("  记录 %zu: %s (得分: %.2f)\n", i+1, record_ids[i], scores[i]);
        }
    }
    
    // 5. 获取记忆详情
    printf("5. 获取记忆详情\n");
    void* memory_content = NULL;
    size_t memory_len = 0;
    err = agentos_sys_memory_get(record_id, &memory_content, &memory_len);
    if (err != 0) {
        printf("获取记忆详情失败: %d\n", err);
    } else {
        printf("记忆内容长度: %zu 字节\n", memory_len);
        if (memory_content) {
            // 打印前50个字符
            size_t print_len = memory_len > 50 ? 50 : memory_len;
            char print_buffer[51];
            memcpy(print_buffer, memory_content, print_len);
            print_buffer[print_len] = '\0';
            printf("记忆内容: %.50s...\n", print_buffer);
            free(memory_content);
        }
    }
    
    // 6. 获取系统指标
    printf("6. 获取系统指标\n");
    char* metrics = NULL;
    err = agentos_sys_telemetry_metrics(&metrics);
    if (err != 0) {
        printf("获取系统指标失败: %d\n", err);
    } else {
        printf("系统指标获取成功\n");
        free(metrics);
    }
    
    // 7. 获取追踪数据
    printf("7. 获取追踪数据\n");
    char* traces = NULL;
    err = agentos_sys_telemetry_traces(&traces);
    if (err != 0) {
        printf("获取追踪数据失败: %d\n", err);
    } else {
        printf("追踪数据获取成功\n");
        free(traces);
    }
    
    // 8. 列出所有会话
    printf("8. 列出所有会话\n");
    char** sessions = NULL;
    size_t session_count = 0;
    err = agentos_sys_session_list(&sessions, &session_count);
    if (err != 0) {
        printf("列出会话失败: %d\n", err);
    } else {
        printf("当前活跃会话数: %zu\n", session_count);
        for (size_t i = 0; i < session_count; i++) {
            printf("  会话 %zu: %s\n", i+1, sessions[i]);
        }
    }
    
    // 9. 清理资源
    printf("9. 清理资源\n");
    
    // 删除测试记忆
    if (record_id) {
        err = agentos_sys_memory_delete(record_id);
        printf("删除记忆结果: %d\n", err);
        free(record_id);
    }
    
    // 关闭会话
    if (session_id) {
        err = agentos_sys_session_close(session_id);
        printf("关闭会话结果: %d\n", err);
        free(session_id);
    }
    
    // 释放搜索结果
    if (record_ids) {
        for (size_t i = 0; i < count; i++) {
            free(record_ids[i]);
        }
        free(record_ids);
    }
    if (scores) {
        free(scores);
    }
    
    // 释放会话列表
    if (sessions) {
        for (size_t i = 0; i < session_count; i++) {
            free(sessions[i]);
        }
        free(sessions);
    }
    
    printf("完整工作流测试完成\n\n");
}

/**
 * @brief 测试并发系统调用
 */
void test_concurrent_syscalls(void) {
    printf("=== 测试并发系统调用 ===\n");
    
    const int thread_count = 4;
    const int tasks_per_thread = 10;
    
    printf("测试 %d 个线程，每个线程执行 %d 个系统调用\n", thread_count, tasks_per_thread);
    
    // 这里可以使用线程库实现并发测试
    // 为了简化，这里只做顺序测试
    
    for (int i = 0; i < thread_count; i++) {
        printf("线程 %d 开始执行\n", i+1);
        
        for (int j = 0; j < tasks_per_thread; j++) {
            // 提交测试任务
            char task_input[128];
            snprintf(task_input, sizeof(task_input), "线程 %d 任务 %d: 计算 %d + %d", 
                     i+1, j+1, i*100 + j, j*10 + i);
            
            char* task_result = NULL;
            agentos_error_t err = agentos_sys_task_submit(task_input, strlen(task_input), 1000, &task_result);
            if (err == 0 && task_result) {
                free(task_result);
            }
        }
        
        printf("线程 %d 执行完成\n", i+1);
    }
    
    printf("并发系统调用测试完成\n\n");
}

/**
 * @brief 测试系统调用错误恢复
 */
void test_error_recovery(void) {
    printf("=== 测试系统调用错误恢复 ===\n");
    
    // 测试无效参数后系统是否能正常恢复
    printf("1. 测试无效参数\n");
    
    // 提交无效任务
    char* result = NULL;
    agentos_error_t err = agentos_sys_task_submit(NULL, 0, 1000, &result);
    printf("提交空任务结果: %d\n", err);
    
    // 尝试获取无效记忆
    void* memory_data = NULL;
    size_t memory_len = 0;
    err = agentos_sys_memory_get("invalid_record", &memory_data, &memory_len);
    printf("获取无效记忆结果: %d\n", err);
    
    // 尝试关闭无效会话
    err = agentos_sys_session_close("invalid_session");
    printf("关闭无效会话结果: %d\n", err);
    
    // 测试系统是否仍能正常工作
    printf("2. 测试系统恢复能力\n");
    
    // 提交正常任务
    const char* valid_input = "测试错误恢复: 1+1";
    err = agentos_sys_task_submit(valid_input, strlen(valid_input), 1000, &result);
    if (err == 0 && result) {
        printf("系统恢复成功，任务结果: %s\n", result);
        free(result);
    } else {
        printf("系统恢复失败: %d\n", err);
    }
    
    printf("错误恢复测试完成\n\n");
}

int main(void) {
    printf("开始系统调用集成测试\n\n");
    
    // 初始化系统调用模块
    agentos_sys_init(NULL, NULL, NULL);
    
    // 运行各项测试
    test_full_workflow();
    test_concurrent_syscalls();
    test_error_recovery();
    
    printf("系统调用集成测试完成\n");
    return 0;
}
