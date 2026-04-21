/**
 * @file rov_ffi.h
 * @brief MemoryRovol 对外 C 接口（由 memoryrovol 模块实现）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_ROV_FFI_H
#define AGENTOS_ROV_FFI_H

#include "../../../atoms/corekern/include/agentos.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief MemoryRovol 句柄（不透明指针）
 */
typedef struct agentos_memoryrov_handle agentos_memoryrov_handle_t;

/**
 * @brief 创建 MemoryRovol 实例
 * @param config_path 配置文件路径（可为 NULL）
 * @param out_handle 输出句柄
 * @return agentos_error_t
 */
agentos_error_t agentos_memoryrov_create(const char* config_path, agentos_memoryrov_handle_t** out_handle);

/**
 * @brief 销毁 MemoryRovol 实例
 * @param handle 句柄
 */
void agentos_memoryrov_destroy(agentos_memoryrov_handle_t* handle);

/**
 * @brief 写入原始记忆
 * @param handle MemoryRovol 句柄
 * @param data 原始数据
 * @param len 数据长度
 * @param metadata JSON 字符串元数据（可为 NULL）
 * @param out_record_id 输出记录 ID（需调用者释放）
 * @return agentos_error_t
 */
agentos_error_t agentos_memoryrov_write_raw(
    agentos_memoryrov_handle_t* handle,
    const void* data,
    size_t len,
    const char* metadata,
    char** out_record_id);

/**
 * @brief 查询记忆
 * @param handle MemoryRovol 句柄
 * @param query 查询文本
 * @param limit 最大返回数量
 * @param out_results 输出结果数组（JSON 格式，需释放）
 * @param out_count 输出结果数量
 * @return agentos_error_t
 */
agentos_error_t agentos_memoryrov_query(
    agentos_memoryrov_handle_t* handle,
    const char* query,
    uint32_t limit,
    char*** out_results,
    size_t* out_count);

/**
 * @brief 根据 ID 获取原始记忆
 * @param handle MemoryRovol 句柄
 * @param record_id 记录 ID
 * @param out_data 输出数据（需调用者释放）
 * @param out_len 输出长度
 * @return agentos_error_t
 */
agentos_error_t agentos_memoryrov_get_raw(
    agentos_memoryrov_handle_t* handle,
    const char* record_id,
    void** out_data,
    size_t* out_len);

/**
 * @brief 挂载记忆（增加访问计数）
 * @param handle MemoryRovol 句柄
 * @param record_id 记录 ID
 * @param context 上下文标识（如任务 ID）
 * @return agentos_error_t
 */
agentos_error_t agentos_memoryrov_mount(
    agentos_memoryrov_handle_t* handle,
    const char* record_id,
    const char* context);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_ROV_FFI_H */
