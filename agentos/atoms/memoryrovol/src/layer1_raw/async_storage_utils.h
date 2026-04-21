/**
 * @file async_storage_utils.h
 * @brief L1 原始卷异步存储工具函数接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef ASYNC_STORAGE_UTILS_H
#define ASYNC_STORAGE_UTILS_H

#include "../../../agentos/commons/utils/memory/include/memory_compat.h"
#include "../../../atoms/corekern/include/agentos.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_FILE_PATH 1024
#define FILE_BUFFER_SIZE (64 * 1024)

agentos_error_t async_storage_ensure_directory_exists(const char* path);
agentos_error_t async_storage_build_file_path(const char* storage_path, const char* id,
                                               char* file_path, size_t max_len);
agentos_error_t async_storage_safe_write_file(const char* file_path, const void* data,
                                              size_t data_len);
agentos_error_t async_storage_safe_read_file(const char* file_path, void** out_data,
                                             size_t* out_len);
agentos_error_t async_storage_safe_delete_file(const char* file_path);

#ifdef __cplusplus
}
#endif

#endif /* ASYNC_STORAGE_UTILS_H */
