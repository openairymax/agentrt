/**
 * @file thread.c
 * @brief 线程管理辅助函数实现（低级线程操作）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * @details
 * 本模块提供低级线程辅助函数：
 * - agentos_thread_self: 获取当前线程ID
 * - agentos_thread_sleep: 线程休眠
 * - agentos_thread_yield: 线程让出CPU
 *
 * 注意：agentos_thread_create 和 agentos_thread_join 的实现
 * 已统一到 scheduler.c 中（使用三层架构：核心层+平台适配器）
 */

#include "task.h"
#include "mem.h"
#include <stdlib.h>

#include "memory_compat.h"
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#include <sched.h>
#endif

agentos_thread_id_t agentos_thread_self(void) {
#ifdef _WIN32
    return (agentos_thread_id_t)GetCurrentThreadId();
#else
    return (agentos_thread_id_t)pthread_self();
#endif
}

void agentos_thread_sleep(uint32_t ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}

void agentos_thread_yield(void) {
#ifdef _WIN32
    SwitchToThread();
#else
    sched_yield();
#endif
}
