// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
/**
 * @file compat.h
 * @brief 跨平台兼容性定义
 *
 * 提供编译器兼容性、平台抽象宏、位操作工具等。
 *
 * @see docs/Capital_Specifications/coding_standard/C_coding_style_standard.md
 */

#ifndef AGENTRT_UTILS_COMPAT_H
#define AGENTRT_UTILS_COMPAT_H

/* ==================== Windows 平台专用定义 ==================== */

#ifdef _WIN32
#ifndef _SSIZE_T_DEFINED
#define _SSIZE_T_DEFINED
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif
#endif

#include "atomic_compat.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 导出宏 ==================== */

#ifndef AGENTRT_API
#ifdef _WIN32
#ifdef AGENTRT_BUILD_SHARED
#define AGENTRT_API __declspec(dllexport)
#elif defined(AGENTRT_USE_SHARED)
#define AGENTRT_API __declspec(dllimport)
#else
#define AGENTRT_API
#endif
#else
#define AGENTRT_API __attribute__((visibility("default")))
#endif
#endif /* AGENTRT_API */

/* ==================== 编译器检测 ==================== */

#if defined(__GNUC__)
#define AGENTRT_COMPILER_GCC 1
#define AGENTRT_COMPILER_NAME "GCC"
#define AGENTRT_COMPILER_VERSION __GNUC__
#elif defined(__clang__)
#define AGENTRT_COMPILER_CLANG 1
#define AGENTRT_COMPILER_NAME "Clang"
#define AGENTRT_COMPILER_VERSION __clang_major__
#elif defined(_MSC_VER)
#define AGENTRT_COMPILER_MSVC 1
#define AGENTRT_COMPILER_NAME "MSVC"
#define AGENTRT_COMPILER_VERSION _MSC_VER
#else
#define AGENTRT_COMPILER_UNKNOWN 1
#define AGENTRT_COMPILER_NAME "Unknown"
#define AGENTRT_COMPILER_VERSION 0
#endif

/* ==================== 平台检测 ==================== */

#if defined(_WIN32) || defined(_WIN64)
#define AGENTRT_PLATFORM_WINDOWS 1
#define AGENTRT_PLATFORM_NAME "Windows"
#elif defined(__linux__)
#define AGENTRT_PLATFORM_LINUX 1
#define AGENTRT_PLATFORM_NAME "Linux"
#elif defined(__APPLE__)
#define AGENTRT_PLATFORM_MACOS 1
#define AGENTRT_PLATFORM_NAME "macOS"
#else
#define AGENTRT_PLATFORM_UNKNOWN 1
#define AGENTRT_PLATFORM_NAME "Unknown"
#endif

/* ==================== 编译器属性宏 ==================== */

#if defined(AGENTRT_COMPILER_GCC) || defined(AGENTRT_COMPILER_CLANG)
#ifndef AGENTRT_INLINE
#define AGENTRT_INLINE static inline __attribute__((always_inline))
#endif
#define AGENTRT_NOINLINE __attribute__((noinline))
#ifndef AGENTRT_UNUSED
#define AGENTRT_UNUSED __attribute__((unused))
#endif
#define AGENTRT_USED __attribute__((used))
#define AGENTRT_WEAK __attribute__((weak))
#define AGENTRT_PACKED __attribute__((packed))
#define AGENTRT_ALIGNED(x) __attribute__((aligned(x)))
#define AGENTRT_DEPRECATED __attribute__((deprecated))
#define AGENTRT_FALLTHROUGH __attribute__((fallthrough))
#define AGENTRT_PRINTF_FORMAT(fmt, args) \
    __attribute__((                      \
        format(__printf__, fmt, args)))
#define AGENTRT_SCANF_FORMAT(fmt, args) \
    __attribute__((                     \
        format(__scanf__, fmt, args)))
#define AGENTRT_LIKELY(x) __builtin_expect(!!(x), 1)
#define AGENTRT_UNLIKELY(x) __builtin_expect(!!(x), 0)
#define AGENTRT_PREFETCH(x) __builtin_prefetch(x)
#define AGENTRT_UNREACHABLE() __builtin_unreachable()
#define AGENTRT_ASSUME(x)            \
    do {                             \
        if (!(x))                    \
            __builtin_unreachable(); \
    } while (0)
#elif defined(AGENTRT_COMPILER_MSVC)
#ifndef AGENTRT_INLINE
#define AGENTRT_INLINE static __forceinline
#endif
#define AGENTRT_NOINLINE __declspec(noinline)
#ifndef AGENTRT_UNUSED
#define AGENTRT_UNUSED
#endif
#define AGENTRT_USED
#define AGENTRT_WEAK
#define AGENTRT_PACKED
#define AGENTRT_ALIGNED(x) __declspec(align(x))
#define AGENTRT_DEPRECATED __declspec(deprecated)
#define AGENTRT_FALLTHROUGH
#define AGENTRT_PRINTF_FORMAT(fmt, args)
#define AGENTRT_SCANF_FORMAT(fmt, args)
#define AGENTRT_LIKELY(x) (x)
#define AGENTRT_UNLIKELY(x) (x)
#define AGENTRT_PREFETCH(x)
#define AGENTRT_UNREACHABLE() __assume(0)
#define AGENTRT_ASSUME(x) __assume(x)
#else
#ifndef AGENTRT_INLINE
#define AGENTRT_INLINE static inline
#endif
#define AGENTRT_NOINLINE
#ifndef AGENTRT_UNUSED
#define AGENTRT_UNUSED
#endif
#define AGENTRT_USED
#define AGENTRT_WEAK
#define AGENTRT_PACKED
#define AGENTRT_ALIGNED(x)
#define AGENTRT_DEPRECATED
#define AGENTRT_FALLTHROUGH
#define AGENTRT_PRINTF_FORMAT(fmt, args)
#define AGENTRT_SCANF_FORMAT(fmt, args)
#define AGENTRT_LIKELY(x) (x)
#define AGENTRT_UNLIKELY(x) (x)
#define AGENTRT_PREFETCH(x)
#define AGENTRT_UNREACHABLE()
#define AGENTRT_ASSUME(x) ((void)0)
#endif

/* ==================== 线程本地存储 ==================== */

#if defined(AGENTRT_PLATFORM_WINDOWS)
#define AGENTRT_THREAD_LOCAL __declspec(thread)
#else
#define AGENTRT_THREAD_LOCAL __thread
#endif

/* ==================== POSIX函数Windows兼容映射 ==================== */

#if defined(AGENTRT_PLATFORM_WINDOWS)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#endif

#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#define strdup _strdup

#include <time.h>
#include <windows.h>

AGENTRT_API int nanosleep(const struct timespec *ts, struct timespec *rem);
AGENTRT_API char *strndup(const char *s, size_t n);
AGENTRT_API struct tm *localtime_r(const time_t *timer, struct tm *buf);

#define AGENTRT_ATOMIC_FETCH_ADD(ptr, val) atomic_fetch_add_explicit(ptr, val, memory_order_seq_cst)
#define AGENTRT_ATOMIC_FETCH_ADD64(ptr, val) \
    atomic_fetch_add_explicit(ptr, val, memory_order_seq_cst)

#ifndef _SC_PAGESIZE
#define _SC_PAGESIZE 1
#endif
#ifndef _SC_NPROCESSORS_ONLN
#define _SC_NPROCESSORS_ONLN 2
#endif
#ifndef _SC_OPEN_MAX
#define _SC_OPEN_MAX 3
#endif
#ifndef _SC_CLK_TCK
#define _SC_CLK_TCK 4
#endif
#else
#define AGENTRT_ATOMIC_FETCH_ADD(ptr, val) atomic_fetch_add_explicit(ptr, val, memory_order_seq_cst)
#define AGENTRT_ATOMIC_FETCH_ADD64(ptr, val) \
    atomic_fetch_add_explicit(ptr, val, memory_order_seq_cst)
#endif

/* ==================== 路径分隔符 ==================== */

#if defined(AGENTRT_PLATFORM_WINDOWS)
#define AGENTRT_PATH_SEP '\\'
#define AGENTRT_PATH_SEP_STR "\\"
#else
#define AGENTRT_PATH_SEP '/'
#define AGENTRT_PATH_SEP_STR "/"
#endif

#ifndef AGENTRT_PATH_MAX
#if defined(AGENTRT_PLATFORM_WINDOWS)
#define AGENTRT_PATH_MAX 260
#else
#define AGENTRT_PATH_MAX 4096
#endif
#endif

/* ==================== 对齐工具 ==================== */

/**
 * @brief 检查指针是否对齐
 * @param ptr 指针
 * @param align 对齐值（必须是2的幂）
 * @return 1对齐，0未对齐
 */
AGENTRT_INLINE int agentrt_is_aligned(const void *ptr, size_t align)
{
    return ((uintptr_t)ptr & (align - 1)) == 0;
}

/**
 * @brief 向上对齐值
 * @param value 原始值
 * @param align 对齐值（必须是2的幂）
 * @return 对齐后的值
 */
AGENTRT_INLINE size_t agentrt_align_up(size_t value, size_t align)
{
    return (value + align - 1) & ~(align - 1);
}

/**
 * @brief 向下对齐值
 * @param value 原始值
 * @param align 对齐值（必须是2的幂）
 * @return 对齐后的值
 */
AGENTRT_INLINE size_t agentrt_align_down(size_t value, size_t align)
{
    return value & ~(align - 1);
}

/* ==================== 数组工具 ==================== */

/**
 * @brief 获取数组元素数量
 * @param arr 数组
 * @return 元素数量
 */
#define AGENTRT_ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

/**
 * @brief 获取结构体成员偏移量
 * @param type 结构体类型
 * @param member 成员名
 * @return 偏移量
 */
#if defined(AGENTRT_COMPILER_GCC) || defined(AGENTRT_COMPILER_CLANG)
#define AGENTRT_OFFSETOF(type, member) __builtin_offsetof(type, member)
#else
#define AGENTRT_OFFSETOF(type, member) ((size_t) & ((type *)0)->member)
#endif

/**
 * @brief 根据成员指针获取结构体指针
 * @param ptr 成员指针
 * @param type 结构体类型
 * @param member 成员名
 * @return 结构体指针
 */
#define AGENTRT_CONTAINER_OF(ptr, type, member) \
    ((type *)((char *)(ptr) - AGENTRT_OFFSETOF(type, member)))

/* ==================== 位操作工具 ==================== */

/**
 * @brief 检查位是否设置
 * @param x 值
 * @param bit 位索引（从0开始）
 * @return 1设置，0未设置
 */
AGENTRT_INLINE int agentrt_bit_test(unsigned int x, unsigned int bit)
{
    return (int)((x >> bit) & 1U);
}

/**
 * @brief 设置位
 * @param x 值指针
 * @param bit 位索引（从0开始）
 */
AGENTRT_INLINE void agentrt_bit_set(unsigned int *x, unsigned int bit)
{
    if (x)
        *x |= (1U << bit);
}

/**
 * @brief 清除位
 * @param x 值指针
 * @param bit 位索引（从0开始）
 */
AGENTRT_INLINE void agentrt_bit_clear(unsigned int *x, unsigned int bit)
{
    if (x)
        *x &= ~(1U << bit);
}

/**
 * @brief 翻转位
 * @param x 值指针
 * @param bit 位索引（从0开始）
 */
AGENTRT_INLINE void agentrt_bit_flip(unsigned int *x, unsigned int bit)
{
    if (x)
        *x ^= (1U << bit);
}

/**
 * @brief 计算置位数量
 * @param x 值
 * @return 置位数量
 */
AGENTRT_INLINE unsigned int agentrt_popcount(unsigned int x)
{
#if defined(AGENTRT_COMPILER_GCC) || defined(AGENTRT_COMPILER_CLANG)
    return (unsigned int)__builtin_popcount(x);
#elif defined(AGENTRT_COMPILER_MSVC)
    return (unsigned int)__popcnt(x);
#else
    unsigned int count = 0;
    while (x) {
        count += x & 1U;
        x >>= 1;
    }
    return count;
#endif
}

/**
 * @brief 计算前导零数量
 * @param x 值
 * @return 前导零数量
 */
AGENTRT_INLINE unsigned int agentrt_clz(unsigned int x)
{
#if defined(AGENTRT_COMPILER_GCC) || defined(AGENTRT_COMPILER_CLANG)
    return (unsigned int)__builtin_clz(x);
#elif defined(AGENTRT_COMPILER_MSVC)
    unsigned long index;
    if (_BitScanReverse(&index, x)) {
        return 31U - (unsigned int)index;
    }
    return 32U;
#else
    unsigned int n = 0;
    if (x == 0)
        return 32U;
    while ((x & 0x80000000U) == 0) {
        n++;
        x <<= 1;
    }
    return n;
#endif
}

/**
 * @brief 计算尾随零数量
 * @param x 值
 * @return 尾随零数量
 */
AGENTRT_INLINE unsigned int agentrt_ctz(unsigned int x)
{
#if defined(AGENTRT_COMPILER_GCC) || defined(AGENTRT_COMPILER_CLANG)
    return (unsigned int)__builtin_ctz(x);
#elif defined(AGENTRT_COMPILER_MSVC)
    unsigned long index;
    if (_BitScanForward(&index, x)) {
        return (unsigned int)index;
    }
    return 32U;
#else
    unsigned int n = 0;
    if (x == 0)
        return 32U;
    while ((x & 1U) == 0) {
        n++;
        x >>= 1;
    }
    return n;
#endif
}

/* ==================== 安全字符串函数 ==================== */

/**
 * @brief 安全字符串复制
 * @param dest 目标缓冲区
 * @param src 源字符串
 * @param dest_size 目标缓冲区大小
 * @return 0成功，非0失败
 */
AGENTRT_API int agentrt_strlcpy(char *dest, const char *src, size_t dest_size);

/**
 * @brief 安全字符串连接
 * @param dest 目标缓冲区
 * @param src 源字符串
 * @param dest_size 目标缓冲区大小
 * @return 0成功，非0失败
 */
AGENTRT_API int agentrt_strlcat(char *dest, const char *src, size_t dest_size);

/**
 * @brief 安全字符串复制（带返回值）
 * @param dest 目标缓冲区
 * @param src 源字符串
 * @param dest_size 目标缓冲区大小
 * @return 目标缓冲区指针
 */
AGENTRT_API char *agentrt_strncpy_safe(char *dest, const char *src, size_t dest_size);

/* ==================== 安全内存函数 ==================== */

/**
 * @brief 安全内存设置
 * @param dest 目标缓冲区
 * @param c 填充值
 * @param dest_size 目标缓冲区大小
 * @param count 要设置的字节数
 * @return 0成功，非0失败
 */
AGENTRT_API int agentrt_memset_s(void *dest, int c, size_t dest_size, size_t count);

/**
 * @brief 安全内存复制
 * @param dest 目标缓冲区
 * @param dest_size 目标缓冲区大小
 * @param src 源缓冲区
 * @param count 要复制的字节数
 * @return 0成功，非0失败
 */
AGENTRT_API int agentrt_memcpy_s(void *dest, size_t dest_size, const void *src, size_t count);

/**
 * @brief 安全内存移动
 * @param dest 目标缓冲区
 * @param dest_size 目标缓冲区大小
 * @param src 源缓冲区
 * @param count 要移动的字节数
 * @return 0成功，非0失败
 */
AGENTRT_API int agentrt_memmove_s(void *dest, size_t dest_size, const void *src, size_t count);

/* ==================== 断言宏 ==================== */

#ifdef NDEBUG
#define AGENTRT_ASSERT(cond) ((void)0)
#define AGENTRT_ASSERT_MSG(cond, msg) ((void)0)
#else
#define AGENTRT_ASSERT(cond)                                          \
    do {                                                              \
        if (!(cond)) {                                                \
            agentrt_assert_fail(#cond, __FILE__, __LINE__, __func__); \
        }                                                             \
    } while (0)

#define AGENTRT_ASSERT_MSG(cond, msg)                                          \
    do {                                                                       \
        if (!(cond)) {                                                         \
            agentrt_assert_fail_msg(#cond, __FILE__, __LINE__, __func__, msg); \
        }                                                                      \
    } while (0)
#endif

/**
 * @brief 断言失败处理函数
 */
AGENTRT_API void agentrt_assert_fail(const char *cond, const char *file, int line,
                                     const char *func);

/**
 * @brief 断言失败处理函数（带消息）
 */
AGENTRT_API void agentrt_assert_fail_msg(const char *cond, const char *file, int line,
                                         const char *func, const char *msg);

/**
 * @brief 自定义断言处理器回调类型
 *
 * 设置后，断言失败时调用此回调而非abort()。
 * 生产环境可设置为日志记录+优雅降级。
 */
typedef void (*agentrt_assert_handler_t)(const char *cond, const char *file, int line,
                                         const char *func, const char *msg);

/**
 * @brief 设置自定义断言处理器
 */
AGENTRT_API void agentrt_set_assert_handler(agentrt_assert_handler_t handler);

/**
 * @brief 获取当前断言处理器
 */
AGENTRT_API agentrt_assert_handler_t agentrt_get_assert_handler(void);

/* ==================== 静态断言 ==================== */

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define AGENTRT_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#elif defined(AGENTRT_COMPILER_GCC) || defined(AGENTRT_COMPILER_CLANG)
#define AGENTRT_STATIC_ASSERT(cond, msg) \
    typedef char agentrt_static_assert_##__LINE__[(cond) ? 1 : -1] __attribute__((unused))
#else
#define AGENTRT_STATIC_ASSERT(cond, msg) \
    typedef char agentrt_static_assert_##__LINE__[(cond) ? 1 : -1]
#endif

/**
 * @brief 编译时检查
 */
#define AGENTRT_COMPILE_TIME_ASSERT(cond) \
    AGENTRT_STATIC_ASSERT(cond, "Compile-time assertion failed")

/**
 * @brief 检查类型大小
 */
#define AGENTRT_CHECK_SIZE(type, size) \
    AGENTRT_STATIC_ASSERT(sizeof(type) == size, "Size mismatch for " #type)

/* ==================== 调试辅助 ==================== */

#ifdef DEBUG
#define AGENTRT_DEBUG_BREAK() agentrt_debug_break()
#else
#define AGENTRT_DEBUG_BREAK() ((void)0)
#endif

/**
 * @brief 调试断点
 */
AGENTRT_API void agentrt_debug_break(void);

/* ==================== 版本信息 ==================== */

#define AGENTRT_VERSION_MAJOR 0
#define AGENTRT_VERSION_MINOR 0
#define AGENTRT_VERSION_PATCH 5
#define AGENTRT_VERSION_STRING "0.1.0"

/**
 * @brief 获取版本字符串
 */
AGENTRT_API const char *agentrt_version_string(void);

/**
 * @brief 获取构建信息
 */
AGENTRT_API const char *agentrt_build_info(void);

#ifdef __cplusplus
}
#endif

#endif /* AGENTRT_UTILS_COMPAT_H */
