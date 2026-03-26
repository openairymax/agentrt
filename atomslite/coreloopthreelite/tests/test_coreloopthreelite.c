/**
 * @file test_coreloopthreelite.c
 * @brief coreloopthreelite模块单元测试
 * @date 2026-03-26
 * @copyright (c) 2026, AgentOS Team
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "agentos_coreloopthreelite.h"

/* 辅助函数声明 */
static int test_initialization_and_cleanup(void);
static int test_task_submission(void);
static int test_core_loop_basic_workflow(void);
static int test_concurrent_tasks(void);

/**
 * @brief 主测试函数
 * @return 成功返回0，失败返回非零
 */
int main(void) {
    int passed = 0;
    int total = 0;
    
    printf("Starting coreloopthreelite unit tests...\n");
    
    total++; if (test_initialization_and_cleanup() == 0) { passed++; printf("."); } else { printf("F"); }
    total++; if (test_task_submission() == 0) { passed++; printf("."); } else { printf("F"); }
    total++; if (test_core_loop_basic_workflow() == 0) { passed++; printf("."); } else { printf("F"); }
    total++; if (test_concurrent_tasks() == 0) { passed++; printf("."); } else { printf("F"); }
    
    printf("\n%d/%d tests passed\n", passed, total);
    
    if (passed != total) {
        printf("Some tests failed. See details above.\n");
        return 1;
    }
    
    printf("All tests passed successfully.\n");
    return 0;
}

/**
 * @brief 测试初始化与清理
 */
static int test_initialization_and_cleanup(void) {
    agentos_clt_error_t err;
    agentos_clt_engine_handle_t* engine = NULL;
    
    /* 测试初始化 */
    err = agentos_clt_engine_init(&engine, 2); /* 2个线程 */
    if (err != AGENTOS_CLT_SUCCESS) {
        printf("Failed to initialize engine: %s\n", agentos_clt_error_str(err));
        return 1;
    }
    assert(engine != NULL);
    
    /* 测试清理 */
    err = agentos_clt_engine_cleanup(&engine);
    if (err != AGENTOS_CLT_SUCCESS) {
        printf("Failed to cleanup engine: %s\n", agentos_clt_error_str(err));
        return 1;
    }
    assert(engine == NULL);
    
    return 0;
}

/**
 * @brief 测试任务提交
 */
static int test_task_submission(void) {
    agentos_clt_error_t err;
    agentos_clt_engine_handle_t* engine = NULL;
    
    err = agentos_clt_engine_init(&engine, 1);
    if (err != AGENTOS_CLT_SUCCESS) {
        printf("Failed to initialize engine for task submission test\n");
        return 1;
    }
    
    /* 创建测试任务数据 */
    const char* task_data = "Test task content";
    size_t task_data_len = strlen(task_data);
    
    agentos_clt_task_id_t task_id = AGENTOS_CLT_INVALID_TASK_ID;
    
    err = agentos_clt_task_submit(engine, task_data, task_data_len, &task_id);
    if (err != AGENTOS_CLT_SUCCESS) {
        printf("Failed to submit task: %s\n", agentos_clt_error_str(err));
        agentos_clt_engine_cleanup(&engine);
        return 1;
    }
    
    assert(task_id != AGENTOS_CLT_INVALID_TASK_ID);
    
    /* 等待任务完成（简化处理） */
    err = agentos_clt_engine_cleanup(&engine);
    if (err != AGENTOS_CLT_SUCCESS) {
        printf("Failed to cleanup engine after task submission\n");
        return 1;
    }
    
    return 0;
}

/**
 * @brief 测试核心循环基本工作流
 */
static int test_core_loop_basic_workflow(void) {
    agentos_clt_error_t err;
    agentos_clt_engine_handle_t* engine = NULL;
    
    err = agentos_clt_engine_init(&engine, 1);
    if (err != AGENTOS_CLT_SUCCESS) {
        printf("Failed to initialize engine for workflow test\n");
        return 1;
    }
    
    /* 提交多个任务 */
    const char* tasks[] = {
        "Task 1: Process data A",
        "Task 2: Process data B",
        "Task 3: Process data C"
    };
    const int num_tasks = sizeof(tasks) / sizeof(tasks[0]);
    agentos_clt_task_id_t task_ids[num_tasks];
    
    for (int i = 0; i < num_tasks; i++) {
        err = agentos_clt_task_submit(engine, tasks[i], strlen(tasks[i]), &task_ids[i]);
        if (err != AGENTOS_CLT_SUCCESS) {
            printf("Failed to submit task %d: %s\n", i, agentos_clt_error_str(err));
            agentos_clt_engine_cleanup(&engine);
            return 1;
        }
        assert(task_ids[i] != AGENTOS_CLT_INVALID_TASK_ID);
    }
    
    /* 等待所有任务完成 */
    err = agentos_clt_engine_cleanup(&engine);
    if (err != AGENTOS_CLT_SUCCESS) {
        printf("Failed to cleanup engine after workflow test\n");
        return 1;
    }
    
    return 0;
}

/**
 * @brief 测试并发任务处理
 */
static int test_concurrent_tasks(void) {
    agentos_clt_error_t err;
    agentos_clt_engine_handle_t* engine = NULL;
    
    /* 使用多个线程进行并发测试 */
    err = agentos_clt_engine_init(&engine, 4);
    if (err != AGENTOS_CLT_SUCCESS) {
        printf("Failed to initialize engine for concurrent test\n");
        return 1;
    }
    
    /* 提交多个并发任务 */
    const int num_tasks = 10;
    agentos_clt_task_id_t task_ids[num_tasks];
    
    for (int i = 0; i < num_tasks; i++) {
        char task_data[64];
        snprintf(task_data, sizeof(task_data), "Concurrent task %d", i);
        
        err = agentos_clt_task_submit(engine, task_data, strlen(task_data), &task_ids[i]);
        if (err != AGENTOS_CLT_SUCCESS) {
            printf("Failed to submit concurrent task %d: %s\n", i, agentos_clt_error_str(err));
            agentos_clt_engine_cleanup(&engine);
            return 1;
        }
        assert(task_ids[i] != AGENTOS_CLT_INVALID_TASK_ID);
    }
    
    /* 等待所有任务完成 */
    err = agentos_clt_engine_cleanup(&engine);
    if (err != AGENTOS_CLT_SUCCESS) {
        printf("Failed to cleanup engine after concurrent test\n");
        return 1;
    }
    
    return 0;
}