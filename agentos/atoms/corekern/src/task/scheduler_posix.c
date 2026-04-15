/**
 * @file scheduler_posix.c
 * @brief POSIX平台调度器适配器实�? * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * @details
 * 本模块实现POSIX平台（Linux、macOS、BSD等）的线程操作适配器，
 * 提供与平台无关核心层的接口�? * 通过实现scheduler_platform_ops_t中定义的所有操作，将POSIX线程API封装为统一接口�? */

#include "scheduler_platform.h"
#include "scheduler_core.h"
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>

/* Unified base library compatibility layer */
#include <agentos/memory.h>
#include <agentos/string.h>
#include <string.h>

/* ==================== 内部类型定义 ==================== */

/**
 * @brief POSIX平台特定任务数据
 *
 * 存储POSIX平台特有的线程相关信息�? */
typedef struct posix_task_data {
    /** @brief POSIX线程句柄 */
    pthread_t thread_handle;

    /** @brief 线程属性，用于栈大小设�?*/
    pthread_attr_t thread_attr;

    /** @brief 是否使用了自定义栈大�?*/
    int has_custom_stack_size;
} posix_task_data_t;

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief POSIX线程入口包装�? *
 * 将POSIX线程入口函数转换为标准C线程入口函数�? *
 * @param param 参数指针，指向task_info_core_t结构
 * @return 线程退出码指针
 */
static void* posix_thread_entry_wrapper(void* param)
{
    task_info_core_t* info = (task_info_core_t*)param;

    /* 设置任务状态为运行�?*/
    info->state = AGENTOS_TASK_STATE_RUNNING;

    /* 调用用户提供的线程入口函�?*/
    info->retval = info->entry(info->arg);

    /* 设置任务状态为已终�?*/
    info->state = AGENTOS_TASK_STATE_TERMINATED;

    return info->retval;
}

/**
 * @brief 将AgentOS优先级转换为POSIX调度参数
 *
 * AgentOS优先级范围为AGENTOS_TASK_PRIORITY_MIN到AGENTOS_TASK_PRIORITY_MAX�? * 需要映射到POSIX调度优先级范围�? *
 * @param agentos_priority AgentOS优先�? * @param sched_policy 调度策略（输出参数）
 * @param sched_param 调度参数（输出参数）
 * @return 0 成功�?1 失败
 */
static int map_priority_to_posix(int agentos_priority, int* sched_policy, struct sched_param* sp)
{
    int min_prio, max_prio;

    /* 使用SCHED_OTHER作为默认调度策略（非实时�?*/
    *sched_policy = SCHED_OTHER;

    /* 获取当前调度策略的优先级范围 */
    min_prio = sched_get_priority_min(*sched_policy);
    max_prio = sched_get_priority_max(*sched_policy);

    if (min_prio == -1 || max_prio == -1) {
        /* 如果SCHED_OTHER不支持优先级，使用SCHED_FIFO尝试 */
        *sched_policy = SCHED_FIFO;
        min_prio = sched_get_priority_min(*sched_policy);
        max_prio = sched_get_priority_max(*sched_policy);

        if (min_prio == -1 || max_prio == -1) {
            /* 如果所有调度策略都不支持优先级，则无法设置优先�?*/
            return -1;
        }
    }

    /* 将AgentOS优先级映射到POSIX优先级范�?*/
    sp->sched_priority = min_prio + (int)((max_prio - min_prio) *
                                        (double)agentos_priority / 100.0);

    return 0;
}

/* ==================== 平台适配器操作实�?==================== */

/**
 * @brief 初始化POSIX平台适配�? *
 * 初始化POSIX平台特定的资源。当前POSIX平台无需特殊初始化�? *
 * @return 0 成功�?1 失败
 */
static int posix_platform_init(void)
{
    /* POSIX平台无需特殊初始�?*/
    return 0;
}

/**
 * @brief 清理POSIX平台适配�? *
 * 清理POSIX平台特定的资源。当前POSIX平台无需特殊清理�? */
static void posix_platform_cleanup(void)
{
    /* POSIX平台无需特殊清理 */
}

/**
 * @brief 创建POSIX线程
 *
 * 使用POSIX线程API创建线程，并设置线程属性和栈大小�? *
 * @param info 任务信息结构，包含线程入口函数和参数
 * @param stack_size 栈大小（0表示使用默认大小�? * @return 平台特定句柄（posix_task_data_t指针），失败返回NULL
 */
static void* posix_thread_create(task_info_core_t* info, size_t stack_size)
{
    posix_task_data_t* data = NULL;
    int ret;

    /* 分配平台特定数据 */
    data = (posix_task_data_t*)AGENTOS_CALLOC(1, sizeof(posix_task_data_t));
    if (!data) {
        return NULL;
    }

    /* 初始化线程属�?*/
    ret = pthread_attr_init(&data->thread_attr);
    if (ret != 0) {
        AGENTOS_FREE(data);
        return NULL;
    }

    /* 设置栈大小（如果指定�?*/
    data->has_custom_stack_size = 0;
    if (stack_size > 0) {
        ret = pthread_attr_setstacksize(&data->thread_attr, stack_size);
        if (ret == 0) {
            data->has_custom_stack_size = 1;
        }
    }

    /* 创建POSIX线程 */
    ret = pthread_create(&data->thread_handle, &data->thread_attr,
                        posix_thread_entry_wrapper, info);

    if (ret != 0) {
        pthread_attr_destroy(&data->thread_attr);
        AGENTOS_FREE(data);
        return NULL;
    }

    /* 设置线程优先级（如果支持�?*/
    if (info->priority != AGENTOS_TASK_PRIORITY_NORMAL) {
        int sched_policy;
        struct sched_param sp = {0};

        if (map_priority_to_posix(info->priority, &sched_policy, &sp) == 0) {
            /* 尝试设置调度参数 */
            pthread_setschedparam(data->thread_handle, sched_policy, &sp);
        }
    }

    /* 返回平台特定数据作为句柄 */
    return data;
}

/**
 * @brief 等待POSIX线程结束
 *
 * 等待指定的POSIX线程结束，并获取线程返回值�? *
 * @param platform_handle 平台特定句柄（posix_task_data_t指针�? * @param retval 返回值输出指针（可为NULL�? * @return 0 成功�?1 失败
 */
static int posix_thread_join(void* platform_handle, void** retval)
{
    posix_task_data_t* data = (posix_task_data_t*)platform_handle;

    if (!data) {
        return -1;
    }

    /* 等待线程结束 */
    int ret = pthread_join(data->thread_handle, retval);

    /* 清理线程属�?*/
    pthread_attr_destroy(&data->thread_attr);

    return (ret == 0) ? 0 : -1;
}

/**
 * @brief 设置POSIX线程优先�? *
 * 设置指定POSIX线程的优先级�? *
 * @param platform_handle 平台特定句柄（posix_task_data_t指针�? * @param priority AgentOS优先�? * @return 0 成功�?1 失败
 */
static int posix_thread_set_priority(void* platform_handle, int priority)
{
    posix_task_data_t* data = (posix_task_data_t*)platform_handle;

    if (!data) {
        return -1;
    }

    /* 验证优先级范�?*/
    if (priority < AGENTOS_TASK_PRIORITY_MIN ||
        priority > AGENTOS_TASK_PRIORITY_MAX) {
        return -1;
    }

    /* 映射优先级到POSIX调度参数 */
    int sched_policy;
    struct sched_param sp = {0};

    if (map_priority_to_posix(priority, &sched_policy, &sp) != 0) {
        return -1;
    }

    /* 设置调度参数 */
    int ret = pthread_setschedparam(data->thread_handle, sched_policy, &sp);

    return (ret == 0) ? 0 : -1;
}

/**
 * @brief 获取当前POSIX线程ID
 *
 * 获取当前执行的POSIX线程ID�? *
 * @return 当前线程ID
 */
static uintptr_t posix_get_current_thread_id(void)
{
    /* POSIX线程ID是pthread_t类型，可以转换为uintptr_t */
    return (uintptr_t)pthread_self();
}

/**
 * @brief 获取POSIX线程的系统线程ID
 *
 * 获取指定POSIX线程的系统线程ID（通常是内核线程ID）�? * 注意：POSIX标准没有提供获取系统线程ID的便携方法，
 * 这里返回pthread_t作为系统线程ID的近似�? *
 * @param platform_handle 平台特定句柄（posix_task_data_t指针�? * @return 系统线程ID，失败返�?
 */
static uintptr_t posix_get_thread_system_id(void* platform_handle)
{
    posix_task_data_t* data = (posix_task_data_t*)platform_handle;

    if (!data) {
        return 0;
    }

    /* POSIX中，pthread_t可以视为系统线程ID的近�?*/
    return (uintptr_t)data->thread_handle;
}

/**
 * @brief POSIX线程休眠
 *
 * 使当前线程休眠指定的毫秒数�? *
 * @param ms 休眠毫秒�? */
static void posix_thread_sleep(uint32_t ms)
{
    struct timespec ts;

    /* 计算休眠时间 */
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;

    /* 使用nanosleep进行高精度休�?*/
    nanosleep(&ts, NULL);
}

/**
 * @brief POSIX线程让出CPU
 *
 * 使当前线程让出CPU，允许其他线程运行�? */
static void posix_thread_yield(void)
{
    sched_yield();
}

/**
 * @brief 清理POSIX平台特定资源
 *
 * 清理POSIX线程相关的资源，包括线程属性和内存�? *
 * @param platform_handle 平台特定句柄（posix_task_data_t指针�? * @param platform_data 平台特定数据（当前未使用�? */
static void posix_cleanup_platform_resources(void* platform_handle, void* platform_data)
{
    posix_task_data_t* data = (posix_task_data_t*)platform_handle;

    if (data) {
        /* 清理线程属�?*/
        pthread_attr_destroy(&data->thread_attr);

        /* 释放平台特定数据内存 */
        AGENTOS_FREE(data);
    }

    /* platform_data当前未使�?*/
    (void)platform_data;
}

/**
 * @brief 获取平台适配器名�? *
 * 返回POSIX平台适配器的名称字符串�? *
 * @return 平台适配器名称字符串
 */
static const char* posix_get_name(void)
{
    return "posix";
}

/* ==================== 平台适配器操作集定义 ==================== */

/**
 * @brief POSIX平台适配器操作集
 *
 * 定义POSIX平台的所有适配器操作函数�? */
static const scheduler_platform_ops_t posix_platform_ops = {
    .init = posix_platform_init,
    .cleanup = posix_platform_cleanup,
    .thread_create = posix_thread_create,
    .thread_join = posix_thread_join,
    .thread_set_priority = posix_thread_set_priority,
    .get_current_thread_id = posix_get_current_thread_id,
    .get_thread_system_id = posix_get_thread_system_id,
    .thread_sleep = posix_thread_sleep,
    .thread_yield = posix_thread_yield,
    .cleanup_platform_resources = posix_cleanup_platform_resources,
    .get_name = posix_get_name
};

/* ==================== 公共接口 ==================== */

/**
 * @brief 获取POSIX平台适配器操作集
 *
 * 返回POSIX平台适配器操作集的指针�? *
 * @return POSIX平台适配器操作集指针
 */
const scheduler_platform_ops_t* scheduler_platform_get_posix_ops(void)
{
    return &posix_platform_ops;
}
