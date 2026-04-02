#ifndef AGENTOS_ATOMIC_COMPAT_H
#define AGENTOS_ATOMIC_COMPAT_H

/**
 * @file atomic_compat.h
 * @brief Windows 平台原子操作兼容层
 *
 * 提供 C11 stdatomic.h 的 Windows 替代实现
 */

#ifdef _WIN32

#include <windows.h>
#include <intrin.h>

/* 内存顺序枚举 */
typedef enum {
    memory_order_relaxed = 0,
    memory_order_consume = 1,
    memory_order_acquire = 2,
    memory_order_release = 3,
    memory_order_acq_rel = 4,
    memory_order_seq_cst = 5
} memory_order;

/* 原子类型定义（使用 volatile + Interlocked API） */
#define _Atomic volatile

/* ==================== 8位原子操作 ==================== */

/* 注意：Windows x64 不支持 InterlockedCompareExchange8，需要使用其他方法 */
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
    /* x64 平台：使用 InterlockedExchange16 并截断 */
    volatile short* p = (volatile short*)((uintptr_t)ptr & ~1);
    return (char)InterlockedExchange16(p, (short)desired);
#else
    return InterlockedExchange8(ptr, desired);
#endif
}

static inline int atomic_compare_exchange_strong_8(volatile char* ptr, char* expected, char desired, memory_order success, memory_order failure) {
    (void)success; (void)failure;
#ifdef _WIN64
    /* x64 平台：使用简单的比较交换 */
    char old = *ptr;
    if (old == *expected) {
        *ptr = desired;
        return 1;
    }
    *expected = old;
    return 0;
#else
    char old = InterlockedCompareExchange8(ptr, desired, *expected);
    if (old == *expected) return 1;
    *expected = old;
    return 0;
#endif
}

static inline char atomic_fetch_add_8(volatile char* ptr, char value, memory_order order) {
    (void)order;
#ifdef _WIN64
    /* x64 平台：使用 InterlockedExchangeAdd16 */
    volatile short* p = (volatile short*)((uintptr_t)ptr & ~1);
    return (char)InterlockedExchangeAdd16(p, (short)value);
#else
    return InterlockedExchangeAdd8(ptr, value);
#endif
}

static inline char atomic_fetch_sub_8(volatile char* ptr, char value, memory_order order) {
    (void)order;
#ifdef _WIN64
    volatile short* p = (volatile short*)((uintptr_t)ptr & ~1);
    return (char)InterlockedExchangeAdd16(p, -(short)value);
#else
    return InterlockedExchangeAdd8(ptr, -value);
#endif
}

/* ==================== 短整型原子操作 ==================== */

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
    return InterlockedExchange16((volatile SHORT*)ptr, desired);
}

static inline int atomic_compare_exchange_strong_16(volatile short* ptr, short* expected, short desired, memory_order success, memory_order failure) {
    (void)success; (void)failure;
    short old = InterlockedCompareExchange16((volatile SHORT*)ptr, desired, *expected);
    if (old == *expected) return 1;
    *expected = old;
    return 0;
}

static inline short atomic_fetch_add_16(volatile short* ptr, short value, memory_order order) {
    (void)order;
    return InterlockedExchangeAdd16((volatile SHORT*)ptr, value);
}

static inline short atomic_fetch_sub_16(volatile short* ptr, short value, memory_order order) {
    (void)order;
    return InterlockedExchangeAdd16((volatile SHORT*)ptr, -value);
}

/* ==================== 32位原子操作 ==================== */

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
    return InterlockedExchange((volatile LONG*)ptr, desired);
}

static inline int atomic_compare_exchange_strong_32(volatile long* ptr, long* expected, long desired, memory_order success, memory_order failure) {
    (void)success; (void)failure;
    long old = InterlockedCompareExchange((volatile LONG*)ptr, desired, *expected);
    if (old == *expected) return 1;
    *expected = old;
    return 0;
}

static inline long atomic_fetch_add_32(volatile long* ptr, long value, memory_order order) {
    (void)order;
    return InterlockedExchangeAdd((volatile LONG*)ptr, value);
}

static inline long atomic_fetch_sub_32(volatile long* ptr, long value, memory_order order) {
    (void)order;
    return InterlockedExchangeAdd((volatile LONG*)ptr, -value);
}

/* ==================== 64位原子操作 ==================== */

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
    return InterlockedExchange64((volatile LONGLONG*)ptr, desired);
}

static inline int atomic_compare_exchange_strong_64(volatile __int64* ptr, __int64* expected, __int64 desired, memory_order success, memory_order failure) {
    (void)success; (void)failure;
    __int64 old = InterlockedCompareExchange64((volatile LONGLONG*)ptr, desired, *expected);
    if (old == *expected) return 1;
    *expected = old;
    return 0;
}

static inline __int64 atomic_fetch_add_64(volatile __int64* ptr, __int64 value, memory_order order) {
    (void)order;
    return InterlockedExchangeAdd64((volatile LONGLONG*)ptr, value);
}

static inline __int64 atomic_fetch_sub_64(volatile __int64* ptr, __int64 value, memory_order order) {
    (void)order;
    return InterlockedExchangeAdd64((volatile LONGLONG*)ptr, -value);
}

/* ==================== 指针原子操作 ==================== */

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

/* ==================== size_t 原子操作 ==================== */

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

/* ==================== 通用宏定义 ==================== */

/* 原子类型别名 */
typedef volatile int atomic_int;
typedef volatile unsigned int atomic_uint;
typedef volatile long atomic_long;
typedef volatile unsigned long atomic_ulong;
typedef volatile __int64 atomic_int64_t;
typedef volatile unsigned __int64 atomic_uint64_t;
typedef volatile size_t atomic_size_t;

/* 初始化宏 */
#define atomic_init(ptr, val) (*(ptr) = (val))

/* 通用加载/存储宏（简化版） */
#define atomic_load(ptr) (*(ptr))
#define atomic_store(ptr, val) (*(ptr) = (val))

/* 显式内存顺序版本 */
#define atomic_load_explicit(ptr, order) (*(ptr))
#define atomic_store_explicit(ptr, val, order) (*(ptr) = (val))

/* 交换操作 */
#define atomic_exchange(ptr, val) \
    (sizeof(*(ptr)) == 4 ? atomic_exchange_32((long*)(ptr), (long)(val), memory_order_seq_cst) : \
     sizeof(*(ptr)) == 8 ? (long)atomic_exchange_64((__int64*)(ptr), (__int64)(val), memory_order_seq_cst) : \
     *(ptr))

/* 比较并交换操作 */
#define atomic_compare_exchange_strong(ptr, expected, desired) \
    (sizeof(*(ptr)) == 4 ? atomic_compare_exchange_strong_32((long*)(ptr), (long*)(expected), (long)(desired), memory_order_seq_cst, memory_order_seq_cst) : \
     sizeof(*(ptr)) == 8 ? atomic_compare_exchange_strong_64((__int64*)(ptr), (__int64*)(expected), (__int64)(desired), memory_order_seq_cst, memory_order_seq_cst) : 0)

#define atomic_compare_exchange_strong_explicit(ptr, expected, desired, succ, fail) \
    atomic_compare_exchange_strong(ptr, expected, desired)

/* 取加/取减操作 */
#define atomic_fetch_add(ptr, val) \
    (sizeof(*(ptr)) == 1 ? atomic_fetch_add_8((char*)(ptr), (char)(val), memory_order_seq_cst) : \
     sizeof(*(ptr)) == 2 ? (short)atomic_fetch_add_16((short*)(ptr), (short)(val), memory_order_seq_cst) : \
     sizeof(*(ptr)) == 4 ? atomic_fetch_add_32((long*)(ptr), (long)(val), memory_order_seq_cst) : \
     sizeof(*(ptr)) == 8 ? (__int64)atomic_fetch_add_64((__int64*)(ptr), (__int64)(val), memory_order_seq_cst) : 0)

#define atomic_fetch_sub(ptr, val) \
    (sizeof(*(ptr)) == 1 ? atomic_fetch_sub_8((char*)(ptr), (char)(val), memory_order_seq_cst) : \
     sizeof(*(ptr)) == 2 ? (short)atomic_fetch_sub_16((short*)(ptr), (short)(val), memory_order_seq_cst) : \
     sizeof(*(ptr)) == 4 ? atomic_fetch_sub_32((long*)(ptr), (long)(val), memory_order_seq_cst) : \
     sizeof(*(ptr)) == 8 ? (__int64)atomic_fetch_sub_64((__int64*)(ptr), (__int64)(val), memory_order_seq_cst) : 0)

#define atomic_fetch_add_explicit(ptr, val, order) atomic_fetch_add(ptr, val)
#define atomic_fetch_sub_explicit(ptr, val, order) atomic_fetch_sub(ptr, val)

#endif /* _WIN32 */

#endif /* AGENTOS_ATOMIC_COMPAT_H */