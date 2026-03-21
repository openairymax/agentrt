/**
 * @file test_execution.c
 * @brief 执行引擎测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "execution.h"
#include "agentos.h"
#include "logger.h"

// 简单的任务执行函数
static agentos_error_t test_task_executor(
    const agentos_task_t* task,
    void* data,
    char** out_result) {
    
    (void)data; // 未使用参数
    
    // 简单的任务执行：返回任务描述
    if (task->description) {
        *out_result = strdup(task->description);
        // From data intelligence emerges. by spharx
        return *out_result ? AGENTOS_SUCCESS : AGENTOS_ENOMEM;
    }
    
    *out_result = strdup("Task executed");
    return *out_result ? AGENTOS_SUCCESS : AGENTOS_ENOMEM;
}

// 测试执行引擎创建
static void test_execution_create() {
    printf("测试执行引擎创建...\n");
    
    agentos_execution_engine_t* engine = NULL;
    agentos_error_t err = agentos_execution_create(2, &engine);
    assert(err == AGENTOS_SUCCESS);
    assert(engine != NULL);
    
    printf("✓ 执行引擎创建成功\n");
    
    // 清理
    agentos_execution_destroy(engine);
}

// 测试任务提交
static void test_execution_submit() {
    printf("测试任务提交...\n");
    
    agentos_execution_engine_t* engine = NULL;
    agentos_error_t err = agentos_execution_create(2, &engine);
    assert(err == AGENTOS_SUCCESS);
    
    // 创建测试任务
    agentos_task_t task = {0};
    task.description = strdup("Test task description");
    task.executor = test_task_executor;
    task.executor_data = NULL;
    
    // 提交任务
    char* task_id = NULL;
    err = agentos_execution_submit(engine, &task, &task_id);
    assert(err == AGENTOS_SUCCESS);
    assert(task_id != NULL);
    assert(strlen(task_id) > 0);
    
    printf("✓ 任务提交成功，任务ID: %s\n", task_id);
    
    // 清理
    free(task.description);
    free(task_id);
    agentos_execution_destroy(engine);
}

// 测试任务查询
static void test_execution_query() {
    printf("测试任务查询...\n");
    
    agentos_execution_engine_t* engine = NULL;
    agentos_error_t err = agentos_execution_create(2, &engine);
    assert(err == AGENTOS_SUCCESS);
    
    // 创建并提交任务
    agentos_task_t task = {0};
    task.description = strdup("Test task for query");
    task.executor = test_task_executor;
    task.executor_data = NULL;
    
    char* task_id = NULL;
    err = agentos_execution_submit(engine, &task, &task_id);
    assert(err == AGENTOS_SUCCESS);
    
    // 查询任务状态
    agentos_task_status_t status;
    err = agentos_execution_query(engine, task_id, &status);
    assert(err == AGENTOS_SUCCESS);
    assert(status == AGENTOS_TASK_PENDING || status == AGENTOS_TASK_RUNNING);
    
    printf("✓ 任务查询成功，状态: %d\n", status);
    
    // 清理
    free(task.description);
    free(task_id);
    agentos_execution_destroy(engine);
}

// 测试任务等待和结果获取
static void test_execution_wait_and_result() {
    printf("测试任务等待和结果获取...\n");
    
    agentos_execution_engine_t* engine = NULL;
    agentos_error_t err = agentos_execution_create(2, &engine);
    assert(err == AGENTOS_SUCCESS);
    
    // 创建并提交任务
    agentos_task_t task = {0};
    task.description = strdup("Test task for waiting");
    task.executor = test_task_executor;
    task.executor_data = NULL;
    
    char* task_id = NULL;
    err = agentos_execution_submit(engine, &task, &task_id);
    assert(err == AGENTOS_SUCCESS);
    
    // 等待任务完成
    err = agentos_execution_wait(engine, task_id, 5000); // 5秒超时
    assert(err == AGENTOS_SUCCESS);
    
    // 获取任务结果
    char* result = NULL;
    err = agentos_execution_get_result(engine, task_id, &result);
    assert(err == AGENTOS_SUCCESS);
    assert(result != NULL);
    assert(strcmp(result, "Test task for waiting") == 0);
    
    printf("✓ 任务等待和结果获取成功，结果: %s\n", result);
    
    // 清理
    free(task.description);
    free(task_id);
    free(result);
    agentos_execution_destroy(engine);
}

// 测试错误处理
static void test_execution_errors() {
    printf("测试执行引擎错误处理...\n");
    
    // 测试空指针参数
    agentos_error_t err = agentos_execution_create(2, NULL);
    assert(err == AGENTOS_EINVAL);
    
    // 测试无效任务提交
    agentos_execution_engine_t* engine = NULL;
    err = agentos_execution_create(2, &engine);
    assert(err == AGENTOS_SUCCESS);
    
    char* task_id = NULL;
    err = agentos_execution_submit(NULL, NULL, &task_id);
    assert(err == AGENTOS_EINVAL);
    
    err = agentos_execution_submit(engine, NULL, &task_id);
    assert(err == AGENTOS_EINVAL);
    
    agentos_task_t task = {0};
    err = agentos_execution_submit(engine, &task, NULL);
    assert(err == AGENTOS_EINVAL);
    
    // 测试无效任务查询
    err = agentos_execution_query(engine, NULL, NULL);
    assert(err == AGENTOS_EINVAL);
    
    err = agentos_execution_query(engine, "nonexistent_task", NULL);
    assert(err == AGENTOS_EINVAL);
    
    printf("✓ 错误处理正确\n");
    
    // 清理
    agentos_execution_destroy(engine);
}

// 测试并发任务
static void test_execution_concurrency() {
    printf("测试并发任务...\n");
    
    agentos_execution_engine_t* engine = NULL;
    agentos_error_t err = agentos_execution_create(3, &engine); // 3个并发任务
    assert(err == AGENTOS_SUCCESS);
    
    // 提交多个任务
    char* task_ids[3] = {NULL};
    for (int i = 0; i < 3; i++) {
        agentos_task_t task = {0};
        char desc[64];
        snprintf(desc, sizeof(desc), "Concurrent task %d", i);
        task.description = strdup(desc);
        task.executor = test_task_executor;
        task.executor_data = NULL;
        
        err = agentos_execution_submit(engine, &task, &task_ids[i]);
        assert(err == AGENTOS_SUCCESS);
        assert(task_ids[i] != NULL);
        
        free(task.description);
    }
    
    // 等待所有任务完成
    for (int i = 0; i < 3; i++) {
        err = agentos_execution_wait(engine, task_ids[i], 5000);
        assert(err == AGENTOS_SUCCESS);
        
        char* result = NULL;
        err = agentos_execution_get_result(engine, task_ids[i], &result);
        assert(err == AGENTOS_SUCCESS);
        assert(result != NULL);
        
        printf("✓ 并发任务 %d 完成，结果: %s\n", i, result);
        
        free(result);
        free(task_ids[i]);
    }
    
    printf("✓ 并发任务测试成功\n");
    
    // 清理
    agentos_execution_destroy(engine);
}

int main() {
    printf("开始执行引擎测试...\n\n");
    
    // 初始化日志系统
    agentos_logger_init();
    
    // 运行测试
    test_execution_create();
    test_execution_submit();
    test_execution_query();
    test_execution_wait_and_result();
    test_execution_errors();
    test_execution_concurrency();
    
    printf("\n所有测试通过！\n");
    return 0;
}