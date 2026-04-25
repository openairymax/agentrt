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

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_TASK_H */
