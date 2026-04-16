/**
 * @file string_common.c
 * @brief 字符串工具公共库实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "string_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/**
 * @brief 安全的字符串复制函数
 */
char* string_common_strlcpy(char* dest, size_t dest_size, const char* src) {
    if (dest_size == 0) {
        return dest;
    }
    
    size_t src_len = strlen(src);
    size_t copy_len = (src_len < dest_size - 1) ? src_len : dest_size - 1;
    
    memcpy(dest, src, copy_len);
    dest[copy_len] = '\0';
    
    return dest;
}

/**
 * @brief 安全的字符串连接函数
 */
char* string_common_strlcat(char* dest, size_t dest_size, const char* src) {
    if (dest_size == 0) {
        return dest;
    }
    
    size_t dest_len = strlen(dest);
    if (dest_len >= dest_size - 1) {
        return dest;
    }
    
    size_t src_len = strlen(src);
    size_t remaining = dest_size - dest_len - 1;
    size_t copy_len = (src_len < remaining) ? src_len : remaining;
    
    memcpy(dest + dest_len, src, copy_len);
    dest[dest_len + copy_len] = '\0';
    
    return dest;
}

/**
 * @brief 字符串复制（动态内存分配）
 */
char* string_common_strdup(const char* str) {
    if (!str) {
        return NULL;
    }
    
    size_t len = strlen(str) + 1;
    char* dup = (char*)malloc(len);
    if (dup) {
        memcpy(dup, str, len);
    }
    return dup;
}

/**
 * @brief 字符串复制（指定长度，动态内存分配）
 */
char* string_common_strndup(const char* str, size_t n) {
    if (!str) {
        return NULL;
    }
    
    size_t len = strnlen(str, n);
    char* dup = (char*)malloc(len + 1);
    if (dup) {
        memcpy(dup, str, len);
        dup[len] = '\0';
    }
    return dup;
}

/**
 * @brief 大小写不敏感的字符串比较
 */
int string_common_strcasecmp(const char* s1, const char* s2) {
    if (!s1 || !s2) {
        return (s1 == s2) ? 0 : (s1 ? 1 : -1);
    }
    
    while (*s1 && *s2) {
        int c1 = tolower((unsigned char)*s1);
        int c2 = tolower((unsigned char)*s2);
        if (c1 != c2) {
            return c1 - c2;
        }
        s1++;
        s2++;
    }
    
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

/**
 * @brief 大小写不敏感的字符串比较（指定长度）
 */
int string_common_strncasecmp(const char* s1, const char* s2, size_t n) {
    if (!s1 || !s2) {
        return (s1 == s2) ? 0 : (s1 ? 1 : -1);
    }
    
    while (n > 0 && *s1 && *s2) {
        int c1 = tolower((unsigned char)*s1);
        int c2 = tolower((unsigned char)*s2);
        if (c1 != c2) {
            return c1 - c2;
        }
        s1++;
        s2++;
        n--;
    }
    
    return 0;
}

/**
 * @brief 字符串查找
 */
char* string_common_strstr(const char* haystack, const char* needle) {
    return strstr(haystack, needle);
}

/**
 * @brief 字符串分割
 */
char** string_common_strsplit(const char* str, const char* delim) {
    if (!str || !delim) {
        return NULL;
    }
    
    size_t count = 0;
    const char* temp = str;
    
    // 计算分割后的字符串数量
    while (strstr(temp, delim)) {
        count++;
        temp = strstr(temp, delim) + strlen(delim);
    }
    count++;
    
    // 分配字符串数组
    char** arr = (char**)malloc((count + 1) * sizeof(char*));
    if (!arr) {
        return NULL;
    }
    
    // 分割字符串
    size_t index = 0;
    temp = str;
    while (index < count) {
        const char* next = strstr(temp, delim);
        if (next) {
            size_t len = next - temp;
            arr[index] = string_common_strndup(temp, len);
            temp = next + strlen(delim);
        } else {
            arr[index] = string_common_strdup(temp);
        }
        index++;
    }
    arr[index] = NULL;
    
    return arr;
}

/**
 * @brief 释放字符串数组
 */
void string_common_strsplit_free(char** arr) {
    if (!arr) {
        return;
    }
    
    for (size_t i = 0; arr[i]; i++) {
        free(arr[i]);
    }
    free(arr);
}

/**
 * @brief 字符串转换为整数
 */
bool string_common_strtoint(const char* str, int base, int* result) {
    if (!str || !result) {
        return false;
    }
    
    char* endptr;
    long value = strtol(str, &endptr, base);
    
    if (*endptr != '\0') {
        return false;
    }
    
    *result = (int)value;
    return true;
}

/**
 * @brief 字符串转换为无符号整数
 */
bool string_common_strtouint(const char* str, int base, uint32_t* result) {
    if (!str || !result) {
        return false;
    }
    
    char* endptr;
    unsigned long value = strtoul(str, &endptr, base);
    
    if (*endptr != '\0') {
        return false;
    }
    
    *result = (uint32_t)value;
    return true;
}

/**
 * @brief 字符串转换为双精度浮点数
 */
bool string_common_strtod(const char* str, double* result) {
    if (!str || !result) {
        return false;
    }
    
    char* endptr;
    double value = strtod(str, &endptr);
    
    if (*endptr != '\0') {
        return false;
    }
    
    *result = value;
    return true;
}

/**
 * @brief 整数转换为字符串
 */
size_t string_common_itoa(int value, int base, char* buf, size_t buf_size) {
    if (!buf || buf_size == 0) {
        return 0;
    }
    
    if (base < 2 || base > 36) {
        return 0;
    }
    
    char* ptr = buf;
    bool negative = (value < 0 && base == 10);
    
    if (negative) {
        value = -value;
    }
    
    // 转换为字符串
    do {
        int digit = value % base;
        *ptr++ = (digit < 10) ? (digit + '0') : (digit - 10 + 'a');
        value /= base;
    } while (value > 0 && (size_t)(ptr - buf) < buf_size - 1);
    
    if (negative) {
        *ptr++ = '-';
    }
    
    *ptr = '\0';
    
    // 反转字符串
    size_t len = ptr - buf;
    for (size_t i = 0; i < len / 2; i++) {
        char temp = buf[i];
        buf[i] = buf[len - i - 1];
        buf[len - i - 1] = temp;
    }
    
    return len;
}

/**
 * @brief 无符号整数转换为字符串
 */
size_t string_common_utoa(uint32_t value, int base, char* buf, size_t buf_size) {
    if (!buf || buf_size == 0) {
        return 0;
    }
    
    if (base < 2 || base > 36) {
        return 0;
    }
    
    char* ptr = buf;
    
    // 转换为字符串
    do {
        int digit = value % base;
        *ptr++ = (digit < 10) ? (digit + '0') : (digit - 10 + 'a');
        value /= base;
    } while (value > 0 && (size_t)(ptr - buf) < buf_size - 1);
    
    *ptr = '\0';
    
    // 反转字符串
    size_t len = ptr - buf;
    for (size_t i = 0; i < len / 2; i++) {
        char temp = buf[i];
        buf[i] = buf[len - i - 1];
        buf[len - i - 1] = temp;
    }
    
    return len;
}

/**
 * @brief 双精度浮点数转换为字符串
 */
size_t string_common_ftoa(double value, int precision, char* buf, size_t buf_size) {
    if (!buf || buf_size == 0) {
        return 0;
    }
    
    int len = snprintf(buf, buf_size, "%.*f", precision, value);
    return (len < 0) ? 0 : (size_t)len;
}

/**
 * @brief 字符串修剪（去除首尾空白字符）
 */
char* string_common_strtrim(char* str) {
    if (!str) {
        return NULL;
    }
    
    // 去除开头空白字符
    while (isspace((unsigned char)*str)) {
        str++;
    }
    
    // 去除结尾空白字符
    char* end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) {
        end--;
    }
    
    *(end + 1) = '\0';
    
    return str;
}

/**
 * @brief 字符串转小写
 */
char* string_common_strtolower(char* str) {
    if (!str) {
        return NULL;
    }
    
    char* ptr = str;
    while (*ptr) {
        *ptr = tolower((unsigned char)*ptr);
        ptr++;
    }
    
    return str;
}

/**
 * @brief 字符串转大写
 */
char* string_common_strtoupper(char* str) {
    if (!str) {
        return NULL;
    }
    
    char* ptr = str;
    while (*ptr) {
        *ptr = toupper((unsigned char)*ptr);
        ptr++;
    }
    
    return str;
}
