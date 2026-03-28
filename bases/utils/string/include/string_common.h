/**
 * @file string_common.h
 * @brief 字符串工具公共库
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 提供统一的字符串操作接口，包括：
 * - 字符串复制和连接
 * - 字符串比较
 * - 字符串查找和替换
 * - 字符串转换
 * - 字符串内存管理
 */

#ifndef STRING_COMMON_H
#define STRING_COMMON_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 安全的字符串复制函数
 * @param dest 目标字符串
 * @param dest_size 目标字符串大小
 * @param src 源字符串
 * @return 目标字符串指针
 */
char* string_common_strlcpy(char* dest, size_t dest_size, const char* src);

/**
 * @brief 安全的字符串连接函数
 * @param dest 目标字符串
 * @param dest_size 目标字符串大小
 * @param src 源字符串
 * @return 目标字符串指针
 */
char* string_common_strlcat(char* dest, size_t dest_size, const char* src);

/**
 * @brief 字符串复制（动态内存分配）
 * @param str 源字符串
 * @return 复制的字符串指针，需要调用 free() 释放
 */
char* string_common_strdup(const char* str);

/**
 * @brief 字符串复制（指定长度，动态内存分配）
 * @param str 源字符串
 * @param n 最大复制长度
 * @return 复制的字符串指针，需要调用 free() 释放
 */
char* string_common_strndup(const char* str, size_t n);

/**
 * @brief 大小写不敏感的字符串比较
 * @param s1 字符串1
 * @param s2 字符串2
 * @return 比较结果
 */
int string_common_strcasecmp(const char* s1, const char* s2);

/**
 * @brief 大小写不敏感的字符串比较（指定长度）
 * @param s1 字符串1
 * @param s2 字符串2
 * @param n 最大比较长度
 * @return 比较结果
 */
int string_common_strncasecmp(const char* s1, const char* s2, size_t n);

/**
 * @brief 字符串查找
 * @param haystack 待查找的字符串
 * @param needle 要查找的子串
 * @return 子串在字符串中的位置指针，未找到返回 NULL
 */
char* string_common_strstr(const char* haystack, const char* needle);

/**
 * @brief 字符串分割
 * @param str 要分割的字符串
 * @param delim 分隔符
 * @return 分割后的字符串数组，最后一个元素为 NULL
 */
char** string_common_strsplit(const char* str, const char* delim);

/**
 * @brief 释放字符串数组
 * @param arr 字符串数组
 */
void string_common_strsplit_free(char** arr);

/**
 * @brief 字符串转换为整数
 * @param str 字符串
 * @param base 进制
 * @param result 转换结果
 * @return 成功返回 true，失败返回 false
 */
bool string_common_strtoint(const char* str, int base, int* result);

/**
 * @brief 字符串转换为无符号整数
 * @param str 字符串
 * @param base 进制
 * @param result 转换结果
 * @return 成功返回 true，失败返回 false
 */
bool string_common_strtouint(const char* str, int base, uint32_t* result);

/**
 * @brief 字符串转换为双精度浮点数
 * @param str 字符串
 * @param result 转换结果
 * @return 成功返回 true，失败返回 false
 */
bool string_common_strtod(const char* str, double* result);

/**
 * @brief 整数转换为字符串
 * @param value 整数值
 * @param base 进制
 * @param buf 缓冲区
 * @param buf_size 缓冲区大小
 * @return 转换后的字符串长度
 */
size_t string_common_itoa(int value, int base, char* buf, size_t buf_size);

/**
 * @brief 无符号整数转换为字符串
 * @param value 无符号整数值
 * @param base 进制
 * @param buf 缓冲区
 * @param buf_size 缓冲区大小
 * @return 转换后的字符串长度
 */
size_t string_common_utoa(uint32_t value, int base, char* buf, size_t buf_size);

/**
 * @brief 双精度浮点数转换为字符串
 * @param value 浮点数值
 * @param precision 小数位数
 * @param buf 缓冲区
 * @param buf_size 缓冲区大小
 * @return 转换后的字符串长度
 */
size_t string_common_ftoa(double value, int precision, char* buf, size_t buf_size);

/**
 * @brief 字符串修剪（去除首尾空白字符）
 * @param str 字符串
 * @return 修剪后的字符串指针
 */
char* string_common_strtrim(char* str);

/**
 * @brief 字符串转小写
 * @param str 字符串
 * @return 转换后的字符串指针
 */
char* string_common_strtolower(char* str);

/**
 * @brief 字符串转大写
 * @param str 字符串
 * @return 转换后的字符串指针
 */
char* string_common_strtoupper(char* str);

#ifdef __cplusplus
}
#endif

#endif /* STRING_COMMON_H */
