/**
 * @file layer1_raw_io.h
 * @brief L1 原始卷文件 I/O 操作接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * @details
 * 提供文件系统相关的底层 I/O 操作，包括：
 * - 目录创建与验证
 * - 文件路径构建（安全检查）
 * - 安全文件写入（带重试机制）
 * - 安全文件读取
 *
 * 本模块是从 async_storage_engine.c 拆分出来的 I/O 操作子模块。
 */

#ifndef LAYER1_RAW_IO_H
#define LAYER1_RAW_IO_H

#include "memory_compat.h"
#include "agentos.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_FILE_PATH_LENGTH 1024
#define FILE_WRITE_BUFFER_SIZE (64 * 1024)
#define MAX_WRITE_RETRIES 5
#define INITIAL_RETRY_DELAY_MS 100
#define MAX_RETRY_DELAY_MS 10000

#ifdef _WIN32
#define PATH_SEPARATOR "\\"
#else
#define PATH_SEPARATOR "/"
#endif

agentos_error_t layer1_raw_ensure_directory_exists(const char* path);

agentos_error_t layer1_raw_build_file_path(const char* storage_path,
                                            const char* id,
                                            char* buffer,
                                            size_t buffer_size);

agentos_error_t layer1_raw_safe_write_file(const char* file_path,
                                            const void* data,
                                            size_t data_len,
                                            uint8_t retry_count);

agentos_error_t layer1_raw_safe_read_file(const char* file_path,
                                           void** out_data,
                                           size_t* out_len);

#ifdef __cplusplus
}
#endif

#endif /* LAYER1_RAW_IO_H */
