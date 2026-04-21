/**
 * @file advanced_storage.h
 * @brief L1 增强存储管理器接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_ADVANCED_STORAGE_H
#define AGENTOS_ADVANCED_STORAGE_H

#include "../../../atoms/corekern/include/agentos.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 增强存储管理器句柄
 */
typedef struct agentos_advanced_storage agentos_advanced_storage_t;

/**
 * @brief 创建增强存储管理器
 * @param storage_id 存储 ID
 * @param base_path 基础路径
 * @param out_storage 输出存储句柄
 * @return AGENTOS_SUCCESS 成功，其他为错误码
 */
agentos_error_t agentos_advanced_storage_create(const char* storage_id,
                                               const char* base_path,
                                               agentos_advanced_storage_t** out_storage);

/**
 * @brief 销毁增强存储管理器
 * @param storage 存储句柄
 */
void agentos_advanced_storage_destroy(agentos_advanced_storage_t* storage);

/**
 * @brief 写入数据（带压缩和加密）
 * @param storage 存储句柄
 * @param id 数据 ID
 * @param data 数据
 * @param data_len 数据长度
 * @return AGENTOS_SUCCESS 成功，其他为错误码
 */
agentos_error_t agentos_advanced_storage_write(agentos_advanced_storage_t* storage,
                                              const char* id,
                                              const void* data,
                                              size_t data_len);

/**
 * @brief 读取数据（带解密和解压）
 * @param storage 存储句柄
 * @param id 数据 ID
 * @param out_data 输出数据（需要调用者释放）
 * @param out_len 输出数据长度
 * @return AGENTOS_SUCCESS 成功，其他为错误码
 */
agentos_error_t agentos_advanced_storage_read(agentos_advanced_storage_t* storage,
                                             const char* id,
                                             void** out_data,
                                             size_t* out_len);

/**
 * @brief 删除数据
 * @param storage 存储句柄
 * @param id 数据 ID
 * @return AGENTOS_SUCCESS 成功，其他为错误码
 */
agentos_error_t agentos_advanced_storage_delete(agentos_advanced_storage_t* storage,
                                               const char* id);

/**
 * @brief 获取统计信息
 * @param storage 存储句柄
 * @param total_writes 总写入次数
 * @param total_reads 总读取次数
 * @param total_errors 总错误次数
 */
void agentos_advanced_storage_get_stats(agentos_advanced_storage_t* storage,
                                       uint64_t* total_writes,
                                       uint64_t* total_reads,
                                       uint64_t* total_errors);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_ADVANCED_STORAGE_H */
