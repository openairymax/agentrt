/**
 * @file io.h
 * @brief 文件操作工具
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#ifndef AGENTOS_UTILS_IO_H
#define AGENTOS_UTILS_IO_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 读取文件全部内容
 * @param path 文件路径
 * @param out_len 输出内容长度（可选）
 * @return 分配的内存，需调用者 free，失败返回 NULL
 */
char* agentos_io_read_file(const char* path, size_t* out_len);

/**
 * @brief 写入文件
 * @param path 文件路径
 * @param data 数据
 * @param len 数据长度（若为 -1 则自动计算字符串长度）
 * @return 0 成功，-1 失败
 */
int agentos_io_write_file(const char* path, const void* data, size_t len);

/**
 * @brief 确保目录存在（如果不存在则创建）
 * @param path 目录路径
 * @return 0 成功，-1 失败
 */
int agentos_io_ensure_dir(const char* path);

/**
 * @brief 列出目录下所有文件（不包含子目录）
 * @param path 目录路径
 * @param out_files 输出文件名数组（需调用 agentos_io_free_list 释放）
 * @param out_count 输出数量
 * @return 0 成功，-1 失败
 */
int agentos_io_list_files(const char* path, char*** out_files, size_t* out_count);

/**
 * @brief 释放文件列表
 */
void agentos_io_free_list(char** files, size_t count);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_UTILS_IO_H */