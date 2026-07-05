/**
 * @file error.h
 * @brief 内核错误码定义
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
 *
 * "From data intelligence emerges."
 *
 * @note 定义了 AgentRT 内核统一的错误码体系，遵循 POSIX 错误码语义
 */

#ifndef AGENTRT_ATOMS_COREKERN_ERROR_H
#define AGENTRT_ATOMS_COREKERN_ERROR_H

#include "export.h"

#include <stdint.h>
/* 统一错误码定义：使用commons权威基础库 */
#include "../../../commons/include/agentrt_types.h"

/* 包含commons统一错误码定义 */
#include "../../../commons/utils/error/include/error.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief API 版本号
 */
#define AGENTRT_COREKERN_API_VERSION 1

/**
 * @brief AgentRT 统一错误类型
 *
 * 使用 int32_t 确保跨平台一致性
 * 正值表示成功，负值表示错误
 */
/* agentrt_error_t 和基础错误码现在由 agentrt_types.h 提供 */

/*
 * 以下为corekern特有的扩展错误码（不在agentrt_types.h中）
 * 基础错误码（SUCCESS, EINVAL, ENOMEM等）已统一到agentrt_types.h
 */

/* 以下错误码现已统一到 centralized error.h (commons/utils/error/include/error.h) */
/* AGENTRT_EINTR: now AGENTRT_ERR_INTERRUPTED in centralized error.h */
/* AGENTRT_EBADF: now AGENTRT_ERR_SYS_FILE in centralized error.h */
/* AGENTRT_ERESOURCE: now AGENTRT_ERR_SYS_RESOURCE in centralized error.h */
/* AGENTRT_ENOSYS: now AGENTRT_ERR_NOT_IMPLEMENTED in centralized error.h */
/* AGENTRT_EFAIL: now AGENTRT_ERR_FAIL in centralized error.h */

#define AGENTRT_ECYCLE -35

/* AGENTRT_ERROR 已统一为 AGENTRT_EUNKNOWN（见agentrt_types.h） */

/* ==================== 日志宏定义 ==================== */

/*
 * agentrt_log_level_t 现在由 types.h 提供（通过 agentrt_types.h 间接包含）
 * 日志级别：DEBUG=0, INFO=1, WARN=2, ERROR=3, FATAL=4
 */

/* 统一日志系统委托：Windows 和 POSIX 均通过 logging_compat.h 接入
 * 统一日志系统（彩色输出 + CLOCK_REALTIME 时间戳）。
 *
 * Task #121 修复: 原 Windows 分支使用 OutputDebugStringA 绕过统一日志系统，
 * 导致 Windows 平台日志无彩色、无时间戳、不经过 log_write() 管线。现统一
 * 委托 logging_compat.h，与 POSIX 路径完全一致。
 *
 * logging_compat.h 的 #ifndef 保护会跳过已由 logging.h/svc_logger.h
 * 先行定义的 AGENTRT_LOG_* 宏，避免重复定义；若未先行包含，则提供
 * 委托到 agentrt_log_write() 的真实实现（彩色输出 + CLOCK_REALTIME 时间戳）。 */
#include "../../../commons/utils/logging/include/logging_compat.h"

/* agentrt_strerror 现在由 platform.h 提供（见commons/platform/include/platform.h） */

#ifdef __cplusplus
}
#endif

#endif /* AGENTRT_ATOMS_COREKERN_ERROR_H */
