/**
 * @file layer1_raw.h
 * @brief L1 原始卷接口：原始记忆的存储、读取、元数据管理（支持异步写入）
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#ifndef AGENTOS_LAYER1_RAW_H
#define AGENTOS_LAYER1_RAW_H

#include "agentos.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 前向声明 */
typedef struct agentos_layer1_raw agentos_layer1_raw_t;

/**
 * @brief 原始记录元数据
 */
typedef struct agentos_raw_metadata {
    char* record_id;           /**< 记录ID（由系统生成） */
    uint64_t timestamp;        /**< 时间戳（纳秒） */
    char* source;              /**< 来源标识 */
    char* trace_id;            /**< 追踪ID */
    size_t data_len;           /**< 原始数据长度 */
    uint32_t access_count;     /**< 访问次数 */
    uint64_t last_access;      /**< 最后访问时间 */
    char* tags_json;           /**< 扩展标签（JSON） */
} agentos_raw_metadata_t;

/**
 * @brief 创建 L1 原始卷实例（同步模式）
 * @param base_path 存储根路径
 * @param out_layer 输出层句柄
 * @return agentos_error_t
 */
agentos_error_t agentos_layer1_raw_create(
    const char* base_path,
    agentos_layer1_raw_t** out_layer);

/**
 * @brief 创建 L1 原始卷实例（异步模式，启动后台写入线程）
 * @param base_path 存储根路径
 * @param queue_size 写入队列最大长度
 * @param num_workers 后台工作线程数
 * @param out_layer 输出层句柄
 * @return agentos_error_t
 */
agentos_error_t agentos_layer1_raw_create_async(
    const char* base_path,
    size_t queue_size,
    uint32_t num_workers,
    agentos_layer1_raw_t** out_layer);

/**
 * @brief 销毁 L1 层
 * @param layer 层句柄
 */
void agentos_layer1_raw_destroy(agentos_layer1_raw_t* layer);

/**
 * @brief 同步写入原始记忆
 * @param layer L1 层句柄
 * @param data 原始数据
 * @param len 数据长度
 * @param metadata 元数据（JSON字符串，可为NULL）
 * @param out_record_id 输出分配的唯一记录ID（需调用者释放）
 * @return agentos_error_t
 */
agentos_error_t agentos_layer1_raw_write(
    agentos_layer1_raw_t* layer,
    const void* data,
    size_t len,
    const char* metadata,
    char** out_record_id);

/**
 * @brief 异步写入原始记忆（立即返回，后台执行）
 * @param layer L1 层句柄
 * @param data 原始数据（调用者需确保在写入完成前不被释放）
 * @param len 数据长度
 * @param metadata 元数据（JSON字符串，可为NULL）
 * @param callback 完成回调（可为NULL）
 * @param userdata 用户数据传递给回调
 * @return agentos_error_t（表示请求是否成功入队）
 */
agentos_error_t agentos_layer1_raw_write_async(
    agentos_layer1_raw_t* layer,
    const void* data,
    size_t len,
    const char* metadata,
    void (*callback)(agentos_error_t err, const char* record_id, void* userdata),
    void* userdata);

/**
 * @brief 等待所有异步写入完成（阻塞）
 * @param layer L1 层句柄
 * @param timeout_ms 超时毫秒（0表示无限等待）
 * @return agentos_error_t
 */
agentos_error_t agentos_layer1_raw_flush(
    agentos_layer1_raw_t* layer,
    uint32_t timeout_ms);

/**
 * @brief 根据记录ID读取原始数据
 * @param layer L1 层句柄
 * @param record_id 记录ID
 * @param out_data 输出数据（需调用者释放）
 * @param out_len 输出数据长度
 * @return agentos_error_t
 */
agentos_error_t agentos_layer1_raw_read(
    agentos_layer1_raw_t* layer,
    const char* record_id,
    void** out_data,
    size_t* out_len);

/**
 * @brief 获取记录的元数据
 * @param layer L1 层句柄
 * @param record_id 记录ID
 * @param out_metadata 输出元数据（需调用者释放）
 * @return agentos_error_t
 */
agentos_error_t agentos_layer1_raw_get_metadata(
    agentos_layer1_raw_t* layer,
    const char* record_id,
    agentos_raw_metadata_t** out_metadata);

/**
 * @brief 释放元数据对象
 * @param metadata 元数据指针
 */
void agentos_layer1_raw_metadata_free(agentos_raw_metadata_t* metadata);

/**
 * @brief 更新记录访问计数（用于遗忘曲线）
 * @param layer L1 层句柄
 * @param record_id 记录ID
 * @return agentos_error_t
 */
agentos_error_t agentos_layer1_raw_touch(
    agentos_layer1_raw_t* layer,
    const char* record_id);

/**
 * @brief 删除记录（物理删除，谨慎使用）
 * @param layer L1 层句柄
 * @param record_id 记录ID
 * @return agentos_error_t
 */
agentos_error_t agentos_layer1_raw_delete(
    agentos_layer1_raw_t* layer,
    const char* record_id);

/**
 * @brief 获取所有记录ID列表
 * @param layer L1 层句柄
 * @param out_ids 输出记录ID数组（需调用者使用 agentos_free_string_array 释放）
 * @param out_count 输出数量
 * @return agentos_error_t
 */
agentos_error_t agentos_layer1_raw_list_ids(
    agentos_layer1_raw_t* layer,
    char*** out_ids,
    size_t* out_count);

/**
 * @brief 释放字符串数组
 * @param arr 数组
 * @param count 数量
 */
void agentos_free_string_array(char** arr, size_t count);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_LAYER1_RAW_H */