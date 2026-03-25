/**
 * @file main.c
 * @brief 内核入口点（初始化与演示）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "agentos.h"

int agentos_core_init(void) {
    agentos_error_t err;

    err = agentos_mem_init(0);
    if (err != AGENTOS_SUCCESS) return (int)err;

    err = agentos_task_init();
    if (err != AGENTOS_SUCCESS) return (int)err;

    err = agentos_ipc_init();
    if (err != AGENTOS_SUCCESS) return (int)err;

    err = agentos_time_eventloop_init();
    if (err != AGENTOS_SUCCESS) return (int)err;

    return 0;
}

void agentos_core_shutdown(void) {
    agentos_ipc_cleanup();
    agentos_mem_cleanup();
}
