/**
 * @file vector_store.h
 * @brief 向量持久化存储接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_VECTOR_STORE_H
#define AGENTOS_VECTOR_STORE_H

#include "../../atoms/corekern/include/agentos.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 前向声明 */
typedef struct agentos_vector_store agentos_vector_store_t;

/**
 * @brief 向量存储配置
 */
typedef struct agentos_vector_store_config {
    const char* db_path;      /**< SQLite 数据库路径 */
    size_t dimension;          /**< 向量维度 */
} agentos_vector_store_config_t;

/**
 * @brief 创建向量存储
 * @param manager 配置
 * @param out_store 输出存储句柄
 * @return agentos_error_t
 */
agentos_error_t agentos_vector_store_create(
    const agentos_vector_store_config_t* manager,
    agentos_vector_store_t** out_store);

/**
 * @brief 销毁向量存储
 * @param store 存储句柄
 */
void agentos_vector_store_destroy(agentos_vector_store_t* store);

/**
 * @brief 存储向量
 * @param store 存储句柄
 * @param record_id 记录ID
 * @param vector 向量数据
 * @param dim 向量维度
 * @return agentos_error_t
 */
agentos_error_t agentos_vector_store_put(
    agentos_vector_store_t* store,
    const char* record_id,
    const float* vector,
    size_t dim);

/**
 * @brief 获取向量
 * @param store 存储句柄
 * @param record_id 记录ID
 * @param out_vector 输出向量（需调用者释放）
 * @param out_dim 输出维度
 * @return agentos_error_t (AGENTOS_ENOENT 表示不存在)
 */
agentos_error_t agentos_vector_store_get(
    agentos_vector_store_t* store,
    const char* record_id,
    float** out_vector,
    size_t* out_dim);

/**
 * @brief 删除向量
 * @param store 存储句柄
 * @param record_id 记录ID
 * @return agentos_error_t
 */
agentos_error_t agentos_vector_store_delete(
    agentos_vector_store_t* store,
    const char* record_id);

/**
 * @brief 列出所有存储的ID
 * @param store 存储句柄
 * @param out_ids 输出ID数组（需调用者释放）
 * @param out_count 输出数量
 * @return agentos_error_t
 */
agentos_error_t agentos_vector_store_list_ids(
    agentos_vector_store_t* store,
    char*** out_ids,
    size_t* out_count);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_VECTOR_STORE_H */
