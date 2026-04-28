/**
 * @file main.c
 * @brief 内核入口点（初始化与关闭）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "agentos.h"

static volatile int g_core_initialized = 0;

int agentos_core_init(void) {
    int expected = 0;
    if (!__atomic_compare_exchange_n(&g_core_initialized, &expected, 1,
                                     0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
        return AGENTOS_SUCCESS;
    }

    int ret = 0;

    ret = agentos_mem_init(0);
    if (ret != 0) goto fail;

    ret = agentos_task_init();
    if (ret != 0) goto cleanup_mem;

    ret = agentos_ipc_init();
    if (ret != 0) goto cleanup_task;

    ret = agentos_time_eventloop_init();
    if (ret != 0) goto cleanup_ipc;

    return AGENTOS_SUCCESS;

cleanup_ipc:
    agentos_ipc_cleanup();
cleanup_task:
    agentos_task_cleanup();
cleanup_mem:
    agentos_mem_cleanup();
fail:
    __atomic_store_n(&g_core_initialized, 0, __ATOMIC_SEQ_CST);
    return ret;
}

void agentos_core_shutdown(void) {
    int expected = 1;
    if (!__atomic_compare_exchange_n(&g_core_initialized, &expected, 0,
                                     0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
        return;
    }

    agentos_time_eventloop_cleanup();
    agentos_time_timer_cleanup();
    agentos_ipc_cleanup();
    agentos_task_cleanup();
    agentos_mem_cleanup();
}
