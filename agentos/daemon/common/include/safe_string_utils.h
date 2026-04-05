/**
 * @file safe_string_utils.h
 * @brief 安全字符串处理工具
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_SAFE_STRING_UTILS_H
#define AGENTOS_SAFE_STRING_UTILS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int safe_strcpy(char* dest, const char* src, size_t dest_size);
int safe_strcat(char* dest, const char* src, size_t dest_size);
int safe_sprintf(char* dest, size_t dest_size, const char* fmt, ...);
size_t safe_strlen(const char* str, size_t max_len);
int safe_strcmp(const char* str1, const char* str2, size_t max_len);
char* safe_strdup_with_limit(const char* str, size_t max_copy_len);
void secure_clear(void* buf, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_SAFE_STRING_UTILS_H */
