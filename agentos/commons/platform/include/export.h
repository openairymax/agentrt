/**
 * @file export.h
 * @brief AgentOS 符号导出管理（commons平台层副本）
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @note 定义了跨平台符号导出宏，支持 Windows 和 POSIX 系统
 *       这是 AGENTOS_API 的权威定义源之一
 */

#ifndef AGENTOS_EXPORT_H
#define AGENTOS_EXPORT_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
    #ifdef AGENTOS_BUILDING_DLL
        #define AGENTOS_API __declspec(dllexport)
    #else
        #define AGENTOS_API __declspec(dllimport)
    #endif
#elif defined(__GNUC__) || defined(__clang__)
    #define AGENTOS_API __attribute__((visibility("default")))
#else
    #define AGENTOS_API
#endif

#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
    #define AGENTOS_INTERNAL
#elif defined(__GNUC__) || defined(__clang__)
    #define AGENTOS_INTERNAL __attribute__((visibility("hidden")))
#else
    #define AGENTOS_INTERNAL
#endif

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_EXPORT_H */
