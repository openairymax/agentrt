#include "agentrt.h"
#include "error.h"
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

#include "mem.h"
#include "scheduler_core.h"
#include "scheduler_platform.h"
#include "task.h"

#include <stdlib.h>

/* Unified base library compatibility layer */
#include "memory_compat.h"
#include "string_compat.h"

/* Check macros for unified error handling */
#include "check.h"

#include <string.h>
/* 跨平台原子操作支持 - 使用统一的 atomic_compat.h */
#include "atomic_compat.h"
#ifdef _MSC_VER
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <unistd.h>
#endif
#include "compat.h"
#include "error_compat.h"

#define ATM_RET_ERR(c) \
    do { agentrt_error_push_ex((c), __FILE__, __LINE__, __func__, "%s", agentrt_error_str(c)); return (c); } while(0)


/* ==================== 类型适配辅助 ==================== */

/**
 * @brief 将用户提供的 void (*func)(void*) 转换为 void* (*entry)(void*) 格式
 *
 * @param user_func 用户线程入口函数
 * @param arg 线程参数
 * @return 始终返回NULL（用户函数无返回值）
 * @note [INFRA] 线程适配器 - 保留供未来线程模型扩展使用
 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
static void *__attribute__((used)) user_thread_entry_adapter(void *(*user_func)(void *), void *arg)
{
    /* 用户函数是 void (*func)(void*)，我们调用它并返回NULL */
    user_func(arg);
    return NULL;
}
#pragma GCC diagnostic pop

/**
 * @brief 创建适配器函数指针
 *
 * 创建适配器函数指针，用于核心层调用
 *
 * @param user_func 用户线程入口函数
 * @return 适配后的函数指针
 */
static void *(*wrap_user_thread_entry(agentrt_thread_func_t user_func))(void *)
{
    /* 返回一个适配器函数，该适配器调用user_func */
    return (void *(*)(void *))user_func;
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
    if (scheduler_core_init() != 0) {
        AGENTRT_ERROR(AGENTRT_EINVAL, "failed to init scheduler: core layer init failed");
    }

    if (scheduler_platform_auto_init() != 0) {
        AGENTRT_ERROR(AGENTRT_EINVAL, "failed to init scheduler: platform adapter init failed");
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
static task_info_core_t *__attribute__((used)) find_task_by_platform_handle(void *platform_handle)
{
    (void)platform_handle;
    scheduler_core_ctx_t *ctx = scheduler_core_get_ctx();
    if (!ctx) {
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
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
static task_info_core_t *find_task_by_id(agentrt_task_id_t tid)
{
    scheduler_core_ctx_t *ctx = scheduler_core_get_ctx();
    if (!ctx) {
        AGENTRT_LOG_ERROR("find_task_by_id: null scheduler context");
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }

    agentrt_mutex_lock(ctx->task_table_lock);

    task_info_core_t *info = scheduler_core_hash_find(tid);

    if (!info) {
        agentrt_mutex_unlock(ctx->task_table_lock);
    }

    return info;
}

static void release_task_lock(void)
{
    scheduler_core_ctx_t *ctx = scheduler_core_get_ctx();
    if (ctx && ctx->task_table_lock) {
        agentrt_mutex_unlock(ctx->task_table_lock);
    }
}

/* ==================== 公共接口实现 ==================== */

/**
 * @brief 初始化任务调度器
 *
 * 初始化调度器核心层和平台适配器
 *
 * @return AGENTRT_SUCCESS 成功，错误码 失败
 */
agentrt_error_t agentrt_task_init(void)
{
    if (ensure_scheduler_fully_initialized() != 0) {
        AGENTRT_LOG_ERROR("agentrt_task_init: scheduler initialization failed");
        ATM_RET_ERR(AGENTRT_ENOMEM);
    }

    return AGENTRT_SUCCESS;
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
int agentrt_thread_create(agentrt_thread_t *thread, agentrt_thread_func_t func, void *arg)
{
    /* 参数检查 */
    if (!thread || !func) {
        AGENTRT_LOG_ERROR("agentrt_thread_create: null parameter, thread=%p func=%p", (void *)thread, (void *)(uintptr_t)func);
        AGENTRT_ERROR(AGENTRT_EINVAL, "failed to create thread: null thread or func pointer");
    }

    /* 确保调度器已初始化 */
    if (ensure_scheduler_fully_initialized() != 0) {
        AGENTRT_LOG_ERROR("agentrt_thread_create: scheduler not initialized");
        ATM_RET_ERR(AGENTRT_ENOMEM);
    }

    /* 获取平台适配器操作集 */
    const scheduler_platform_ops_t *ops = scheduler_platform_get_ops();
    if (!ops) {
        AGENTRT_LOG_ERROR("agentrt_thread_create: platform ops not available");
        ATM_RET_ERR(AGENTRT_EPLATFORM);
    }

    /* 生成任务ID */
    uint64_t task_id = scheduler_core_fetch_add_task_id();
    if (task_id == 0) {
        AGENTRT_LOG_ERROR("agentrt_thread_create: failed to generate task ID");
        ATM_RET_ERR(AGENTRT_ENOMEM);
    }

    /* 解析线程属性（使用默认值） */
    const char *task_name = "unnamed";
    int priority = AGENTRT_TASK_PRIORITY_NORMAL;
    size_t stack_size = 0;

    /* 创建核心任务信息结构 */
    task_info_core_t *task_info =
        scheduler_core_task_info_create(task_id, wrap_user_thread_entry(func), /* 适配用户函数 */
                                        arg, task_name, priority);

    if (!task_info) {
        AGENTRT_LOG_ERROR("agentrt_thread_create: task_info create failed, ENOMEM, task_id=%llu", (unsigned long long)task_id);
        ATM_RET_ERR(AGENTRT_ENOMEM);
    }

    /* 使用平台适配器创建线程 */
    void *platform_handle = ops->thread_create(task_info, stack_size);
    if (!platform_handle) {
        AGENTRT_LOG_ERROR("agentrt_thread_create: platform thread create failed, task_id=%llu", (unsigned long long)task_id);
        scheduler_core_task_info_destroy(task_info);
        ATM_RET_ERR(AGENTRT_ENOMEM);
    }

    /* 设置平台句柄到任务信息 */
    task_info->platform_handle = platform_handle;

    /* 获取核心上下文 */
    scheduler_core_ctx_t *ctx = scheduler_core_get_ctx();
    if (!ctx) {
        AGENTRT_LOG_ERROR("agentrt_thread_create: failed to get scheduler context, task_id=%llu", (unsigned long long)task_id);
        ops->cleanup_platform_resources(platform_handle, NULL);
        scheduler_core_task_info_destroy(task_info);
        ATM_RET_ERR(AGENTRT_ENOMEM);
    }
    agentrt_mutex_lock(ctx->task_table_lock);

    int add_result = scheduler_core_task_table_add(task_info);
    if (add_result == 0) {
        /* 添加到哈希表 */
        scheduler_core_hash_insert(task_id, task_info);
    }

    agentrt_mutex_unlock(ctx->task_table_lock);

    if (add_result != 0) {
        /* 添加失败，清理资源 */
        AGENTRT_LOG_ERROR("agentrt_thread_create: task table add failed, task_id=%llu add_result=%d", (unsigned long long)task_id, add_result);
        ops->cleanup_platform_resources(platform_handle, NULL);
        scheduler_core_task_info_destroy(task_info);
        ATM_RET_ERR(AGENTRT_ENOMEM);
    }

    /* 设置输出线程句柄 */
#if defined(_WIN32) || defined(_WIN64)
    {
        typedef struct windows_task_data {
            HANDLE thread_handle;
            DWORD thread_id;
        } wtd_t;
        wtd_t *wdata = (wtd_t *)platform_handle;
        *thread = wdata->thread_handle;
    }
#else
    {
        typedef struct posix_task_data {
            pthread_t thread_handle;
            pthread_attr_t thread_attr;
            int has_custom_stack_size;
        } ptd_t;
        ptd_t *pdata = (ptd_t *)platform_handle;
        *thread = pdata->thread_handle;
    }
#endif

    return AGENTRT_SUCCESS;
}

/**
 * @brief 等待线程结束
 *
 * 等待指定的线程结束，并获取线程返回值
 *
 * @param thread 线程句柄
 * @param retval 返回值输出
 * @return AGENTRT_SUCCESS 成功，错误码 失败
 */
agentrt_error_t agentrt_thread_join(agentrt_thread_t thread, void **retval)
{
    if (!thread) {
        AGENTRT_LOG_ERROR("agentrt_thread_join: null thread handle");
        AGENTRT_ERROR(AGENTRT_EINVAL, "failed to join thread: null thread handle");
    }

    const scheduler_platform_ops_t *ops = scheduler_platform_get_ops();
    if (!ops) {
        AGENTRT_LOG_ERROR("agentrt_thread_join: platform ops not available");
        ATM_RET_ERR(AGENTRT_EPLATFORM);
    }

    scheduler_core_ctx_t *ctx = scheduler_core_get_ctx();
    if (!ctx) {
        AGENTRT_LOG_ERROR("agentrt_thread_join: null scheduler context");
        ATM_RET_ERR(AGENTRT_EINVAL);
    }

    task_info_core_t *task_info = NULL;

    agentrt_mutex_lock(ctx->task_table_lock);
    for (uint32_t i = 0; i < ctx->task_count; i++) {
        task_info_core_t *info = ctx->task_table[i];
        if (!info || !info->platform_handle)
            continue;

#if defined(_WIN32) || defined(_WIN64)
        typedef struct windows_task_data {
            HANDLE thread_handle;
            DWORD thread_id;
        } wtd_t;
        wtd_t *wdata = (wtd_t *)info->platform_handle;
        if (wdata->thread_handle == thread) {
            task_info = info;
            break;
        }
#else
        typedef struct posix_task_data {
            pthread_t thread_handle;
            pthread_attr_t thread_attr;
            int has_custom_stack_size;
        } ptd_t;
        ptd_t *pdata = (ptd_t *)info->platform_handle;
        if (pthread_equal(pdata->thread_handle, thread)) {
            task_info = info;
            break;
        }
#endif
    }
    agentrt_mutex_unlock(ctx->task_table_lock);

    if (!task_info) {
        AGENTRT_LOG_ERROR("agentrt_thread_join: task not found in table");
        ATM_RET_ERR(AGENTRT_EINVAL);
    }

    int result = ops->thread_join(task_info->platform_handle, retval);
    if (result != 0) {
        AGENTRT_LOG_ERROR("agentrt_thread_join: platform thread_join failed, result=%d", result);
        ATM_RET_ERR(AGENTRT_EINVAL);
    }

    if (retval && task_info->retval) {
        *retval = task_info->retval;
    }

    return AGENTRT_SUCCESS;
}

/**
 * @brief 获取当前任务ID
 *
 * 获取当前执行线程的任务ID
 *
 * @return 当前任务ID，失败返回0
 */
agentrt_task_id_t agentrt_task_self(void)
{
    /* 获取平台适配器操作集 */
    const scheduler_platform_ops_t *ops = scheduler_platform_get_ops();
    if (!ops) {
        return 0;
    }

    /* 获取当前平台线程ID */
    uintptr_t current_thread_id = ops->get_current_thread_id();
    if (current_thread_id == 0) {
        return 0;
    }

    /* 获取核心上下文 */
    scheduler_core_ctx_t *ctx = scheduler_core_get_ctx();
    if (!ctx) {
        return 0;
    }

    /* 查找匹配的任务 */
    agentrt_mutex_lock(ctx->task_table_lock);

    for (uint32_t i = 0; i < ctx->task_count; i++) {
        task_info_core_t *info = ctx->task_table[i];
        if (!info || !info->platform_handle) {
            continue;
        }

        /* 获取任务的系统线程ID */
        uintptr_t task_system_id = ops->get_thread_system_id(info->platform_handle);
        if (task_system_id == current_thread_id) {
            agentrt_task_id_t id = info->id;
            agentrt_mutex_unlock(ctx->task_table_lock);
            return id;
        }
    }

    agentrt_mutex_unlock(ctx->task_table_lock);

    return 0;
}

/**
 * @brief 线程休眠
 *
 * 使当前线程休眠指定的毫秒数
 *
 * @param ms 休眠毫秒数
 */
void agentrt_task_sleep(uint32_t ms)
{
    /* 获取平台适配器操作集 */
    const scheduler_platform_ops_t *ops = scheduler_platform_get_ops();
    if (!ops || !ops->thread_sleep) {
        /* 如果没有平台适配器，使用简单循环等待（仅用于紧急情况） */
        volatile uint32_t i;
        for (i = 0; i < (uint32_t)(1000 * ms); i++)
            ;
        return;
    }

    ops->thread_sleep(ms);
}

/**
 * @brief 线程让出CPU
 *
 * 使当前线程让出CPU，允许其他线程运行
 */
void agentrt_task_yield(void)
{
    /* 获取平台适配器操作集 */
    const scheduler_platform_ops_t *ops = scheduler_platform_get_ops();
    if (!ops || !ops->thread_yield) {
        return; /* 无法让出 */
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
 * @return AGENTRT_SUCCESS 成功，错误码 失败
 */
agentrt_error_t agentrt_task_set_priority(agentrt_task_id_t tid, int priority)
{
    /* 验证优先级范围 */
    if (priority < AGENTRT_TASK_PRIORITY_MIN || priority > AGENTRT_TASK_PRIORITY_MAX) {
        AGENTRT_LOG_ERROR("agentrt_task_set_priority: priority out of range, priority=%d min=%d max=%d", priority, AGENTRT_TASK_PRIORITY_MIN, AGENTRT_TASK_PRIORITY_MAX);
        AGENTRT_ERROR(AGENTRT_EINVAL, "failed to set task priority: priority out of valid range");
    }

    /* 查找任务信息 */
    task_info_core_t *task_info = find_task_by_id(tid);
    if (!task_info) {
        AGENTRT_LOG_ERROR("agentrt_task_set_priority: task not found, tid=%llu", (unsigned long long)tid);
        ATM_RET_ERR(AGENTRT_EINVAL);
    }

    const scheduler_platform_ops_t *ops = scheduler_platform_get_ops();
    if (!ops) {
        AGENTRT_LOG_ERROR("agentrt_task_set_priority: platform ops not available");
        release_task_lock();
        ATM_RET_ERR(AGENTRT_EPLATFORM);
    }

    int result = ops->thread_set_priority(task_info->platform_handle, priority);
    if (result != 0) {
        AGENTRT_LOG_ERROR("agentrt_task_set_priority: platform set_priority failed, result=%d tid=%llu priority=%d", result, (unsigned long long)tid, priority);
        release_task_lock();
        ATM_RET_ERR(AGENTRT_EINVAL);
    }

    task_info->priority = priority;

    release_task_lock();
    return AGENTRT_SUCCESS;
}

/**
 * @brief 获取任务优先级
 *
 * 获取指定任务的优先级
 *
 * @param tid 任务ID
 * @param out_priority 优先级输出
 * @return AGENTRT_SUCCESS 成功，错误码 失败
 */
agentrt_error_t agentrt_task_get_priority(agentrt_task_id_t tid, int *out_priority)
{
    if (!out_priority) {
        AGENTRT_LOG_ERROR("agentrt_task_get_priority: null output pointer");
        AGENTRT_ERROR(AGENTRT_EINVAL, "failed to get task priority: null output pointer");
    }

    task_info_core_t *task_info = find_task_by_id(tid);
    if (!task_info) {
        AGENTRT_LOG_ERROR("agentrt_task_get_priority: task not found, tid=%llu", (unsigned long long)tid);
        AGENTRT_ERROR(AGENTRT_EINVAL, "failed to get task priority: task not found");
    }

    *out_priority = task_info->priority;

    release_task_lock();
    return AGENTRT_SUCCESS;
}

/**
 * @brief 获取任务状态
 *
 * 获取指定任务的状态
 *
 * @param tid 任务ID
 * @param out_state 状态输出
 * @return AGENTRT_SUCCESS 成功，错误码 失败
 */
agentrt_error_t agentrt_task_get_state(agentrt_task_id_t tid, agentrt_task_state_t *out_state)
{
    if (!out_state) {
        AGENTRT_LOG_ERROR("agentrt_task_get_state: null output pointer");
        AGENTRT_ERROR(AGENTRT_EINVAL, "failed to get task state: null output pointer");
    }

    task_info_core_t *task_info = find_task_by_id(tid);
    if (!task_info) {
        AGENTRT_LOG_ERROR("agentrt_task_get_state: task not found, tid=%llu", (unsigned long long)tid);
        AGENTRT_ERROR(AGENTRT_EINVAL, "failed to get task state: task not found");
    }

    *out_state = task_info->state;

    release_task_lock();
    return AGENTRT_SUCCESS;
}

/* ==================== 调度器高级功能 ==================== */

agentrt_error_t agentrt_scheduler_resolve_dependencies(const uint64_t *dep_from,
                                                       const uint64_t *dep_to, size_t edge_count,
                                                       agentrt_dep_result_t *out_result)
{

    if (!dep_from || !dep_to || !out_result) {
        AGENTRT_LOG_ERROR("agentrt_scheduler_resolve_dependencies: null parameter, dep_from=%p dep_to=%p out_result=%p", (void *)dep_from, (void *)dep_to, (void *)out_result);
        AGENTRT_ERROR(AGENTRT_EINVAL, "failed to resolve dependencies: null dep_from, dep_to, or out_result");
    }

    __builtin_memset(out_result, 0, sizeof(agentrt_dep_result_t));

    if (edge_count == 0)
        return AGENTRT_SUCCESS;

    /* 收集所有唯一节点 */
    uint64_t *nodes;
    SAFE_MALLOC_ARRAY(nodes, edge_count * 2, sizeof(uint64_t));
    if (!nodes) {
        AGENTRT_LOG_ERROR("agentrt_scheduler_resolve_dependencies: nodes alloc failed, edge_count=%zu", edge_count);
        ATM_RET_ERR(AGENTRT_ENOMEM);
    }
    size_t node_count = 0;

    for (size_t i = 0; i < edge_count; i++) {
        nodes[node_count++] = dep_from[i];
        nodes[node_count++] = dep_to[i];
    }

    /* 排序去重 */
    for (size_t i = 0; i < node_count; i++) {
        for (size_t j = i + 1; j < node_count; j++) {
            if (nodes[i] > nodes[j]) {
                uint64_t tmp = nodes[i];
                nodes[i] = nodes[j];
                nodes[j] = tmp;
            }
        }
    }

    size_t unique_count = 0;
    for (size_t i = 0; i < node_count; i++) {
        if (i == 0 || nodes[i] != nodes[unique_count - 1]) {
            nodes[unique_count++] = nodes[i];
        }
    }

    /* 建图: in_degree, adjacency list, 反向邻接表 (用于循环检测路径追踪) */
    size_t *in_degree = (size_t *)AGENTRT_CALLOC(unique_count, sizeof(size_t));
    size_t **adj = (size_t **)AGENTRT_CALLOC(unique_count, sizeof(size_t *));
    size_t *adj_cap = (size_t *)AGENTRT_CALLOC(unique_count, sizeof(size_t));
    size_t *adj_cnt = (size_t *)AGENTRT_CALLOC(unique_count, sizeof(size_t));

    size_t **rev_adj = (size_t **)AGENTRT_CALLOC(unique_count, sizeof(size_t *));
    size_t *rev_cap = (size_t *)AGENTRT_CALLOC(unique_count, sizeof(size_t));
    size_t *rev_cnt = (size_t *)AGENTRT_CALLOC(unique_count, sizeof(size_t));

    if (!in_degree || !adj || !adj_cap || !adj_cnt || !rev_adj || !rev_cap || !rev_cnt) {
        AGENTRT_LOG_ERROR("agentrt_scheduler_resolve_dependencies: graph alloc failed, unique_count=%zu", unique_count);
        AGENTRT_FREE(nodes);
        AGENTRT_FREE(in_degree);
        AGENTRT_FREE(adj);
        AGENTRT_FREE(adj_cap);
        AGENTRT_FREE(adj_cnt);
        AGENTRT_FREE(rev_adj);
        AGENTRT_FREE(rev_cap);
        AGENTRT_FREE(rev_cnt);
        ATM_RET_ERR(AGENTRT_ENOMEM);
    }

    for (size_t i = 0; i < edge_count; i++) {
        size_t from_idx = unique_count, to_idx = unique_count;
        for (size_t j = 0; j < unique_count; j++) {
            if (nodes[j] == dep_from[i])
                from_idx = j;
            if (nodes[j] == dep_to[i])
                to_idx = j;
        }
        if (from_idx >= unique_count || to_idx >= unique_count) {
            goto cleanup_fail;
        }

        /* forward: from -> to */
        if (adj_cnt[from_idx] >= adj_cap[from_idx]) {
            size_t new_cap = adj_cap[from_idx] ? adj_cap[from_idx] * 2 : 4;
            size_t *new_adj = (size_t *)AGENTRT_REALLOC(adj[from_idx], new_cap * sizeof(size_t));
            if (!new_adj) {
                AGENTRT_LOG_ERROR("agentrt_scheduler_resolve_dependencies: adj realloc failed, from_idx=%zu new_cap=%zu", from_idx, new_cap);
                goto cleanup_oom;
            }
            adj[from_idx] = new_adj;
            adj_cap[from_idx] = new_cap;
        }
        adj[from_idx][adj_cnt[from_idx]++] = to_idx;
        in_degree[to_idx]++;

        /* reverse: to -> from (for cycle backtracking) */
        if (rev_cnt[to_idx] >= rev_cap[to_idx]) {
            size_t new_cap = rev_cap[to_idx] ? rev_cap[to_idx] * 2 : 4;
            size_t *new_rev = (size_t *)AGENTRT_REALLOC(rev_adj[to_idx], new_cap * sizeof(size_t));
            if (!new_rev) {
                AGENTRT_LOG_ERROR("agentrt_scheduler_resolve_dependencies: rev_adj realloc failed, to_idx=%zu new_cap=%zu", to_idx, new_cap);
                goto cleanup_oom;
            }
            rev_adj[to_idx] = new_rev;
            rev_cap[to_idx] = new_cap;
        }
        rev_adj[to_idx][rev_cnt[to_idx]++] = from_idx;
    }

    /* Kahn 拓扑排序 + 循环参与者追踪 */
    size_t *queue;
    SAFE_MALLOC_ARRAY(queue, unique_count, sizeof(size_t));
    if (!queue) {
        AGENTRT_LOG_ERROR("agentrt_scheduler_resolve_dependencies: queue alloc failed, unique_count=%zu", unique_count);
        goto cleanup_oom;
    }

    size_t *in_degree_copy;
    SAFE_MALLOC_ARRAY(in_degree_copy, unique_count, sizeof(size_t));
    if (!in_degree_copy) {
        AGENTRT_LOG_ERROR("agentrt_scheduler_resolve_dependencies: in_degree_copy alloc failed, unique_count=%zu", unique_count);
        AGENTRT_FREE(queue);
        goto cleanup_oom;
    }
    __builtin_memcpy(in_degree_copy, in_degree, unique_count * sizeof(size_t));

    size_t q_head = 0, q_tail = 0;
    for (size_t i = 0; i < unique_count; i++) {
        if (in_degree_copy[i] == 0)
            queue[q_tail++] = i;
    }

    SAFE_MALLOC_ARRAY(out_result->sorted_tasks, unique_count, sizeof(uint64_t));
    if (!out_result->sorted_tasks) {
        AGENTRT_LOG_ERROR("agentrt_scheduler_resolve_dependencies: sorted_tasks alloc failed");
        AGENTRT_FREE(queue);
        AGENTRT_FREE(in_degree_copy);
        goto cleanup_oom;
    }

    size_t sorted_cnt = 0;
    while (q_head < q_tail) {
        size_t u = queue[q_head++];
        out_result->sorted_tasks[sorted_cnt++] = nodes[u];

        for (size_t j = 0; j < adj_cnt[u]; j++) {
            size_t v = adj[u][j];
            in_degree_copy[v]--;
            if (in_degree_copy[v] == 0)
                queue[q_tail++] = v;
        }
    }

    out_result->sorted_count = sorted_cnt;
    AGENTRT_FREE(queue);
    queue = NULL;

    /* 循环检测: 若 sorted_cnt < unique_count，收集参与循环的节点 */
    if (sorted_cnt < unique_count) {
        /* in_degree_copy 仍在手: 入度 > 0 的节点即循环参与者 */
        size_t cycle_count = 0;
        for (size_t i = 0; i < unique_count; i++) {
            if (in_degree_copy[i] > 0)
                cycle_count++;
        }

        if (cycle_count > 0) {
            out_result->cycle =
                (agentrt_cycle_report_t *)AGENTRT_CALLOC(1, sizeof(agentrt_cycle_report_t));
            if (out_result->cycle) {
                SAFE_MALLOC_ARRAY(out_result->cycle->cycle_nodes, cycle_count, sizeof(uint64_t));
                if (out_result->cycle->cycle_nodes) {
                    out_result->cycle->cycle_node_count = cycle_count;
                    size_t ci = 0;
                    for (size_t i = 0; i < unique_count; i++) {
                        if (in_degree_copy[i] > 0)
                            out_result->cycle->cycle_nodes[ci++] = nodes[i];
                    }

                    char desc_buf[256];
                    int dl = snprintf(desc_buf, sizeof(desc_buf),
                                      "Cycle detected: %zu nodes in dependency loop", cycle_count);
                    out_result->cycle->description = (char *)AGENTRT_MALLOC((size_t)dl + 1);
                    if (out_result->cycle->description) {
                        __builtin_memcpy(out_result->cycle->description, desc_buf, (size_t)dl);
                        out_result->cycle->description[dl] = '\0';
                        out_result->cycle->description_len = (size_t)dl;
                    }
                }
            }
        }
        AGENTRT_FREE(in_degree_copy);
        in_degree_copy = NULL;

        /* 释放排序结果（循环时无有效排序） */
        AGENTRT_FREE(out_result->sorted_tasks);
        out_result->sorted_tasks = NULL;
        out_result->sorted_count = 0;

        /* 清理并返回循环错误 */
        AGENTRT_LOG_ERROR("agentrt_scheduler_resolve_dependencies: dependency cycle detected, cycle_nodes=%zu total_nodes=%zu", cycle_count, unique_count);
        for (size_t k = 0; k < unique_count; k++)
            AGENTRT_FREE(adj[k]);
        AGENTRT_FREE(adj);
        AGENTRT_FREE(adj_cap);
        AGENTRT_FREE(adj_cnt);
        for (size_t k = 0; k < unique_count; k++)
            AGENTRT_FREE(rev_adj[k]);
        AGENTRT_FREE(rev_adj);
        AGENTRT_FREE(rev_cap);
        AGENTRT_FREE(rev_cnt);
        AGENTRT_FREE(nodes);
        AGENTRT_FREE(in_degree);
        ATM_RET_ERR(AGENTRT_ECYCLE);
    }

    /* 优先级组继承: 对于每条边 from->to，将 from 在排序中的位置优先级传递 */
    SAFE_MALLOC_ARRAY(out_result->inherited_priorities, sorted_cnt, sizeof(int));
    if (out_result->inherited_priorities) {
        for (size_t i = 0; i < sorted_cnt; i++)
            out_result->inherited_priorities[i] = AGENTRT_TASK_PRIORITY_NORMAL;

        /* 从后向前遍历排序列表，传递优先级（依赖者继承被依赖者的优先级） */
        int *base_prio;
        SAFE_MALLOC_ARRAY(base_prio, sorted_cnt, sizeof(int));
        if (base_prio) {
            for (size_t i = 0; i < sorted_cnt; i++)
                base_prio[i] = AGENTRT_TASK_PRIORITY_NORMAL;
            for (size_t i = 0; i < edge_count; i++) {
                size_t from_pos = sorted_cnt, to_pos = sorted_cnt;
                for (size_t j = 0; j < sorted_cnt; j++) {
                    if (out_result->sorted_tasks[j] == dep_from[i])
                        from_pos = j;
                    if (out_result->sorted_tasks[j] == dep_to[i])
                        to_pos = j;
                }
                /* from depends on to, so from should inherit to's priority if higher */
                if (from_pos < sorted_cnt && to_pos < sorted_cnt) {
                    if (base_prio[to_pos] > base_prio[from_pos])
                        base_prio[from_pos] = base_prio[to_pos];
                }
            }
            __builtin_memcpy(out_result->inherited_priorities, base_prio, sorted_cnt * sizeof(int));
            AGENTRT_FREE(base_prio);
            base_prio = NULL;
        }
        out_result->priority_count = sorted_cnt;
    }

    /* 清理内存 */
    for (size_t k = 0; k < unique_count; k++)
        AGENTRT_FREE(adj[k]);
    AGENTRT_FREE(adj);
    AGENTRT_FREE(adj_cap);
    AGENTRT_FREE(adj_cnt);
    for (size_t k = 0; k < unique_count; k++)
        AGENTRT_FREE(rev_adj[k]);
    AGENTRT_FREE(rev_adj);
    AGENTRT_FREE(rev_cap);
    AGENTRT_FREE(rev_cnt);
    AGENTRT_FREE(nodes);
    AGENTRT_FREE(in_degree);

    return AGENTRT_SUCCESS;

cleanup_oom:
    AGENTRT_LOG_ERROR("agentrt_scheduler_resolve_dependencies: OOM during graph processing, unique_count=%zu", unique_count);
    for (size_t k = 0; k < unique_count; k++)
        AGENTRT_FREE(adj[k]);
    AGENTRT_FREE(adj);
    AGENTRT_FREE(adj_cap);
    AGENTRT_FREE(adj_cnt);
    for (size_t k = 0; k < unique_count; k++)
        AGENTRT_FREE(rev_adj[k]);
    AGENTRT_FREE(rev_adj);
    AGENTRT_FREE(rev_cap);
    AGENTRT_FREE(rev_cnt);
    AGENTRT_FREE(nodes);
    AGENTRT_FREE(in_degree);
    ATM_RET_ERR(AGENTRT_ENOMEM);

cleanup_fail:
    AGENTRT_LOG_ERROR("agentrt_scheduler_resolve_dependencies: invalid edge index during graph processing");
    for (size_t k = 0; k < unique_count; k++)
        AGENTRT_FREE(adj[k]);
    AGENTRT_FREE(adj);
    AGENTRT_FREE(adj_cap);
    AGENTRT_FREE(adj_cnt);
    for (size_t k = 0; k < unique_count; k++)
        AGENTRT_FREE(rev_adj[k]);
    AGENTRT_FREE(rev_adj);
    AGENTRT_FREE(rev_cap);
    AGENTRT_FREE(rev_cnt);
    AGENTRT_FREE(nodes);
    AGENTRT_FREE(in_degree);
    ATM_RET_ERR(AGENTRT_EINVAL);
}

void agentrt_scheduler_dep_result_free(agentrt_dep_result_t *result)
{
    if (!result)
        return;
    if (result->sorted_tasks)
        AGENTRT_FREE(result->sorted_tasks);
    if (result->inherited_priorities)
        AGENTRT_FREE(result->inherited_priorities);
    if (result->cycle) {
        if (result->cycle->cycle_nodes)
            AGENTRT_FREE(result->cycle->cycle_nodes);
        if (result->cycle->description)
            AGENTRT_FREE(result->cycle->description);
        AGENTRT_FREE(result->cycle);
    }
    __builtin_memset(result, 0, sizeof(agentrt_dep_result_t));
}

agentrt_error_t agentrt_scheduler_priority_inherit(agentrt_task_id_t blocking_task_id,
                                                   agentrt_task_id_t blocked_task_id)
{
    if (blocking_task_id == 0 || blocked_task_id == 0) {
        AGENTRT_LOG_ERROR("agentrt_scheduler_priority_inherit: null task id, blocking=%llu blocked=%llu", (unsigned long long)blocking_task_id, (unsigned long long)blocked_task_id);
        ATM_RET_ERR(AGENTRT_EINVAL);
    }

    /* task_table_lock 由 agentrt_mutex_create() 创建，是默认非递归锁
     * （pthread_mutex_init(NULL)），不能重入加锁。本函数只加锁一次，
     * 在锁内通过 scheduler_core_hash_find（纯查找，不加锁）查找两个任务，
     * 避免旧实现两次调用 find_task_by_id 对同一非递归锁重入加锁：
     *   - 成功路径：第二次 find_task_by_id 重入加锁 → 死锁/UB
     *   - blocked_task 未找到的错误路径：只释放一次锁，blocking_task 的锁泄漏
     * 旧实现末尾两次 release_task_lock() 是对上述错误假设的补偿，但无法
     * 掩盖重入死锁的根本问题。 */
    scheduler_core_ctx_t *ctx = scheduler_core_get_ctx();
    if (!ctx) {
        AGENTRT_LOG_ERROR("agentrt_scheduler_priority_inherit: null scheduler context");
        ATM_RET_ERR(AGENTRT_EINVAL);
    }

    agentrt_mutex_lock(ctx->task_table_lock);

    task_info_core_t *blocking_task = scheduler_core_hash_find(blocking_task_id);
    if (!blocking_task) {
        AGENTRT_LOG_ERROR("agentrt_scheduler_priority_inherit: blocking task not found, tid=%llu", (unsigned long long)blocking_task_id);
        agentrt_mutex_unlock(ctx->task_table_lock);
        ATM_RET_ERR(AGENTRT_EINVAL);
    }

    task_info_core_t *blocked_task = scheduler_core_hash_find(blocked_task_id);
    if (!blocked_task) {
        AGENTRT_LOG_ERROR("agentrt_scheduler_priority_inherit: blocked task not found, tid=%llu", (unsigned long long)blocked_task_id);
        agentrt_mutex_unlock(ctx->task_table_lock);
        ATM_RET_ERR(AGENTRT_EINVAL);
    }

    int new_priority = blocking_task->priority;
    if (blocked_task->priority > new_priority) {
        new_priority = blocked_task->priority;
    }

    if (new_priority > AGENTRT_TASK_PRIORITY_MAX)
        new_priority = AGENTRT_TASK_PRIORITY_MAX;

    if (new_priority != blocking_task->priority) {
        const scheduler_platform_ops_t *ops = scheduler_platform_get_ops();
        if (ops && ops->thread_set_priority) {
            ops->thread_set_priority(blocking_task->platform_handle, new_priority);
        }
        blocking_task->priority = new_priority;
    }

    agentrt_mutex_unlock(ctx->task_table_lock);
    return AGENTRT_SUCCESS;
}

agentrt_error_t agentrt_scheduler_resource_reserve(size_t est_memory_kb, int est_cpu_cores)
{

    if (est_cpu_cores < 1)
        est_cpu_cores = 1;

    size_t avail_mem_kb = 0;
    int avail_cpu_cores = 1;

#ifdef _WIN32
    MEMORYSTATUSEX mem_status;
    mem_status.dwLength = sizeof(mem_status);
    if (GlobalMemoryStatusEx(&mem_status)) {
        avail_mem_kb = (size_t)(mem_status.ullAvailPhys / 1024);
    }
    SYSTEM_INFO sys_info;
    GetSystemInfo(&sys_info);
    avail_cpu_cores = (int)sys_info.dwNumberOfProcessors;
#elif defined(__APPLE__)
    long phys_pages = sysconf(_SC_PHYS_PAGES);
    long page_size = sysconf(_SC_PAGESIZE);
    if (phys_pages > 0 && page_size > 0) {
        avail_mem_kb = (size_t)((phys_pages * page_size) / 1024);
    }
    long nproc = sysconf(_SC_NPROCESSORS_ONLN);
    avail_cpu_cores = (int)(nproc > 0 ? nproc : 1);
#else
    long phys_pages = sysconf(_SC_AVPHYS_PAGES);
    long page_size = sysconf(_SC_PAGESIZE);
    if (phys_pages > 0 && page_size > 0) {
        avail_mem_kb = (size_t)((phys_pages * page_size) / 1024);
    }
    long nproc = sysconf(_SC_NPROCESSORS_ONLN);
    avail_cpu_cores = (int)(nproc > 0 ? nproc : 1);
#endif

    size_t reserved_margin_kb = avail_mem_kb / 10;
    if (avail_mem_kb > reserved_margin_kb) {
        avail_mem_kb -= reserved_margin_kb;
    } else {
        avail_mem_kb = 0;
    }

    if (est_memory_kb > avail_mem_kb) {
        AGENTRT_LOG_WARN("agentrt_scheduler_resource_reserve: memory insufficient, est=%zuKB avail=%zuKB", est_memory_kb, avail_mem_kb);
        ATM_RET_ERR(AGENTRT_ERESOURCE);
    }

    if (est_cpu_cores > avail_cpu_cores) {
        AGENTRT_LOG_WARN("agentrt_scheduler_resource_reserve: cpu cores insufficient, est=%d avail=%d", est_cpu_cores, avail_cpu_cores);
        ATM_RET_ERR(AGENTRT_ERESOURCE);
    }

    return AGENTRT_SUCCESS;
}

void agentrt_task_cleanup(void)
{
    scheduler_core_ctx_t *ctx = scheduler_core_get_ctx();
    if (!ctx || !scheduler_core_is_initialized()) {
        return;
    }

    const scheduler_platform_ops_t *ops = scheduler_platform_get_ops();

    agentrt_mutex_lock(ctx->task_table_lock);

    for (uint32_t i = 0; i < ctx->task_count; i++) {
        task_info_core_t *info = ctx->task_table[i];
        if (!info)
            continue;

        if (ops && ops->cleanup_platform_resources && info->platform_handle) {
            ops->cleanup_platform_resources(info->platform_handle, info->platform_data);
        }

        scheduler_core_task_info_destroy(info);
        ctx->task_table[i] = NULL;
    }

    for (size_t b = 0; b < HASH_TABLE_BUCKETS; b++) {
        task_hash_node_t *node = ctx->id_hash_table[b];
        while (node) {
            task_hash_node_t *next = node->next;
            AGENTRT_FREE(node);
            node = next;
        }
        ctx->id_hash_table[b] = NULL;
    }

    ctx->task_count = 0;

    agentrt_mutex_unlock(ctx->task_table_lock);

    if (ops && ops->cleanup) {
        ops->cleanup();
    }

    scheduler_core_destroy();
}
