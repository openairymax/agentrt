/**
 * @file layer1_raw.h
 * @brief L1 原始卷接口（简化版）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_LAYER1_RAW_H
#define AGENTOS_LAYER1_RAW_H

#include "../../atoms/corekern/include/agentos.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief L1 原始卷句柄
 */
typedef struct agentos_layer1_raw agentos_layer1_raw_t;

/**
 * @brief L1 配置
 */
typedef struct agentos_layer1_raw_config {
    const char* storage_path;
    uint32_t queue_size;
    uint32_t async_workers;
} agentos_layer1_raw_config_t;

/**
 * @brief 创建异步L1原始卷
 */
agentos_error_t agentos_layer1_raw_create_async(
    const char* path,
    uint32_t queue_size,
    uint32_t workers,
    agentos_layer1_raw_t** out);

/**
 * @brief 销毁L1原始卷
 */
void agentos_layer1_raw_destroy(agentos_layer1_raw_t* l1);

/**
 * @brief 写入数据
 */
agentos_error_t agentos_layer1_raw_write(
    agentos_layer1_raw_t* l1,
    const char* id,
    const void* data,
    size_t len);

/**
 * @brief 读取数据
 */
agentos_error_t agentos_layer1_raw_read(
    agentos_layer1_raw_t* l1,
    const char* id,
    void** out_data,
    size_t* out_len);

/**
 * @brief 删除数据
 */
agentos_error_t agentos_layer1_raw_delete(
    agentos_layer1_raw_t* l1,
    const char* id);

/**
 * @brief 列出所有ID
 */
agentos_error_t agentos_layer1_raw_list_ids(
    agentos_layer1_raw_t* l1,
    char*** out_ids,
    size_t* out_count);

/**
 * @brief 刷新缓冲区
 */
agentos_error_t agentos_layer1_raw_flush(
    agentos_layer1_raw_t* l1,
    uint32_t timeout_ms);

/**
 * @brief 释放字符串数组
 */
void agentos_free_string_array(char** arr, size_t count);

typedef struct agentos_raw_metadata {
    char* record_id;
    char* source;
    char* content_type;
    uint64_t created_ns;
    uint64_t modified_ns;
    uint64_t last_access;
    size_t data_size;
    size_t data_len;
    uint32_t access_count;
    double importance;
    char* trace_id;
    char* tags_json;
    char** tags;
    size_t tag_count;
    uint64_t timestamp;
} agentos_raw_metadata_t;

typedef struct agentos_raw_metadata_db agentos_raw_metadata_db_t;

agentos_error_t agentos_raw_metadata_db_open(
    const char* path,
    agentos_raw_metadata_db_t** out_db);

void agentos_raw_metadata_db_close(
    agentos_raw_metadata_db_t* db_handle);

agentos_error_t agentos_raw_metadata_db_upsert(
    agentos_raw_metadata_db_t* db_handle,
    const agentos_raw_metadata_t* meta);

agentos_error_t agentos_raw_metadata_db_query(
    agentos_raw_metadata_db_t* db_handle,
    const char* record_id,
    agentos_raw_metadata_t** out_meta);

void agentos_raw_metadata_free(agentos_raw_metadata_t* meta);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_LAYER1_RAW_H */
