/**
 * @file error_utils.h
 * @brief 错误处理工具函数
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#ifndef AGENTOS_ERROR_UTILS_H
#define AGENTOS_ERROR_UTILS_H

#include "agentos.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 将错误码转换为可读的错误消息
 * @param err 错误码
 * @return 错误消息字符串（静态字符串，无需释放）
 */
const char* agentos_error_string(agentos_error_t err);

/**
 * @brief 将错误码转换为JSON格式的错误响应
 * @param err 错误码
 * @param message 附加的错误消息（可为NULL）
 * @param out_json 输出JSON字符串（需调用者释放）
 * @return agentos_error_t
 */
agentos_error_t agentos_error_to_json(
    agentos_error_t err,
    const char* message,
    char** out_json);

/**
 * @brief 错误上下文结构，用于记录错误的详细信息
 */
typedef struct agentos_error_context {
    agentos_error_t code;           /**< 错误码 */
    char* message;                   /**< 错误消息 */
    char* file;                      /**< 发生错误的文件名 */
    int line;                        /**< 发生错误的行号 */
    char* function;                  /**< 发生错误的函数名 */
    uint64_t timestamp_ns;           /**< 错误发生时间戳 */
} agentos_error_context_t;

/**
 * @brief 创建错误上下文
 * @param code 错误码
 * @param message 错误消息
 * @param file 文件名
 * @param line 行号
 * @param function 函数名
 * @param out_context 输出错误上下文（需调用 agentos_error_context_free 释放）
 * @return agentos_error_t
 */
agentos_error_t agentos_error_context_create(
    agentos_error_t code,
    const char* message,
    const char* file,
    int line,
    const char* function,
    agentos_error_context_t** out_context);

/**
 * @brief 释放错误上下文
 * @param context 错误上下文
 */
void agentos_error_context_free(agentos_error_context_t* context);

/**
 * @brief 将错误上下文转换为JSON格式
 * @param context 错误上下文
 * @param out_json 输出JSON字符串（需调用者释放）
 * @return agentos_error_t
 */
agentos_error_t agentos_error_context_to_json(
    const agentos_error_context_t* context,
    char** out_json);

/**
 * @brief 便捷宏：创建错误上下文，自动填充文件名、行号和函数名
 */
#define AGENTOS_ERROR_CONTEXT_CREATE(code, message, out_context) \
    agentos_error_context_create((code), (message), __FILE__, __LINE__, __func__, (out_context))

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_ERROR_UTILS_H */
