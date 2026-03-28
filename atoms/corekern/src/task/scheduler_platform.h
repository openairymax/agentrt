/**
 * @file scheduler_platform.h
 * @brief 调度器平台适配器接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * 
 * @details
 * 本模块定义平台适配器接口，用于将平台特定的线程操作
 * 与平台无关的核心层解耦。
 */

#ifndef AGENTOS_SCHEDULER_PLATFORM_H
#define AGENTOS_SCHEDULER_PLATFORM_H

#include "scheduler_core.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 平台适配器操作结构 ==================== */

/**
 * @brief 平台适配器操作集
 * 
 * 每个平台需要提供这些操作的实现。
 */
typedef struct scheduler_platform_ops {
    /**
     * @brief 初始化平台适配器
     * @return 0 成功，-1 失败
     */
    int (*init)(void);
    
    /**
     * @brief 清理平台适配器
     */
    void (*cleanup)(void);
    
    /**
     * @brief 创建线程
     * @param info 任务信息（包含entry和arg）
     * @param stack_size 栈大小（0表示默认）
     * @return 平台特定句柄，失败返回NULL
     */
    void* (*thread_create)(task_info_core_t* info, size_t stack_size);
    
    /**
     * @brief 等待线程结束
     * @param platform_handle 平台特定句柄
     * @param retval 返回值输出（可为NULL）
     * @return 0 成功，-1 失败
     */
    int (*thread_join)(void* platform_handle, void** retval);
    
    /**
     * @brief 设置线程优先级
     * @param platform_handle 平台特定句柄
     * @param priority 优先级（AGENTOS_TASK_PRIORITY_*）
     * @return 0 成功，-1 失败
     */
    int (*thread_set_priority)(void* platform_handle, int priority);
    
    /**
     * @brief 获取当前平台线程ID
     * @return 平台特定的线程ID
     */
    uintptr_t (*get_current_thread_id)(void);
    
    /**
     * @brief 将平台线程ID转换为系统线程ID
     * @param platform_handle 平台特定句柄
     * @return 系统线程ID，失败返回0
     */
    uintptr_t (*get_thread_system_id)(void* platform_handle);
    
    /**
     * @brief 线程休眠
     * @param ms 休眠毫秒数
     */
    void (*thread_sleep)(uint32_t ms);
    
    /**
     * @brief 线程让出CPU
     */
    void (*thread_yield)(void);
    
    /**
     * @brief 清理平台特定资源
     * @param platform_handle 平台特定句柄
     * @param platform_data 平台特定数据
     */
    void (*cleanup_platform_resources)(void* platform_handle, void* platform_data);
    
    /**
     * @brief 获取平台适配器名称
     * @return 平台适配器名称字符串
     */
    const char* (*get_name)(void);
} scheduler_platform_ops_t;

/* ==================== 平台适配器注册 ==================== */

/**
 * @brief 注册平台适配器操作集
 * @param ops 平台适配器操作集
 * 
 * @note 应该在系统初始化时调用，通常在agentos_task_init()中
 */
void scheduler_platform_register_ops(const scheduler_platform_ops_t* ops);

/**
 * @brief 获取当前平台适配器操作集
 * @return 平台适配器操作集，未注册返回NULL
 */
const scheduler_platform_ops_t* scheduler_platform_get_ops(void);

/* ==================== 平台检测宏 ==================== */

#if defined(_WIN32) || defined(_WIN64)
    #define AGENTOS_PLATFORM_WINDOWS 1
    #define AGENTOS_PLATFORM_POSIX 0
#else
    #define AGENTOS_PLATFORM_WINDOWS 0
    #define AGENTOS_PLATFORM_POSIX 1
#endif

/* ==================== 便捷函数 ==================== */

/**
 * @brief 初始化平台适配器（自动选择）
 * @return 0 成功，-1 失败
 * 
 * @note 根据编译平台自动选择Windows或POSIX适配器
 */
int scheduler_platform_auto_init(void);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_SCHEDULER_PLATFORM_H */