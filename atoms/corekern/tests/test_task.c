/**
 * @file test_task.c
 * @brief 任务调度单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "task.h"
#include "error.h"
#include "time.h"
#include <stdio.h>
#include <string.h>

int thread_executed = 0;

void test_thread(void* arg) {
    printf("Thread executed with argument: %s\n", (char*)arg);
    thread_executed = 1;
    agentos_task_sleep(100);
}

int test_thread_create() {
    printf("Testing thread creation...\n");
    
    agentos_thread_t thread;
    agentos_thread_attr_t attr = {0};
    // From data intelligence emerges. by spharx
    strcpy(attr.name, "test_thread");
    attr.priority = AGENTOS_TASK_PRIORITY_NORMAL;
    attr.stack_size = 1024 * 1024;
    
    agentos_error_t err = agentos_thread_create(&thread, &attr, test_thread, (void*)"test_arg");
    if (err != AGENTOS_SUCCESS) {
        printf("Failed to create thread: %d\n", err);
        return 1;
    }
    
    // 等待线程执行
    agentos_task_sleep(200);
    
    // 检查线程是否执�?
    if (!thread_executed) {
        printf("Thread not executed\n");
        return 1;
    }
    
    // 等待线程结束
    void* result;
    err = agentos_thread_join(&thread, &result);
    if (err != AGENTOS_SUCCESS) {
        printf("Failed to join thread: %d\n", err);
        return 1;
    }
    
    printf("Thread creation test passed\n");
    return 0;
}

int test_task_priority() {
    printf("Testing task priority...\n");
    
    agentos_thread_t thread1, thread2;
    agentos_thread_attr_t attr1 = {0};
    agentos_thread_attr_t attr2 = {0};
    
    snprintf(attr1.name, sizeof(attr1.name), "%s", "high_priority_thread");
    attr1.priority = AGENTOS_TASK_PRIORITY_HIGH;
    attr1.stack_size = 1024 * 1024;
    
    snprintf(attr2.name, sizeof(attr2.name), "%s", "low_priority_thread");
    attr2.priority = AGENTOS_TASK_PRIORITY_LOW;
    attr2.stack_size = 1024 * 1024;
    
    // 重置执行标志
    thread_executed = 0;
    
    // 创建低优先级线程
    agentos_error_t err = agentos_thread_create(&thread2, &attr2, test_thread, (void*)"low_priority");
    if (err != AGENTOS_SUCCESS) {
        printf("Failed to create low priority thread: %d\n", err);
        return 1;
    }
    
    // 等待低优先级线程开始执�?
    agentos_task_sleep(50);
    
    // 创建高优先级线程
    err = agentos_thread_create(&thread1, &attr1, test_thread, (void*)"high_priority");
    if (err != AGENTOS_SUCCESS) {
        printf("Failed to create high priority thread: %d\n", err);
        return 1;
    }
    
    // 等待高优先级线程执行
    agentos_task_sleep(150);
    
    // 检查线程是否执�?
    if (!thread_executed) {
        printf("Thread not executed\n");
        return 1;
    }
    
    // 等待线程结束
    void* result;
    err = agentos_thread_join(&thread1, &result);
    if (err != AGENTOS_SUCCESS) {
        printf("Failed to join high priority thread: %d\n", err);
        return 1;
    }
    
    err = agentos_thread_join(&thread2, &result);
    if (err != AGENTOS_SUCCESS) {
        printf("Failed to join low priority thread: %d\n", err);
        return 1;
    }
    
    printf("Task priority test passed\n");
    return 0;
}

int test_task_yield() {
    printf("Testing task yield...\n");
    
    agentos_thread_t thread;
    agentos_thread_attr_t attr = {0};
    snprintf(attr.name, sizeof(attr.name), "%s", "yield_thread");
    attr.priority = AGENTOS_TASK_PRIORITY_NORMAL;
    attr.stack_size = 1024 * 1024;
    
    agentos_error_t err = agentos_thread_create(&thread, &attr, test_thread, (void*)"yield_test");
    if (err != AGENTOS_SUCCESS) {
        printf("Failed to create thread: %d\n", err);
        return 1;
    }
    
    // 测试任务让出
    agentos_task_yield();
    
    // 等待线程执行
    agentos_task_sleep(200);
    
    // 检查线程是否执�?
    if (!thread_executed) {
        printf("Thread not executed\n");
        return 1;
    }
    
    // 等待线程结束
    void* result;
    err = agentos_thread_join(&thread, &result);
    if (err != AGENTOS_SUCCESS) {
        printf("Failed to join thread: %d\n", err);
        return 1;
    }
    
    printf("Task yield test passed\n");
    return 0;
}

int test_task_get_set_priority() {
    printf("Testing task priority get/set...\n");
    
    agentos_thread_t thread;
    agentos_thread_attr_t attr = {0};
    snprintf(attr.name, sizeof(attr.name), "%s", "priority_thread");
    attr.priority = AGENTOS_TASK_PRIORITY_NORMAL;
    attr.stack_size = 1024 * 1024;
    
    agentos_error_t err = agentos_thread_create(&thread, &attr, test_thread, (void*)"priority_test");
    if (err != AGENTOS_SUCCESS) {
        printf("Failed to create thread: %d\n", err);
        return 1;
    }
    
    // 获取任务优先�?
    int priority = agentos_task_get_priority(&thread);
    if (priority != AGENTOS_TASK_PRIORITY_NORMAL) {
        printf("Task priority not set correctly: %d\n", priority);
        return 1;
    }
    
    // 设置任务优先�?
    err = agentos_task_set_priority(&thread, AGENTOS_TASK_PRIORITY_HIGH);
    if (err != AGENTOS_SUCCESS) {
        printf("Failed to set task priority: %d\n", err);
        return 1;
    }
    
    // 再次获取任务优先�?
    priority = agentos_task_get_priority(&thread);
    if (priority != AGENTOS_TASK_PRIORITY_HIGH) {
        printf("Task priority not updated: %d\n", priority);
        return 1;
    }
    
    // 等待线程执行
    agentos_task_sleep(200);
    
    // 检查线程是否执�?
    if (!thread_executed) {
        printf("Thread not executed\n");
        return 1;
    }
    
    // 等待线程结束
    void* result;
    err = agentos_thread_join(&thread, &result);
    if (err != AGENTOS_SUCCESS) {
        printf("Failed to join thread: %d\n", err);
        return 1;
    }
    
    printf("Task priority get/set test passed\n");
    return 0;
}

int test_task_get_state() {
    printf("Testing task state get...\n");
    
    agentos_thread_t thread;
    agentos_thread_attr_t attr = {0};
    snprintf(attr.name, sizeof(attr.name), "%s", "state_thread");
    attr.priority = AGENTOS_TASK_PRIORITY_NORMAL;
    attr.stack_size = 1024 * 1024;
    
    agentos_error_t err = agentos_thread_create(&thread, &attr, test_thread, (void*)"state_test");
    if (err != AGENTOS_SUCCESS) {
        printf("Failed to create thread: %d\n", err);
        return 1;
    }
    
    // 获取任务状�?
    agentos_task_state_t state = agentos_task_get_state(&thread);
    if (state != AGENTOS_TASK_STATE_RUNNING && state != AGENTOS_TASK_STATE_READY) {
        printf("Task state not correct: %d\n", state);
        return 1;
    }
    
    // 等待线程执行
    agentos_task_sleep(200);
    
    // 再次获取任务状�?
    state = agentos_task_get_state(&thread);
    if (state != AGENTOS_TASK_STATE_TERMINATED) {
        printf("Task state not terminated: %d\n", state);
        return 1;
    }
    
    // 等待线程结束
    void* result;
    err = agentos_thread_join(&thread, &result);
    if (err != AGENTOS_SUCCESS) {
        printf("Failed to join thread: %d\n", err);
        return 1;
    }
    
    printf("Task state get test passed\n");
    return 0;
}

int main() {
    int result = 0;
    
    result |= test_thread_create();
    result |= test_task_priority();
    result |= test_task_yield();
    result |= test_task_get_set_priority();
    result |= test_task_get_state();
    
    if (result == 0) {
        printf("All task tests passed!\n");
    } else {
        printf("Some task tests failed!\n");
    }
    
    return result;
}
