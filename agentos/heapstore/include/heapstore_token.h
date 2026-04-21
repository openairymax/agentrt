/**
 * @file heapstore_token.h
 * @brief AgentOS heapstore Token 计数接口
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * "From data intelligence emerges."
 *
 * @note 本模块实现 Token 使用统计和监控功能，
 *       符合 ARCHITECTURAL_PRINCIPLES.md 中的 E-2 可观测性原则。
 */

#ifndef AGENTOS_HEAPSTORE_TOKEN_H
#define AGENTOS_HEAPSTORE_TOKEN_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "heapstore.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Token 统计数据类型
 */
typedef enum {
    HEAPSTORE_TOKEN_TYPE_PROMPT = 0,
    HEAPSTORE_TOKEN_TYPE_COMPLETION = 1,
    HEAPSTORE_TOKEN_TYPE_SYSTEM = 2,
    HEAPSTORE_TOKEN_TYPE_USER = 3,
    HEAPSTORE_TOKEN_TYPE_CACHE_HIT = 4,
    HEAPSTORE_TOKEN_TYPE_TOTAL = 5
} heapstore_token_type_t;

/**
 * @brief Token 操作类型
 */
typedef enum {
    HEAPSTORE_TOKEN_OP_WRITE = 0,
    HEAPSTORE_TOKEN_OP_READ = 1,
    HEAPSTORE_TOKEN_OP_BATCH = 2
} heapstore_token_operation_t;

/**
 * @brief Token 统计数据结构
 */
typedef struct {
    uint64_t total_prompt_tokens;         /**< 总 Prompt Token 数 */
    uint64_t total_completion_tokens;    /**< 总 Completion Token 数 */
    uint64_t total_system_tokens;        /**< 总 System Token 数 */
    uint64_t total_user_tokens;          /**< 总 User Token 数 */
    uint64_t tokens_saved_by_cache;      /**< 缓存节省的 Token 数 */
    uint64_t total_write_operations;    /**< 总写入操作次数 */
    uint64_t total_read_operations;     /**< 总读取操作次数 */
    uint64_t total_batch_operations;    /**< 总批量操作次数 */
    uint64_t last_operation_time;        /**< 上次操作时间戳 */
    double average_tokens_per_operation; /**< 平均每次操作的 Token 数 */
} heapstore_token_stats_t;

/**
 * @brief Token 预算配置
 */
typedef struct {
    uint64_t max_tokens_per_task;       /**< 任务最大 Token 数 */
    uint64_t warning_threshold_percent; /**< 警告阈值百分比 */
    uint64_t critical_threshold_percent; /**< 临界阈值百分比 */
    bool enable_budget_enforcement;       /**< 是否强制执行预算 */
} heapstore_token_budget_t;

/**
 * @brief 初始化 Token 计数器
 *
 * @return heapstore_error_t 错误码
 *
 * @threadsafe 是
 * @reentrant 是
 *
 * @see heapstore_token_shutdown()
 * @since v1.0.0.6
 */
heapstore_error_t heapstore_token_init(void);

/**
 * @brief 关闭 Token 计数器
 *
 * @return heapstore_error_t 错误码
 *
 * @threadsafe 是
 * @reentrant 是
 *
 * @see heapstore_token_init()
 * @since v1.0.0.6
 */
heapstore_error_t heapstore_token_shutdown(void);

/**
 * @brief 记录 Token 使用
 *
 * @param type [in] Token 类型
 * @param count [in] Token 数量
 * @param operation [in] 操作类型
 * @return heapstore_error_t 错误码
 *
 * @threadsafe 是
 * @reentrant 是
 *
 * @see heapstore_token_get_stats()
 * @since v1.0.0.6
 */
heapstore_error_t heapstore_token_record(
    heapstore_token_type_t type,
    uint64_t count,
    heapstore_token_operation_t operation);

/**
 * @brief 获取 Token 统计信息
 *
 * @param out_stats [out] 输出统计信息
 * @return heapstore_error_t 错误码
 *
 * @threadsafe 是
 * @reentrant 是
 *
 * @see heapstore_token_record()
 * @since v1.0.0.6
 */
heapstore_error_t heapstore_token_get_stats(heapstore_token_stats_t* out_stats);

/**
 * @brief 重置 Token 统计
 *
 * @return heapstore_error_t 错误码
 *
 * @threadsafe 是
 * @reentrant 是
 */
heapstore_error_t heapstore_token_reset_stats(void);

/**
 * @brief 设置任务 Token 预算
 *
 * @param task_id [in] 任务 ID
 * @param budget [in] 预算配置
 * @return heapstore_error_t 错误码
 *
 * @threadsafe 是
 * @reentrant 是
 *
 * @see heapstore_token_check_budget()
 * @since v1.0.0.6
 */
heapstore_error_t heapstore_token_set_budget(
    const char* task_id,
    const heapstore_token_budget_t* budget);

/**
 * @brief 检查任务 Token 预算
 *
 * @param task_id [in] 任务 ID
 * @param requested_tokens [in] 请求的 Token 数
 * @param allowed [out] 是否允许
 * @return heapstore_error_t 错误码
 *
 * @threadsafe 是
 * @reentrant 是
 *
 * @see heapstore_token_set_budget()
 * @since v1.0.0.6
 */
heapstore_error_t heapstore_token_check_budget(
    const char* task_id,
    uint64_t requested_tokens,
    bool* allowed);

/**
 * @brief 获取任务已使用的 Token 数
 *
 * @param task_id [in] 任务 ID
 * @param out_used [out] 已使用的 Token 数
 * @return heapstore_error_t 错误码
 *
 * @threadsafe 是
 * @reentrant 是
 */
heapstore_error_t heapstore_token_get_task_usage(
    const char* task_id,
    uint64_t* out_used);

/**
 * @brief 将 Token 类型转换为字符串
 *
 * @param type [in] Token 类型
 * @return const char* 类型字符串
 *
 * @threadsafe 是
 * @reentrant 是
 */
const char* heapstore_token_type_to_string(heapstore_token_type_t type);

/**
 * @brief 将 Token 操作转换为字符串
 *
 * @param operation [in] 操作类型
 * @return const char* 操作字符串
 *
 * @threadsafe 是
 * @reentrant 是
 */
const char* heapstore_token_op_to_string(heapstore_token_operation_t operation);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_HEAPSTORE_TOKEN_H */
