/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file secure_mem.c
 * @brief 安全内存操作实现
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "secure_mem.h"
#include <string.h>

/*
 * 安全内存操作实现策略：
 *
 * 1. 使用 volatile 关键字防止编译器优化
 *    - volatile 告诉编译器不要缓存值，每次都必须从内存读取/写入
 *
 * 2. 使用内存屏障（memory barrier）
 *    - 确保在继续执行之前完成所有内存操作
 *
 * 3. 使用临时变量
 *    - 避免编译器将多个操作合并为一个
 *
 * 4. 引用参数
 *    - 确保函数不能被内联或优化掉
 */

void secure_memset(void* ptr, uint8_t value, size_t n) {
    if (!ptr || n == 0) {
        return;
    }

    /* 使用 volatile 指针防止编译器优化 */
    volatile uint8_t* vp = (volatile uint8_t*)ptr;

    /* 先设置值 */
    for (size_t i = 0; i < n; i++) {
        vp[i] = value;
    }

    /* 内存屏障 - 确保所有写入完成 */
#if defined(__GNUC__) || defined(__clang__)
    __asm__ __volatile__("" ::: "memory");
#elif defined(_MSC_VER)
    _ReadWriteBarrier();
#endif

    /* 再次设置值以防止编译器将多个操作合并 */
    for (size_t i = 0; i < n; i++) {
        vp[i] = value;
    }

#if defined(__GNUC__) || defined(__clang__)
    __asm__ __volatile__("" ::: "memory");
#elif defined(_MSC_VER)
    _ReadWriteBarrier();
#endif
}

void secure_free(void* ptr, size_t size) {
    if (!ptr) {
        return;
    }

    /* 先清零内存 */
    secure_memset(ptr, 0, size);

    /* 然后释放 */
    free(ptr);
}

int secure_memcmp(const void* s1, const void* s2, size_t n) {
    if (!s1 || !s2) {
        return (s1 != s2) ? -1 : 0;
    }

    if (n == 0) {
        return 0;
    }

    const uint8_t* p1 = (const uint8_t*)s1;
    const uint8_t* p2 = (const uint8_t*)s2;

    /* 使用 volatile 防止优化 */
    volatile uint8_t diff = 0;
    volatile uint8_t tmp = 0;

    /* 常量时间比较 - 不使用短路求值 */
    for (size_t i = 0; i < n; i++) {
        tmp = p1[i] ^ p2[i];
        diff |= tmp;
    }

    /* 内存屏障 */
#if defined(__GNUC__) || defined(__clang__)
    __asm__ __volatile__("" ::: "memory");
#elif defined(_MSC_VER)
    _ReadWriteBarrier();
#endif

    return (int)diff;
}

void secure_memcpy(void* dest, const void* src, size_t n) {
    if (!dest || !src || n == 0) {
        return;
    }

    /* 先复制数据 */
    memcpy(dest, src, n);

    /* 内存屏障 */
#if defined(__GNUC__) || defined(__clang__)
    __asm__ __volatile__("" ::: "memory");
#elif defined(_MSC_VER)
    _ReadWriteBarrier();
#endif

    /* 清零源缓冲区 */
    secure_memset((void*)src, 0, n);

    /* 内存屏障 */
#if defined(__GNUC__) || defined(__clang__)
    __asm__ __volatile__("" ::: "memory");
#elif defined(_MSC_VER)
    _ReadWriteBarrier();
#endif
}

bool is_mem_zero(const void* ptr, size_t n) {
    if (!ptr || n == 0) {
        return true;
    }

    const uint8_t* p = (const uint8_t*)ptr;

    /* 使用 volatile 防止优化 */
    volatile uint8_t result = 0;

    /* 常量时间检查 - 不使用短路求值 */
    for (size_t i = 0; i < n; i++) {
        result |= p[i];
    }

    /* 内存屏障 */
#if defined(__GNUC__) || defined(__clang__)
    __asm__ __volatile__("" ::: "memory");
#elif defined(_MSC_VER)
    _ReadWriteBarrier();
#endif

    return (result == 0);
}

int secure_strcmp(const char* s1, const char* s2) {
    if (!s1 || !s2) {
        return (s1 != s2) ? -1 : 0;
    }

    /* 使用常量时间比较 */
    size_t len1 = strlen(s1);
    size_t len2 = strlen(s2);

    /* 如果长度不同，先比较长度（但不泄露具体差异） */
    volatile uint8_t diff = (len1 != len2) ? 1 : 0;

    /* 比较较短的长度 */
    size_t min_len = (len1 < len2) ? len1 : len2;

    /* 常量时间比较 */
    for (size_t i = 0; i < min_len; i++) {
        diff |= (s1[i] != s2[i]);
    }

    /* 内存屏障 */
#if defined(__GNUC__) || defined(__clang__)
    __asm__ __volatile__("" ::: "memory");
#elif defined(_MSC_VER)
    _ReadWriteBarrier();
#endif

    return (int)diff;
}
