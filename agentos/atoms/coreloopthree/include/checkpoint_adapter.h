/**
 * @file checkpoint_adapter.h
 * @brief C-L07: Checkpoint → CoreLoopThree 检查点适配器
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 在 CoreLoopThree 认知循环中集成检查点系统，
 * 支持长任务的状态持久化和崩溃恢复。
 *
 * 使用方式：
 * @code
 *   // 1. 初始化适配器
 *   checkpoint_adapter_t *ckpt = checkpoint_adapter_create(NULL);
 *
 *   // 2. 注册到 CoreLoopThree 循环
 *   agentos_loop_set_checkpoint_adapter(loop, ckpt);
 *
 *   // 3. 每 N 轮自动保存检查点
 *   //    N 由 agentos.yaml 的 multi_agent.checkpoint.interval_turns 配置
 *
 *   // 4. 崩溃后恢复
 *   checkpoint_adapter_restore(ckpt, task_id, &restored_state);
 *
 *   // 5. 关闭时销毁
 *   checkpoint_adapter_destroy(ckpt);
 * @endcode
 *
 * @see checkpoint.h
 * @see loop.h
 * @see P1.6 C-L07 连接线
 */

#ifndef AGENTOS_CORELOOPTHREE_CHECKPOINT_ADAPTER_H
#define AGENTOS_CORELOOPTHREE_CHECKPOINT_ADAPTER_H

/* P4.7 阶段 A（ACC-DT28）：移除不必要 `#include "checkpoint.h"`。
 * 本头文件所有类型均自定义（checkpoint_adapter_s / checkpoint_adapter_config_t /
 * checkpoint_snapshot_t），不引用 checkpoint.h 中的任何类型或符号。
 * checkpoint_adapter.c 实现端仍保留 include（调用 agentos_checkpoint_save /
 * agentos_checkpoint_restore 等 API 与 agentos_task_checkpoint_t 类型）。
 * 头文件依赖最小化是 atoms 层架构清理的目标之一，可减少传递性 include 污染
 * （loop.c/orch_adapter.c 等仅 include 本 header 不再被迫拉入 checkpoint.h）。 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 适配器句柄 ==================== */

typedef struct checkpoint_adapter_s checkpoint_adapter_t;

/* ==================== 适配器配置 ==================== */

typedef struct {
    const char *storage_path;         /**< 检查点存储路径，默认 "/var/lib/agentos/checkpoints" */
    uint32_t save_interval_turns;     /**< 每 N 轮保存一次检查点，0 使用默认 10 */
    uint32_t save_interval_ms;        /**< 每 N 毫秒保存一次检查点，0 使用默认 30000 */
    bool enable_incremental_save;     /**< 是否启用增量保存 */
    bool enable_compression;          /**< 是否启用压缩 */
    uint32_t max_checkpoints_per_task; /**< 每个任务最大检查点数，0 无限制 */
    uint64_t max_age_seconds;         /**< 检查点最大保留时间（秒），0 永不过期 */
} checkpoint_adapter_config_t;

/* ==================== 检查点状态快照 ==================== */

typedef struct {
    char *task_id;                    /**< 任务 ID */
    char *session_id;                 /**< 会话 ID */
    uint64_t sequence_num;            /**< 序列号 */
    uint64_t timestamp;               /**< 时间戳 */
    char *cognition_state_json;       /**< 认知状态 JSON */
    char *memory_context_json;        /**< 记忆上下文 JSON */
    char *tool_call_history_json;     /**< 工具调用历史 JSON */
    char *pending_nodes_json;         /**< 待执行节点 JSON */
    char *completed_nodes_json;       /**< 已完成节点 JSON */
    uint32_t current_turn;            /**< 当前轮次 */
    uint32_t total_turns;             /**< 总轮次 */
    float progress_percent;           /**< 进度百分比 */
} checkpoint_snapshot_t;

/* ==================== 生命周期 ==================== */

/**
 * @brief 创建检查点适配器
 *
 * @param config 适配器配置（NULL 使用默认）
 * @return 适配器句柄，失败返回 NULL
 *
 * @ownership return: OWNER
 */
checkpoint_adapter_t *checkpoint_adapter_create(const checkpoint_adapter_config_t *config);

/**
 * @brief 销毁检查点适配器
 *
 * @param adapter 适配器句柄
 *
 * @ownership adapter: TRANSFER
 */
void checkpoint_adapter_destroy(checkpoint_adapter_t *adapter);

/* ==================== 检查点保存 ==================== */

/**
 * @brief C-L07: 保存 CoreLoopThree 状态到检查点
 *
 * 将认知引擎的当前状态序列化并保存到检查点存储。
 * 在 CoreLoopThree 循环中每 N 轮调用一次。
 *
 * @param adapter 适配器句柄
 * @param task_id 任务 ID
 * @param session_id 会话 ID
 * @param sequence_num 序列号
 * @param snapshot 状态快照
 * @return 0 成功，非0 失败
 *
 * @ownership snapshot: BORROW
 */
int checkpoint_adapter_save(checkpoint_adapter_t *adapter,
                            const char *task_id,
                            const char *session_id,
                            uint64_t sequence_num,
                            const checkpoint_snapshot_t *snapshot);

/**
 * @brief C-L07: 从检查点恢复 CoreLoopThree 状态
 *
 * 查找指定任务的最新检查点，恢复认知状态、记忆上下文
 * 和工具调用历史。
 *
 * @param adapter 适配器句柄
 * @param task_id 原任务 ID
 * @param out_snapshot 输出恢复的状态快照（需调用者释放）
 * @return 0 成功，AGENTOS_ENOENT 无检查点，其他为错误
 *
 * @ownership out_snapshot: OWNER
 */
int checkpoint_adapter_restore(checkpoint_adapter_t *adapter,
                               const char *task_id,
                               checkpoint_snapshot_t **out_snapshot);

/**
 * @brief 从指定序列号恢复检查点
 *
 * @param adapter 适配器句柄
 * @param task_id 任务 ID
 * @param sequence_num 序列号
 * @param out_snapshot 输出恢复的状态快照
 * @return 0 成功，非0 失败
 *
 * @ownership out_snapshot: OWNER
 */
int checkpoint_adapter_restore_seq(checkpoint_adapter_t *adapter,
                                   const char *task_id,
                                   uint64_t sequence_num,
                                   checkpoint_snapshot_t **out_snapshot);

/**
 * @brief 列出任务的所有检查点
 *
 * @param adapter 适配器句柄
 * @param task_id 任务 ID
 * @param out_snapshots 输出快照数组（需调用者释放）
 * @param out_count 输出数量
 * @return 0 成功，非0 失败
 *
 * @ownership out_snapshots: OWNER
 */
int checkpoint_adapter_list(checkpoint_adapter_t *adapter,
                            const char *task_id,
                            checkpoint_snapshot_t ***out_snapshots,
                            size_t *out_count);

/**
 * @brief 删除任务的检查点
 *
 * @param adapter 适配器句柄
 * @param task_id 任务 ID
 * @param sequence_num 序列号（0 表示删除所有）
 * @return 0 成功，非0 失败
 */
int checkpoint_adapter_delete(checkpoint_adapter_t *adapter,
                              const char *task_id,
                              uint64_t sequence_num);

/* ==================== 快照管理 ==================== */

/**
 * @brief 创建完整快照（序列化所有状态到文件）
 *
 * @param adapter 适配器句柄
 * @param task_id 任务 ID
 * @param snapshot_path 快照文件路径
 * @return 0 成功，非0 失败
 */
int checkpoint_adapter_snapshot_create(checkpoint_adapter_t *adapter,
                                       const char *task_id,
                                       const char *snapshot_path);

/**
 * @brief 从快照文件恢复
 *
 * @param adapter 适配器句柄
 * @param snapshot_path 快照文件路径
 * @param out_task_id 输出任务 ID
 * @return 0 成功，非0 失败
 *
 * @ownership out_task_id: OWNER
 */
int checkpoint_adapter_snapshot_restore(checkpoint_adapter_t *adapter,
                                        const char *snapshot_path,
                                        char **out_task_id);

/* ==================== 快照内存管理 ==================== */

/**
 * @brief 释放快照结构
 *
 * @param snapshot 快照指针
 *
 * @ownership snapshot: TRANSFER
 */
void checkpoint_snapshot_free(checkpoint_snapshot_t *snapshot);

/* ==================== 状态查询 ==================== */

/**
 * @brief 获取适配器统计信息
 *
 * @param adapter 适配器句柄
 * @param out_total_saves 输出总保存次数
 * @param out_total_restores 输出总恢复次数
 * @param out_total_errors 输出总错误数
 * @param out_last_save_time 输出最后保存时间
 */
void checkpoint_adapter_get_stats(checkpoint_adapter_t *adapter,
                                  uint64_t *out_total_saves,
                                  uint64_t *out_total_restores,
                                  uint64_t *out_total_errors,
                                  uint64_t *out_last_save_time);

/**
 * @brief 检查适配器是否已初始化
 *
 * @param adapter 适配器句柄
 * @return true 已初始化
 */
bool checkpoint_adapter_is_ready(checkpoint_adapter_t *adapter);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_CORELOOPTHREE_CHECKPOINT_ADAPTER_H */