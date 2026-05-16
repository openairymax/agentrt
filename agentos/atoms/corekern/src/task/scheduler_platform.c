/**
 * @file scheduler_platform.c
 * @brief 调度器平台适配器注册与管理
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * @details
 * 本模块实现平台适配器的注册、管理和自动初始化功能。
 * 根据编译平台自动选择Windows或POSIX适配器，并提供统一的接口访问。
 */

#include "scheduler_platform.h"
#include "atomic_compat.h"
#include <stddef.h>

/* ==================== 平台适配器操作集声明 ==================== */

#if AGENTOS_PLATFORM_WINDOWS
extern const scheduler_platform_ops_t* scheduler_platform_get_windows_ops(void);
#endif

#if AGENTOS_PLATFORM_POSIX
extern const scheduler_platform_ops_t* scheduler_platform_get_posix_ops(void);
#endif

/* ==================== 静态全局状态 ==================== */

static const scheduler_platform_ops_t* g_current_platform_ops = NULL;

static atomic_int g_platform_initialized = 0;

/* ==================== 内部辅助函数 ==================== */

static const scheduler_platform_ops_t* detect_platform_ops(void)
{
#if AGENTOS_PLATFORM_WINDOWS
    return scheduler_platform_get_windows_ops();
#elif AGENTOS_PLATFORM_POSIX
    return scheduler_platform_get_posix_ops();
#else
    return NULL;
#endif
}

/* ==================== 公共API实现 ==================== */

void scheduler_platform_register_ops(const scheduler_platform_ops_t* ops)
{
    if (atomic_load_explicit(&g_platform_initialized, memory_order_acquire)) {
        return;
    }

    const scheduler_platform_ops_t* expected = NULL;
    atomic_compare_exchange_strong_ptr(
        (_Atomic void**)&g_current_platform_ops, (void**)&expected, (void*)ops,
        memory_order_seq_cst, memory_order_seq_cst);
}

const scheduler_platform_ops_t* scheduler_platform_get_ops(void)
{
    const scheduler_platform_ops_t* ops = atomic_load_ptr(
        (_Atomic void**)&g_current_platform_ops, memory_order_acquire);
    if (!ops) {
        const scheduler_platform_ops_t* new_ops = detect_platform_ops();
        if (new_ops) {
            const scheduler_platform_ops_t* expected = NULL;
            if (atomic_compare_exchange_strong_ptr(
                    (_Atomic void**)&g_current_platform_ops, (void**)&expected, (void*)new_ops,
                    memory_order_seq_cst, memory_order_seq_cst)) {
                ops = new_ops;
            } else {
                ops = expected;
            }
        }
    }

    return ops;
}

int scheduler_platform_auto_init(void)
{
    int expected = 0;
    if (!atomic_compare_exchange_strong_explicit(&g_platform_initialized, &expected, 1,
                                                  memory_order_seq_cst, memory_order_seq_cst)) {
        return 0;
    }

    const scheduler_platform_ops_t* ops = scheduler_platform_get_ops();
    if (!ops) {
        atomic_store_explicit(&g_platform_initialized, 0, memory_order_seq_cst);
        return -1;
    }

    if (!ops->init || !ops->cleanup || !ops->thread_create ||
        !ops->thread_join || !ops->thread_set_priority ||
        !ops->get_current_thread_id || !ops->get_thread_system_id ||
        !ops->thread_sleep || !ops->thread_yield ||
        !ops->cleanup_platform_resources || !ops->get_name) {
        atomic_store_explicit(&g_platform_initialized, 0, memory_order_seq_cst);
        return -1;
    }

    if (ops->init() != 0) {
        atomic_store_explicit(&g_platform_initialized, 0, memory_order_seq_cst);
        return -1;
    }

    return 0;
}

int scheduler_platform_is_initialized(void)
{
    return atomic_load_explicit(&g_platform_initialized, memory_order_acquire);
}

void scheduler_platform_cleanup(void)
{
    int expected = 1;
    if (!atomic_compare_exchange_strong_explicit(&g_platform_initialized, &expected, 0,
                                                  memory_order_seq_cst, memory_order_seq_cst)) {
        return;
    }

    const scheduler_platform_ops_t* ops =
        atomic_load_ptr((_Atomic void**)&g_current_platform_ops, memory_order_acquire);
    if (ops && ops->cleanup) {
        ops->cleanup();
    }

    atomic_store_ptr((_Atomic void**)&g_current_platform_ops, NULL, memory_order_seq_cst);
}

const char* scheduler_platform_get_name(void)
{
    const scheduler_platform_ops_t* ops = scheduler_platform_get_ops();
    if (ops && ops->get_name) {
        return ops->get_name();
    }

    return "unknown";
}
