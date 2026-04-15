/**
 * @file safe_string_utils.h
 * @brief 安全字符串处理工具
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * @details
 * 提供安全字符串操作函数，替代不安全的 strcpy/strcat/sprintf/gets 等。
 * 所有函数均进行边界检查和空指针验证，防止缓冲区溢出。
 * 遵循 AgentOS 安全编码规范 3.2.2 节要求。
 */

#ifndef AGENTOS_SAFE_STRING_UTILS_H
#define AGENTOS_SAFE_STRING_UTILS_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 安全字符串操作 ==================== */

/** @brief 安全字符串拷贝（替代 strcpy/strncpy） */
int safe_strcpy(char* dest, const char* src, size_t dest_size);

/** @brief 安全字符串连接（替代 strcat/strncat） */
int safe_strcat(char* dest, const char* src, size_t dest_size);

/** @brief 安全格式化输出（替代 sprintf/vsprintf） */
int safe_sprintf(char* dest, size_t dest_size, const char* fmt, ...);

/** @brief 安全计算字符串长度（带最大长度限制） */
size_t safe_strlen(const char* str, size_t max_len);

/** @brief 安全字符串比较（带最大长度限制） */
int safe_strcmp(const char* str1, const char* str2, size_t max_len);

/** @brief 安全字符串复制（带最大复制长度限制） */
char* safe_strdup_with_limit(const char* str, size_t max_copy_len);

/** @brief 安全内存清零（防止编译器优化删除） */
void secure_clear(void* buf, size_t size);

/* ==================== 输入验证函数（规范 3.2.2） ==================== */

/** @brief 验证字符串输入（非空、非超长、无控制字符） */
bool validate_string_input(const char* str, size_t max_len);

/** @brief 验证指针非空 */
bool validate_pointer(const void* ptr);

/** @brief 验证数值范围 */
bool validate_range(int64_t value, int64_t min_val, int64_t max_val);

/* ==================== 安全内存操作 ==================== */

/** @brief 安全内存分配（带用途标记） */
void* safe_malloc(size_t size, const char* purpose);

/** @brief 安全内存分配-清零（带用途标记） */
void* safe_calloc(size_t count, size_t size, const char* purpose);

/** @brief 安全内存重分配（带用途标记） */
void* safe_realloc(void* ptr, size_t new_size, const char* purpose);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_SAFE_STRING_UTILS_H */
