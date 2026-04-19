/**
 * @file scheduler_windows.c
 * @brief Windows平台调度器适配器实�? * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * @details
 * 本模块实现Windows平台的线程操作适配器，提供与平台无关核心层的接口�? * 通过实现scheduler_platform_ops_t中定义的所有操作，将Windows API封装为统一接口�? */

#define __STDC_NO_ATOMICS__

#include "scheduler_platform.h"
#include "scheduler_core.h"
#include <windows.h>
#include <stdlib.h>

/* Unified base library compatibility layer */
#include <agentos/utils/memory/memory_compat.h>
#include <string.h>

/* ==================== 内部类型定义 ==================== */

/**
 * @brief Windows平台特定任务数据
 *
 * 存储Windows平台特有的线程相关信息�? */
typedef struct windows_task_data {
    /** @brief Windows线程句柄 */
    HANDLE thread_handle;

    /** @brief Windows线程ID */
    DWORD thread_id;

    /** @brief 线程入口函数包装器，用于转换调用约定 */
    DWORD (WINAPI *thread_entry_wrapper)(LPVOID);
} windows_task_data_t;

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief Windows线程入口包装�? *
 * 将Windows线程入口函数转换为标准C线程入口函数�? *
 * @param param 参数指针，指向task_info_core_t结构
 * @return 线程退出码
 */
static DWORD WINAPI windows_thread_entry_wrapper(LPVOID param)
{
    task_info_core_t* info = (task_info_core_t*)param;

    /* 设置任务状态为运行�?*/
    info->state = AGENTOS_TASK_STATE_RUNNING;

    /* 调用用户提供的线程入口函�?*/
    info->retval = info->entry(info->arg);

    /* 设置任务状态为已终�?*/
    info->state = AGENTOS_TASK_STATE_TERMINATED;

    return 0;
}

/**
 * @brief 将AgentOS优先级转换为Windows优先�? *
 * AgentOS优先级范围为AGENTOS_TASK_PRIORITY_MIN到AGENTOS_TASK_PRIORITY_MAX�? * 需要映射到Windows的THREAD_PRIORITY_*常量�? *
 * @param agentos_priority AgentOS优先�? * @return Windows优先级常�? */
static int map_priority_to_windows(int agentos_priority)
{
    /* AgentOS优先级范围映射到Windows优先�?*/
    if (agentos_priority >= AGENTOS_TASK_PRIORITY_HIGH) {
        return THREAD_PRIORITY_HIGHEST;
    } else if (agentos_priority <= AGENTOS_TASK_PRIORITY_LOW) {
        return THREAD_PRIORITY_LOWEST;
    } else {
        return THREAD_PRIORITY_NORMAL;
    }
}

/* ==================== 平台适配器操作实�?==================== */

/**
 * @brief 初始化Windows平台适配�? *
 * 初始化Windows平台特定的资源。当前Windows平台无需特殊初始化�? *
 * @return 0 成功�?1 失败
 */
static int windows_platform_init(void)
{
    /* Windows平台无需特殊初始�?*/
    return 0;
}

/**
 * @brief 清理Windows平台适配�? *
 * 清理Windows平台特定的资源。当前Windows平台无需特殊清理�? */
static void windows_platform_cleanup(void)
{
    /* Windows平台无需特殊清理 */
}

/**
 * @brief 创建Windows线程
 *
 * 使用Windows API创建线程，并设置线程优先级和栈大小�? *
 * @param info 任务信息结构，包含线程入口函数和参数
 * @param stack_size 栈大小（0表示使用默认大小�? * @return 平台特定句柄（windows_task_data_t指针），失败返回NULL
 */
static void* windows_thread_create(task_info_core_t* info, size_t stack_size)
{
    windows_task_data_t* data = NULL;

    /* 分配平台特定数据 */
    data = (windows_task_data_t*)AGENTOS_CALLOC(1, sizeof(windows_task_data_t));
    if (!data) {
        return NULL;
    }

    /* 设置线程入口包装�?*/
    data->thread_entry_wrapper = windows_thread_entry_wrapper;

    /* 创建Windows线程 */
    data->thread_handle = CreateThread(
        NULL,                           /* 默认安全属�?*/
        (stack_size > 0) ? (DWORD)stack_size : 0,  /* 栈大�?*/
        windows_thread_entry_wrapper,   /* 线程入口函数 */
        info,                           /* 参数 */
        0,                              /* 创建标志（立即运行） */
        &data->thread_id                /* 线程ID输出 */
    );

    if (!data->thread_handle) {
        AGENTOS_FREE(data);
        return NULL;
    }

    /* 设置线程优先�?*/
    int win_priority = map_priority_to_windows(info->priority);
    SetThreadPriority(data->thread_handle, win_priority);

    /* 返回平台特定数据作为句柄 */
    return data;
}

/**
 * @brief 等待Windows线程结束
 *
 * 等待指定的Windows线程结束，并获取线程返回值�? *
 * @param platform_handle 平台特定句柄（windows_task_data_t指针�? * @param retval 返回值输出指针（可为NULL�? * @return 0 成功�?1 失败
 */
static int windows_thread_join(void* platform_handle, void** retval)
{
    windows_task_data_t* data = (windows_task_data_t*)platform_handle;

    if (!data || !data->thread_handle) {
        return -1;
    }

    /* 等待线程结束 */
    if (WaitForSingleObject(data->thread_handle, INFINITE) != WAIT_OBJECT_0) {
        return -1;
    }

    /* 如果调用者请求返回值，需要从任务信息结构中获�?*/
    if (retval) {
        /* 注意：返回值存储在task_info_core_t.retval中，由线程入口函数设�?         * 调用者需要自己从任务信息结构中获�?*/
        *retval = NULL;  /* 平台适配器不直接提供返回�?*/
    }

    return 0;
}

/**
 * @brief 设置Windows线程优先�? *
 * 设置指定Windows线程的优先级�? *
 * @param platform_handle 平台特定句柄（windows_task_data_t指针�? * @param priority AgentOS优先�? * @return 0 成功�?1 失败
 */
static int windows_thread_set_priority(void* platform_handle, int priority)
{
    windows_task_data_t* data = (windows_task_data_t*)platform_handle;

    if (!data || !data->thread_handle) {
        return -1;
    }

    /* 验证优先级范�?*/
    if (priority < AGENTOS_TASK_PRIORITY_MIN ||
        priority > AGENTOS_TASK_PRIORITY_MAX) {
        return -1;
    }

    /* 映射优先级到Windows常量 */
    int win_priority = map_priority_to_windows(priority);

    /* 设置线程优先�?*/
    if (!SetThreadPriority(data->thread_handle, win_priority)) {
        return -1;
    }

    return 0;
}

/**
 * @brief 获取当前Windows线程ID
 *
 * 获取当前执行的Windows线程ID�? *
 * @return 当前线程ID
 */
static uintptr_t windows_get_current_thread_id(void)
{
    return (uintptr_t)GetCurrentThreadId();
}

/**
 * @brief 获取Windows线程的系统线程ID
 *
 * 获取指定Windows线程的系统线程ID�? *
 * @param platform_handle 平台特定句柄（windows_task_data_t指针�? * @return 系统线程ID，失败返�?
 */
static uintptr_t windows_get_thread_system_id(void* platform_handle)
{
    windows_task_data_t* data = (windows_task_data_t*)platform_handle;

    if (!data) {
        return 0;
    }

    return (uintptr_t)data->thread_id;
}

/**
 * @brief Windows线程休眠
 *
 * 使当前线程休眠指定的毫秒数�? *
 * @param ms 休眠毫秒�? */
static void windows_thread_sleep(uint32_t ms)
{
    Sleep((DWORD)ms);
}

/**
 * @brief Windows线程让出CPU
 *
 * 使当前线程让出CPU，允许其他线程运行�? */
static void windows_thread_yield(void)
{
    SwitchToThread();
}

/**
 * @brief 清理Windows平台特定资源
 *
 * 清理Windows线程相关的资源，包括关闭句柄和释放内存�? *
 * @param platform_handle 平台特定句柄（windows_task_data_t指针�? * @param platform_data 平台特定数据（当前未使用�? */
static void windows_cleanup_platform_resources(void* platform_handle, void* platform_data)
{
    windows_task_data_t* data = (windows_task_data_t*)platform_handle;

    if (data) {
        /* 关闭线程句柄 */
        if (data->thread_handle) {
            CloseHandle(data->thread_handle);
        }

        /* 释放平台特定数据内存 */
        AGENTOS_FREE(data);
    }

    /* platform_data当前未使�?*/
    (void)platform_data;
}

/**
 * @brief 获取平台适配器名�? *
 * 返回Windows平台适配器的名称字符串�? *
 * @return 平台适配器名称字符串
 */
static const char* windows_get_name(void)
{
    return "windows";
}

/* ==================== 平台适配器操作集定义 ==================== */

/**
 * @brief Windows平台适配器操作集
 *
 * 定义Windows平台的所有适配器操作函数�? */
static const scheduler_platform_ops_t windows_platform_ops = {
    .init = windows_platform_init,
    .cleanup = windows_platform_cleanup,
    .thread_create = windows_thread_create,
    .thread_join = windows_thread_join,
    .thread_set_priority = windows_thread_set_priority,
    .get_current_thread_id = windows_get_current_thread_id,
    .get_thread_system_id = windows_get_thread_system_id,
    .thread_sleep = windows_thread_sleep,
    .thread_yield = windows_thread_yield,
    .cleanup_platform_resources = windows_cleanup_platform_resources,
    .get_name = windows_get_name
};

/* ==================== 公共接口 ==================== */

/**
 * @brief 获取Windows平台适配器操作集
 *
 * 返回Windows平台适配器操作集的指针�? *
 * @return Windows平台适配器操作集指针
 */
const scheduler_platform_ops_t* scheduler_platform_get_windows_ops(void)
{
    return &windows_platform_ops;
}
