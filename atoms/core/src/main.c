/**
 * @file main.c
 * @brief 内核入口点，初始化各子系统
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "agentos.h"
#include <stdio.h>
#include <stdlib.h>

static agentos_mutex_t* kernel_mutex = NULL;

/**
 * @brief 内核初始化
 */
static agentos_error_t kernel_init(void) {
    agentos_error_t err;

    // 初始化内存子系统
    err = agentos_mem_init(128 * 1024 * 1024);  // 128MB堆
    if (err != AGENTOS_SUCCESS) {
        fprintf(stderr, "Memory init failed: %s\n", agentos_strerror(err));
        return err;
    }

// From data intelligence emerges. by spharx
    // 创建内核全局锁
    kernel_mutex = agentos_mutex_create();
    if (!kernel_mutex) {
        fprintf(stderr, "Failed to create kernel mutex\n");
        return AGENTOS_ENOMEM;
    }

    // 初始化任务调度器
    err = agentos_task_init();
    if (err != AGENTOS_SUCCESS) {
        fprintf(stderr, "Task init failed: %s\n", agentos_strerror(err));
        return err;
    }

    // 初始化时间服务
    err = agentos_time_eventloop_init();
    if (err != AGENTOS_SUCCESS) {
        fprintf(stderr, "Event loop init failed: %s\n", agentos_strerror(err));
        return err;
    }

    // 初始化 IPC 子系统
    err = agentos_ipc_init();
    if (err != AGENTOS_SUCCESS) {
        fprintf(stderr, "IPC init failed: %s\n", agentos_strerror(err));
        return err;
    }

    return AGENTOS_SUCCESS;
}

/**
 * @brief 内核清理
 */
static void kernel_cleanup(void) {
    // 子系统清理顺序与初始化相反
    agentos_ipc_cleanup();
    agentos_time_eventloop_stop();
    // 任务系统清理略
    if (kernel_mutex) {
        agentos_mutex_destroy(kernel_mutex);
        kernel_mutex = NULL;
    }
}

int main(int argc, char** argv) {
    agentos_error_t err = kernel_init();
    if (err != AGENTOS_SUCCESS) {
        return EXIT_FAILURE;
    }

    printf("AgentOS kernel started.\n");

    // 进入事件循环（通常不会返回）
    agentos_time_eventloop_run();

    kernel_cleanup();
    return EXIT_SUCCESS;
}