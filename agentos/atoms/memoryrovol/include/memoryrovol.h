/**
 * @file memoryrovol.h
 * @brief MemoryRovol 系统主接口头文件
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_MEMORYROV_H
#define AGENTOS_MEMORYROV_H

#include "../../atoms/corekern/include/agentos.h"
#include "config.h"
#include "forgetting.h"
#include <observability_compat.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief MemoryRovol 系统句柄（不透明指针）
 */
typedef struct agentos_memoryrov_handle agentos_memoryrov_handle_t;

/**
 * @brief 记忆记录结构体
 *
 * 表示从记忆系统中检索到的一条记忆记录，
 * 包含原始数据、元数据和相关性分数。
 */
typedef struct agentos_memory {
    char* record_id;          /**< 记录唯一标识符 */
    void* data;               /**< 原始数据指针 */
    size_t data_len;          /**< 数据长度 */
    char* metadata;           /**< 元数据（JSON格式） */
    float score;              /**< 相关性分数（0.0-1.0） */
    time_t created_at;        /**< 创建时间戳 */
    time_t updated_at;        /**< 最后更新时间戳 */
} agentos_memory_t;

/**
 * @brief 初始化 MemoryRovol 系统
 * @param manager [in] 配置参数（如果为 NULL 则使用默认配置）
 * @param out_handle [out] 输出系统句柄
 * @ownership 调用者负责释放 out_handle
 * @threadsafe 是（可多次调用，但建议单次初始化）
 * @reentrant 否
 * @return agentos_error_t
 */
agentos_error_t agentos_memoryrov_init(
    const agentos_memoryrov_config_t* manager,
    agentos_memoryrov_handle_t** out_handle);

/**
 * @brief 销毁 MemoryRovol 系统，释放所有资源
 * @param handle [in] 系统句柄
 * @ownership 释放 handle 及其内部所有资源
 * @threadsafe 否（销毁后不能再使用该句柄）
 * @reentrant 否
 */
void agentos_memoryrov_cleanup(agentos_memoryrov_handle_t* handle);

/**
 * @brief 执行记忆进化（模式挖掘、固化等）
 * @param handle [in] 系统句柄（非NULL）
 * @param force [in] 强制立即执行（忽略周期设置）
 * @threadsafe 是（内部使用锁保护）
 * @reentrant 否
 * @return agentos_error_t
 */
agentos_error_t agentos_memoryrov_evolve(
    agentos_memoryrov_handle_t* handle,
    int force);

/**
 * @brief 获取系统统计信息（JSON 格式）
 * @param handle [in] 系统句柄（非NULL）
 * @param out_stats [out] 输出 JSON 字符串（需调用者释放）
 * @ownership 调用者负责释放 out_stats
 * @threadsafe 是（内部使用锁保护）
 * @reentrant 否
 * @return agentos_error_t
 */
agentos_error_t agentos_memoryrov_stats(
    agentos_memoryrov_handle_t* handle,
    char** out_stats);

/* ==================== 高层封装接口（供 syscall 层使用） ==================== */

/**
 * @brief 写入原始记忆数据
 * @param handle [in] 系统句柄（非NULL）
 * @param data [in] 原始数据（非NULL）
 * @param len [in] 数据长度
 * @param metadata [in] 元数据（JSON字符串，可为NULL）
 * @param out_record_id [out] 输出分配的唯一记录ID
 * @ownership 调用者负责释放 out_record_id
 * @threadsafe 是（内部使用锁保护）
 * @reentrant 否
 * @return agentos_error_t
 */
agentos_error_t agentos_memoryrov_write_raw(
    agentos_memoryrov_handle_t* handle,
    const void* data,
    size_t len,
    const char* metadata,
    char** out_record_id);

/**
 * @brief 读取原始记忆数据
 * @param handle [in] 系统句柄（非NULL）
 * @param record_id [in] 记录ID（非NULL）
 * @param out_data [out] 输出数据
 * @param out_len [out] 输出数据长度
 * @ownership 调用者负责释放 out_data
 * @threadsafe 是（内部使用锁保护）
 * @reentrant 否
 * @return agentos_error_t
 */
agentos_error_t agentos_memoryrov_get_raw(
    agentos_memoryrov_handle_t* handle,
    const char* record_id,
    void** out_data,
    size_t* out_len);

/**
 * @brief 删除原始记忆数据
 * @param handle [in] 系统句柄（非NULL）
 * @param record_id [in] 记录ID（非NULL）
 * @threadsafe 是（内部使用锁保护）
 * @reentrant 否
 * @return agentos_error_t
 */
agentos_error_t agentos_memoryrov_delete_raw(
    agentos_memoryrov_handle_t* handle,
    const char* record_id);

/**
 * @brief 查询记忆（语义搜索）
 * @param handle [in] 系统句柄（非NULL）
 * @param query [in] 查询字符串（非NULL）
 * @param limit [in] 返回结果数量上限
 * @param out_record_ids [out] 输出记录ID数组
 * @param out_scores [out] 输出相关性分数数组
 * @param out_count [out] 输出结果数量
 * @ownership 调用者负责释放 out_record_ids 和 out_scores
 * @threadsafe 是（内部使用锁保护）
 * @reentrant 否
 * @return agentos_error_t
 */
agentos_error_t agentos_memoryrov_query(
    agentos_memoryrov_handle_t* handle,
    const char* query,
    uint32_t limit,
    char*** out_record_ids,
    float** out_scores,
    size_t* out_count);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_MEMORYROV_H */
