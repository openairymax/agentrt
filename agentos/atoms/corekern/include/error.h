/**
 * @file error.h
 * @brief 内核错误码定义
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * "From data intelligence emerges."
 *
 * @note 定义了 AgentOS 内核统一的错误码体系，遵循 POSIX 错误码语义
 */

#ifndef AGENTOS_ATOMS_COREKERN_ERROR_H
#define AGENTOS_ATOMS_COREKERN_ERROR_H

#include <stdint.h>
#include "export.h"
/* 统一错误码定义：使用commons权威基础库 */
#include "../../../commons/include/agentos_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief API 版本号
 */
#define AGENTOS_COREKERN_API_VERSION 1

/**
 * @brief AgentOS 统一错误类型
 *
 * 使用 int32_t 确保跨平台一致性
 * 正值表示成功，负值表示错误
 */
/* agentos_error_t 和基础错误码现在由 agentos_types.h 提供 */

/*
 * 以下为corekern特有的扩展错误码（不在agentos_types.h中）
 * 基础错误码（SUCCESS, EINVAL, ENOMEM等）已统一到agentos_types.h
 */

/**
 * @brief 操作被中断
 *
 * 操作被信号中断
 */
#define AGENTOS_EINTR                  -11

/**
 * @brief 错误的文件描述符
 *
 * 文件描述符无效或已关闭
 */
#define AGENTOS_EBADF                  -13

/**
 * @brief 资源相关错误
 *
 * 资源耗尽、不足或状态异常
 */
#define AGENTOS_ERESOURCE              -15

/**
 * @brief 功能未实现
 *
 * 请求的功能未在此平台实现
 */
#define AGENTOS_ENOSYS                 -16

/* AGENTOS_ERROR 已统一为 AGENTOS_EUNKNOWN（见agentos_types.h） */

/* ==================== 日志宏定义 ==================== */

/*
 * agentos_log_level_t 现在由 types.h 提供（通过 agentos_types.h 间接包含）
 * 日志级别：DEBUG=0, INFO=1, WARN=2, ERROR=3, FATAL=4
 */

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/**
 * @brief 输出调试日志
 * @param fmt 格式字符串
 * @param ... 可变参数
 */
#define AGENTOS_LOG_DEBUG(fmt, ...) \
    do { \
        OutputDebugStringA("[DEBUG] " fmt "\n"); \
    } while(0)

/**
 * @brief 输出信息日志
 * @param fmt 格式字符串
 * @param ... 可变参数
 */
#define AGENTOS_LOG_INFO(fmt, ...) \
    do { \
        OutputDebugStringA("[INFO] " fmt "\n"); \
    } while(0)

/**
 * @brief 输出警告日志
 * @param fmt 格式字符串
 * @param ... 可变参数
 */
#define AGENTOS_LOG_WARN(fmt, ...) \
    do { \
        OutputDebugStringA("[WARN] " fmt "\n"); \
    } while(0)

/**
 * @brief 输出错误日志
 * @param fmt 格式字符串
 * @param ... 可变参数
 */
#define AGENTOS_LOG_ERROR(fmt, ...) \
    do { \
        OutputDebugStringA("[ERROR] " fmt "\n"); \
    } while(0)

#else /* POSIX */

#include <stdio.h>

#define AGENTOS_LOG_DEBUG(fmt, ...) \
    fprintf(stderr, "[DEBUG] " fmt "\n", ##__VA_ARGS__)

#define AGENTOS_LOG_INFO(fmt, ...) \
    fprintf(stderr, "[INFO] " fmt "\n", ##__VA_ARGS__)

#define AGENTOS_LOG_WARN(fmt, ...) \
    fprintf(stderr, "[WARN] " fmt "\n", ##__VA_ARGS__)

#define AGENTOS_LOG_ERROR(fmt, ...) \
    fprintf(stderr, "[ERROR] " fmt "\n", ##__VA_ARGS__)

#endif /* _WIN32 */

/* agentos_strerror 现在由 platform.h 提供（见commons/platform/include/platform.h） */

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_ATOMS_COREKERN_ERROR_H */
