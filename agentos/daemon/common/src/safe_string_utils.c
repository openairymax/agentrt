/**
 * @file safe_string_utils.c
 * @brief 安全字符串处理工具实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "safe_string_utils.h"
#include "svc_logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

int safe_strcpy(char* dest, const char* src, size_t dest_size) {
    if (!dest || !src || dest_size == 0) return -1;

    size_t src_len = strlen(src);
    if (src_len >= dest_size) {
        memcpy(dest, src, dest_size - 1);
        dest[dest_size - 1] = '\0';
        return -2;
    }

    memcpy(dest, src, src_len + 1);
    return 0;
}

int safe_strcat(char* dest, const char* src, size_t dest_size) {
    if (!dest || !src || dest_size == 0) return -1;

    size_t dest_len = strlen(dest);
    size_t src_len = strlen(src);

    if (dest_len + src_len >= dest_size) {
        size_t remaining = dest_size - dest_len - 1;
        memcpy(dest + dest_len, src, remaining);
        dest[dest_len + remaining] = '\0';
        return -2;
    }

    memcpy(dest + dest_len, src, src_len + 1);
    return 0;
}

int safe_sprintf(char* dest, size_t dest_size, const char* fmt, ...) {
    if (!dest || !fmt || dest_size == 0) return -1;
    
    va_list args;
    va_start(args, fmt);
    int written = vsnprintf(dest, dest_size, fmt, args); /* flawfinder: ignore - safe_sprintf wrapper with bounds-checked dest */
    va_end(args);
    
    if (written < 0 || (size_t)written >= dest_size) {
        dest[dest_size - 1] = '\0';
        return -1;
    }
    
    return written;
}

size_t safe_strlen(const char* str, size_t max_len) {
    if (!str) return 0;
    for (size_t i = 0; i < max_len; i++) {
        if (str[i] == '\0') return i;
    }
    return max_len;
}

int safe_strcmp(const char* str1, const char* str2, size_t max_len) {
    if (!str1 && !str2) return 0;
    if (!str1) return -1;
    if (!str2) return 1;
    
    for (size_t i = 0; i < max_len; i++) {
        if (str1[i] == '\0' && str2[i] == '\0') return 0;
        if (str1[i] == '\0') return -1;
        if (str2[i] == '\0') return 1;
        int diff = (unsigned char)str1[i] - (unsigned char)str2[i];
        if (diff != 0) return diff;
    }
    return 0;
}

char* safe_strdup_with_limit(const char* str, size_t max_copy_len) {
    if (!str) return NULL;
    
    size_t len = strlen(str);
    if (max_copy_len > 0 && len > max_copy_len) len = max_copy_len;
    
    char* copy = (char*)malloc(len + 1);
    if (!copy) return NULL;
    
    memcpy(copy, str, len);
    copy[len] = '\0';
    return copy;
}

void secure_clear(void* buf, size_t size) {
    if (!buf || size == 0) return;
    volatile unsigned char* p = (volatile unsigned char*)buf;
    for (size_t i = 0; i < size; i++) p[i] = 0;
}

/* ==================== 输入验证函数（规范 3.2.2） ==================== */

bool validate_string_input(const char* str, size_t max_len) {
    if (!str) return false;
    size_t len = 0;
    for (size_t i = 0; i < max_len; i++) {
        if (str[i] == '\0') { len = i; break; }
        if (i == max_len - 1) return false;
    }
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)str[i];
        if (c < 0x20 && c != '\t' && c != '\n' && c != '\r') return false;
    }
    return true;
}

bool validate_pointer(const void* ptr) {
    return ptr != NULL;
}

bool validate_range(int64_t value, int64_t min_val, int64_t max_val) {
    return value >= min_val && value <= max_val;
}

/* ==================== 安全内存操作 ==================== */

void* safe_malloc(size_t size, const char* purpose) {
    (void)purpose;
    if (size == 0) return NULL;
    void* ptr = malloc(size);
    return ptr;
}

void* safe_calloc(size_t count, size_t size, const char* purpose) {
    (void)purpose;
    if (count == 0 || size == 0) return NULL;
    if (count > SIZE_MAX / size) return NULL;
    void* ptr = calloc(count, size);
    return ptr;
}

void* safe_realloc(void* ptr, size_t new_size, const char* purpose) {
    (void)purpose;
    if (new_size == 0) { free(ptr); return NULL; }
    void* new_ptr = realloc(ptr, new_size);
    return new_ptr;
}
