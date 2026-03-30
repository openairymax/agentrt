/**
 * @file utils.h
 * @brief AgentOS heapstore 公共工具函数接口
 *
 * Copyright (c) 2026 SPHARX. All Rights Reserved.
 * "From data intelligence emerges."
 */

#ifndef heapstore_UTILS_H
#define heapstore_UTILS_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 确保目录存在，必要时创建嵌套目录
 *
 * @param path 目录路径
 * @return bool 成功返回 true，失败返回 false
 */
bool heapstore_ensure_directory(const char* path);

#ifdef __cplusplus
}
#endif

#endif /* heapstore_UTILS_H */
