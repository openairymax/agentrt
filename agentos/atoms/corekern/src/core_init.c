/**
 * @file core_init.c
 * @brief 内核入口点（初始化与关闭）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "agentos.h"
#include "atomic_compat.h"
#include <stdio.h>

static atomic_long g_core_initialized = 0;

int agentos_core_init(void) {
    long expected = 0;
    if (!atomic_compare_exchange_strong_explicit(&g_core_initialized, &expected, 1,
                                                  memory_order_seq_cst, memory_order_seq_cst)) {
        return AGENTOS_SUCCESS;
    }

    int ret = 0;

    fprintf(stderr, "[DBG] agentos_core_init: calling agentos_mem_init\n"); fflush(stderr);
    ret = agentos_mem_init(0);
    fprintf(stderr, "[DBG] agentos_core_init: agentos_mem_init returned %d\n", ret); fflush(stderr);
    if (ret != 0) goto fail;

    fprintf(stderr, "[DBG] agentos_core_init: calling agentos_task_init\n"); fflush(stderr);
    ret = agentos_task_init();
    fprintf(stderr, "[DBG] agentos_core_init: agentos_task_init returned %d\n", ret); fflush(stderr);
    if (ret != 0) goto cleanup_mem;

    fprintf(stderr, "[DBG] agentos_core_init: calling agentos_ipc_init\n"); fflush(stderr);
    ret = agentos_ipc_init();
    fprintf(stderr, "[DBG] agentos_core_init: agentos_ipc_init returned %d\n", ret); fflush(stderr);
    if (ret != 0) goto cleanup_task;

    fprintf(stderr, "[DBG] agentos_core_init: calling agentos_time_eventloop_init\n"); fflush(stderr);
    ret = agentos_time_eventloop_init();
    fprintf(stderr, "[DBG] agentos_core_init: agentos_time_eventloop_init returned %d\n", ret); fflush(stderr);
    if (ret != 0) goto cleanup_ipc;

    fprintf(stderr, "[DBG] agentos_core_init: all init done\n"); fflush(stderr);
    return AGENTOS_SUCCESS;

cleanup_ipc:
    agentos_ipc_cleanup();
cleanup_task:
    agentos_task_cleanup();
cleanup_mem:
    agentos_mem_cleanup();
fail:
    atomic_store_explicit(&g_core_initialized, 0, memory_order_seq_cst);
    return ret;
}

void agentos_core_shutdown(void) {
    long expected = 1;
    if (!atomic_compare_exchange_strong_explicit(&g_core_initialized, &expected, 0,
                                                  memory_order_seq_cst, memory_order_seq_cst)) {
        return;
    }

    agentos_time_eventloop_cleanup();
    agentos_time_timer_cleanup();
    agentos_ipc_cleanup();
    agentos_task_cleanup();
    agentos_mem_cleanup();
}
