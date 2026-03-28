/**
 * @file compat.h
 * @brief 跨平台兼容性宏定义
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * 
 * 提供 Windows/POSIX 兼容性支持
 */

#ifndef AGENTOS_COMPAT_H
#define AGENTOS_COMPAT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 平台检测 ==================== */
#if defined(_WIN32) || defined(_WIN64)
    #define AGENTOS_PLATFORM_WINDOWS 1
    #define AGENTOS_PLATFORM_POSIX 0
#else
    #define AGENTOS_PLATFORM_WINDOWS 0
    #define AGENTOS_PLATFORM_POSIX 1
#endif

/* ==================== 字符串函数兼容性 ==================== */
#if AGENTOS_PLATFORM_WINDOWS
    #ifndef strncasecmp
        #define strncasecmp _strnicmp
    #endif
    #ifndef strcasecmp
        #define strcasecmp _stricmp
    #endif
    #ifndef snprintf
        #define snprintf _snprintf
    #endif
    #ifndef vsnprintf
        #define vsnprintf _vsnprintf
    #endif
#endif

/* ==================== 线程局部存储 ==================== */
#if AGENTOS_PLATFORM_WINDOWS
    #define AGENTOS_THREAD_LOCAL __declspec(thread)
#else
    #define AGENTOS_THREAD_LOCAL __thread
#endif

/* ==================== 内联函数 ==================== */
#if AGENTOS_PLATFORM_WINDOWS
    #define AGENTOS_INLINE __forceinline
#else
    #define AGENTOS_INLINE static inline __attribute__((always_inline))
#endif

/* ==================== 未使用参数 ==================== */
#ifndef AGENTOS_UNUSED
#define AGENTOS_UNUSED(x) ((void)(x))
#endif

/* ==================== 格式化输出 ==================== */
#if AGENTOS_PLATFORM_WINDOWS
    #define AGENTOS_PRId64 "I64d"
    #define AGENTOS_PRIu64 "I64u"
    #define AGENTOS_PRIx64 "I64x"
#else
    #define AGENTOS_PRId64 "lld"
    #define AGENTOS_PRIu64 "llu"
    #define AGENTOS_PRIx64 "llx"
#endif

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_COMPAT_H */
