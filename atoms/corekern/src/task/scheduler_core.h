/**
 * @file scheduler_core.h
 * @brief 调度器核心层 - 平台无关的任务管理
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * @details
 * 本模块提供平台无关的调度器核心功能：
 * - 任务信息管理
 * - 哈希表索引优化
 * - 原子操作封装
 * - 资源管理
 *
 * 平台特定功能通过适配器接口实现。
 */

#ifndef AGENTOS_SCHEDULER_CORE_H
#define AGENTOS_SCHEDULER_CORE_H

#include "task.h"
#include <stddef.h>
#include <stdatomic.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 配置常量 ==================== */

/** @brief 任务表最大容量 */
#define TASK_TABLE_CAPACITY 4096

/** @brief 哈希表桶数量（质数，减少冲突） */
#define HASH_TABLE_BUCKETS 4093

/* ==================== 核心数据结构 ==================== */

/**
 * @brief 平台无关任务信息结构
 *
 * 包含平台无关的任务信息和指向平台特定数据的指针。
 */
typedef struct task_info_core {
    /** @brief 任务ID，全局唯一 */
    agentos_task_id_t id;

    /** @brief 任务名称，用于调试 */
    char name[64];

    /** @brief 任务优先级 */
    int priority;

    /** @brief 任务状态 */
    volatile agentos_task_state_t state;

    /** @brief 线程入口函数 */
    void* (*entry)(void*);

    /** @brief 线程参数 */
    void* arg;

    /** @brief 线程返回值 */
    void* retval;

    /** @brief 平台特定句柄（由适配器管理） */
    void* platform_handle;

    /** @brief 平台特定数据（由适配器管理） */
    void* platform_data;
} task_info_core_t;

/**
 * @brief 哈希表节点
 */
typedef struct task_hash_node {
    struct task_hash_node* next;
    agentos_task_id_t id;
    task_info_core_t* task_info;
} task_hash_node_t;

/* ==================== 核心管理结构 ==================== */

/**
 * @brief 调度器核心上下文
 */
typedef struct scheduler_core_ctx {
    /** @brief 任务信息数组 */
    task_info_core_t* task_table[TASK_TABLE_CAPACITY];

    /** @brief 当前任务数量 */
    uint32_t task_count;

    /** @brief 任务表互斥锁 */
    void* task_table_lock;  /* 平台无关锁句柄 */

    /** @brief 初始化状态标志 */
    _Atomic int initialized;

    /** @brief 下一个任务ID */
    volatile uint64_t next_task_id;

    /** @brief ID到任务信息的哈希索引 */
    task_hash_node_t* id_hash_table[HASH_TABLE_BUCKETS];
} scheduler_core_ctx_t;

/* ==================== 核心API声明 ==================== */

/**
 * @brief 获取调度器核心上下文（单例模式）
 * @return 核心上下文指针，失败返回NULL
 */
scheduler_core_ctx_t* scheduler_core_get_ctx(void);

/**
 * @brief 初始化调度器核心
 * @return 0 成功，-1 失败
 *
 * @note 线程安全，使用双重检查锁模式
 */
int scheduler_core_init(void);

/**
 * @brief 销毁调度器核心
 *
 * @note 清理所有核心资源，但不清除平台适配器资源
 */
void scheduler_core_destroy(void);

/**
 * @brief 检查调度器是否已初始化
 * @return 1 已初始化，0 未初始化
 */
int scheduler_core_is_initialized(void);

/**
 * @brief 原子获取并递增任务ID
 * @return 新任务ID
 */
uint64_t scheduler_core_fetch_add_task_id(void);

/**
 * @brief 简单哈希函数
 * @param id 任务ID
 * @return 哈希桶索引
 */
static inline size_t task_hash_core(agentos_task_id_t id) {
    return (size_t)(id % HASH_TABLE_BUCKETS);
}

/**
 * @brief 向哈希表插入任务
 * @param id 任务ID
 * @param info 任务信息指针
 *
 * @note 调用者必须确保已持有适当的锁
 */
void scheduler_core_hash_insert(agentos_task_id_t id, task_info_core_t* info);

/**
 * @brief 从哈希表查找任务
 * @param id 任务ID
 * @return 任务信息指针，未找到返回NULL
 *
 * @note 调用者必须确保已持有适当的锁
 */
task_info_core_t* scheduler_core_hash_find(agentos_task_id_t id);

/**
 * @brief 从哈希表移除任务
 * @param id 任务ID
 *
 * @note 调用者必须确保已持有适当的锁
 */
void scheduler_core_hash_remove(agentos_task_id_t id);

/**
 * @brief 创建核心任务信息结构
 * @param id 任务ID
 * @param entry 线程入口函数
 * @param arg 线程参数
 * @param name 任务名称（可为NULL）
 * @param priority 任务优先级
 * @return 任务信息指针，失败返回NULL
 */
task_info_core_t* scheduler_core_task_info_create(
    agentos_task_id_t id,
    void* (*entry)(void*),
    void* arg,
    const char* name,
    int priority);

/**
 * @brief 销毁核心任务信息结构
 * @param info 任务信息指针
 *
 * @note 只销毁核心部分，平台特定部分由适配器清理
 */
void scheduler_core_task_info_destroy(task_info_core_t* info);

/**
 * @brief 将任务添加到任务表
 * @param info 任务信息指针
 * @return 0 成功，-1 失败（表已满）
 *
 * @note 调用者必须确保已持有适当的锁
 */
int scheduler_core_task_table_add(task_info_core_t* info);

/**
 * @brief 从任务表移除任务
 * @param id 任务ID
 * @return 被移除的任务信息指针，未找到返回NULL
 *
 * @note 调用者必须确保已持有适当的锁
 */
task_info_core_t* scheduler_core_task_table_remove(agentos_task_id_t id);

/**
 * @brief 通过平台句柄查找任务
 * @param platform_handle 平台特定句柄
 * @return 任务信息指针，未找到返回NULL
 *
 * @note 调用者必须确保已持有适当的锁
 */
task_info_core_t* scheduler_core_find_by_platform_handle(void* platform_handle);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_SCHEDULER_CORE_H */
