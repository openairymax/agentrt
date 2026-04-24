/**
 * @file scheduler.c
 * @brief 任务调度器（基于新架构：核心层 + 平台适配器）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * @details
 * 本模块使用新的三层架构实现跨平台线程管理：
 * 1. 核心层（platform-agnostic）：任务管理、哈希表、原子操作
 * 2. 适配器接口（platform-interface）：统一平台操作定义
 * 3. 平台适配器（platform-specific）：Windows/POSIX具体实现
 *
 * 优势：
 * - 消除Windows/POSIX代码重复
 * - 提高可测试性和可维护性
 * - 降低圈复杂度（通过模块分解）
 * - 支持未来平台扩展
 */

#include "task.h"
#include "scheduler_core.h"
#include "scheduler_platform.h"
#include "mem.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "memory_compat.h"
#include "string_compat.h"

/* Check macros for unified error handling */
#include "check.h"
#include <string.h>
/* 跨平台原子操作支持 - 使用统一的 atomic_compat.h */
#include "atomic_compat.h"

/* ==================== 类型适配辅助 ==================== */

/**
 * @brief 将用户提供的 void (*func)(void*) 转换为 void* (*entry)(void*) 格式
 *
 * @param user_func 用户线程入口函数
 * @param arg 线程参数
 * @return 始终返回NULL（用户函数无返回值）
 * @note [INFRA] 线程适配器 - 保留供未来线程模型扩展使用
 */
static void* user_thread_entry_adapter(void* (*user_func)(void*), void* arg)
{
    /* 用户函数是 void (*func)(void*)，我们调用它并返回NULL */
    user_func(arg);
    return NULL;
}

/**
 * @brief 包装用户线程入口函数
 *
 * 创建适配器函数指针，用于核心层调用
 *
 * @param user_func 用户线程入口函数
 * @return 适配后的函数指针
 */
static void* (*wrap_user_thread_entry(agentos_thread_func_t user_func))(void*)
{
    /* 返回一个适配器函数，该适配器调用user_func */
    return (void* (*)(void*))user_func;
}

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 确保调度器完全初始化
 *
 * 初始化调度器核心层和平台适配器
 *
 * @return 0 成功，-1 失败
 */
static int ensure_scheduler_fully_initialized(void)
{
    /* 初始化调度器核心层 */
    if (scheduler_core_init() != 0) {
        return -1;
    }

    /* 初始化平台适配器 */
    if (scheduler_platform_auto_init() != 0) {
        return -1;
    }

    return 0;
}

/**
 * @brief 根据平台句柄查找任务信息
 *
 * 通过平台特定句柄查找对应的任务信息结构
 *
 * @param platform_handle 平台特定句柄
 * @return 任务信息指针，未找到返回NULL
 */
static task_info_core_t* find_task_by_platform_handle(void* platform_handle)
{
    scheduler_core_ctx_t* ctx = scheduler_core_get_ctx();
    if (!ctx) {
        return NULL;
    }

    /* 使用核心层提供的查找函数 */
    return scheduler_core_find_by_platform_handle(platform_handle);
}

/**
 * @brief 通过任务ID查找任务信息
 *
 * 通过任务ID查找对应的任务信息结构
 *
 * @param tid 任务ID
 * @return 任务信息指针，未找到返回NULL
 */
static task_info_core_t* find_task_by_id(agentos_task_id_t tid)
{
    scheduler_core_ctx_t* ctx = scheduler_core_get_ctx();
    if (!ctx) {
        return NULL;
    }

    /* 获取核心上下文锁 */
    agentos_mutex_lock(ctx->task_table_lock);

    /* 使用核心层哈希查找 */
    task_info_core_t* info = scheduler_core_hash_find(tid);

    agentos_mutex_unlock(ctx->task_table_lock);

    return info;
}

/* ==================== 公共接口实现 ==================== */

/**
 * @brief 初始化任务调度器
 *
 * 初始化调度器核心层和平台适配器
 *
 * @return AGENTOS_SUCCESS 成功，错误码 失败
 */
agentos_error_t agentos_task_init(void)
{
    if (ensure_scheduler_fully_initialized() != 0) {
        return AGENTOS_ENOMEM;
    }

    return AGENTOS_SUCCESS;
}

/**
 * @brief 创建线程
 *
 * 使用新的架构创建线程：核心层管理任务信息，平台适配器执行具体线程操作
 *
 * @param thread 线程句柄输出
 * @param func 线程入口函数
 * @param arg 线程参数
 * @return 0 成功，错误码 失败
 */
int agentos_thread_create(
    agentos_thread_t* thread,
    agentos_thread_func_t func,
    void* arg)
{
    /* 参数检查 */
    if (!thread || !func) {
        return AGENTOS_EINVAL;
    }

    /* 确保调度器已初始化 */
    if (ensure_scheduler_fully_initialized() != 0) {
        return AGENTOS_ENOMEM;
    }

    /* 获取平台适配器操作集 */
    const scheduler_platform_ops_t* ops = scheduler_platform_get_ops();
    if (!ops) {
        return AGENTOS_ENOSYS;  /* 不支持的系统 */
    }

    /* 生成任务ID */
    uint64_t task_id = scheduler_core_fetch_add_task_id();
    if (task_id == 0) {
        return AGENTOS_ENOMEM;
    }

    /* 解析线程属性（使用默认值） */
    const char* task_name = "unnamed";
    int priority = AGENTOS_TASK_PRIORITY_NORMAL;
    size_t stack_size = 0;

    /* 创建核心任务信息结构 */
    task_info_core_t* task_info = scheduler_core_task_info_create(
        task_id,
        wrap_user_thread_entry(func),  /* 适配用户函数 */
        arg,
        task_name,
        priority);

    if (!task_info) {
        return AGENTOS_ENOMEM;
    }

    /* 使用平台适配器创建线程 */
    void* platform_handle = ops->thread_create(task_info, stack_size);
    if (!platform_handle) {
        scheduler_core_task_info_destroy(task_info);
        return AGENTOS_ENOMEM;
    }

    /* 设置平台句柄到任务信息 */
    task_info->platform_handle = platform_handle;

    /* 获取核心上下文 */
    scheduler_core_ctx_t* ctx = scheduler_core_get_ctx();
    if (!ctx) {
        ops->cleanup_platform_resources(platform_handle, NULL);
        scheduler_core_task_info_destroy(task_info);
        return AGENTOS_ENOMEM;
    }

    /* 将任务添加到核心层管理 */
    agentos_mutex_lock(ctx->task_table_lock);

    int add_result = scheduler_core_task_table_add(task_info);
    if (add_result == 0) {
        /* 添加到哈希表 */
        scheduler_core_hash_insert(task_id, task_info);
    }

    agentos_mutex_unlock(ctx->task_table_lock);

    if (add_result != 0) {
        /* 添加失败，清理资源 */
        ops->cleanup_platform_resources(platform_handle, NULL);
        scheduler_core_task_info_destroy(task_info);
        return AGENTOS_ENOMEM;
    }

    /* 设置输出线程句柄 */
#if defined(_WIN32) || defined(_WIN64)
    {
        typedef struct windows_task_data { HANDLE thread_handle; DWORD thread_id; } wtd_t;
        wtd_t* wdata = (wtd_t*)platform_handle;
        *thread = wdata->thread_handle;
    }
#else
    {
        typedef struct posix_task_data { pthread_t thread_handle; pthread_attr_t thread_attr; int has_custom_stack_size; } ptd_t;
        ptd_t* pdata = (ptd_t*)platform_handle;
        *thread = pdata->thread_handle;
    }
#endif

    return AGENTOS_SUCCESS;
}

/**
 * @brief 等待线程结束
 *
 * 等待指定的线程结束，并获取线程返回值
 *
 * @param thread 线程句柄
 * @param retval 返回值输出
 * @return AGENTOS_SUCCESS 成功，错误码 失败
 */
agentos_error_t agentos_thread_join(agentos_thread_t thread, void** retval)
{
    if (!thread) {
        return AGENTOS_EINVAL;
    }

    const scheduler_platform_ops_t* ops = scheduler_platform_get_ops();
    if (!ops) {
        return AGENTOS_ENOSYS;
    }

    scheduler_core_ctx_t* ctx = scheduler_core_get_ctx();
    if (!ctx) {
        return AGENTOS_EINVAL;
    }

    task_info_core_t* task_info = NULL;

    agentos_mutex_lock(ctx->task_table_lock);
    for (uint32_t i = 0; i < ctx->task_count; i++) {
        task_info_core_t* info = ctx->task_table[i];
        if (!info || !info->platform_handle) continue;

#if defined(_WIN32) || defined(_WIN64)
        typedef struct windows_task_data { HANDLE thread_handle; DWORD thread_id; } wtd_t;
        wtd_t* wdata = (wtd_t*)info->platform_handle;
        if (wdata->thread_handle == thread) {
            task_info = info;
            break;
        }
#else
        typedef struct posix_task_data { pthread_t thread_handle; pthread_attr_t thread_attr; int has_custom_stack_size; } ptd_t;
        ptd_t* pdata = (ptd_t*)info->platform_handle;
        if (pthread_equal(pdata->thread_handle, thread)) {
            task_info = info;
            break;
        }
#endif
    }
    agentos_mutex_unlock(ctx->task_table_lock);

    if (!task_info) {
        return AGENTOS_EINVAL;
    }

    int result = ops->thread_join(task_info->platform_handle, retval);
    if (result != 0) {
        return AGENTOS_EINVAL;
    }

    if (retval && task_info->retval) {
        *retval = task_info->retval;
    }

    return AGENTOS_SUCCESS;
}

/**
 * @brief 获取当前任务ID
 *
 * 获取当前执行线程的任务ID
 *
 * @return 当前任务ID，失败返回0
 */
agentos_task_id_t agentos_task_self(void)
{
    /* 获取平台适配器操作集 */
    const scheduler_platform_ops_t* ops = scheduler_platform_get_ops();
    if (!ops) {
        return 0;
    }

    /* 获取当前平台线程ID */
    uintptr_t current_thread_id = ops->get_current_thread_id();
    if (current_thread_id == 0) {
        return 0;
    }

    /* 获取核心上下文 */
    scheduler_core_ctx_t* ctx = scheduler_core_get_ctx();
    if (!ctx) {
        return 0;
    }

    /* 查找匹配的任务 */
    agentos_mutex_lock(ctx->task_table_lock);

    for (uint32_t i = 0; i < ctx->task_count; i++) {
        task_info_core_t* info = ctx->task_table[i];
        if (!info || !info->platform_handle) {
            continue;
        }

        /* 获取任务的系统线程ID */
        uintptr_t task_system_id = ops->get_thread_system_id(info->platform_handle);
        if (task_system_id == current_thread_id) {
            agentos_task_id_t id = info->id;
            agentos_mutex_unlock(ctx->task_table_lock);
            return id;
        }
    }

    agentos_mutex_unlock(ctx->task_table_lock);

    return 0;
}

/**
 * @brief 线程休眠
 *
 * 使当前线程休眠指定的毫秒数
 *
 * @param ms 休眠毫秒数
 */
void agentos_task_sleep(uint32_t ms)
{
    /* 获取平台适配器操作集 */
    const scheduler_platform_ops_t* ops = scheduler_platform_get_ops();
    if (!ops || !ops->thread_sleep) {
        /* 如果没有平台适配器，使用简单循环等待（仅用于紧急情况） */
        volatile uint32_t i;
        for (i = 0; i < (uint32_t)(1000 * ms); i++);
        return;
    }

    ops->thread_sleep(ms);
}

/**
 * @brief 线程让出CPU
 *
 * 使当前线程让出CPU，允许其他线程运行
 */
void agentos_task_yield(void)
{
    /* 获取平台适配器操作集 */
    const scheduler_platform_ops_t* ops = scheduler_platform_get_ops();
    if (!ops || !ops->thread_yield) {
        return;  /* 无法让出 */
    }

    ops->thread_yield();
}

/**
 * @brief 设置任务优先级
 *
 * 设置指定任务的优先级
 *
 * @param tid 任务ID
 * @param priority 优先级
 * @return AGENTOS_SUCCESS 成功，错误码 失败
 */
agentos_error_t agentos_task_set_priority(agentos_task_id_t tid, int priority)
{
    /* 验证优先级范围 */
    if (priority < AGENTOS_TASK_PRIORITY_MIN ||
        priority > AGENTOS_TASK_PRIORITY_MAX) {
        return AGENTOS_EINVAL;
    }

    /* 查找任务信息 */
    task_info_core_t* task_info = find_task_by_id(tid);
    if (!task_info) {
        return AGENTOS_EINVAL;
    }

    /* 获取平台适配器操作集 */
    const scheduler_platform_ops_t* ops = scheduler_platform_get_ops();
    if (!ops) {
        return AGENTOS_ENOSYS;
    }

    /* 使用平台适配器设置线程优先级 */
    int result = ops->thread_set_priority(task_info->platform_handle, priority);
    if (result != 0) {
        return AGENTOS_EINVAL;
    }

    /* 更新任务信息中的优先级 */
    task_info->priority = priority;

    return AGENTOS_SUCCESS;
}

/**
 * @brief 获取任务优先级
 *
 * 获取指定任务的优先级
 *
 * @param tid 任务ID
 * @param out_priority 优先级输出
 * @return AGENTOS_SUCCESS 成功，错误码 失败
 */
agentos_error_t agentos_task_get_priority(agentos_task_id_t tid, int* out_priority)
{
    if (!out_priority) {
        return AGENTOS_EINVAL;
    }

    /* 查找任务信息 */
    task_info_core_t* task_info = find_task_by_id(tid);
    if (!task_info) {
        return AGENTOS_EINVAL;
    }

    *out_priority = task_info->priority;

    return AGENTOS_SUCCESS;
}

/**
 * @brief 获取任务状态
 *
 * 获取指定任务的状态
 *
 * @param tid 任务ID
 * @param out_state 状态输出
 * @return AGENTOS_SUCCESS 成功，错误码 失败
 */
agentos_error_t agentos_task_get_state(agentos_task_id_t tid, agentos_task_state_t* out_state)
{
    if (!out_state) {
        return AGENTOS_EINVAL;
    }

    /* 查找任务信息 */
    task_info_core_t* task_info = find_task_by_id(tid);
    if (!task_info) {
        return AGENTOS_EINVAL;
    }

    *out_state = task_info->state;

    return AGENTOS_SUCCESS;
}

void agentos_task_cleanup(void)
{
    scheduler_core_ctx_t* ctx = scheduler_core_get_ctx();
    if (!ctx || !scheduler_core_is_initialized()) {
        return;
    }

    const scheduler_platform_ops_t* ops = scheduler_platform_get_ops();

    agentos_mutex_lock(ctx->task_table_lock);

    for (uint32_t i = 0; i < ctx->task_count; i++) {
        task_info_core_t* info = ctx->task_table[i];
        if (!info) continue;

        if (ops && ops->cleanup_platform_resources && info->platform_handle) {
            ops->cleanup_platform_resources(info->platform_handle, info->platform_data);
        }

        scheduler_core_task_info_destroy(info);
        ctx->task_table[i] = NULL;
    }

    for (size_t b = 0; b < HASH_TABLE_BUCKETS; b++) {
        task_hash_node_t* node = ctx->id_hash_table[b];
        while (node) {
            task_hash_node_t* next = node->next;
            free(node);
            node = next;
        }
        ctx->id_hash_table[b] = NULL;
    }

    ctx->task_count = 0;

    agentos_mutex_unlock(ctx->task_table_lock);

    if (ops && ops->cleanup) {
        ops->cleanup();
    }

    scheduler_core_destroy();
}
