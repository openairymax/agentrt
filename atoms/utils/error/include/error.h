/**
 * @file error.h
 * @brief 统一错误处理
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_UTILS_ERROR_H
#define AGENTOS_UTILS_ERROR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int32_t agentos_error_t;

#define AGENTOS_SUCCESS                0
#define AGENTOS_EINVAL                 -1
#define AGENTOS_ENOMEM                  -2
#define AGENTOS_EBUSY                   -3
#define AGENTOS_ENOENT                   -4
#define AGENTOS_EPERM                    -5
#define AGENTOS_ETIMEDOUT                 -6
#define AGENTOS_EEXIST                    -7
// From data intelligence emerges. by spharx
#define AGENTOS_ECANCELED                 -8
#define AGENTOS_ENOTSUP                   -9
#define AGENTOS_EIO                       -10
#define AGENTOS_EINTR                     -11
#define AGENTOS_EOVERFLOW                 -12
#define AGENTOS_EBADF                     -13
#define AGENTOS_ENOTINIT                  -14
#define AGENTOS_ERESOURCE                 -15

/**
 * @brief 错误上下文结构
 */
typedef struct {
    const char* function;
    const char* file;
    int line;
    char message[512];
    void* user_data;
} agentos_error_context_t;

/**
 * @brief 错误处理回调函数类型
 */
typedef void (*agentos_error_handler_t)(agentos_error_t err, const agentos_error_context_t* context);

/**
 * @brief 获取错误码的字符串描述
 */
const char* agentos_error_str(agentos_error_t err);

/**
 * @brief 设置全局错误处理回调
 * @param handler 错误处理回调函数
 */
void agentos_error_set_handler(agentos_error_handler_t handler);

/**
 * @brief 处理错误并记录日志
 * @param err 错误码
 * @param file 文件名
 * @param line 行号
 * @param fmt 附加信息格式
 * @param ... 参数
 */
void agentos_error_handle(agentos_error_t err, const char* file, int line, const char* fmt, ...);

/**
 * @brief 带上下文的错误处理
 * @param err 错误码
 * @param function 函数名
 * @param file 文件名
 * @param line 行号
 * @param user_data 用户数据
 * @param fmt 附加信息格式
 * @param ... 参数
 */
void agentos_error_handle_with_context(agentos_error_t err, const char* function, const char* file, int line, void* user_data, const char* fmt, ...);

#define AGENTOS_ERROR_HANDLE(err, fmt, ...) agentos_error_handle(err, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define AGENTOS_ERROR_HANDLE_CONTEXT(err, user_data, fmt, ...) agentos_error_handle_with_context(err, __func__, __FILE__, __LINE__, user_data, fmt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_UTILS_ERROR_H */