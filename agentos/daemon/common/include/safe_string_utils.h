/**
 * @file safe_string_utils.h
 * @brief 安全字符串处理工具
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * @version 1.0.0
 * @date 2026-04-04
 *
 * 设计说明：
 * - 提供安全的字符串操作函数，防止缓冲区溢出
 * - 所有函数都有明确的边界检查
 * - 遵循ARCHITECTURAL_PRINCIPLES.md E-3资源确定性原则
 */

#ifndef AGENTOS_SAFE_STRING_UTILS_H
#define AGENTOS_SAFE_STRING_UTILS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 安全复制字符串（带长度限制）
 * @param dest 目标缓冲区
 * @param src 源字符串
 * @param dest_size 目标缓冲区大小
 * @return int 0成功，非0失败
 */
int safe_strcpy(char* dest, const char* src, size_t dest_size);

/**
 * @brief 安全拼接字符串（带长度限制）
 * @param dest 目标缓冲区
 * @param src 源字符串
 * @param dest_size 目标缓冲区大小
 * @return int 0成功，非0失败
 */
int safe_strcat(char* dest, const char* src, size_t dest_size);

/**
 * @brief 安全格式化字符串（带长度限制）
 * @param dest 目标缓冲区
 * @param dest_size 目标缓冲区大小
 * @param fmt 格式化字符串
 * @param ... 可变参数
 * @return int 写入的字符数，失败返回负数
 */
int safe_sprintf(char* dest, size_t dest_size, const char* fmt, ...);

/**
 * @brief 计算字符串长度（安全版本）
 * @param str 字符串
 * @param max_len 最大长度限制
 * @return size_t 字符串长度
 */
size_t safe_strlen(const char* str, size_t max_len);

/**
 * @brief 比较两个字符串（安全版本）
 * @param str1 第一个字符串
 * @param str2 第二个字符串
 * @param max_len 最大比较长度
 * @return int 相等返回0，不等返回差值
 */
int safe_strcmp(const char* str1, const char* str2, size_t max_len);

/**
 * @brief 安全分配内存并复制字符串
 * @param str 源字符串
 * @param max_copy_len 最大复制长度（0表示不限制）
 * @return char* 新分配的字符串，调用者负责free()
 */
char* safe_strdup_with_limit(const char* str, size_t max_copy_len);

/**
 * @brief 验证字符串是否为有效ASCII
 * @param str 字符串
 * @param len 长度
 * @return int 1有效，0无效
 */
int is_valid_ascii(const char* str, size_t len);

/**
 * @brief 清除敏感信息（安全擦除）
 * @param buf 缓冲区
 * @param size 大小
 */
void secure_clear(void* buf, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_SAFE_STRING_UTILS_H */
