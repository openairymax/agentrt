/**
 * @file main.c
 * @brief 内核入口点（初始化与演示）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "agentos.h"

int agentos_core_init(void) {
    int ret = 0;

    ret = agentos_mem_init(0);
    if (ret != 0) goto fail;

    ret = agentos_task_init();
    if (ret != 0) goto cleanup_mem;

    ret = agentos_ipc_init();
    if (ret != 0) goto cleanup_task;

    ret = agentos_time_eventloop_init();
    if (ret != 0) goto cleanup_ipc;

    return 0;

cleanup_ipc:
    agentos_ipc_cleanup();
cleanup_task:
    agentos_task_cleanup();
cleanup_mem:
    agentos_mem_cleanup();
fail:
    return ret;
}

void agentos_core_shutdown(void) {
    agentos_time_eventloop_cleanup();
    agentos_ipc_cleanup();
    agentos_task_cleanup();
    agentos_mem_cleanup();
}