// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file compat.h
 * @brief 兼容性定义兼容层
 * 
 * 本文件是 agentos/commons/utils/compat 的兼容层，提供向后兼容的 API。
 * 新代码应直接使用 #include <compat.h>
 * 
 * @see agentos/commons/utils/compat/include/compat.h
 */

#ifndef AGENTOS_DAEMON_COMMON_COMPAT_H
#define AGENTOS_DAEMON_COMMON_COMPAT_H

/* 包含 commons 的统一兼容性定义 */
#include <compat.h>
#include <stdlib.h>

/* ==================== 额外的兼容性别名 ==================== */

/* 静态断言兼容 */
#ifndef AGENTOS_STATIC_ASSERT
    #define AGENTOS_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#endif

/* 调试断点兼容 */
#ifndef AGENTOS_DEBUG_BREAK
    #define AGENTOS_DEBUG_BREAK() agentos_debug_break()
#endif

/* 安全字符串函数别名 */
#define agentos_strlcpy_safe  agentos_strncpy_safe

/* ==================== 额外的位操作宏 ==================== */

/**
 * @brief 位掩码生成
 */
#define AGENTOS_BIT_MASK(bit)          (1U << (bit))

/**
 * @brief 字节掩码生成
 */
#define AGENTOS_BYTE_MASK(byte)        (0xFFU << ((byte) * 8))

/**
 * @brief 字提取
 */
#define AGENTOS_GET_BYTE(value, byte)  (((value) >> ((byte) * 8)) & 0xFFU)

/**
 * @brief 字设置
 */
#define AGENTOS_SET_BYTE(value, byte, val) \
    (((value) & ~AGENTOS_BYTE_MASK(byte)) | (((val) & 0xFFU) << ((byte) * 8)))

/* ==================== 编译器特定扩展 ==================== */

/**
 * @brief 函数签名（用于调试）
 */
#if defined(AGENTOS_COMPILER_GCC) || defined(AGENTOS_COMPILER_CLANG)
    #define AGENTOS_FUNC_SIGNATURE __PRETTY_FUNCTION__
#elif defined(AGENTOS_COMPILER_MSVC)
    #define AGENTOS_FUNC_SIGNATURE __FUNCSIG__
#else
    #define AGENTOS_FUNC_SIGNATURE __func__
#endif

/* ==================== 分支预测辅助宏 ==================== */

/**
 * @brief 检查指针是否有效（非空且可读）
 */
#define AGENTOS_LIKELY_VALID(ptr)      AGENTOS_LIKELY((ptr) != NULL)
#define AGENTOS_UNLIKELY_NULL(ptr)     AGENTOS_UNLIKELY((ptr) == NULL)

/* ==================== 资源管理辅助 ==================== */

/**
 * @brief 自动清理属性
 */
#if defined(AGENTOS_COMPILER_GCC) || defined(AGENTOS_COMPILER_CLANG)
    #define AGENTOS_CLEANUP(func) __attribute__((cleanup(func)))
#else
    #define AGENTOS_CLEANUP(func)
#endif

/**
 * @brief 自动释放宏
 */
#define AGENTOS_AUTO_FREE __attribute__((cleanup(agentos_auto_free_helper)))
static inline void agentos_auto_free_helper(void** ptr) {
    if (ptr && *ptr) {
        free(*ptr);
        *ptr = NULL;
    }
}

/* ==================== 编译时字符串操作 ==================== */

/**
 * @brief 字符串化宏
 */
#define AGENTOS_STRINGIFY(x)           #x
#define AGENTOS_TOSTRING(x)            AGENTOS_STRINGIFY(x)

/**
 * @brief 连接宏
 */
#define AGENTOS_CONCAT(a, b)           a##b
#define AGENTOS_CONCAT3(a, b, c)       a##b##c

/* ==================== 类型安全宏 ==================== */

/**
 * @brief 类型安全的数组大小
 */
#define AGENTOS_ARRAY_SIZE_SAFE(arr) \
    (sizeof(arr) / sizeof((arr)[0]) + sizeof(typeof(arr[0])) * 0)

/**
 * @brief 类型检查
 */
#define AGENTOS_TYPE_CHECK(type, expr) \
    ((type){0}, (expr))

/* ==================== 编译器版本检查 ==================== */

/**
 * @brief 检查 GCC 版本是否至少为指定版本
 */
#if defined(AGENTOS_COMPILER_GCC)
    #define AGENTOS_GCC_VERSION_AT_LEAST(major, minor) \
        (__GNUC__ > (major) || (__GNUC__ == (major) && __GNUC_MINOR__ >= (minor)))
#else
    #define AGENTOS_GCC_VERSION_AT_LEAST(major, minor) 0
#endif

/**
 * @brief 检查 Clang 版本是否至少为指定版本
 */
#if defined(AGENTOS_COMPILER_CLANG)
    #define AGENTOS_CLANG_VERSION_AT_LEAST(major, minor) \
        (__clang_major__ > (major) || (__clang_major__ == (major) && __clang_minor__ >= (minor)))
#else
    #define AGENTOS_CLANG_VERSION_AT_LEAST(major, minor) 0
#endif

/* ==================== C11 特性检测 ==================== */

/**
 * @brief 检查是否支持 _Generic
 */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    #define AGENTOS_HAS_GENERIC 1
#else
    #define AGENTOS_HAS_GENERIC 0
#endif

/**
 * @brief 检查是否支持 _Static_assert
 */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    #define AGENTOS_HAS_STATIC_ASSERT 1
#else
    #define AGENTOS_HAS_STATIC_ASSERT 0
#endif

/**
 * @brief 检查是否支持匿名结构体/联合体
 */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    #define AGENTOS_HAS_ANONYMOUS 1
#else
    #define AGENTOS_HAS_ANONYMOUS 0
#endif

#endif /* AGENTOS_DAEMON_COMMON_COMPAT_H */
