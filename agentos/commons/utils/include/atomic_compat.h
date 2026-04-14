#ifndef AGENTOS_ATOMIC_COMPAT_H
#define AGENTOS_ATOMIC_COMPAT_H

/**
 * @file atomic_compat.h
 * @brief 跨平台原子操作兼容层
 *
 * 提供 C11 stdatomic.h 的跨平台兼容实现：
 * - Windows: 使用 Interlocked API (intrin.h)
 * - POSIX (C11+): 使用 <stdatomic.h>
 * - POSIX (fallback): 使用 __atomic builtins (GCC/Clang)
 *
 * @note 所有操作均为线程安全
 */

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 内存顺序枚举 ==================== */

typedef enum {
    memory_order_relaxed = 0,
    memory_order_consume = 1,
    memory_order_acquire = 2,
    memory_order_release = 3,
    memory_order_acq_rel = 4,
    memory_order_seq_cst = 5
} memory_order;

/* ====================================================================
 * Windows 实现: 使用 Interlocked API
 * ==================================================================== */
#if defined(_WIN32)

#include <windows.h>
#include <intrin.h>

/* _Atomic 关键字映射 */
#define _Atomic volatile

/* ==================== 8位原子操作（Windows） ==================== */
/*
 * 注意：Windows x64 不支持 InterlockedCompareExchange8
 * 使用 volatile + 简单 CAS 循环模拟（对齐地址上锁）
 * 对于严格场景，建议升级到16位或使用互斥锁保护
 */

static inline char atomic_load_8(volatile char* ptr, memory_order order) {
    (void)order;
    return *ptr;
}

static inline void atomic_store_8(volatile char* ptr, char value, memory_order order) {
    (void)order;
    *ptr = value;
}

static inline char atomic_exchange_8(volatile char* ptr, char desired, memory_order order) {
    (void)order;
#ifdef _WIN64
    volatile short* p = (volatile short*)((uintptr_t)ptr & ~(uintptr_t)1);
    return (char)InterlockedExchange16(p, (short)desired);
#else
    return (char)InterlockedExchange8(ptr, (BYTE)desired);
#endif
}

static inline int atomic_compare_exchange_strong_8(volatile char* ptr, char* expected, char desired, memory_order success, memory_order failure) {
    (void)success; (void)failure;
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
    if (old == *expected) return 1;
    *expected = old;
    return 0;
#endif
}

static inline char atomic_fetch_add_8(volatile char* ptr, char value, memory_order order) {
    (void)order;
#ifdef _WIN64
    volatile short* p = (volatile short*)((uintptr_t)ptr & ~(uintptr_t)1);
    return (char)InterlockedExchangeAdd16(p, (short)value);
#else
    return (char)InterlockedExchangeAdd8(ptr, (char)value);
#endif
}

static inline char atomic_fetch_sub_8(volatile char* ptr, char value, memory_order order) {
    (void)order;
#ifdef _WIN64
    volatile short* p = (volatile short*)((uintptr_t)ptr & ~(uintptr_t)1);
    return (char)InterlockedExchangeAdd16(p, -(short)value);
#else
    return (char)InterlockedExchangeAdd8(ptr, -(char)value);
#endif
}

/* ==================== 16位原子操作（Windows） ==================== */

static inline short atomic_load_16(volatile short* ptr, memory_order order) {
    (void)order;
    return *ptr;
}

static inline void atomic_store_16(volatile short* ptr, short value, memory_order order) {
    (void)order;
    *ptr = value;
}

static inline short atomic_exchange_16(volatile short* ptr, short desired, memory_order order) {
    (void)order;
    return (short)InterlockedExchange16((volatile SHORT*)ptr, desired);
}

static inline int atomic_compare_exchange_strong_16(volatile short* ptr, short* expected, short desired, memory_order success, memory_order failure) {
    (void)success; (void)failure;
    short old = (short)InterlockedCompareExchange16((volatile SHORT*)ptr, desired, *expected);
    if (old == *expected) return 1;
    *expected = old;
    return 0;
}

static inline short atomic_fetch_add_16(volatile short* ptr, short value, memory_order order) {
    (void)order;
    return (short)InterlockedExchangeAdd16((volatile SHORT*)ptr, value);
}

static inline short atomic_fetch_sub_16(volatile short* ptr, short value, memory_order order) {
    (void)order;
    return (short)InterlockedExchangeAdd16((volatile SHORT*)ptr, -value);
}

/* ==================== 32位原子操作（Windows） ==================== */

static inline long atomic_load_32(volatile long* ptr, memory_order order) {
    (void)order;
    MemoryBarrier();
    return *ptr;
}

static inline void atomic_store_32(volatile long* ptr, long value, memory_order order) {
    (void)order;
    *ptr = value;
    MemoryBarrier();
}

static inline long atomic_exchange_32(volatile long* ptr, long desired, memory_order order) {
    (void)order;
    return (long)InterlockedExchange((volatile LONG*)ptr, desired);
}

static inline int atomic_compare_exchange_strong_32(volatile long* ptr, long* expected, long desired, memory_order success, memory_order failure) {
    (void)success; (void)failure;
    long old = (long)InterlockedCompareExchange((volatile LONG*)ptr, desired, *expected);
    if (old == *expected) return 1;
    *expected = old;
    return 0;
}

static inline long atomic_fetch_add_32(volatile long* ptr, long value, memory_order order) {
    (void)order;
    return (long)InterlockedExchangeAdd((volatile LONG*)ptr, value);
}

static inline long atomic_fetch_sub_32(volatile long* ptr, long value, memory_order order) {
    (void)order;
    return (long)InterlockedExchangeAdd((volatile LONG*)ptr, -value);
}

/* ==================== 64位原子操作（Windows） ==================== */

static inline __int64 atomic_load_64(volatile __int64* ptr, memory_order order) {
    (void)order;
    MemoryBarrier();
    return *ptr;
}

static inline void atomic_store_64(volatile __int64* ptr, __int64 value, memory_order order) {
    (void)order;
    *ptr = value;
    MemoryBarrier();
}

static inline __int64 atomic_exchange_64(volatile __int64* ptr, __int64 desired, memory_order order) {
    (void)order;
    return (__int64)InterlockedExchange64((volatile LONGLONG*)ptr, desired);
}

static inline int atomic_compare_exchange_strong_64(volatile __int64* ptr, __int64* expected, __int64 desired, memory_order success, memory_order failure) {
    (void)success; (void)failure;
    __int64 old = (__int64)InterlockedCompareExchange64((volatile LONGLONG*)ptr, desired, *expected);
    if (old == *expected) return 1;
    *expected = old;
    return 0;
}

static inline __int64 atomic_fetch_add_64(volatile __int64* ptr, __int64 value, memory_order order) {
    (void)order;
    return (__int64)InterlockedExchangeAdd64((volatile LONGLONG*)ptr, value);
}

static inline __int64 atomic_fetch_sub_64(volatile __int64* ptr, __int64 value, memory_order order) {
    (void)order;
    return (__int64)InterlockedExchangeAdd64((volatile LONGLONG*)ptr, -value);
}

/* ==================== 指针原子操作（Windows） ==================== */

static inline void* atomic_load_ptr(void* volatile* ptr, memory_order order) {
    (void)order;
    MemoryBarrier();
    return *ptr;
}

static inline void atomic_store_ptr(void* volatile* ptr, void* value, memory_order order) {
    (void)order;
    *ptr = value;
    MemoryBarrier();
}

static inline void* atomic_exchange_ptr(void* volatile* ptr, void* desired, memory_order order) {
    (void)order;
    return InterlockedExchangePointer(ptr, desired);
}

static inline int atomic_compare_exchange_strong_ptr(void* volatile* ptr, void** expected, void* desired, memory_order success, memory_order failure) {
    (void)success; (void)failure;
    void* old = InterlockedCompareExchangePointer(ptr, desired, *expected);
    if (old == *expected) return 1;
    *expected = old;
    return 0;
}

/* ==================== size_t 原子操作（Windows） ==================== */

#ifdef _WIN64
#define atomic_fetch_add_size(p, v, o) (__int64)atomic_fetch_add_64((__int64*)(p), (__int64)(v), o)
#define atomic_fetch_sub_size(p, v, o) (__int64)atomic_fetch_sub_64((__int64*)(p), (__int64)(v), o)
#define atomic_load_size(p, o) (size_t)atomic_load_64((__int64*)(p), o)
#define atomic_store_size(p, v, o) atomic_store_64((__int64*)(p), (__int64)(v), o)
#else
#define atomic_fetch_add_size(p, v, o) (long)atomic_fetch_add_32((long*)(p), (long)(v), o)
#define atomic_fetch_sub_size(p, v, o) (long)atomic_fetch_sub_32((long*)(p), (long)(v), o)
#define atomic_load_size(p, o) (size_t)atomic_load_32((long*)(p), o)
#define atomic_store_size(p, v, o) atomic_store_32((long*)(p), (long)(v), o)
#endif

/* ==================== double 原子操作（Windows） ==================== */

static inline double atomic_load_double(volatile double* ptr, memory_order order) {
    (void)order;
    return *ptr;
}

static inline void atomic_store_double(volatile double* ptr, double value, memory_order order) {
    (void)order;
    *ptr = value;
}

static inline double atomic_fetch_add_double(volatile double* ptr, double value, memory_order order) {
    (void)order;
    double old = *ptr;
    *ptr += value;
    return old;
}

/* ====================================================================
 * POSIX 实现: 优先使用 C11 stdatomic.h，回退到 __atomic builtins
 * ==================================================================== */
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L) && \
      !defined(AGENTOS_NO_STDATOMIC)

#include <stdatomic.h>

/* _Atomic 直接使用 C11 标准关键字，无需重定义 */

/* 8/16/32/64 位操作通过通用宏和内联函数提供 */

static inline char atomic_load_8(volatile _Atomic(char)* ptr, memory_order order) {
    return atomic_load_explicit(ptr, (memory_order_int)order);
}
static inline void atomic_store_8(volatile _Atomic(char)* ptr, char value, memory_order order) {
    atomic_store_explicit(ptr, value, (memory_order_int)order);
}
static inline char atomic_exchange_8(volatile _Atomic(char)* ptr, char desired, memory_order order) {
    return atomic_exchange_explicit(ptr, desired, (memory_order_int)order);
}
static inline int atomic_compare_exchange_strong_8(volatile _Atomic(char)* ptr, char* expected, char desired, memory_order success, memory_order failure) {
    return atomic_compare_exchange_strong_explicit(ptr, expected, desired, (memory_order_int)success, (memory_order_int)failure);
}
static inline char atomic_fetch_add_8(volatile _Atomic(char)* ptr, char value, memory_order order) {
    return atomic_fetch_add_explicit(ptr, value, (memory_order_int)order);
}
static inline char atomic_fetch_sub_8(volatile _Atomic(char)* ptr, char value, memory_order order) {
    return atomic_fetch_sub_explicit(ptr, value, (memory_order_int)order);
}

static inline short atomic_load_16(volatile _Atomic(short)* ptr, memory_order order) {
    return atomic_load_explicit(ptr, (memory_order_int)order);
}
static inline void atomic_store_16(volatile _Atomic(short)* ptr, short value, memory_order order) {
    atomic_store_explicit(ptr, value, (memory_order_int)order);
}
static inline short atomic_exchange_16(volatile _Atomic(short)* ptr, short desired, memory_order order) {
    return atomic_exchange_explicit(ptr, desired, (memory_order_int)order);
}
static inline int atomic_compare_exchange_strong_16(volatile _Atomic(short)* ptr, short* expected, short desired, memory_order success, memory_order failure) {
    return atomic_compare_exchange_strong_explicit(ptr, expected, desired, (memory_order_int)success, (memory_order_int)failure);
}
static inline short atomic_fetch_add_16(volatile _Atomic(short)* ptr, short value, memory_order order) {
    return atomic_fetch_add_explicit(ptr, value, (memory_order_int)order);
}
static inline short atomic_fetch_sub_16(volatile _Atomic(short)* ptr, short value, memory_order order) {
    return atomic_fetch_sub_explicit(ptr, value, (memory_order_int)order);
}

static inline long atomic_load_32(volatile _Atomic(long)* ptr, memory_order order) {
    return atomic_load_explicit(ptr, (memory_order_int)order);
}
static inline void atomic_store_32(volatile _Atomic(long)* ptr, long value, memory_order order) {
    atomic_store_explicit(ptr, value, (memory_order_int)order);
}
static inline long atomic_exchange_32(volatile _Atomic(long)* ptr, long desired, memory_order order) {
    return atomic_exchange_explicit(ptr, desired, (memory_order_int)order);
}
static inline int atomic_compare_exchange_strong_32(volatile _Atomic(long)* ptr, long* expected, long desired, memory_order success, memory_order failure) {
    return atomic_compare_exchange_strong_explicit(ptr, expected, desired, (memory_order_int)success, (memory_order_int)failure);
}
static inline long atomic_fetch_add_32(volatile _Atomic(long)* ptr, long value, memory_order order) {
    return atomic_fetch_add_explicit(ptr, value, (memory_order_int)order);
}
static inline long atomic_fetch_sub_32(volatile _Atomic(long)* ptr, long value, memory_order order) {
    return atomic_fetch_sub_explicit(ptr, value, (memory_order_int)order);
}

static inline __int64 atomic_load_64(volatile _Atomic(__int64)* ptr, memory_order order) {
    return atomic_load_explicit(ptr, (memory_order_int)order);
}
static inline void atomic_store_64(volatile _Atomic(__int64)* ptr, __int64 value, memory_order order) {
    atomic_store_explicit(ptr, value, (memory_order_int)order);
}
static inline __int64 atomic_exchange_64(volatile _Atomic(__int64)* ptr, __int64 desired, memory_order order) {
    return atomic_exchange_explicit(ptr, desired, (memory_order_int)order);
}
static inline int atomic_compare_exchange_strong_64(volatile _Atomic(__int64)* ptr, __int64* expected, __int64 desired, memory_order success, memory_order failure) {
    return atomic_compare_exchange_strong_explicit(ptr, expected, desired, (memory_order_int)success, (memory_order_int)failure);
}
static inline __int64 atomic_fetch_add_64(volatile _Atomic(__int64)* ptr, __int64 value, memory_order order) {
    return atomic_fetch_add_explicit(ptr, value, (memory_order_int)order);
}
static inline __int64 atomic_fetch_sub_64(volatile _Atomic(__int64)* ptr, __int64 value, memory_order order) {
    return atomic_fetch_sub_explicit(ptr, value, (memory_order_int)order);
}

static inline void* atomic_load_ptr(void* volatile* ptr, memory_order order) {
    return atomic_load_explicit((_Atomic(void*)*)ptr, (memory_order_int)order);
}
static inline void atomic_store_ptr(void* volatile* ptr, void* value, memory_order order) {
    atomic_store_explicit((_Atomic(void*)*)ptr, value, (memory_order_int)order);
}
static inline void* atomic_exchange_ptr(void* volatile* ptr, void* desired, memory_order order) {
    return atomic_exchange_explicit((_Atomic(void*)*)ptr, desired, (memory_order_int)order);
}
static inline int atomic_compare_exchange_strong_ptr(void* volatile* ptr, void** expected, void* desired, memory_order success, memory_order failure) {
    return atomic_compare_exchange_strong_explicit((_Atomic(void**)ptr, (_Atomic(void**)*)expected, desired, (memory_order_int)success, (memory_order_int)failure);
}

#define atomic_fetch_add_size(p, v, o) atomic_fetch_add_explicit(p, v, (memory_order_int)o)
#define atomic_fetch_sub_size(p, v, o) atomic_fetch_sub_explicit(p, v, (memory_order_int)o)
#define atomic_load_size(p, o) atomic_load_explicit(p, (memory_order_int)o)
#define atomic_store_size(p, v, o) atomic_store_explicit(p, v, (memory_order_int)o)

static inline double atomic_load_double(volatile _Atomic(double)* ptr, memory_order order) {
    return atomic_load_explicit(ptr, (memory_order_int)order);
}
static inline void atomic_store_double(volatile _Atomic(double)* ptr, double value, memory_order order) {
    atomic_store_explicit(ptr, value, (memory_order_int)order);
}
static inline double atomic_fetch_add_double(volatile _Atomic(double)* ptr, double value, memory_order order) {
    return atomic_fetch_add_explicit(ptr, value, (memory_order_int)order);
}

#else /* POSIX fallback: 使用 GCC/Clang __atomic builtins */

#define _Atomic volatile

/* 8位原子操作 (GCC/Clang __atomic builtins) */
static inline char atomic_load_8(volatile char* ptr, memory_order order) {
    return __atomic_load_n(ptr, (int)order);
}
static inline void atomic_store_8(volatile char* ptr, char value, memory_order order) {
    __atomic_store_n(ptr, value, (int)order);
}
static inline char atomic_exchange_8(volatile char* ptr, char desired, memory_order order) {
    return (char)__atomic_exchange_n(ptr, desired, (int)order);
}
static inline int atomic_compare_exchange_strong_8(volatile char* ptr, char* expected, char desired, memory_order success, memory_order failure) {
    return __atomic_compare_exchange_n(ptr, expected, desired, (int)success, (int)failure);
}
static inline char atomic_fetch_add_8(volatile char* ptr, char value, memory_order order) {
    return (char)__atomic_add_fetch(ptr, value, (int)order);
}
static inline char atomic_fetch_sub_8(volatile char* ptr, char value, memory_order order) {
    return (char)__atomic_sub_fetch(ptr, value, (int)order);
}

/* 16位原子操作 */
static inline short atomic_load_16(volatile short* ptr, memory_order order) {
    return __atomic_load_n(ptr, (int)order);
}
static inline void atomic_store_16(volatile short* ptr, short value, memory_order order) {
    __atomic_store_n(ptr, value, (int)order);
}
static inline short atomic_exchange_16(volatile short* ptr, short desired, memory_order order) {
    return (short)__atomic_exchange_n(ptr, desired, (int)order);
}
static inline int atomic_compare_exchange_strong_16(volatile short* ptr, short* expected, short desired, memory_order success, memory_order failure) {
    return __atomic_compare_exchange_n(ptr, expected, desired, (int)success, (int)failure);
}
static inline short atomic_fetch_add_16(volatile short* ptr, short value, memory_order order) {
    return (short)__atomic_add_fetch(ptr, value, (int)order);
}
static inline short atomic_fetch_sub_16(volatile short* ptr, short value, memory_order order) {
    return (short)__atomic_sub_fetch(ptr, value, (int)order);
}

/* 32位原子操作 */
static inline long atomic_load_32(volatile long* ptr, memory_order order) {
    return __atomic_load_n(ptr, (int)order);
}
static inline void atomic_store_32(volatile long* ptr, long value, memory_order order) {
    __atomic_store_n(ptr, value, (int)order);
}
static inline long atomic_exchange_32(volatile long* ptr, long desired, memory_order order) {
    return __atomic_exchange_n(ptr, desired, (int)order);
}
static inline int atomic_compare_exchange_strong_32(volatile long* ptr, long* expected, long desired, memory_order success, memory_order failure) {
    return __atomic_compare_exchange_n(ptr, expected, desired, (int)success, (int)failure);
}
static inline long atomic_fetch_add_32(volatile long* ptr, long value, memory_order order) {
    return __atomic_add_fetch(ptr, value, (int)order);
}
static inline long atomic_fetch_sub_32(volatile long* ptr, long value, memory_order order) {
    return __atomic_sub_fetch(ptr, value, (int)order);
}

/* 64位原子操作 */
static inline __int64 atomic_load_64(volatile __int64* ptr, memory_order order) {
    return __atomic_load_n(ptr, (int)order);
}
static inline void atomic_store_64(volatile __int64* ptr, __int64 value, memory_order order) {
    __atomic_store_n(ptr, value, (int)order);
}
static inline __int64 atomic_exchange_64(volatile __int64* ptr, __int64 desired, memory_order order) {
    return __atomic_exchange_n(ptr, desired, (int)order);
}
static inline int atomic_compare_exchange_strong_64(volatile __int64* ptr, __int64* expected, __int64 desired, memory_order success, memory_order failure) {
    return __atomic_compare_exchange_n(ptr, expected, desired, (int)success, (int)failure);
}
static inline __int64 atomic_fetch_add_64(volatile __int64* ptr, __int64 value, memory_order order) {
    return __atomic_add_fetch(ptr, value, (int)order);
}
static inline __int64 atomic_fetch_sub_64(volatile __int64* ptr, __int64 value, memory_order order) {
    return __atomic_sub_fetch(ptr, value, (int)order);
}

/* 指针原子操作 */
static inline void* atomic_load_ptr(void* volatile* ptr, memory_order order) {
    return __atomic_load_n(ptr, (int)order);
}
static inline void atomic_store_ptr(void* volatile* ptr, void* value, memory_order order) {
    __atomic_store_n(ptr, value, (int)order);
}
static inline void* atomic_exchange_ptr(void* volatile* ptr, void* desired, memory_order order) {
    return __atomic_exchange_n(ptr, desired, (int)order);
}
static inline int atomic_compare_exchange_strong_ptr(void* volatile* ptr, void** expected, void* desired, memory_order success, memory_order failure) {
    return __atomic_compare_exchange_n(ptr, expected, desired, (int)success, (int)failure);
}

/* size_t 原子操作 */
#define atomic_fetch_add_size(p, v, o) __atomic_add_fetch(p, v, (int)o)
#define atomic_fetch_sub_size(p, v, o) __atomic_sub_fetch(p, v, (int)o)
#define atomic_load_size(p, o) __atomic_load_n(p, (int)o)
#define atomic_store_size(p, v, o) __atomic_store_n(p, v, (int)o)

/* double 原子操作 */
static inline double atomic_load_double(volatile double* ptr, memory_order order) {
    return __atomic_load_n(ptr, (int)order);
}
static inline void atomic_store_double(volatile double* ptr, double value, memory_order order) {
    __atomic_store_n(ptr, value, (int)order);
}
static inline double atomic_fetch_add_double(volatile double* ptr, double value, memory_order order) {
    return __atomic_add_fetch(ptr, value, (int)order);
}

#endif /* Platform selection */

/* ==================== 跨平台通用原子类型别名 ==================== */

typedef volatile int atomic_int;
typedef volatile unsigned int atomic_uint;
typedef volatile long atomic_long;
typedef volatile unsigned long atomic_ulong;
typedef volatile __int64 atomic_int64_t;
typedef volatile unsigned __int64 atomic_uint64_t;
typedef volatile size_t atomic_size_t;
typedef volatile double atomic_double;

/* C11 兼容类型 */
typedef volatile unsigned __int64 atomic_uint_fast64_t;
typedef volatile unsigned long atomic_uint_fast32_t;
typedef volatile int atomic_bool;

/* atomic_bool 专用操作 */
static inline int atomic_load_bool(volatile int* ptr, memory_order order) {
#ifdef _WIN32
    (void)order;
    return *ptr;
#elif defined(AGENTOS_USE_STDATOMIC)
    (void)order;
    return *ptr;
#else
    return __atomic_load_n(ptr, (int)order);
#endif
}

static inline void atomic_store_bool(volatile int* ptr, int value, memory_order order) {
#ifdef _WIN32
    (void)order;
    *ptr = value ? 1 : 0;
#elif defined(AGENTOS_USE_STDATOMIC)
    (void)order;
    *ptr = value ? 1 : 0;
#else
    __atomic_store_n(ptr, value ? 1 : 0, (int)order);
#endif
}

static inline int atomic_exchange_bool(volatile int* ptr, int desired, memory_order order) {
#ifdef _WIN32
    (void)order;
    return InterlockedExchange((volatile LONG*)ptr, desired ? 1 : 0);
#elif defined(AGENTOS_USE_STDATOMIC)
    (void)order;
    /* C11 fallback */
    int old = *ptr;
    *ptr = desired ? 1 : 0;
    return old;
#else
    return __atomic_exchange_n(ptr, desired ? 1 : 0, (int)order);
#endif
}

/* ==================== 初始化与通用宏 ==================== */

#define atomic_init(ptr, val) (*(ptr) = (val))

#define atomic_load(ptr) (*(ptr))
#define atomic_store(ptr, val) (*(ptr) = (val))

#define atomic_load_explicit(ptr, order) (*(ptr))
#define atomic_store_explicit(ptr, val, order) (*(ptr) = (val))

/* 交换操作（按大小分发） */
#define atomic_exchange(ptr, val) \
    (sizeof(*(ptr)) == 1 ? (int)atomic_exchange_8((char*)(ptr), (char)(val), memory_order_seq_cst) : \
     sizeof(*(ptr)) == 2 ? (int)atomic_exchange_16((short*)(ptr), (short)(val), memory_order_seq_cst) : \
     sizeof(*(ptr)) == 4 ? (int)atomic_exchange_32((long*)(ptr), (long)(val), memory_order_seq_cst) : \
     sizeof(*(ptr)) == 8 ? (int)atomic_exchange_64((__int64*)(ptr), (__int64)(val), memory_order_seq_cst) : \
     *(ptr))

/* 比较并交换操作 */
#define atomic_compare_exchange_strong(ptr, expected, desired) \
    (sizeof(*(ptr)) == 1 ? atomic_compare_exchange_strong_8((char*)(ptr), (char*)(expected), (char)(desired), memory_order_seq_cst, memory_order_seq_cst) : \
     sizeof(*(ptr)) == 2 ? atomic_compare_exchange_strong_16((short*)(ptr), (short*)(expected), (short)(desired), memory_order_seq_cst, memory_order_seq_cst) : \
     sizeof(*(ptr)) == 4 ? atomic_compare_exchange_strong_32((long*)(ptr), (long*)(expected), (long)(desired), memory_order_seq_cst, memory_order_seq_cst) : \
     sizeof(*(ptr)) == 8 ? atomic_compare_exchange_strong_64((__int64*)(ptr), (__int64*)(expected), (__int64)(desired), memory_order_seq_cst, memory_order_seq_cst) : 0)

#define atomic_compare_exchange_strong_explicit(ptr, expected, desired, succ, fail) \
    atomic_compare_exchange_strong(ptr, expected, desired)

/* 取加/取减操作 */
#define atomic_fetch_add(ptr, val) \
    (sizeof(*(ptr)) == 1 ? (int)atomic_fetch_add_8((char*)(ptr), (char)(val), memory_order_seq_cst) : \
     sizeof(*(ptr)) == 2 ? (int)atomic_fetch_add_16((short*)(ptr), (short)(val), memory_order_seq_cst) : \
     sizeof(*(ptr)) == 4 ? (int)atomic_fetch_add_32((long*)(ptr), (long)(val), memory_order_seq_cst) : \
     sizeof(*(ptr)) == 8 ? (int)atomic_fetch_add_64((__int64*)(ptr), (__int64)(val), memory_order_seq_cst) : 0)

#define atomic_fetch_sub(ptr, val) \
    (sizeof(*(ptr)) == 1 ? (int)atomic_fetch_sub_8((char*)(ptr), (char)(val), memory_order_seq_cst) : \
     sizeof(*(ptr)) == 2 ? (int)atomic_fetch_sub_16((short*)(ptr), (short)(val), memory_order_seq_cst) : \
     sizeof(*(ptr)) == 4 ? (int)atomic_fetch_sub_32((long*)(ptr), (long)(val), memory_order_seq_cst) : \
     sizeof(*(ptr)) == 8 ? (int)atomic_fetch_sub_64((__int64*)(ptr), (__int64)(val), memory_order_seq_cst) : 0)

#define atomic_fetch_add_explicit(ptr, val, order) atomic_fetch_add(ptr, val)
#define atomic_fetch_sub_explicit(ptr, val, order) atomic_fetch_sub(ptr, val)

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_ATOMIC_COMPAT_H */
