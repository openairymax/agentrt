#ifndef AGENTRT_ATOMIC_COMPAT_H
#define AGENTRT_ATOMIC_COMPAT_H

#pragma GCC system_header

/**
 * @file atomic_compat.h
 * @brief 跨平台原子操作兼容层
 *
 * 提供 C11 stdatomic.h 的跨平台兼容实现：
 * - C11+ (Linux/macOS): 使用 <stdatomic.h>
 * - Windows: 使用 Interlocked API (intrin.h)
 * - POSIX fallback: 使用 __atomic builtins (GCC/Clang)
 *
 * 统一类型系统：
 * - atomic_bool, atomic_int, atomic_uint, atomic_long, atomic_ulong
 * - atomic_int64_t, atomic_uint64_t, atomic_size_t
 * - atomic_uint_fast64_t, atomic_uint_fast32_t
 * - atomic_double
 *
 * @note 所有操作均为线程安全
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 平台检测与内存顺序定义 ==================== */

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L) && !defined(_WIN32) && \
    !defined(AGENTRT_NO_STDATOMIC) && !defined(_MSC_VER)

/* C11 环境：使用系统 stdatomic.h（跳过本地shim） */
#ifndef AGENTRT_COREKERN_STDATOMIC_SHIM
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#include_next <stdatomic.h>
#pragma GCC diagnostic pop
#endif

#define AGENTRT_USE_STDATOMIC 1

static inline int atomic_load_32(volatile _Atomic int *ptr, memory_order order)
{
    return (int)atomic_load_explicit(ptr, order);
}
static inline void atomic_store_32(volatile _Atomic int *ptr, int val, memory_order order)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overflow="
    atomic_store_explicit(ptr, val, order);
#pragma GCC diagnostic pop
}
static inline int atomic_fetch_add_32(volatile _Atomic int *ptr, int val, memory_order order)
{
    return (int)atomic_fetch_add_explicit(ptr, val, order);
}
static inline int atomic_fetch_sub_32(volatile _Atomic int *ptr, int val, memory_order order)
{
    return (int)atomic_fetch_sub_explicit(ptr, val, order);
}
static inline int atomic_exchange_32(volatile _Atomic int *ptr, int val, memory_order order)
{
    return (int)atomic_exchange_explicit(ptr, val, order);
}
static inline _Bool atomic_compare_exchange_strong_32(volatile _Atomic int *ptr, int *expected,
                                                      int desired, memory_order succ,
                                                      memory_order fail)
{
    return atomic_compare_exchange_strong_explicit(ptr, expected, desired, succ, fail);
}
static inline int64_t atomic_load_64(volatile _Atomic int64_t *ptr, memory_order order)
{
    return (int64_t)atomic_load_explicit(ptr, order);
}
static inline void atomic_store_64(volatile _Atomic int64_t *ptr, int64_t val, memory_order order)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overflow="
    atomic_store_explicit(ptr, val, order);
#pragma GCC diagnostic pop
}
static inline int64_t atomic_fetch_add_64(volatile _Atomic int64_t *ptr, int64_t val,
                                          memory_order order)
{
    return (int64_t)atomic_fetch_add_explicit(ptr, val, order);
}
static inline int64_t atomic_fetch_sub_64(volatile _Atomic int64_t *ptr, int64_t val,
                                          memory_order order)
{
    return (int64_t)atomic_fetch_sub_explicit(ptr, val, order);
}
static inline int64_t atomic_exchange_64(volatile _Atomic int64_t *ptr, int64_t val,
                                         memory_order order)
{
    return (int64_t)atomic_exchange_explicit(ptr, val, order);
}
static inline _Bool atomic_compare_exchange_strong_64(volatile _Atomic int64_t *ptr,
                                                      int64_t *expected, int64_t desired,
                                                      memory_order succ, memory_order fail)
{
    return atomic_compare_exchange_strong_explicit(ptr, expected, desired, succ, fail);
}
static inline char atomic_load_8(volatile _Atomic char *ptr, memory_order order)
{
    return (char)atomic_load_explicit(ptr, order);
}
static inline void atomic_store_8(volatile _Atomic char *ptr, char val, memory_order order)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overflow="
    atomic_store_explicit(ptr, val, order);
#pragma GCC diagnostic pop
}
static inline char atomic_fetch_add_8(volatile _Atomic char *ptr, char val, memory_order order)
{
    return (char)atomic_fetch_add_explicit(ptr, val, order);
}
static inline char atomic_fetch_sub_8(volatile _Atomic char *ptr, char val, memory_order order)
{
    return (char)atomic_fetch_sub_explicit(ptr, val, order);
}
static inline char atomic_exchange_8(volatile _Atomic char *ptr, char val, memory_order order)
{
    return (char)atomic_exchange_explicit(ptr, val, order);
}
static inline _Bool atomic_compare_exchange_strong_8(volatile _Atomic char *ptr, char *expected,
                                                     char desired, memory_order succ,
                                                     memory_order fail)
{
    return atomic_compare_exchange_strong_explicit(ptr, expected, desired, succ, fail);
}
static inline short atomic_load_16(volatile _Atomic short *ptr, memory_order order)
{
    return (short)atomic_load_explicit(ptr, order);
}
static inline void atomic_store_16(volatile _Atomic short *ptr, short val, memory_order order)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overflow="
    atomic_store_explicit(ptr, val, order);
#pragma GCC diagnostic pop
}
static inline short atomic_fetch_add_16(volatile _Atomic short *ptr, short val, memory_order order)
{
    return (short)atomic_fetch_add_explicit(ptr, val, order);
}
static inline short atomic_fetch_sub_16(volatile _Atomic short *ptr, short val, memory_order order)
{
    return (short)atomic_fetch_sub_explicit(ptr, val, order);
}
static inline short atomic_exchange_16(volatile _Atomic short *ptr, short val, memory_order order)
{
    return (short)atomic_exchange_explicit(ptr, val, order);
}
static inline _Bool atomic_compare_exchange_strong_16(volatile _Atomic short *ptr, short *expected,
                                                      short desired, memory_order succ,
                                                      memory_order fail)
{
    return atomic_compare_exchange_strong_explicit(ptr, expected, desired, succ, fail);
}

static inline void *atomic_load_ptr(_Atomic void **ptr, memory_order order)
{
    return (void *)atomic_load_explicit(ptr, order);
}
static inline void atomic_store_ptr(_Atomic void **ptr, void *val, memory_order order)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overflow="
    atomic_store_explicit(ptr, val, order);
#pragma GCC diagnostic pop
}
static inline void *atomic_exchange_ptr(_Atomic void **ptr, void *val, memory_order order)
{
    return (void *)atomic_exchange_explicit(ptr, val, order);
}
static inline _Bool atomic_compare_exchange_strong_ptr(_Atomic void **ptr, void **expected,
                                                       void *desired, memory_order succ,
                                                       memory_order fail)
{
    return atomic_compare_exchange_strong_explicit(ptr, expected, desired, succ, fail);
}

#else

/* 非 C11 环境或 Windows：定义自定义 memory_order 枚举 */
typedef enum {
    memory_order_relaxed = 0,
    memory_order_consume = 1,
    memory_order_acquire = 2,
    memory_order_release = 3,
    memory_order_acq_rel = 4,
    memory_order_seq_cst = 5
} memory_order;

#define AGENTRT_USE_STDATOMIC 0

#endif

/* ====================================================================
 * Windows 实现: 使用 Interlocked API
 * ==================================================================== */
#if defined(_WIN32)

#include <intrin.h>
#include <windows.h>

#define _Atomic volatile

/* ==================== 8位原子操作（Windows） ==================== */

static inline char atomic_load_8(volatile char *ptr, memory_order order)
{
    (void)order;
    return *ptr;
}

static inline void atomic_store_8(volatile char *ptr, char value, memory_order order)
{
    (void)order;
    *ptr = value;
}

static inline char atomic_exchange_8(volatile char *ptr, char desired, memory_order order)
{
    (void)order;
#ifdef _WIN64
    volatile short *p = (volatile short *)((uintptr_t)ptr & ~(uintptr_t)1);
    return (char)InterlockedExchange16(p, (short)desired);
#else
    return (char)InterlockedExchange8(ptr, (BYTE)desired);
#endif
}

static inline int atomic_compare_exchange_strong_8(volatile char *ptr, char *expected, char desired,
                                                   memory_order success, memory_order failure)
{
    (void)success;
    (void)failure;
#ifdef _WIN64
    char old = *ptr;
    if (old == *expected) {
        *ptr = desired;
        return 1;
    }
    *expected = old;
    return 0;
#else
    char old = (char)InterlockedCompareExchange8(ptr, (BYTE)desired, (BYTE)*expected);
    if (old == *expected)
        return 1;
    *expected = old;
    return 0;
#endif
}

static inline char atomic_fetch_add_8(volatile char *ptr, char value, memory_order order)
{
    (void)order;
#ifdef _WIN64
    volatile short *p = (volatile short *)((uintptr_t)ptr & ~(uintptr_t)1);
    return (char)InterlockedExchangeAdd16(p, (short)value);
#else
    return (char)InterlockedExchangeAdd8(ptr, (char)value);
#endif
}

static inline char atomic_fetch_sub_8(volatile char *ptr, char value, memory_order order)
{
    (void)order;
#ifdef _WIN64
    volatile short *p = (volatile short *)((uintptr_t)ptr & ~(uintptr_t)1);
    return (char)InterlockedExchangeAdd16(p, -(short)value);
#else
    return (char)InterlockedExchangeAdd8(ptr, -(char)value);
#endif
}

/* ==================== 16位原子操作（Windows） ==================== */

static inline short atomic_load_16(volatile short *ptr, memory_order order)
{
    (void)order;
    return *ptr;
}

static inline void atomic_store_16(volatile short *ptr, short value, memory_order order)
{
    (void)order;
    *ptr = value;
}

static inline short atomic_exchange_16(volatile short *ptr, short desired, memory_order order)
{
    (void)order;
    return (short)InterlockedExchange16((volatile SHORT *)ptr, desired);
}

static inline int atomic_compare_exchange_strong_16(volatile short *ptr, short *expected,
                                                    short desired, memory_order success,
                                                    memory_order failure)
{
    (void)success;
    (void)failure;
    short old = (short)InterlockedCompareExchange16((volatile SHORT *)ptr, desired, *expected);
    if (old == *expected)
        return 1;
    *expected = old;
    return 0;
}

static inline short atomic_fetch_add_16(volatile short *ptr, short value, memory_order order)
{
    (void)order;
    return (short)InterlockedExchangeAdd16((volatile SHORT *)ptr, value);
}

static inline short atomic_fetch_sub_16(volatile short *ptr, short value, memory_order order)
{
    (void)order;
    return (short)InterlockedExchangeAdd16((volatile SHORT *)ptr, -value);
}

/* ==================== 32位原子操作（Windows） ==================== */

static inline long atomic_load_32(volatile long *ptr, memory_order order)
{
    (void)order;
    MemoryBarrier();
    return *ptr;
}

static inline void atomic_store_32(volatile long *ptr, long value, memory_order order)
{
    (void)order;
    *ptr = value;
    MemoryBarrier();
}

static inline long atomic_exchange_32(volatile long *ptr, long desired, memory_order order)
{
    (void)order;
    return (long)InterlockedExchange((volatile LONG *)ptr, desired);
}

static inline int atomic_compare_exchange_strong_32(volatile long *ptr, long *expected,
                                                    long desired, memory_order success,
                                                    memory_order failure)
{
    (void)success;
    (void)failure;
    long old = (long)InterlockedCompareExchange((volatile LONG *)ptr, desired, *expected);
    if (old == *expected)
        return 1;
    *expected = old;
    return 0;
}

static inline long atomic_fetch_add_32(volatile long *ptr, long value, memory_order order)
{
    (void)order;
    return (long)InterlockedExchangeAdd((volatile LONG *)ptr, value);
}

static inline long atomic_fetch_sub_32(volatile long *ptr, long value, memory_order order)
{
    (void)order;
    return (long)InterlockedExchangeAdd((volatile LONG *)ptr, -value);
}

/* ==================== 64位原子操作（Windows） ==================== */

static inline __int64 atomic_load_64(volatile __int64 *ptr, memory_order order)
{
    (void)order;
    MemoryBarrier();
    return *ptr;
}

static inline void atomic_store_64(volatile __int64 *ptr, __int64 value, memory_order order)
{
    (void)order;
    *ptr = value;
    MemoryBarrier();
}

static inline __int64 atomic_exchange_64(volatile __int64 *ptr, __int64 desired, memory_order order)
{
    (void)order;
    return (__int64)InterlockedExchange64((volatile LONGLONG *)ptr, desired);
}

static inline int atomic_compare_exchange_strong_64(volatile __int64 *ptr, __int64 *expected,
                                                    __int64 desired, memory_order success,
                                                    memory_order failure)
{
    (void)success;
    (void)failure;
    __int64 old =
        (__int64)InterlockedCompareExchange64((volatile LONGLONG *)ptr, desired, *expected);
    if (old == *expected)
        return 1;
    *expected = old;
    return 0;
}

static inline __int64 atomic_fetch_add_64(volatile __int64 *ptr, __int64 value, memory_order order)
{
    (void)order;
    return (__int64)InterlockedExchangeAdd64((volatile LONGLONG *)ptr, value);
}

static inline __int64 atomic_fetch_sub_64(volatile __int64 *ptr, __int64 value, memory_order order)
{
    (void)order;
    return (__int64)InterlockedExchangeAdd64((volatile LONGLONG *)ptr, -value);
}

/* ==================== 指针原子操作（Windows） ==================== */

static inline void *atomic_load_ptr(void *volatile *ptr, memory_order order)
{
    (void)order;
    MemoryBarrier();
    return *ptr;
}

static inline void atomic_store_ptr(void *volatile *ptr, void *value, memory_order order)
{
    (void)order;
    *ptr = value;
    MemoryBarrier();
}

static inline void *atomic_exchange_ptr(void *volatile *ptr, void *desired, memory_order order)
{
    (void)order;
    return InterlockedExchangePointer(ptr, desired);
}

static inline int atomic_compare_exchange_strong_ptr(void *volatile *ptr, void **expected,
                                                     void *desired, memory_order success,
                                                     memory_order failure)
{
    (void)success;
    (void)failure;
    void *old = InterlockedCompareExchangePointer(ptr, desired, *expected);
    if (old == *expected)
        return 1;
    *expected = old;
    return 0;
}

/* ==================== size_t 原子操作（Windows） ==================== */

#ifdef _WIN64
#define atomic_fetch_add_size(p, v, o) (__int64)atomic_fetch_add_64((__int64 *)(p), (__int64)(v), o)
#define atomic_fetch_sub_size(p, v, o) (__int64)atomic_fetch_sub_64((__int64 *)(p), (__int64)(v), o)
#define atomic_load_size(p, o) (size_t) atomic_load_64((__int64 *)(p), o)
#define atomic_store_size(p, v, o) atomic_store_64((__int64 *)(p), (__int64)(v), o)
#else
#define atomic_fetch_add_size(p, v, o) (long)atomic_fetch_add_32((long *)(p), (long)(v), o)
#define atomic_fetch_sub_size(p, v, o) (long)atomic_fetch_sub_32((long *)(p), (long)(v), o)
#define atomic_load_size(p, o) (size_t) atomic_load_32((long *)(p), o)
#define atomic_store_size(p, v, o) atomic_store_32((long *)(p), (long)(v), o)
#endif

/* ==================== double 原子操作（Windows） ==================== */

static inline double atomic_load_double(volatile double *ptr, memory_order order)
{
    (void)order;
    return *ptr;
}

static inline void atomic_store_double(volatile double *ptr, double value, memory_order order)
{
    (void)order;
    *ptr = value;
}

static inline double atomic_fetch_add_double(volatile double *ptr, double value, memory_order order)
{
    (void)order;
    double old = *ptr;
    *ptr += value;
    return old;
}

/* ====================================================================
 * POSIX fallback 实现: 使用 GCC/Clang __atomic builtins
 * 当 AGENTRT_USE_STDATOMIC=0 且非 Windows 时使用
 * ==================================================================== */
#elif !AGENTRT_USE_STDATOMIC

#define _Atomic volatile

static inline char atomic_load_8(volatile char *ptr, memory_order order)
{
    return __atomic_load_n(ptr, (int)order);
}
static inline void atomic_store_8(volatile char *ptr, char value, memory_order order)
{
    __atomic_store_n(ptr, value, (int)order);
}
static inline char atomic_exchange_8(volatile char *ptr, char desired, memory_order order)
{
    return (char)__atomic_exchange_n(ptr, desired, (int)order);
}
static inline int atomic_compare_exchange_strong_8(volatile char *ptr, char *expected, char desired,
                                                   memory_order success, memory_order failure)
{
    return __atomic_compare_exchange_n(ptr, expected, desired, 0, (int)success, (int)failure);
}
static inline char atomic_fetch_add_8(volatile char *ptr, char value, memory_order order)
{
    return (char)__atomic_fetch_add(ptr, value, (int)order);
}
static inline char atomic_fetch_sub_8(volatile char *ptr, char value, memory_order order)
{
    return (char)__atomic_fetch_sub(ptr, value, (int)order);
}

static inline short atomic_load_16(volatile short *ptr, memory_order order)
{
    return __atomic_load_n(ptr, (int)order);
}
static inline void atomic_store_16(volatile short *ptr, short value, memory_order order)
{
    __atomic_store_n(ptr, value, (int)order);
}
static inline short atomic_exchange_16(volatile short *ptr, short desired, memory_order order)
{
    return (short)__atomic_exchange_n(ptr, desired, (int)order);
}
static inline int atomic_compare_exchange_strong_16(volatile short *ptr, short *expected,
                                                    short desired, memory_order success,
                                                    memory_order failure)
{
    return __atomic_compare_exchange_n(ptr, expected, desired, 0, (int)success, (int)failure);
}
static inline short atomic_fetch_add_16(volatile short *ptr, short value, memory_order order)
{
    return (short)__atomic_fetch_add(ptr, value, (int)order);
}
static inline short atomic_fetch_sub_16(volatile short *ptr, short value, memory_order order)
{
    return (short)__atomic_fetch_sub(ptr, value, (int)order);
}

static inline long atomic_load_32(volatile long *ptr, memory_order order)
{
    return __atomic_load_n(ptr, (int)order);
}
static inline void atomic_store_32(volatile long *ptr, long value, memory_order order)
{
    __atomic_store_n(ptr, value, (int)order);
}
static inline long atomic_exchange_32(volatile long *ptr, long desired, memory_order order)
{
    return __atomic_exchange_n(ptr, desired, (int)order);
}
static inline int atomic_compare_exchange_strong_32(volatile long *ptr, long *expected,
                                                    long desired, memory_order success,
                                                    memory_order failure)
{
    return __atomic_compare_exchange_n(ptr, expected, desired, 0, (int)success, (int)failure);
}
static inline long atomic_fetch_add_32(volatile long *ptr, long value, memory_order order)
{
    return __atomic_fetch_add(ptr, value, (int)order);
}
static inline long atomic_fetch_sub_32(volatile long *ptr, long value, memory_order order)
{
    return __atomic_fetch_sub(ptr, value, (int)order);
}

static inline int64_t atomic_load_64(volatile int64_t *ptr, memory_order order)
{
    return __atomic_load_n(ptr, (int)order);
}
static inline void atomic_store_64(volatile int64_t *ptr, int64_t value, memory_order order)
{
    __atomic_store_n(ptr, value, (int)order);
}
static inline int64_t atomic_exchange_64(volatile int64_t *ptr, int64_t desired, memory_order order)
{
    return __atomic_exchange_n(ptr, desired, (int)order);
}
static inline int atomic_compare_exchange_strong_64(volatile int64_t *ptr, int64_t *expected,
                                                    int64_t desired, memory_order success,
                                                    memory_order failure)
{
    return __atomic_compare_exchange_n(ptr, expected, desired, 0, (int)success, (int)failure);
}
static inline int64_t atomic_fetch_add_64(volatile int64_t *ptr, int64_t value, memory_order order)
{
    return __atomic_fetch_add(ptr, value, (int)order);
}
static inline int64_t atomic_fetch_sub_64(volatile int64_t *ptr, int64_t value, memory_order order)
{
    return __atomic_fetch_sub(ptr, value, (int)order);
}

static inline void *atomic_load_ptr(void *volatile *ptr, memory_order order)
{
    return __atomic_load_n(ptr, (int)order);
}
static inline void atomic_store_ptr(void *volatile *ptr, void *value, memory_order order)
{
    __atomic_store_n(ptr, value, (int)order);
}
static inline void *atomic_exchange_ptr(void *volatile *ptr, void *desired, memory_order order)
{
    return __atomic_exchange_n(ptr, desired, (int)order);
}
static inline int atomic_compare_exchange_strong_ptr(void *volatile *ptr, void **expected,
                                                     void *desired, memory_order success,
                                                     memory_order failure)
{
    return __atomic_compare_exchange_n(ptr, expected, desired, 0, (int)success, (int)failure);
}

#define atomic_fetch_add_size(p, v, o) __atomic_fetch_add(p, v, (int)o)
#define atomic_fetch_sub_size(p, v, o) __atomic_fetch_sub(p, v, (int)o)
#define atomic_load_size(p, o) __atomic_load_n(p, (int)o)
#define atomic_store_size(p, v, o) __atomic_store_n(p, v, (int)o)

static inline double atomic_load_double(volatile double *ptr, memory_order order)
{
    double val;
    __atomic_load((const double *)ptr, &val, (int)order);
    return val;
}
static inline void atomic_store_double(volatile double *ptr, double value, memory_order order)
{
    __atomic_store(ptr, &value, (int)order);
}
static inline double atomic_fetch_add_double(volatile double *ptr, double value, memory_order order)
{
    double old;
    __atomic_load((const double *)ptr, &old, (int)memory_order_relaxed);
    double new_val;
    do {
        new_val = old + value;
    } while (
        !__atomic_compare_exchange_n(ptr, &old, new_val, 0, (int)order, (int)memory_order_relaxed));
    return old;
}

#endif /* Platform selection */

/* ==================== 跨平台通用原子类型别名 ==================== */

#if AGENTRT_USE_STDATOMIC

typedef _Atomic double atomic_double;
typedef _Atomic uint64_t atomic_uint64_t;
typedef _Atomic int64_t atomic_int64_t;
typedef _Atomic size_t atomic_size_t;
typedef _Atomic uint_fast64_t atomic_uint_fast64_t;
typedef _Atomic unsigned long atomic_uint_fast32_t;

/* C11 stdatomic.h 已经定义了 atomic_int, atomic_uint, atomic_bool, atomic_long, atomic_ulong */

/* C11 double 原子操作补充 */
static inline double atomic_load_double_fn(_Atomic double *ptr, memory_order order)
{
    return (double)atomic_load_explicit(ptr, order);
}
static inline void atomic_store_double_fn(_Atomic double *ptr, double value, memory_order order)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overflow="
    atomic_store_explicit(ptr, value, order);
#pragma GCC diagnostic pop
}
static inline double atomic_fetch_add_double_fn(_Atomic double *ptr, double value,
                                                memory_order order)
{
    double old = (double)atomic_load_explicit(ptr, memory_order_relaxed);
    double new_val;
    do {
        new_val = old + value;
    } while (
        !atomic_compare_exchange_weak_explicit(ptr, &old, new_val, order, memory_order_relaxed));
    return old;
}
static inline double atomic_exchange_double_fn(_Atomic double *ptr, double desired,
                                               memory_order order)
{
    return (double)atomic_exchange_explicit(ptr, desired, order);
}

#define atomic_fetch_add_double(ptr, val, ord) atomic_fetch_add_double_fn(ptr, val, ord)
#define atomic_load_double(ptr, ord) atomic_load_double_fn(ptr, ord)
#define atomic_store_double(ptr, val, ord) atomic_store_double_fn(ptr, val, ord)
#define atomic_exchange_double(ptr, val, ord) atomic_exchange_double_fn(ptr, val, ord)

#else

typedef volatile int atomic_int;
typedef volatile unsigned int atomic_uint;
typedef volatile long atomic_long;
typedef volatile unsigned long atomic_ulong;
typedef volatile int64_t atomic_int64_t;
typedef volatile uint64_t atomic_uint64_t;
typedef volatile size_t atomic_size_t;
typedef volatile double atomic_double;

#ifdef _WIN32
typedef volatile uint64_t atomic_uint_fast64_t;
#else
typedef volatile uint_fast64_t atomic_uint_fast64_t;
#endif
typedef volatile unsigned long atomic_uint_fast32_t;
typedef volatile int atomic_bool;

#endif

/* ==================== atomic_bool 专用操作 ==================== */

#if AGENTRT_USE_STDATOMIC

static inline _Bool atomic_load_bool(_Atomic _Bool *ptr, memory_order order)
{
    return (_Bool)atomic_load_explicit(ptr, order);
}

static inline void atomic_store_bool(_Atomic _Bool *ptr, _Bool value, memory_order order)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overflow="
    atomic_store_explicit(ptr, value, order);
#pragma GCC diagnostic pop
}

static inline _Bool atomic_exchange_bool(_Atomic _Bool *ptr, _Bool desired, memory_order order)
{
    return (_Bool)atomic_exchange_explicit(ptr, desired, order);
}

#elif defined(_WIN32)

static inline int atomic_load_bool(volatile int *ptr, memory_order order)
{
    (void)order;
    return *ptr;
}

static inline void atomic_store_bool(volatile int *ptr, int value, memory_order order)
{
    (void)order;
    *ptr = value ? 1 : 0;
}

static inline int atomic_exchange_bool(volatile int *ptr, int desired, memory_order order)
{
    (void)order;
    return InterlockedExchange((volatile LONG *)ptr, desired ? 1 : 0);
}

#else

static inline int atomic_load_bool(volatile int *ptr, memory_order order)
{
    return __atomic_load_n(ptr, (int)order);
}

static inline void atomic_store_bool(volatile int *ptr, int value, memory_order order)
{
    __atomic_store_n(ptr, value ? 1 : 0, (int)order);
}

static inline int atomic_exchange_bool(volatile int *ptr, int desired, memory_order order)
{
    return __atomic_exchange_n(ptr, desired ? 1 : 0, (int)order);
}

#endif

/* ==================== 初始化与通用宏 ==================== */

#if !AGENTRT_USE_STDATOMIC
#define atomic_init(ptr, val) (*(ptr) = (val))

#define atomic_load_explicit(ptr, order)                                           \
    (sizeof(*(ptr)) == 1   ? (int)atomic_load_8((volatile char *)(ptr), order)     \
     : sizeof(*(ptr)) == 2 ? (int)atomic_load_16((volatile short *)(ptr), order)   \
     : sizeof(*(ptr)) == 4 ? (int)atomic_load_32((volatile long *)(ptr), order)    \
     : sizeof(*(ptr)) == 8 ? (int)atomic_load_64((volatile int64_t *)(ptr), order) \
                           : *(ptr))

#define atomic_store_explicit(ptr, val, order)                                                 \
    (sizeof(*(ptr)) == 1   ? atomic_store_8((volatile char *)(ptr), (char)(val), order)        \
     : sizeof(*(ptr)) == 2 ? atomic_store_16((volatile short *)(ptr), (short)(val), order)     \
     : sizeof(*(ptr)) == 4 ? atomic_store_32((volatile long *)(ptr), (long)(val), order)       \
     : sizeof(*(ptr)) == 8 ? atomic_store_64((volatile int64_t *)(ptr), (int64_t)(val), order) \
                           : (*(ptr) = (val)))

#define atomic_load(ptr) atomic_load_explicit(ptr, memory_order_seq_cst)
#define atomic_store(ptr, val) atomic_store_explicit(ptr, val, memory_order_seq_cst)

#define atomic_exchange(ptr, val)                                                          \
    (sizeof(*(ptr)) == 1                                                                   \
         ? (int)atomic_exchange_8((char *)(ptr), (char)(val), memory_order_seq_cst)        \
     : sizeof(*(ptr)) == 2                                                                 \
         ? (int)atomic_exchange_16((short *)(ptr), (short)(val), memory_order_seq_cst)     \
     : sizeof(*(ptr)) == 4                                                                 \
         ? (int)atomic_exchange_32((long *)(ptr), (long)(val), memory_order_seq_cst)       \
     : sizeof(*(ptr)) == 8                                                                 \
         ? (int)atomic_exchange_64((int64_t *)(ptr), (int64_t)(val), memory_order_seq_cst) \
         : *(ptr))

#define atomic_compare_exchange_strong(ptr, expected, desired)                                   \
    (sizeof(*(ptr)) == 1                                                                         \
         ? atomic_compare_exchange_strong_8((char *)(ptr), (char *)(expected), (char)(desired),  \
                                            memory_order_seq_cst, memory_order_seq_cst)          \
     : sizeof(*(ptr)) == 2 ? atomic_compare_exchange_strong_16(                                  \
                                 (short *)(ptr), (short *)(expected), (short)(desired),          \
                                 memory_order_seq_cst, memory_order_seq_cst)                     \
     : sizeof(*(ptr)) == 4                                                                       \
         ? atomic_compare_exchange_strong_32((long *)(ptr), (long *)(expected), (long)(desired), \
                                             memory_order_seq_cst, memory_order_seq_cst)         \
     : sizeof(*(ptr)) == 8 ? atomic_compare_exchange_strong_64(                                  \
                                 (int64_t *)(ptr), (int64_t *)(expected), (int64_t)(desired),    \
                                 memory_order_seq_cst, memory_order_seq_cst)                     \
                           : 0)

#define atomic_compare_exchange_strong_explicit(ptr, expected, desired, succ, fail)            \
    (sizeof(*(ptr)) == 1                                                                       \
         ? atomic_compare_exchange_strong_8((volatile char *)(ptr), (char *)(expected),        \
                                            (char)(desired), succ, fail)                       \
     : sizeof(*(ptr)) == 2                                                                     \
         ? atomic_compare_exchange_strong_16((volatile short *)(ptr), (short *)(expected),     \
                                             (short *)(desired), succ, fail)                   \
     : sizeof(*(ptr)) == 4                                                                     \
         ? atomic_compare_exchange_strong_32((volatile long *)(ptr), (long *)(expected),       \
                                             (long)(desired), succ, fail)                      \
     : sizeof(*(ptr)) == 8                                                                     \
         ? atomic_compare_exchange_strong_64((volatile int64_t *)(ptr), (int64_t *)(expected), \
                                             (int64_t *)(desired), succ, fail)                 \
         : 0)

#define atomic_fetch_add(ptr, val)                                                          \
    (sizeof(*(ptr)) == 1                                                                    \
         ? (int)atomic_fetch_add_8((char *)(ptr), (char)(val), memory_order_seq_cst)        \
     : sizeof(*(ptr)) == 2                                                                  \
         ? (int)atomic_fetch_add_16((short *)(ptr), (short)(val), memory_order_seq_cst)     \
     : sizeof(*(ptr)) == 4                                                                  \
         ? (int)atomic_fetch_add_32((long *)(ptr), (long)(val), memory_order_seq_cst)       \
     : sizeof(*(ptr)) == 8                                                                  \
         ? (int)atomic_fetch_add_64((int64_t *)(ptr), (int64_t)(val), memory_order_seq_cst) \
         : 0)

#define atomic_fetch_sub(ptr, val)                                                          \
    (sizeof(*(ptr)) == 1                                                                    \
         ? (int)atomic_fetch_sub_8((char *)(ptr), (char)(val), memory_order_seq_cst)        \
     : sizeof(*(ptr)) == 2                                                                  \
         ? (int)atomic_fetch_sub_16((short *)(ptr), (short)(val), memory_order_seq_cst)     \
     : sizeof(*(ptr)) == 4                                                                  \
         ? (int)atomic_fetch_sub_32((long *)(ptr), (long)(val), memory_order_seq_cst)       \
     : sizeof(*(ptr)) == 8                                                                  \
         ? (int)atomic_fetch_sub_64((int64_t *)(ptr), (int64_t)(val), memory_order_seq_cst) \
         : 0)

#define atomic_fetch_add_explicit(ptr, val, order)                                                \
    (sizeof(*(ptr)) == 1 ? (int)atomic_fetch_add_8((volatile char *)(ptr), (char)(val), order)    \
     : sizeof(*(ptr)) == 2                                                                        \
         ? (int)atomic_fetch_add_16((volatile short *)(ptr), (short)(val), order)                 \
     : sizeof(*(ptr)) == 4 ? (int)atomic_fetch_add_32((volatile long *)(ptr), (long)(val), order) \
     : sizeof(*(ptr)) == 8                                                                        \
         ? (int)atomic_fetch_add_64((volatile int64_t *)(ptr), (int64_t)(val), order)             \
         : 0)

#define atomic_fetch_sub_explicit(ptr, val, order)                                                \
    (sizeof(*(ptr)) == 1 ? (int)atomic_fetch_sub_8((volatile char *)(ptr), (char)(val), order)    \
     : sizeof(*(ptr)) == 2                                                                        \
         ? (int)atomic_fetch_sub_16((volatile short *)(ptr), (short)(val), order)                 \
     : sizeof(*(ptr)) == 4 ? (int)atomic_fetch_sub_32((volatile long *)(ptr), (long)(val), order) \
     : sizeof(*(ptr)) == 8                                                                        \
         ? (int)atomic_fetch_sub_64((volatile int64_t *)(ptr), (int64_t)(val), order)             \
         : 0)
#endif /* !AGENTRT_USE_STDATOMIC */

/* ==================== 内存屏障 ==================== */

#if AGENTRT_USE_STDATOMIC

/* When using system <stdatomic.h>, atomic_thread_fence is already provided
 * as a macro: #define atomic_thread_fence(MO) __atomic_thread_fence(MO)
 * Do NOT redefine it as a static inline function here, because the macro
 * would expand the function name, creating infinite recursion:
 *   static inline void __atomic_thread_fence(memory_order order) {
 *       __atomic_thread_fence((int)order);  // calls itself!
 *   }
 */

#elif defined(_WIN32)

static inline void atomic_thread_fence(memory_order order)
{
    (void)order;
    MemoryBarrier();
}

#else

static inline void atomic_thread_fence(memory_order order)
{
    __atomic_thread_fence((int)order);
}

#endif

#ifdef __cplusplus
}
#endif

#endif /* AGENTRT_ATOMIC_COMPAT_H */
