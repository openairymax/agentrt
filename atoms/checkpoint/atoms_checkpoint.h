/**
 * @file atoms_checkpoint.h
 * @brief AgentOS检查点与快照恢复接口
 *
 * 提供任务状态检查点和快照恢复功能，支持长时间任务（1000小时+）的中断恢复。
 * 遵循E-3资源确定性原则和E-6错误可追溯原则。
 *
 * @copyright Copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_ATOMS_CHECKPOINT_H
#define AGENTOS_ATOMS_CHECKPOINT_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup checkpoint
 * @{
 */

/**
 * @brief 检查点元数据
 */
typedef struct agentos_checkpoint_metadata {
    char* task_id;
    char* session_id;
    uint64_t sequence_num;
    time_t created_at;
    time_t expires_at;
    size_t state_size;
    char* state_hash;
    char* parent_checkpoint_id;
    uint32_t flags;
} agentos_checkpoint_metadata_t;

/**
 * @brief 检查点状态类型
 */
typedef enum {
    CHECKPOINT_STATE_PENDING,
    CHECKPOINT_STATE_COMPLETE,
    CHECKPOINT_STATE_FAILED,
    CHECKPOINT_STATE_EXPIRED
} agentos_checkpoint_state_t;

/**
 * @brief 检查点数据
 */
typedef struct agentos_checkpoint {
    agentos_checkpoint_metadata_t* metadata;
    void* state_data;
    size_t state_size;
} agentos_checkpoint_t;

/**
 * @brief 检查点配置
 */
typedef struct agentos_checkpoint_config {
    size_t max_checkpoints_per_task;
    size_t max_checkpoint_size_bytes;
    time_t checkpoint_ttl_seconds;
    time_t auto_save_interval_seconds;
    bool enable_compression;
    bool enable_deduplication;
    const char* storage_path;
} agentos_checkpoint_config_t;

/**
 * @brief 快照类型
 */
typedef enum {
    SNAPSHOT_TYPE_FULL,
    SNAPSHOT_TYPE_INCREMENTAL,
    SNAPSHOT_TYPE_DELTA
} agentos_snapshot_type_t;

/**
 * @brief 快照数据
 */
typedef struct agentos_snapshot {
    char* snapshot_id;
    char* task_id;
    agentos_snapshot_type_t type;
    time_t created_at;
    size_t size_bytes;
    void* data;
    char* description;
} agentos_snapshot_t;

/**
 * @brief 创建检查点管理器
 * @param[in] config 检查点配置（可为NULL，使用默认配置）
 * @param[out] manager 检查点管理器（需调用checkpoint_manager_destroy释放）
 * @return 0成功，非0失败
 */
int agentos_checkpoint_manager_create(
    const agentos_checkpoint_config_t* config,
    void** manager);

/**
 * @brief 销毁检查点管理器
 * @param[in] manager 检查点管理器
 * @return 0成功，非0失败
 */
int agentos_checkpoint_manager_destroy(void* manager);

/**
 * @brief 创建任务检查点
 * @param[in] manager 检查点管理器
 * @param[in] task_id 任务ID
 * @param[in] session_id 会话ID
 * @param[in] state_data 状态数据
 * @param[in] state_size 状态数据大小
 * @param[out] checkpoint 检查点（需调用checkpoint_destroy释放）
 * @return 0成功，非0失败
 *
 * @note 线程安全
 */
int agentos_checkpoint_create(
    void* manager,
    const char* task_id,
    const char* session_id,
    const void* state_data,
    size_t state_size,
    agentos_checkpoint_t** checkpoint);

/**
 * @brief 保存检查点到持久化存储
 * @param[in] manager 检查点管理器
 * @param[in] checkpoint 检查点
 * @return 0成功，非0失败
 *
 * @note 线程安全
 */
int agentos_checkpoint_save(
    void* manager,
    const agentos_checkpoint_t* checkpoint);

/**
 * @brief 加载检查点
 * @param[in] manager 检查点管理器
 * @param[in] task_id 任务ID
 * @param[in] sequence_num 序列号
 * @param[out] checkpoint 检查点（需调用checkpoint_destroy释放）
 * @return 0成功，非0失败
 */
int agentos_checkpoint_load(
    void* manager,
    const char* task_id,
    uint64_t sequence_num,
    agentos_checkpoint_t** checkpoint);

/**
 * @brief 获取任务最新的检查点
 * @param[in] manager 检查点管理器
 * @param[in] task_id 任务ID
 * @param[out] checkpoint 检查点（需调用checkpoint_destroy释放）
 * @return 0成功，非0失败
 */
int agentos_checkpoint_get_latest(
    void* manager,
    const char* task_id,
    agentos_checkpoint_t** checkpoint);

/**
 * @brief 删除检查点
 * @param[in] manager 检查点管理器
 * @param[in] task_id 任务ID
 * @param[in] sequence_num 序列号
 * @return 0成功，非0失败
 */
int agentos_checkpoint_delete(
    void* manager,
    const char* task_id,
    uint64_t sequence_num);

/**
 * @brief 列出任务的所有检查点
 * @param[in] manager 检查点管理器
 * @param[in] task_id 任务ID
 * @param[out] checkpoints 检查点数组（需调用checkpoint_list_destroy释放）
 * @param[out] count 检查点数量
 * @return 0成功，非0失败
 */
int agentos_checkpoint_list(
    void* manager,
    const char* task_id,
    agentos_checkpoint_metadata_t*** checkpoints,
    size_t* count);

/**
 * @brief 销毁检查点
 * @param[in] checkpoint 检查点
 * @return 0成功，非0失败
 */
int agentos_checkpoint_destroy(agentos_checkpoint_t* checkpoint);

/**
 * @brief 销毁检查点列表
 * @param[in] checkpoints 检查点数组
 * @param[in] count 检查点数量
 * @return 0成功，非0失败
 */
int agentos_checkpoint_list_destroy(
    agentos_checkpoint_metadata_t** checkpoints,
    size_t count);

/**
 * @brief 创建任务快照
 * @param[in] manager 检查点管理器
 * @param[in] task_id 任务ID
 * @param[in] type 快照类型
 * @param[in] data 快照数据
 * @param[in] size 快照大小
 * @param[in] description 快照描述
 * @param[out] snapshot 快照（需调用snapshot_destroy释放）
 * @return 0成功，非0失败
 */
int agentos_snapshot_create(
    void* manager,
    const char* task_id,
    agentos_snapshot_type_t type,
    const void* data,
    size_t size,
    const char* description,
    agentos_snapshot_t** snapshot);

/**
 * @brief 保存快照到持久化存储
 * @param[in] manager 检查点管理器
 * @param[in] snapshot 快照
 * @return 0成功，非0失败
 */
int agentos_snapshot_save(
    void* manager,
    const agentos_snapshot_t* snapshot);

/**
 * @brief 恢复任务快照
 * @param[in] manager 检查点管理器
 * @param[in] snapshot_id 快照ID
 * @param[out] data 快照数据（调用者负责释放）
 * @param[out] size 快照大小
 * @return 0成功，非0失败
 */
int agentos_snapshot_restore(
    void* manager,
    const char* snapshot_id,
    void** data,
    size_t* size);

/**
 * @brief 删除快照
 * @param[in] manager 检查点管理器
 * @param[in] snapshot_id 快照ID
 * @return 0成功，非0失败
 */
int agentos_snapshot_delete(
    void* manager,
    const char* snapshot_id);

/**
 * @brief 销毁快照
 * @param[in] snapshot 快照
 * @return 0成功，非0失败
 */
int agentos_snapshot_destroy(agentos_snapshot_t* snapshot);

/**
 * @brief 获取检查点统计信息
 * @param[in] manager 检查点管理器
 * @param[out] total_checkpoints 检查点总数
 * @param[out] total_size_bytes 总大小
 * @param[out] oldest_checkpoint 最老检查点时间戳
 * @return 0成功，非0失败
 */
int agentos_checkpoint_get_stats(
    void* manager,
    size_t* total_checkpoints,
    size_t* total_size_bytes,
    time_t* oldest_checkpoint);

/**
 * @brief 清理过期检查点
 * @param[in] manager 检查点管理器
 * @param[out] deleted_count 删除数量
 * @return 0成功，非0失败
 */
int agentos_checkpoint_cleanup_expired(
    void* manager,
    size_t* deleted_count);

/**
 * @brief 获取默认的检查点配置
 * @return 默认配置
 */
agentos_checkpoint_config_t agentos_checkpoint_get_default_config(void);

/**
 * @brief 验证检查点数据完整性
 * @param[in] checkpoint 检查点
 * @return true如果完整，false否则
 */
bool agentos_checkpoint_verify_integrity(const agentos_checkpoint_t* checkpoint);

/**
 * @brief 计算检查点的序列号
 * @param[in] manager 检查点管理器
 * @param[in] task_id 任务ID
 * @param[out] sequence_num 序列号
 * @return 0成功，非0失败
 */
int agentos_checkpoint_get_next_sequence(
    void* manager,
    const char* task_id,
    uint64_t* sequence_num);

/**
 * @brief 设置检查点自动保存
 * @param[in] manager 检查点管理器
 * @param[in] task_id 任务ID
 * @param[in] enabled 是否启用
 * @return 0成功，非0失败
 */
int agentos_checkpoint_set_auto_save(
    void* manager,
    const char* task_id,
    bool enabled);

/**
 * @brief 导出检查点到文件
 * @param[in] manager 检查点管理器
 * @param[in] task_id 任务ID
 * @param[in] sequence_num 序列号
 * @param[in] file_path 文件路径
 * @return 0成功，非0失败
 */
int agentos_checkpoint_export_to_file(
    void* manager,
    const char* task_id,
    uint64_t sequence_num,
    const char* file_path);

/**
 * @brief 从文件导入检查点
 * @param[in] manager 检查点管理器
 * @param[in] file_path 文件路径
 * @param[out] checkpoint 检查点（需调用checkpoint_destroy释放）
 * @return 0成功，非0失败
 */
int agentos_checkpoint_import_from_file(
    void* manager,
    const char* file_path,
    agentos_checkpoint_t** checkpoint);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_ATOMS_CHECKPOINT_H */
