/**
 * @file checkpoint.h
 * @brief AgentOS 任务检查点接口
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * "From data intelligence emerges."
 *
 * @note 本模块实现任务检查点机制，支持长时间任务（1000小时+）的
 *       状态保存和恢复，符合 ARCHITECTURAL_PRINCIPLES.md 中的
 *       S-1 反馈闭环原则和 C-2 增量演化原则。
 */

#ifndef AGENTOS_ATOMS_CHECKPOINT_H
#define AGENTOS_ATOMS_CHECKPOINT_H

#include "../../../../agentos/commons/utils/error/include/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 检查点状态
 */
typedef enum {
    CHECKPOINT_STATE_PENDING = 0,
    CHECKPOINT_STATE_COMPLETED = 1,
    CHECKPOINT_STATE_FAILED = 2,
    CHECKPOINT_STATE_INVALID = 3
} agentos_checkpoint_state_t;

/**
 * @brief 任务检查点结构
 *
 * 包含任务执行状态的完整快照，支持断点续传和故障恢复。
 */
typedef struct agentos_task_checkpoint {
    char task_id[128];                  /**< 任务唯一标识符 */
    char session_id[128];                /**< 会话唯一标识符 */
    uint64_t sequence_num;              /**< 检查点序列号 */
    uint64_t timestamp;                 /**< 创建时间戳（纳秒） */
    char* state_json;                   /**< 状态快照（JSON格式） */
    size_t state_size;                  /**< 状态数据大小 */
    char** completed_nodes;             /**< 已完成节点列表 */
    size_t completed_count;             /**< 已完成节点数量 */
    char** pending_nodes;               /**< 待执行节点列表 */
    size_t pending_count;               /**< 待执行节点数量 */
    agentos_checkpoint_state_t state;   /**< 检查点状态 */
    uint32_t checksum;                 /**< 数据校验和 */
    char metadata[512];                 /**< 额外元数据 */
} agentos_task_checkpoint_t;

/**
 * @brief 检查点统计信息
 */
typedef struct agentos_checkpoint_stats {
    uint64_t total_checkpoints;         /**< 总检查点数量 */
    uint64_t successful_checkpoints;    /**< 成功检查点数量 */
    uint64_t failed_checkpoints;       /**< 失败检查点数量 */
    uint64_t total_restore_ops;        /**< 总恢复操作次数 */
    uint64_t last_checkpoint_time;     /**< 上次检查点时间 */
    uint64_t avg_checkpoint_size;      /**< 平均检查点大小 */
} agentos_checkpoint_stats_t;

/**
 * @brief 创建新检查点
 *
 * @param task_id [in] 任务ID（非NULL）
 * @param session_id [in] 会话ID（非NULL）
 * @param sequence_num [in] 检查点序列号
 * @param state_json [in] 状态快照JSON（非NULL）
 * @param completed_nodes [in] 已完成节点列表（可为NULL）
 * @param completed_count [in] 已完成节点数量
 * @param pending_nodes [in] 待执行节点列表（可为NULL）
 * @param pending_count [in] 待执行节点数量
 * @param out_checkpoint [out] 输出检查点指针
 *
 * @return agentos_error_t 错误码
 *
 * @ownership 调用者负责释放 out_checkpoint
 * @threadsafe 否
 * @reentrant 否
 *
 * @see agentos_checkpoint_save()
 * @see agentos_checkpoint_restore()
 */
agentos_error_t agentos_checkpoint_create(
    const char* task_id,
    const char* session_id,
    uint64_t sequence_num,
    const char* state_json,
    char** completed_nodes,
    size_t completed_count,
    char** pending_nodes,
    size_t pending_count,
    agentos_task_checkpoint_t** out_checkpoint);

/**
 * @brief 保存检查点到持久化存储
 *
 * @param checkpoint [in] 检查点指针（非NULL）
 * @return agentos_error_t 错误码
 *
 * @threadsafe 否
 * @reentrant 否
 *
 * @see agentos_checkpoint_create()
 * @see agentos_checkpoint_restore()
 */
agentos_error_t agentos_checkpoint_save(const agentos_task_checkpoint_t* checkpoint);

/**
 * @brief 恢复检查点
 *
 * @param task_id [in] 任务ID（非NULL）
 * @param sequence_num [in] 检查点序列号（0表示最新）
 * @param out_checkpoint [out] 输出检查点指针
 * @return agentos_error_t 错误码
 *
 * @ownership 调用者负责释放 out_checkpoint
 * @threadsafe 否
 * @reentrant 否
 *
 * @see agentos_checkpoint_create()
 * @see agentos_checkpoint_save()
 */
agentos_error_t agentos_checkpoint_restore(
    const char* task_id,
    uint64_t sequence_num,
    agentos_task_checkpoint_t** out_checkpoint);

/**
 * @brief 删除检查点
 *
 * @param task_id [in] 任务ID（非NULL）
 * @param sequence_num [in] 检查点序列号（0表示所有）
 * @return agentos_error_t 错误码
 *
 * @threadsafe 否
 * @reentrant 否
 */
agentos_error_t agentos_checkpoint_delete(const char* task_id, uint64_t sequence_num);

/**
 * @brief 列出任务的所有检查点
 *
 * @param task_id [in] 任务ID（非NULL）
 * @param out_checkpoints [out] 输出检查点列表
 * @param out_count [out] 输出检查点数量
 * @return agentos_error_t 错误码
 *
 * @ownership 调用者负责释放 out_checkpoints 和其元素
 * @threadsafe 否
 * @reentrant 否
 */
agentos_error_t agentos_checkpoint_list(
    const char* task_id,
    agentos_task_checkpoint_t*** out_checkpoints,
    size_t* out_count);

/**
 * @brief 获取检查点统计信息
 *
 * @param out_stats [out] 输出统计信息
 * @return agentos_error_t 错误码
 *
 * @threadsafe 是
 * @reentrant 是
 */
agentos_error_t agentos_checkpoint_get_stats(agentos_checkpoint_stats_t* out_stats);

/**
 * @brief 验证检查点完整性
 *
 * @param checkpoint [in] 检查点指针
 * @param is_valid [out] 验证结果
 * @return agentos_error_t 错误码
 *
 * @threadsafe 否
 * @reentrant 否
 */
agentos_error_t agentos_checkpoint_verify(
    const agentos_task_checkpoint_t* checkpoint,
    bool* is_valid);

/**
 * @brief 释放检查点内存
 *
 * @param checkpoint [in] 检查点指针
 * @return agentos_error_t 错误码
 *
 * @threadsafe 否
 * @reentrant 否
 */
agentos_error_t agentos_checkpoint_destroy(agentos_task_checkpoint_t* checkpoint);

/**
 * @brief 初始化检查点子系统
 *
 * @param storage_path [in] 存储路径（可为NULL，使用默认路径）
 * @return agentos_error_t 错误码
 *
 * @threadsafe 否
 * @reentrant 否
 *
 * @see agentos_checkpoint_shutdown()
 */
agentos_error_t agentos_checkpoint_init(const char* storage_path);

/**
 * @brief 关闭检查点子系统
 *
 * 释放所有资源，停止后台任务
 *
 * @return agentos_error_t 错误码
 *
 * @threadsafe 否
 * @reentrant 否
 *
 * @see agentos_checkpoint_init()
 */
agentos_error_t agentos_checkpoint_shutdown(void);

/**
 * @brief 执行检查点自动清理
 *
 * 删除超过保留策略的旧检查点
 *
 * @param max_age_seconds [in] 最大保留时间（秒）
 * @param max_count [in] 最大保留数量（0表示不限制）
 * @return agentos_error_t 错误码
 *
 * @threadsafe 否
 * @reentrant 否
 */
agentos_error_t agentos_checkpoint_cleanup(uint64_t max_age_seconds, size_t max_count);

/**
 * @brief 创建检查点快照
 *
 * 将当前系统状态保存为快照文件
 *
 * @param task_id [in] 任务ID（非NULL）
 * @param snapshot_path [in] 快照文件路径（非NULL）
 * @return agentos_error_t 错误码
 *
 * @threadsafe 否
 * @reentrant 否
 *
 * @see agentos_snapshot_restore()
 */
agentos_error_t agentos_snapshot_create(const char* task_id, const char* snapshot_path);

/**
 * @brief 从快照恢复
 *
 * @param snapshot_path [in] 快照文件路径（非NULL）
 * @param task_id [out] 输出的任务ID（需调用者释放）
 * @return agentos_error_t 错误码
 *
 * @ownership 调用者负责释放 task_id
 * @threadsafe 否
 * @reentrant 否
 *
 * @see agentos_snapshot_create()
 */
agentos_error_t agentos_snapshot_restore(const char* snapshot_path, char** task_id);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_ATOMS_CHECKPOINT_H */