/**
 * @file error_utils.h
 * @brief 错误处理工具函数
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTRT_ERROR_UTILS_H
#define AGENTRT_ERROR_UTILS_H

#include "agentrt.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 将错误码转换为可读的错误消息
 * @param err 错误码
 * @return 错误消息字符串（静态字符串，无需释放）
 */
const char *agentrt_error_string(agentrt_error_t err);

/**
 * @brief 将错误码转换为JSON格式的错误响应
 * @param err 错误码
 * @param message 附加的错误消息（可为NULL）
 * @param out_json 输出JSON字符串（需调用者释放）
 * @return agentrt_error_t
 */
agentrt_error_t agentrt_error_to_json(agentrt_error_t err, const char *message, char **out_json);

/* agentrt_error_context_t 的权威定义位于 commons/utils/error/include/error.h
 * （guard: AGENTRT_ERROR_CONTEXT_T_DEFINED）。本文件通过 #include "agentrt.h"
 * → corekern/error.h → commons/error.h 传递包含该类型，无需重复定义。
 * （G2.3 统一错误码表：消除重复定义） */

/**
 * @brief 创建错误上下文
 * @param code 错误码
 * @param message 错误消息
 * @param file 文件名
 * @param line 行号
 * @param function 函数名
 * @param out_context 输出错误上下文（需调用 agentrt_error_context_free 释放）
 * @return agentrt_error_t
 */
agentrt_error_t agentrt_error_context_create(agentrt_error_t code, const char *message,
                                             const char *file, int line, const char *function,
                                             agentrt_error_context_t **out_context);

/**
 * @brief 释放错误上下文
 * @param context 错误上下文
 */
void agentrt_error_context_free(agentrt_error_context_t *context);

/**
 * @brief 将错误上下文转换为JSON格式
 * @param context 错误上下文
 * @param out_json 输出JSON字符串（需调用者释放）
 * @return agentrt_error_t
 */
agentrt_error_t agentrt_error_context_to_json(const agentrt_error_context_t *context,
                                              char **out_json);

/**
 * @brief 便捷宏：创建错误上下文，自动填充文件名、行号和函数名
 */
#define AGENTRT_ERROR_CONTEXT_CREATE(code, message, out_context) \
    agentrt_error_context_create((code), (message), __FILE__, __LINE__, __func__, (out_context))

#ifdef __cplusplus
}
#endif

#endif /* AGENTRT_ERROR_UTILS_H */
