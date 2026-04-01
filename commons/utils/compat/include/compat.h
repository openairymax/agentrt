// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file compat.h
 * @brief 跨平台兼容性定义
 * 
 * 提供编译器兼容性、平台抽象宏、位操作工具等。
 * 
 * @see manuals/specifications/coding_standard/C_coding_style_guide.md
 */

#ifndef AGENTOS_UTILS_COMPAT_H
#define AGENTOS_UTILS_COMPAT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 导出宏 ==================== */

#ifdef _WIN32
    #ifdef AGENTOS_BUILD_SHARED
        #define AGENTOS_API __declspec(dllexport)
    #elif defined(AGENTOS_USE_SHARED)
        #define AGENTOS_API __declspec(dllimport)
    #else
        #define AGENTOS_API
    #endif
#else
    #define AGENTOS_API __attribute__((visibility("default")))
#endif

/* ==================== 编译器检测 ==================== */

#if defined(__GNUC__)
    #define AGENTOS_COMPILER_GCC     1
    #define AGENTOS_COMPILER_NAME    "GCC"
    #define AGENTOS_COMPILER_VERSION __GNUC__
#elif defined(__clang__)
    #define AGENTOS_COMPILER_CLANG   1
    #define AGENTOS_COMPILER_NAME    "Clang"
    #define AGENTOS_COMPILER_VERSION __clang_major__
#elif defined(_MSC_VER)
    #define AGENTOS_COMPILER_MSVC    1
    #define AGENTOS_COMPILER_NAME    "MSVC"
    #define AGENTOS_COMPILER_VERSION _MSC_VER
#else
    #define AGENTOS_COMPILER_UNKNOWN 1
    #define AGENTOS_COMPILER_NAME    "Unknown"
    #define AGENTOS_COMPILER_VERSION 0
#endif

/* ==================== 平台检测 ==================== */

#if defined(_WIN32) || defined(_WIN64)
    #define AGENTOS_PLATFORM_WINDOWS 1
    #define AGENTOS_PLATFORM_NAME    "Windows"
#elif defined(__linux__)
    #define AGENTOS_PLATFORM_LINUX   1
    #define AGENTOS_PLATFORM_NAME    "Linux"
#elif defined(__APPLE__)
    #define AGENTOS_PLATFORM_MACOS   1
    #define AGENTOS_PLATFORM_NAME    "macOS"
#else
    #define AGENTOS_PLATFORM_UNKNOWN 1
    #define AGENTOS_PLATFORM_NAME    "Unknown"
#endif

/* ==================== 编译器属性宏 ==================== */

#if defined(AGENTOS_COMPILER_GCC) || defined(AGENTOS_COMPILER_CLANG)
    #define AGENTOS_INLINE          static inline __attribute__((always_inline))
    #define AGENTOS_NOINLINE        __attribute__((noinline))
    #define AGENTOS_UNUSED          __attribute__((unused))
    #define AGENTOS_USED            __attribute__((used))
    #define AGENTOS_WEAK            __attribute__((weak))
    #define AGENTOS_PACKED          __attribute__((packed))
    #define AGENTOS_ALIGNED(x)      __attribute__((aligned(x)))
    #define AGENTOS_DEPRECATED      __attribute__((deprecated))
    #define AGENTOS_FALLTHROUGH     __attribute__((fallthrough))
    #define AGENTOS_PRINTF_FORMAT(fmt, args) __attribute__((format(printf, fmt, args)))
    #define AGENTOS_SCANF_FORMAT(fmt, args)  __attribute__((format(scanf, fmt, args)))
    #define AGENTOS_LIKELY(x)       __builtin_expect(!!(x), 1)
    #define AGENTOS_UNLIKELY(x)     __builtin_expect(!!(x), 0)
    #define AGENTOS_PREFETCH(x)     __builtin_prefetch(x)
    #define AGENTOS_UNREACHABLE()   __builtin_unreachable()
    #define AGENTOS_ASSUME(x)       do { if (!(x)) __builtin_unreachable(); } while (0)
#elif defined(AGENTOS_COMPILER_MSVC)
    #define AGENTOS_INLINE          static __forceinline
    #define AGENTOS_NOINLINE        __declspec(noinline)
    #define AGENTOS_UNUSED
    #define AGENTOS_USED
    #define AGENTOS_WEAK
    #define AGENTOS_PACKED
    #define AGENTOS_ALIGNED(x)      __declspec(align(x))
    #define AGENTOS_DEPRECATED      __declspec(deprecated)
    #define AGENTOS_FALLTHROUGH
    #define AGENTOS_PRINTF_FORMAT(fmt, args)
    #define AGENTOS_SCANF_FORMAT(fmt, args)
    #define AGENTOS_LIKELY(x)       (x)
    #define AGENTOS_UNLIKELY(x)     (x)
    #define AGENTOS_PREFETCH(x)
    #define AGENTOS_UNREACHABLE()   __assume(0)
    #define AGENTOS_ASSUME(x)       __assume(x)
#else
    #define AGENTOS_INLINE          static inline
    #define AGENTOS_NOINLINE
    #define AGENTOS_UNUSED
    #define AGENTOS_USED
    #define AGENTOS_WEAK
    #define AGENTOS_PACKED
    #define AGENTOS_ALIGNED(x)
    #define AGENTOS_DEPRECATED
    #define AGENTOS_FALLTHROUGH
    #define AGENTOS_PRINTF_FORMAT(fmt, args)
    #define AGENTOS_SCANF_FORMAT(fmt, args)
    #define AGENTOS_LIKELY(x)       (x)
    #define AGENTOS_UNLIKELY(x)     (x)
    #define AGENTOS_PREFETCH(x)
    #define AGENTOS_UNREACHABLE()
    #define AGENTOS_ASSUME(x)       ((void)0)
#endif

/* ==================== 线程本地存储 ==================== */

#if defined(AGENTOS_PLATFORM_WINDOWS)
    #define AGENTOS_THREAD_LOCAL __declspec(thread)
#else
    #define AGENTOS_THREAD_LOCAL __thread
#endif

/* ==================== 路径分隔符 ==================== */

#if defined(AGENTOS_PLATFORM_WINDOWS)
    #define AGENTOS_PATH_SEP     '\\'
    #define AGENTOS_PATH_SEP_STR "\\"
#else
    #define AGENTOS_PATH_SEP     '/'
    #define AGENTOS_PATH_SEP_STR "/"
#endif

#ifndef AGENTOS_PATH_MAX
    #if defined(AGENTOS_PLATFORM_WINDOWS)
        #define AGENTOS_PATH_MAX 260
    #else
        #define AGENTOS_PATH_MAX 4096
    #endif
#endif

/* ==================== 对齐工具 ==================== */

/**
 * @brief 检查指针是否对齐
 * @param ptr 指针
 * @param align 对齐值（必须是2的幂）
 * @return 1对齐，0未对齐
 */
AGENTOS_INLINE int agentos_is_aligned(const void* ptr, size_t align) {
    return ((uintptr_t)ptr & (align - 1)) == 0;
}

/**
 * @brief 向上对齐值
 * @param value 原始值
 * @param align 对齐值（必须是2的幂）
 * @return 对齐后的值
 */
AGENTOS_INLINE size_t agentos_align_up(size_t value, size_t align) {
    return (value + align - 1) & ~(align - 1);
}

/**
 * @brief 向下对齐值
 * @param value 原始值
 * @param align 对齐值（必须是2的幂）
 * @return 对齐后的值
 */
AGENTOS_INLINE size_t agentos_align_down(size_t value, size_t align) {
    return value & ~(align - 1);
}

/* ==================== 数组工具 ==================== */

/**
 * @brief 获取数组元素数量
 * @param arr 数组
 * @return 元素数量
 */
#define AGENTOS_ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

/**
 * @brief 获取结构体成员偏移量
 * @param type 结构体类型
 * @param member 成员名
 * @return 偏移量
 */
#if defined(AGENTOS_COMPILER_GCC) || defined(AGENTOS_COMPILER_CLANG)
    #define AGENTOS_OFFSETOF(type, member) __builtin_offsetof(type, member)
#else
    #define AGENTOS_OFFSETOF(type, member) ((size_t)&((type*)0)->member)
#endif

/**
 * @brief 根据成员指针获取结构体指针
 * @param ptr 成员指针
 * @param type 结构体类型
 * @param member 成员名
 * @return 结构体指针
 */
#define AGENTOS_CONTAINER_OF(ptr, type, member) \
    ((type*)((char*)(ptr) - AGENTOS_OFFSETOF(type, member)))

/* ==================== 位操作工具 ==================== */

/**
 * @brief 检查位是否设置
 * @param x 值
 * @param bit 位索引（从0开始）
 * @return 1设置，0未设置
 */
AGENTOS_INLINE int agentos_bit_test(unsigned int x, unsigned int bit) {
    return (int)((x >> bit) & 1U);
}

/**
 * @brief 设置位
 * @param x 值指针
 * @param bit 位索引（从0开始）
 */
AGENTOS_INLINE void agentos_bit_set(unsigned int* x, unsigned int bit) {
    if (x) *x |= (1U << bit);
}

/**
 * @brief 清除位
 * @param x 值指针
 * @param bit 位索引（从0开始）
 */
AGENTOS_INLINE void agentos_bit_clear(unsigned int* x, unsigned int bit) {
    if (x) *x &= ~(1U << bit);
}

/**
 * @brief 翻转位
 * @param x 值指针
 * @param bit 位索引（从0开始）
 */
AGENTOS_INLINE void agentos_bit_flip(unsigned int* x, unsigned int bit) {
    if (x) *x ^= (1U << bit);
}

/**
 * @brief 计算置位数量
 * @param x 值
 * @return 置位数量
 */
AGENTOS_INLINE unsigned int agentos_popcount(unsigned int x) {
#if defined(AGENTOS_COMPILER_GCC) || defined(AGENTOS_COMPILER_CLANG)
    return (unsigned int)__builtin_popcount(x);
#elif defined(AGENTOS_COMPILER_MSVC)
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
AGENTOS_INLINE unsigned int agentos_clz(unsigned int x) {
#if defined(AGENTOS_COMPILER_GCC) || defined(AGENTOS_COMPILER_CLANG)
    return (unsigned int)__builtin_clz(x);
#elif defined(AGENTOS_COMPILER_MSVC)
    unsigned long index;
    if (_BitScanReverse(&index, x)) {
        return 31U - (unsigned int)index;
    }
    return 32U;
#else
    unsigned int n = 0;
    if (x == 0) return 32U;
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
AGENTOS_INLINE unsigned int agentos_ctz(unsigned int x) {
#if defined(AGENTOS_COMPILER_GCC) || defined(AGENTOS_COMPILER_CLANG)
    return (unsigned int)__builtin_ctz(x);
#elif defined(AGENTOS_COMPILER_MSVC)
    unsigned long index;
    if (_BitScanForward(&index, x)) {
        return (unsigned int)index;
    }
    return 32U;
#else
    unsigned int n = 0;
    if (x == 0) return 32U;
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
AGENTOS_API int agentos_strlcpy(char* dest, const char* src, size_t dest_size);

/**
 * @brief 安全字符串连接
 * @param dest 目标缓冲区
 * @param src 源字符串
 * @param dest_size 目标缓冲区大小
 * @return 0成功，非0失败
 */
AGENTOS_API int agentos_strlcat(char* dest, const char* src, size_t dest_size);

/**
 * @brief 安全字符串复制（带返回值）
 * @param dest 目标缓冲区
 * @param src 源字符串
 * @param dest_size 目标缓冲区大小
 * @return 目标缓冲区指针
 */
AGENTOS_API char* agentos_strncpy_safe(char* dest, const char* src, size_t dest_size);

/* ==================== 安全内存函数 ==================== */

/**
 * @brief 安全内存设置
 * @param dest 目标缓冲区
 * @param c 填充值
 * @param dest_size 目标缓冲区大小
 * @param count 要设置的字节数
 * @return 0成功，非0失败
 */
AGENTOS_API int agentos_memset_s(void* dest, int c, size_t dest_size, size_t count);

/**
 * @brief 安全内存复制
 * @param dest 目标缓冲区
 * @param dest_size 目标缓冲区大小
 * @param src 源缓冲区
 * @param count 要复制的字节数
 * @return 0成功，非0失败
 */
AGENTOS_API int agentos_memcpy_s(void* dest, size_t dest_size, const void* src, size_t count);

/**
 * @brief 安全内存移动
 * @param dest 目标缓冲区
 * @param dest_size 目标缓冲区大小
 * @param src 源缓冲区
 * @param count 要移动的字节数
 * @return 0成功，非0失败
 */
AGENTOS_API int agentos_memmove_s(void* dest, size_t dest_size, const void* src, size_t count);

/* ==================== 断言宏 ==================== */

#ifdef NDEBUG
    #define AGENTOS_ASSERT(cond)            ((void)0)
    #define AGENTOS_ASSERT_MSG(cond, msg)   ((void)0)
#else
    #define AGENTOS_ASSERT(cond) \
        do { \
            if (!(cond)) { \
                agentos_assert_fail(#cond, __FILE__, __LINE__, __func__); \
            } \
        } while (0)
    
    #define AGENTOS_ASSERT_MSG(cond, msg) \
        do { \
            if (!(cond)) { \
                agentos_assert_fail_msg(#cond, __FILE__, __LINE__, __func__, msg); \
            } \
        } while (0)
#endif

/**
 * @brief 断言失败处理函数
 */
AGENTOS_API void agentos_assert_fail(const char* cond, const char* file, int line, const char* func);

/**
 * @brief 断言失败处理函数（带消息）
 */
AGENTOS_API void agentos_assert_fail_msg(const char* cond, const char* file, int line, const char* func, const char* msg);

/* ==================== 静态断言 ==================== */

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    #define AGENTOS_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#elif defined(AGENTOS_COMPILER_GCC) || defined(AGENTOS_COMPILER_CLANG)
    #define AGENTOS_STATIC_ASSERT(cond, msg) \
        typedef char agentos_static_assert_##__LINE__[(cond) ? 1 : -1] __attribute__((unused))
#else
    #define AGENTOS_STATIC_ASSERT(cond, msg) \
        typedef char agentos_static_assert_##__LINE__[(cond) ? 1 : -1]
#endif

/**
 * @brief 编译时检查
 */
#define AGENTOS_COMPILE_TIME_ASSERT(cond) AGENTOS_STATIC_ASSERT(cond, "Compile-time assertion failed")

/**
 * @brief 检查类型大小
 */
#define AGENTOS_CHECK_SIZE(type, size) AGENTOS_STATIC_ASSERT(sizeof(type) == size, "Size mismatch for " #type)

/* ==================== 调试辅助 ==================== */

#ifdef DEBUG
    #define AGENTOS_DEBUG_BREAK() agentos_debug_break()
#else
    #define AGENTOS_DEBUG_BREAK() ((void)0)
#endif

/**
 * @brief 调试断点
 */
AGENTOS_API void agentos_debug_break(void);

/* ==================== 版本信息 ==================== */

#define AGENTOS_VERSION_MAJOR  1
#define AGENTOS_VERSION_MINOR  0
#define AGENTOS_VERSION_PATCH  0
#define AGENTOS_VERSION_STRING "1.0.0"

/**
 * @brief 获取版本字符串
 */
AGENTOS_API const char* agentos_version_string(void);

/**
 * @brief 获取构建信息
 */
AGENTOS_API const char* agentos_build_info(void);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_UTILS_COMPAT_H */
