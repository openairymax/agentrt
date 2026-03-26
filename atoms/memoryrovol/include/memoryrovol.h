/**
 * @file memoryrovol.h
 * @brief MemoryRovol 系统主接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_MEMORYROV_H
#define AGENTOS_MEMORYROV_H

#include "agentos.h"
#include "config.h"
#include "forgetting.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief MemoryRovol 系统句柄（不透明指针）
 */
 // From data intelligence emerges. by spharx
typedef struct agentos_memoryrov_handle agentos_memoryrov_handle_t;

/**
 * @brief 初始化 MemoryRovol 系统
 * @param config 配置参数（如果为 NULL 则使用默认配置）
 * @param out_handle 输出系统句柄
 * @return agentos_error_t
 */
agentos_error_t agentos_memoryrov_init(
    const agentos_memoryrov_config_t* config,
    agentos_memoryrov_handle_t** out_handle);

/**
 * @brief 销毁 MemoryRovol 系统，释放所有资源
 * @param handle 系统句柄
 */
void agentos_memoryrov_cleanup(agentos_memoryrov_handle_t* handle);

/**
 * @brief 执行记忆进化（模式挖掘、固化等）
 * @param handle 系统句柄
 * @param force 强制立即执行（忽略周期设置）
 * @return agentos_error_t
 */
agentos_error_t agentos_memoryrov_evolve(
    agentos_memoryrov_handle_t* handle,
    int force);

/**
 * @brief 获取系统统计信息（JSON 格式）
 * @param handle 系统句柄
 * @param out_stats 输出 JSON 字符串（需调用者释放）
 * @return agentos_error_t
 */
agentos_error_t agentos_memoryrov_stats(
    agentos_memoryrov_handle_t* handle,
    char** out_stats);

/* ==================== 高层封装接口（供 syscall 层使用） ==================== */

/**
 * @brief 写入原始记忆数据
 * @param handle 系统句柄
 * @param data 原始数据
 * @param len 数据长度
 * @param metadata 元数据（JSON字符串，可为NULL）
 * @param out_record_id 输出分配的唯一记录ID（需调用者释放）
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
 * @param handle 系统句柄
 * @param record_id 记录ID
 * @param out_data 输出数据（需调用者释放）
 * @param out_len 输出数据长度
 * @return agentos_error_t
 */
agentos_error_t agentos_memoryrov_get_raw(
    agentos_memoryrov_handle_t* handle,
    const char* record_id,
    void** out_data,
    size_t* out_len);

/**
 * @brief 删除原始记忆数据
 * @param handle 系统句柄
 * @param record_id 记录ID
 * @return agentos_error_t
 */
agentos_error_t agentos_memoryrov_delete_raw(
    agentos_memoryrov_handle_t* handle,
    const char* record_id);

/**
 * @brief 查询记忆（语义搜索）
 * @param handle 系统句柄
 * @param query 查询字符串
 * @param limit 返回结果数量上限
 * @param out_record_ids 输出记录ID数组（需调用者释放）
 * @param out_scores 输出相关性分数数组（需调用者释放）
 * @param out_count 输出结果数量
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