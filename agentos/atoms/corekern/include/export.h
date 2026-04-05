/**
 * @file export.h
 * @brief AgentOS 符号导出管理
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * "From data intelligence emerges."
 *
 * @note 定义了跨平台符号导出宏，支持 Windows 和 POSIX 系统
 */

#ifndef AGENTOS_EXPORT_H
#define AGENTOS_EXPORT_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 构建共享库时的符号导出
 */
#ifdef AGENTOS_BUILDING_SHARED
    #if defined(_WIN32) || defined(_WIN64)
        #define AGENTOS_API __declspec(dllexport)
    #elif defined(__GNUC__) || defined(__clang__)
        #define AGENTOS_API __attribute__((visibility("default")))
    #else
        #define AGENTOS_API
    #endif
/**
 * @brief 使用共享库时的符号导入
 */
#else
    #if defined(_WIN32) || defined(_WIN64)
        #define AGENTOS_API __declspec(dllimport)
    #else
        #define AGENTOS_API
    #endif
#endif

/**
 * @brief 内部符号标记（隐藏可见性）
 *
 * 用于标记不应被外部使用的内部符号
 */
#if defined(_WIN32) || defined(_WIN64)
    #define AGENTOS_INTERNAL
#else
    #define AGENTOS_INTERNAL __attribute__((visibility("hidden")))
#endif

#ifndef AGENTOS_API
    #define AGENTOS_API
#endif

#ifndef AGENTOS_INTERNAL
    #define AGENTOS_INTERNAL
#endif

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_EXPORT_H */
