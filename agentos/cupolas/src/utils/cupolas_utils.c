/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * cupolas_utils.c - Common Utility Functions Implementation
 */

#include "cupolas_utils.h"
#include <stdio.h>
#include <stdarg.h>

/* 简单的字符串复制实现 */
char* cupolas_strdup(const char* str) {
    if (!str) return NULL;
    
    size_t len = strlen(str) + 1;
    char* dup = (char*)malloc(len);
    if (dup) {
        memcpy(dup, str, len);
    }
    return dup;
}

/* 安全的字符串复制 */
int cupolas_strlcpy(char* dst, const char* src, size_t size) {
    if (!dst || size == 0) return -1;
    if (!src) {
        dst[0] = '\0';
        return 0;
    }
    
    size_t src_len = strlen(src);
    size_t copy_len = (src_len >= size) ? size - 1 : src_len;
    
    memcpy(dst, src, copy_len);
    dst[copy_len] = '\0';
    
    return (int)src_len;
}

/* 安全的内存设置 */
void* cupolas_memset_s(void* ptr, size_t size, int value) {
    if (!ptr || size == 0) return NULL;
    
    volatile unsigned char* p = (volatile unsigned char*)ptr;
    while (size--) {
        *p++ = (unsigned char)value;
    }
    
    return ptr;
}

/* 安全的内存比较 */
int cupolas_memcmp_s(const void* ptr1, const void* ptr2, size_t size) {
    if (!ptr1 || !ptr2) return -1;
    
    const unsigned char* p1 = (const unsigned char*)ptr1;
    const unsigned char* p2 = (const unsigned char*)ptr2;
    
    while (size--) {
        if (*p1 != *p2) {
            return (*p1 - *p2);
        }
        p1++;
        p2++;
    }
    
    return 0;
}

/* 时间戳获取 */
uint64_t cupolas_get_timestamp_ms(void) {
#ifdef _WIN32
    return GetTickCount64();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
#endif
}

/* 简单哈希函数 */
uint32_t cupolas_hash_string(const char* str) {
    if (!str) return 0;
    
    uint32_t hash = 5381;
    int c;
    
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    
    return hash;
}

/* 日志格式化输出 */
void cupolas_log_message(const char* level, const char* format, ...) {
    if (!level || !format) return;
    
    va_list args;
    va_start(args, format);
    
    fprintf(stderr, "[CUPOLAS %s] ", level);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    
    va_end(args);
}
