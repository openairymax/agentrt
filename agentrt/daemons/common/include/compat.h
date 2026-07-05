// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
/**
 * @file compat.h
 * @brief 兼容性定义兼容层
 *
 * 本文件是 agentrt/commons/utils/compat 的兼容层，提供向后兼容的 API。
 * 新代码应直接使用 #include <compat.h>
 *
 * @see agentrt/commons/utils/compat/include/compat.h
 */

#ifndef AGENTRT_DAEMON_COMMON_COMPAT_H
#define AGENTRT_DAEMON_COMMON_COMPAT_H

#include <compat.h>
#include <stdlib.h>
#include "memory_compat.h"

/* ==================== 额外的兼容性别名 ==================== */

/* 静态断言兼容 */
#ifndef AGENTRT_STATIC_ASSERT
#define AGENTRT_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#endif

/* 调试断点兼容 */
#ifndef AGENTRT_DEBUG_BREAK
#define AGENTRT_DEBUG_BREAK() agentrt_debug_break()
#endif

/* 安全字符串函数别名 */
#define agentrt_strlcpy_safe agentrt_strncpy_safe

/* ==================== 额外的位操作宏 ==================== */

/**
 * @brief 位掩码生成
 */
#define AGENTRT_BIT_MASK(bit) (1U << (bit))

/**
 * @brief 字节掩码生成
 */
#define AGENTRT_BYTE_MASK(byte) (0xFFU << ((byte) * 8))

/**
 * @brief 字提取
 */
#define AGENTRT_GET_BYTE(value, byte) (((value) >> ((byte) * 8)) & 0xFFU)

/**
 * @brief 字设置
 */
#define AGENTRT_SET_BYTE(value, byte, val) \
    (((value) & ~AGENTRT_BYTE_MASK(byte)) | (((val) & 0xFFU) << ((byte) * 8)))

/* ==================== 编译器特定扩展 ==================== */

/**
 * @brief 函数签名（用于调试）
 */
#if defined(AGENTRT_COMPILER_GCC) || defined(AGENTRT_COMPILER_CLANG)
#define AGENTRT_FUNC_SIGNATURE __PRETTY_FUNCTION__
#elif defined(AGENTRT_COMPILER_MSVC)
#define AGENTRT_FUNC_SIGNATURE __FUNCSIG__
#else
#define AGENTRT_FUNC_SIGNATURE __func__
#endif

/* ==================== 分支预测辅助宏 ==================== */

/**
 * @brief 检查指针是否有效（非空且可读）
 */
#define AGENTRT_LIKELY_VALID(ptr) AGENTRT_LIKELY((ptr) != NULL)
#define AGENTRT_UNLIKELY_NULL(ptr) AGENTRT_UNLIKELY((ptr) == NULL)

/* ==================== 资源管理辅助 ==================== */

/**
 * @brief 自动清理属性
 */
#if defined(AGENTRT_COMPILER_GCC) || defined(AGENTRT_COMPILER_CLANG)
#define AGENTRT_CLEANUP(func) __attribute__((cleanup(func)))
#else
#define AGENTRT_CLEANUP(func)
#endif

/**
 * @brief 自动释放宏
 */
#define AGENTRT_AUTO_FREE __attribute__((cleanup(agentrt_auto_free_helper)))
static inline void agentrt_auto_free_helper(void **ptr)
{
    if (ptr && *ptr) {
        AGENTRT_FREE(*ptr);
        *ptr = NULL;
    }
}

/* ==================== 编译时字符串操作 ==================== */

/**
 * @brief 字符串化宏
 */
#define AGENTRT_STRINGIFY(x) #x
#define AGENTRT_TOSTRING(x) AGENTRT_STRINGIFY(x)

/**
 * @brief 连接宏
 */
#define AGENTRT_CONCAT(a, b) a##b
#define AGENTRT_CONCAT3(a, b, c) a##b##c

/* ==================== 类型安全宏 ==================== */

/**
 * @brief 类型安全的数组大小
 */
#define AGENTRT_ARRAY_SIZE_SAFE(arr) (sizeof(arr) / sizeof((arr)[0]) + sizeof(typeof(arr[0])) * 0)

/**
 * @brief 类型检查
 */
#define AGENTRT_TYPE_CHECK(type, expr) ((type){0}, (expr))

/* ==================== 编译器版本检查 ==================== */

/**
 * @brief 检查 GCC 版本是否至少为指定版本
 */
#if defined(AGENTRT_COMPILER_GCC)
#define AGENTRT_GCC_VERSION_AT_LEAST(major, minor) \
    (__GNUC__ > (major) || (__GNUC__ == (major) && __GNUC_MINOR__ >= (minor)))
#else
#define AGENTRT_GCC_VERSION_AT_LEAST(major, minor) 0
#endif

/**
 * @brief 检查 Clang 版本是否至少为指定版本
 */
#if defined(AGENTRT_COMPILER_CLANG)
#define AGENTRT_CLANG_VERSION_AT_LEAST(major, minor) \
    (__clang_major__ > (major) || (__clang_major__ == (major) && __clang_minor__ >= (minor)))
#else
#define AGENTRT_CLANG_VERSION_AT_LEAST(major, minor) 0
#endif

/* ==================== C11 特性检测 ==================== */

/**
 * @brief 检查是否支持 _Generic
 */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define AGENTRT_HAS_GENERIC 1
#else
#define AGENTRT_HAS_GENERIC 0
#endif

/**
 * @brief 检查是否支持 _Static_assert
 */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define AGENTRT_HAS_STATIC_ASSERT 1
#else
#define AGENTRT_HAS_STATIC_ASSERT 0
#endif

/**
 * @brief 检查是否支持匿名结构体/联合体
 */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define AGENTRT_HAS_ANONYMOUS 1
#else
#define AGENTRT_HAS_ANONYMOUS 0
#endif

#endif /* AGENTRT_DAEMON_COMMON_COMPAT_H */
