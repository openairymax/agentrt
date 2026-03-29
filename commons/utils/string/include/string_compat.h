/**
 * @file string_compat.h
 * @brief 统一字符串处理模块 - 向后兼容层
 * 
 * 提供与标准C库字符串函数兼容的接口，便于现有代码逐步迁移到统一字符串处理模块。
 * 包含安全包装器和迁移辅助宏，防止缓冲区溢出等安全问题。
 * 
 * @copyright Copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_STRING_COMPAT_H
#define AGENTOS_STRING_COMPAT_H

#include <stddef.h>
#include <stdarg.h>
#include "string.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup string_compat_api 字符串兼容API
 * @{
 */

/**
 * @brief 安全字符串复制函数（兼容strcpy）
 * 
 * @param[out] dest 目标缓冲区
 * @param[in] src 源字符串
 * @param[in] dest_size 目标缓冲区大小
 * @return 成功返回复制的字符数，失败返回-1
 */
static inline int agentos_strcpy(char* dest, const char* src, size_t dest_size) {
    return string_copy(dest, src, dest_size);
}

/**
 * @brief 安全字符串复制函数（兼容strncpy）
 * 
 * @param[out] dest 目标缓冲区
 * @param[in] src 源字符串
 * @param[in] count 要复制的最大字符数
 * @param[in] dest_size 目标缓冲区大小
 * @return 成功返回复制的字符数，失败返回-1
 */
static inline int agentos_strncpy(char* dest, const char* src, size_t count, size_t dest_size) {
    return string_copy_n(dest, src, count, dest_size);
}

/**
 * @brief 安全字符串连接函数（兼容strcat）
 * 
 * @param[inout] dest 目标缓冲区（必须为空字符结尾）
 * @param[in] src 源字符串
 * @param[in] dest_size 目标缓冲区总大小
 * @return 成功返回连接后的总长度，失败返回-1
 */
static inline int agentos_strcat(char* dest, const char* src, size_t dest_size) {
    return string_concat(dest, src, dest_size);
}

/**
 * @brief 安全字符串连接函数（兼容strncat）
 * 
 * @param[inout] dest 目标缓冲区（必须为空字符结尾）
 * @param[in] src 源字符串
 * @param[in] count 要连接的最大字符数
 * @param[in] dest_size 目标缓冲区总大小
 * @return 成功返回连接后的总长度，失败返回-1
 */
static inline int agentos_strncat(char* dest, const char* src, size_t count, size_t dest_size) {
    return string_concat_n(dest, src, count, dest_size);
}

/**
 * @brief 安全字符串比较函数（兼容strcmp）
 * 
 * @param[in] str1 第一个字符串
 * @param[in] str2 第二个字符串
 * @return 相等返回0，str1 < str2返回负数，str1 > str2返回正数
 */
static inline int agentos_strcmp(const char* str1, const char* str2) {
    return string_compare(str1, str2, STRING_COMPARE_CASE_SENSITIVE);
}

/**
 * @brief 安全字符串比较函数（兼容strncmp）
 * 
 * @param[in] str1 第一个字符串
 * @param[in] str2 第二个字符串
 * @param[in] n 要比较的最大字符数
 * @return 相等返回0，str1 < str2返回负数，str1 > str2返回正数
 */
static inline int agentos_strncmp(const char* str1, const char* str2, size_t n) {
    return string_compare_n(str1, str2, n, STRING_COMPARE_CASE_SENSITIVE);
}

/**
 * @brief 不区分大小写的字符串比较（兼容stricmp/strcasecmp）
 * 
 * @param[in] str1 第一个字符串
 * @param[in] str2 第二个字符串
 * @return 相等返回0，str1 < str2返回负数，str1 > str2返回正数
 */
static inline int agentos_stricmp(const char* str1, const char* str2) {
    return string_compare(str1, str2, STRING_COMPARE_CASE_INSENSITIVE);
}

/**
 * @brief 不区分大小写的字符串比较（兼容strnicmp/strncasecmp）
 * 
 * @param[in] str1 第一个字符串
 * @param[in] str2 第二个字符串
 * @param[in] n 要比较的最大字符数
 * @return 相等返回0，str1 < str2返回负数，str1 > str2返回正数
 */
static inline int agentos_strnicmp(const char* str1, const char* str2, size_t n) {
    return string_compare_n(str1, str2, n, STRING_COMPARE_CASE_INSENSITIVE);
}

/**
 * @brief 安全字符串长度计算（兼容strlen）
 * 
 * @param[in] str 字符串
 * @param[in] max_len 最大检查长度（防止无界字符串）
 * @return 字符串长度（不包括空字符）
 */
static inline size_t agentos_strlen(const char* str, size_t max_len) {
    return string_length(str, max_len);
}

/**
 * @brief 查找子字符串（兼容strstr）
 * 
 * @param[in] haystack 要搜索的字符串
 * @param[in] needle 要查找的子字符串
 * @return 找到返回子字符串起始位置，未找到返回NULL
 */
static inline const char* agentos_strstr(const char* haystack, const char* needle) {
    return string_find(haystack, needle, STRING_COMPARE_CASE_SENSITIVE);
}

/**
 * @brief 查找字符（兼容strchr）
 * 
 * @param[in] str 字符串
 * @param[in] ch 要查找的字符
 * @return 找到返回字符位置，未找到返回NULL
 */
static inline const char* agentos_strchr(const char* str, char ch) {
    return string_find_char(str, ch);
}

/**
 * @brief 从末尾查找字符（兼容strrchr）
 * 
 * @param[in] str 字符串
 * @param[in] ch 要查找的字符
 * @return 找到返回字符位置，未找到返回NULL
 */
static inline const char* agentos_strrchr(const char* str, char ch) {
    return string_find_char_last(str, ch);
}

/**
 * @brief 安全格式化字符串（兼容snprintf）
 * 
 * @param[out] buffer 输出缓冲区
 * @param[in] buffer_size 缓冲区大小
 * @param[in] format 格式字符串
 * @param[in] ... 格式化参数
 * @return 成功返回写入的字符数，失败返回-1
 */
static inline int agentos_snprintf(char* buffer, size_t buffer_size, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int result = string_format_v(buffer, buffer_size, format, args);
    va_end(args);
    return result;
}

/**
 * @brief 安全格式化字符串（兼容vsnprintf）
 * 
 * @param[out] buffer 输出缓冲区
 * @param[in] buffer_size 缓冲区大小
 * @param[in] format 格式字符串
 * @param[in] args 格式化参数
 * @return 成功返回写入的字符数，失败返回-1
 */
static inline int agentos_vsnprintf(char* buffer, size_t buffer_size, const char* format, va_list args) {
    return string_format_v(buffer, buffer_size, format, args);
}

/**
 * @brief 兼容性宏定义
 */

/**
 * @def AGENTOS_STRCPY(dest, src, dest_size)
 * @brief 安全字符串复制宏
 */
#define AGENTOS_STRCPY(dest, src, dest_size) agentos_strcpy(dest, src, dest_size)

/**
 * @def AGENTOS_STRNCPY(dest, src, count, dest_size)
 * @brief 安全字符串复制（带长度限制）宏
 */
#define AGENTOS_STRNCPY(dest, src, count, dest_size) agentos_strncpy(dest, src, count, dest_size)

/**
 * @def AGENTOS_STRCAT(dest, src, dest_size)
 * @brief 安全字符串连接宏
 */
#define AGENTOS_STRCAT(dest, src, dest_size) agentos_strcat(dest, src, dest_size)

/**
 * @def AGENTOS_STRNCAT(dest, src, count, dest_size)
 * @brief 安全字符串连接（带长度限制）宏
 */
#define AGENTOS_STRNCAT(dest, src, count, dest_size) agentos_strncat(dest, src, count, dest_size)

/**
 * @def AGENTOS_STRCMP(str1, str2)
 * @brief 安全字符串比较宏
 */
#define AGENTOS_STRCMP(str1, str2) agentos_strcmp(str1, str2)

/**
 * @def AGENTOS_STRNCMP(str1, str2, n)
 * @brief 安全字符串比较（带长度限制）宏
 */
#define AGENTOS_STRNCMP(str1, str2, n) agentos_strncmp(str1, str2, n)

/**
 * @def AGENTOS_STRICMP(str1, str2)
 * @brief 不区分大小写的安全字符串比较宏
 */
#define AGENTOS_STRICMP(str1, str2) agentos_stricmp(str1, str2)

/**
 * @def AGENTOS_STRNICMP(str1, str2, n)
 * @brief 不区分大小写的安全字符串比较（带长度限制）宏
 */
#define AGENTOS_STRNICMP(str1, str2, n) agentos_strnicmp(str1, str2, n)

/**
 * @def AGENTOS_STRLEN(str, max_len)
 * @brief 安全字符串长度计算宏
 */
#define AGENTOS_STRLEN(str, max_len) agentos_strlen(str, max_len)

/**
 * @def AGENTOS_STRSTR(haystack, needle)
 * @brief 安全子字符串查找宏
 */
#define AGENTOS_STRSTR(haystack, needle) agentos_strstr(haystack, needle)

/**
 * @def AGENTOS_STRCHR(str, ch)
 * @brief 安全字符查找宏
 */
#define AGENTOS_STRCHR(str, ch) agentos_strchr(str, ch)

/**
 * @def AGENTOS_STRRCHR(str, ch)
 * @brief 安全字符从末尾查找宏
 */
#define AGENTOS_STRRCHR(str, ch) agentos_strrchr(str, ch)

/**
 * @def AGENTOS_SNPRINTF(buffer, buffer_size, format, ...)
 * @brief 安全格式化字符串宏
 */
#define AGENTOS_SNPRINTF(buffer, buffer_size, format, ...) \
    agentos_snprintf(buffer, buffer_size, format, ##__VA_ARGS__)

/**
 * @def AGENTOS_VSNPRINTF(buffer, buffer_size, format, args)
 * @brief 安全格式化字符串（va_list版本）宏
 */
#define AGENTOS_VSNPRINTF(buffer, buffer_size, format, args) \
    agentos_vsnprintf(buffer, buffer_size, format, args)

/**
 * @brief 迁移辅助：替换标准库字符串函数
 * 
 * 建议的迁移步骤：
 * 1. 包含本头文件
 * 2. 将strcpy替换为AGENTOS_STRCPY并添加缓冲区大小参数
 * 3. 将strcat替换为AGENTOS_STRCAT并添加缓冲区大小参数
 * 4. 将strcmp替换为AGENTOS_STRCMP
 * 5. 逐步迁移其他函数
 */

/** @} */ // end of string_compat_api

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_STRING_COMPAT_H */