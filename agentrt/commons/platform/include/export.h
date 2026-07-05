/**
 * @file export.h
 * @brief AgentRT 符号导出管理（commons平台层副本）
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
 *
 * @note 定义了跨平台符号导出宏，支持 Windows 和 POSIX 系统
 *       这是 AGENTRT_API 的权威定义源之一
 */

#ifndef AGENTRT_EXPORT_H
#define AGENTRT_EXPORT_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
#ifdef AGENTRT_BUILDING_DLL
#ifndef AGENTRT_API
#define AGENTRT_API __declspec(dllexport)
#endif
#else
#ifndef AGENTRT_API
#define AGENTRT_API __declspec(dllimport)
#endif
#endif
#elif defined(__GNUC__) || defined(__clang__)
#ifndef AGENTRT_API
#define AGENTRT_API __attribute__((visibility("default")))
#endif
#else
#ifndef AGENTRT_API
#define AGENTRT_API
#endif
#endif

#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
#define AGENTRT_INTERNAL
#elif defined(__GNUC__) || defined(__clang__)
#define AGENTRT_INTERNAL __attribute__((visibility("hidden")))
#else
#define AGENTRT_INTERNAL
#endif

#ifdef __cplusplus
}
#endif

#endif /* AGENTRT_EXPORT_H */
