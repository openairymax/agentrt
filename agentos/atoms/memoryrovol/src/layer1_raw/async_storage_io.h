/**
 * @file async_storage_io.h
 * @brief 异步存储引擎内部I/O操作接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef ASYNC_STORAGE_IO_H
#define ASYNC_STORAGE_IO_H

#include "memory_compat.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 最大文件路径长度 */
#define MAX_FILE_PATH_LENGTH 1024

/** @brief 文件写缓冲区大小（字节） */
#define FILE_WRITE_BUFFER_SIZE (64 * 1024)

/** @brief 路径分隔符 */
#ifdef _WIN32
#define PATH_SEPARATOR "\\"
#else
#define PATH_SEPARATOR "/"
#endif

agentos_error_t ensure_directory_exists(const char* path);
agentos_error_t build_file_path(const char* storage_path, const char* id,
                                 char* buffer, size_t buffer_size);
agentos_error_t safe_write_file(const char* file_path, const void* data,
                                size_t data_len, uint8_t retry_count);
agentos_error_t safe_read_file(const char* file_path, void** out_data, size_t* out_len);

#ifdef __cplusplus
}
#endif

#endif /* ASYNC_STORAGE_IO_H */
