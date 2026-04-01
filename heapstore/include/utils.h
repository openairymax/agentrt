/**
 * @file utils.h
 * @brief AgentOS heapstore 公共工具函数接口
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
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
 * @param path [in] 目录路径
 * @return bool 成功返回 true，失败返回 false
 *
 * @ownership 调用者负责 path 的生命周期
 * @threadsafe 是
 * @reentrant 是
 *
 * @note 支持创建多级嵌套目录
 */
bool heapstore_ensure_directory(const char* path);

#ifdef __cplusplus
}
#endif

#endif /* heapstore_UTILS_H */
