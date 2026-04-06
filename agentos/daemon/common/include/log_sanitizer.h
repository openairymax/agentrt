/**
 * @file log_sanitizer.h
 * @brief 日志脱敏过滤器 - 防止敏感信息泄露到日志
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * 功能：
 * 1. 自动检测并脱敏敏感字段（API Key、密码、Token 等）
 * 2. 支持自定义敏感字段模式
 * 3. 线程安全设计
 * 4. 零拷贝模式（性能优化）
 */

#ifndef AGENTOS_LOG_SANITIZER_H
#define AGENTOS_LOG_SANITIZER_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 敏感字段模式
 */
typedef struct {
    const char* pattern;      /* 匹配模式（如 "api_key", "password"） */
    const char* replacement;  /* 替换字符串（默认为 "***"） */
} sensitive_field_t;

/**
 * @brief 初始化日志脱敏器
 * @param max_fields 最大敏感字段数量
 */
void log_sanitizer_init(size_t max_fields);

/**
 * @brief 销毁日志脱敏器
 */
void log_sanitizer_destroy(void);

/**
 * @brief 添加敏感字段模式
 * @param pattern 匹配模式
 * @param replacement 替换字符串（可为 NULL，使用默认 "***"）
 * @return true 成功，false 失败
 */
bool log_sanitizer_add_pattern(const char* pattern, const char* replacement);

/**
 * @brief 脱敏日志消息
 * @param message 原始消息
 * @param buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 * @return 脱敏后的消息长度，-1 失败
 */
int log_sanitize(const char* message, char* buffer, size_t buffer_size);

/**
 * @brief 脱敏日志消息（动态分配版本）
 * @param message 原始消息
 * @return 脱敏后的消息（需调用者 free），NULL 失败
 */
char* log_sanitize_dup(const char* message);

/**
 * @brief 检查消息是否包含敏感信息
 * @param message 消息
 * @return true 包含敏感信息，false 不包含
 */
bool log_contains_sensitive(const char* message);

/**
 * @brief 获取默认的敏感字段列表
 * @return 敏感字段数组
 */
const sensitive_field_t* log_get_default_patterns(size_t* count);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_LOG_SANITIZER_H */
