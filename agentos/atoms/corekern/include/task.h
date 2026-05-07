/**
 * @file task.h
 * @brief 任务调度接口（基于系统原生线程）
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * "From data intelligence emerges."
 *
 * @note 提供基于系统原生线程的任务调度功能
 * 
 * @note 线程同步原语（mutex, cond, thread）由 platform.h 提供，
 *       本文件仅提供任务调度相关的扩展功能
 */

#ifndef AGENTOS_TASK_H
#define AGENTOS_TASK_H

#include <stdint.h>
#include <stddef.h>
#include "error.h"
#include "export.h"
/* 统一类型定义：使用commons作为权威基础库 */
#include "../../../commons/include/agentos_types.h"
/* 线程同步原语由platform.h提供 */
#include "../../../commons/platform/include/platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 任务优先级常量
 */
#define AGENTOS_TASK_PRIORITY_MIN     0   /**< 最低优先级 */
#define AGENTOS_TASK_PRIORITY_LOW     25  /**< 低优先级 */
#define AGENTOS_TASK_PRIORITY_NORMAL  50  /**< 普通优先级 */
#define AGENTOS_TASK_PRIORITY_HIGH    75  /**< 高优先级 */
#define AGENTOS_TASK_PRIORITY_MAX     100 /**< 最高优先级 */

/**
 * @brief 任务状态枚举
 */
typedef enum {
    AGENTOS_TASK_STATE_CREATED,   /**< 已创建 */
    AGENTOS_TASK_STATE_READY,     /**< 就绪 */
    AGENTOS_TASK_STATE_RUNNING,    /**< 运行中 */
    AGENTOS_TASK_STATE_BLOCKED,    /**< 阻塞 */
    AGENTOS_TASK_STATE_TERMINATED  /**< 已终止 */
} agentos_task_state_t;

/**
 * @brief 线程属性结构
 */
typedef struct {
    const char* name;      /**< 线程名称 */
    int priority;          /**< 线程优先级 */
    size_t stack_size;    /**< 栈大小 */
    int detach_state;     /**< 分离状态 */
} agentos_thread_attr_t;

/* ==================== 任务调度核心接口 ==================== */

/**
 * @brief 初始化任务调度系统
 *
 * @return agentos_error_t 错误码
 *
 * @ownership 内部管理所有任务调度资源
 * @threadsafe 否
 * @reentrant 否
 *
 * @see agentos_task_cleanup()
 */
AGENTOS_API agentos_error_t agentos_task_init(void);

/**
 * @brief 获取当前任务 ID
 *
 * @return agentos_task_id_t 当前任务 ID
 *
 * @threadsafe 是
 * @reentrant 是
 */
AGENTOS_API agentos_task_id_t agentos_task_self(void);

/**
 * @brief 任务休眠
 *
 * @param ms [in] 休眠时间（毫秒）
 *
 * @threadsafe 是
 * @reentrant 是
 */
AGENTOS_API void agentos_task_sleep(uint32_t ms);

/**
 * @brief 让出 CPU 时间片
 *
 * @threadsafe 是
 * @reentrant 是
 */
AGENTOS_API void agentos_task_yield(void);

/**
 * @brief 设置任务优先级
 *
 * @param tid [in] 任务 ID
 * @param priority [in] 优先级（0-100）
 * @return agentos_error_t 错误码
 *
 * @threadsafe 是
 * @reentrant 否
 */
AGENTOS_API agentos_error_t agentos_task_set_priority(agentos_task_id_t tid, int priority);

/**
 * @brief 获取任务优先级
 *
 * @param tid [in] 任务 ID
 * @param out_priority [out] 输出优先级
 * @return agentos_error_t 错误码
 *
 * @ownership 调用者负责 out_priority 的分配和释放
 * @threadsafe 是
 * @reentrant 否
 */
AGENTOS_API agentos_error_t agentos_task_get_priority(agentos_task_id_t tid, int* out_priority);

/**
 * @brief 获取任务状态
 *
 * @param tid [in] 任务 ID
 * @param out_state [out] 输出任务状态
 * @return agentos_error_t 错误码
 *
 * @ownership 调用者负责 out_state 的分配和释放
 * @threadsafe 是
 * @reentrant 否
 */
AGENTOS_API agentos_error_t agentos_task_get_state(agentos_task_id_t tid, agentos_task_state_t* out_state);

/**
 * @brief 清理任务调度系统
 *
 * @threadsafe 否
 * @reentrant 否
 *
 * @see agentos_task_init()
 */
AGENTOS_API void agentos_task_cleanup(void);

/* ==================== 依赖解析增强接口 (IMP-A1) ==================== */

/**
 * @brief 循环依赖检测报告结构
 */
typedef struct agentos_cycle_report {
    uint64_t* cycle_nodes;    /**< 参与循环的节点ID数组 */
    size_t cycle_node_count;  /**< 参与循环的节点数量 */
    char* description;        /**< 可读循环描述 */
    size_t description_len;   /**< 描述长度 */
} agentos_cycle_report_t;

/**
 * @brief 依赖解析结果（增强版）
 */
typedef struct agentos_dep_result {
    uint64_t* sorted_tasks;   /**< 拓扑排序后的任务ID数组 */
    size_t sorted_count;      /**< 排序后的任务数量 */
    int* inherited_priorities; /**< 优先级继承结果 (与sorted_tasks对应) */
    size_t priority_count;    /**< 优先级数组大小 */
    agentos_cycle_report_t* cycle; /**< 循环依赖报告（无循环时为NULL） */
} agentos_dep_result_t;

/**
 * @brief 解析任务依赖关系（拓扑排序 + 循环检测）
 *
 * @param dep_from [in] 依赖源数组 (from depends on to)
 * @param dep_to [in] 依赖目标数组
 * @param edge_count [in] 边的数量
 * @param out_result [out] 解析结果，调用者负责释放
 * @return agentos_error_t AGENTOS_SUCCESS 成功，AGENTOS_ECYCLE 存在循环依赖
 *
 * @ownership out_result 由调用者负责通过 agentos_scheduler_dep_result_free() 释放
 * @threadsafe 否
 */
AGENTOS_API agentos_error_t agentos_scheduler_resolve_dependencies(
    const uint64_t* dep_from, const uint64_t* dep_to, size_t edge_count,
    agentos_dep_result_t* out_result);

/**
 * @brief 释放依赖解析结果
 * @param result [in] 要释放的结果
 */
AGENTOS_API void agentos_scheduler_dep_result_free(agentos_dep_result_t* result);

/**
 * @brief 优先级继承 — 将高优先级任务传递给其依赖链
 *
 * @param blocking_task_id [in] 被阻塞的高优先级任务ID
 * @param blocked_task_id [in] 正在阻塞其他任务的任务ID
 * @return agentos_error_t
 */
AGENTOS_API agentos_error_t agentos_scheduler_priority_inherit(
    agentos_task_id_t blocking_task_id, agentos_task_id_t blocked_task_id);

/**
 * @brief 资源预留检查
 *
 * @param est_memory_kb [in] 预估内存需求(KB)
 * @param est_cpu_cores [in] 预估CPU核心数
 * @return agentos_error_t AGENTOS_SUCCESS 资源充足，AGENTOS_ERESOURCE 资源不足
 */
AGENTOS_API agentos_error_t agentos_scheduler_resource_reserve(
    size_t est_memory_kb, int est_cpu_cores);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_TASK_H */
