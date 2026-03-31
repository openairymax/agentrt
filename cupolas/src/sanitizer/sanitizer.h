/**
 * @file sanitizer.h
 * @brief 输入净化器公共接口 - 防止注入攻击
 * @author Spharx
 * @date 2024
 * 
 * 设计原则：
 * - 白名单优先：只允许已知安全的模式
 * - 多层防御：规则 + 长度 + 编码检查
 * - 可配置：支持自定义规则
 */

#ifndef CUPOLAS_SANITIZER_H
#define CUPOLAS_SANITIZER_H

#include "../platform/platform.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 净化结果 */
typedef enum sanitize_result {
    SANITIZE_OK = 0,
    SANITIZE_MODIFIED,
    SANITIZE_REJECTED,
    SANITIZE_ERROR
} sanitize_result_t;

/* 净化级别 */
typedef enum sanitize_level {
    SANITIZE_LEVEL_STRICT = 0,
    SANITIZE_LEVEL_NORMAL,
    SANITIZE_LEVEL_RELAXED
} sanitize_level_t;

/* 净化上下文 */
typedef struct sanitize_context {
    const char* agent_id;
    const char* input_type;
    sanitize_level_t level;
    size_t max_length;
    bool allow_html;
    bool allow_sql;
    bool allow_shell;
    bool allow_path;
} sanitize_context_t;

/* 净化器句柄 */
typedef struct sanitizer sanitizer_t;

/**
 * @brief 创建净化器
 * @param rules_path 规则文件路径（可选）
 * @return 净化器句柄，失败返回 NULL
 */
sanitizer_t* sanitizer_create(const char* rules_path);

/**
 * @brief 销毁净化器
 * @param sanitizer 净化器句柄
 */
void sanitizer_destroy(sanitizer_t* sanitizer);

/**
 * @brief 净化输入
 * @param sanitizer 净化器句柄
 * @param input 输入字符串
 * @param output 输出缓冲区
 * @param output_size 输出缓冲区大小
 * @param ctx 净化上下文
 * @return 净化结果
 */
sanitize_result_t sanitizer_sanitize(sanitizer_t* sanitizer,
                                      const char* input,
                                      char* output,
                                      size_t output_size,
                                      const sanitize_context_t* ctx);

/**
 * @brief 检查输入是否安全
 * @param sanitizer 净化器句柄
 * @param input 输入字符串
 * @param ctx 净化上下文
 * @return true 安全，false 不安全
 */
bool sanitizer_is_safe(sanitizer_t* sanitizer,
                       const char* input,
                       const sanitize_context_t* ctx);

/**
 * @brief 转义 HTML 特殊字符
 * @param input 输入字符串
 * @param output 输出缓冲区
 * @param output_size 输出缓冲区大小
 * @return 0 成功，其他失败
 */
int sanitizer_escape_html(const char* input, char* output, size_t output_size);

/**
 * @brief 转义 SQL 特殊字符
 * @param input 输入字符串
 * @param output 输出缓冲区
 * @param output_size 输出缓冲区大小
 * @return 0 成功，其他失败
 */
int sanitizer_escape_sql(const char* input, char* output, size_t output_size);

/**
 * @brief 转义 Shell 特殊字符
 * @param input 输入字符串
 * @param output 输出缓冲区
 * @param output_size 输出缓冲区大小
 * @return 0 成功，其他失败
 */
int sanitizer_escape_shell(const char* input, char* output, size_t output_size);

/**
 * @brief 转义路径特殊字符
 * @param input 输入字符串
 * @param output 输出缓冲区
 * @param output_size 输出缓冲区大小
 * @return 0 成功，其他失败
 */
int sanitizer_escape_path(const char* input, char* output, size_t output_size);

/**
 * @brief 获取默认净化上下文
 * @param ctx 上下文输出
 */
void sanitizer_default_context(sanitize_context_t* ctx);

/**
 * @brief 添加净化规则
 * @param sanitizer 净化器句柄
 * @param pattern 匹配模式（正则表达式）
 * @param replacement 替换字符串（NULL 表示拒绝）
 * @return 0 成功，其他失败
 */
int sanitizer_add_rule(sanitizer_t* sanitizer,
                       const char* pattern,
                       const char* replacement);

/**
 * @brief 清除所有规则
 * @param sanitizer 净化器句柄
 */
void sanitizer_clear_rules(sanitizer_t* sanitizer);

#ifdef __cplusplus
}
#endif

#endif /* CUPOLAS_SANITIZER_H */
