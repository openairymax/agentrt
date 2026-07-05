/**
 * @file error_compat.h
 * @brief 错误处理模块向后兼容层
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 本文件提供从其他错误处理模块到统一错误处理模块（agentrt/commons/utils/error/）的
 * 向后兼容映射，支持渐进式迁移。
 *
 * 设计原则：
 * 1. 保持现有代码不变，通过映射层实现兼容
 * 2. 提供逐步迁移路径
 * 3. 保持错误处理语义一致性
 */

#ifndef AGENTRT_UTILS_ERROR_COMPAT_H
#define AGENTRT_UTILS_ERROR_COMPAT_H

#include "error.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== agentrt/atoms/utils/error/ 模块兼容层 ==================== */
/**
 * 为 agentrt/atoms/utils/error/include/error.h 提供的兼容层。
 * 该模块使用正数错误码（1000+），需要映射到统一模块的负数错误码。
 */

/* 错误码映射表 */
#define AGENTRT_COMPAT_ERRNO_BASE 1000

/* 成功码映射 */
#define AGENTRT_COMPAT_SUCCESS AGENTRT_OK

/* 通用错误映射 (1001-1010) */
#define AGENTRT_COMPAT_EUNKNOWN AGENTRT_ERR_UNKNOWN
#define AGENTRT_COMPAT_EINVAL AGENTRT_ERR_INVALID_PARAM
#define AGENTRT_COMPAT_ENOMEM AGENTRT_ERR_OUT_OF_MEMORY
#define AGENTRT_COMPAT_EBUSY AGENTRT_ERR_BUSY
#define AGENTRT_COMPAT_ENOENT AGENTRT_ERR_NOT_FOUND
#define AGENTRT_COMPAT_EPERM AGENTRT_ERR_PERMISSION_DENIED
#define AGENTRT_COMPAT_ETIMEDOUT AGENTRT_ERR_TIMEOUT
#define AGENTRT_COMPAT_EEXIST AGENTRT_ERR_ALREADY_EXISTS
#define AGENTRT_COMPAT_ECANCELED AGENTRT_ERR_CANCELED
#define AGENTRT_COMPAT_ENOTSUP AGENTRT_ERR_NOT_SUPPORTED

/* 系统错误映射 (1011-1020) */
#define AGENTRT_COMPAT_EIO AGENTRT_ERR_IO
#define AGENTRT_COMPAT_EINTR AGENTRT_ERR_INTERRUPTED
#define AGENTRT_COMPAT_EOVERFLOW AGENTRT_ERR_OVERFLOW
#define AGENTRT_COMPAT_EBADF AGENTRT_ERR_SYS_FILE
#define AGENTRT_COMPAT_ENOTINIT AGENTRT_ERR_SYS_NOT_INIT
#define AGENTRT_COMPAT_ERESOURCE AGENTRT_ERR_SYS_RESOURCE

/* 内核错误映射 (1021-1030) */
#define AGENTRT_COMPAT_EIPCFAIL AGENTRT_ERR_KERN_IPC
#define AGENTRT_COMPAT_ETASKFAIL AGENTRT_ERR_KERN_TASK
#define AGENTRT_COMPAT_ESYNCFAIL AGENTRT_ERR_KERN_SYNC
#define AGENTRT_COMPAT_ELOCKFAIL AGENTRT_ERR_KERN_LOCK

/* 认知层错误映射 (1031-1040) */
#define AGENTRT_COMPAT_EPLANFAIL AGENTRT_ERR_COORD_PLAN_FAIL
#define AGENTRT_COMPAT_ECOORDFAIL AGENTRT_ERR_COORD_SYNC_FAIL
#define AGENTRT_COMPAT_EDISPFAIL AGENTRT_ERR_COORD_DISPATCH
#define AGENTRT_COMPAT_EINTENTFAIL AGENTRT_ERR_COORD_INTENT

/* 执行层错误映射 (1041-1050) */
#define AGENTRT_COMPAT_EEXECFAIL AGENTRT_ERR_EXEC_FAIL
#define AGENTRT_COMPAT_ECOMPENSATE AGENTRT_ERR_COORD_COMPENSATE
#define AGENTRT_COMPAT_ERETRYEXCEEDED AGENTRT_ERR_COORD_RETRY_EXCEED
#define AGENTRT_COMPAT_EUNITNOTFOUND AGENTRT_ERR_EXEC_NOT_FOUND

/* 记忆层错误映射 (1051-1060) */
#define AGENTRT_COMPAT_EMEMWRITE AGENTRT_ERR_MEM_WRITE
#define AGENTRT_COMPAT_EMEMREAD AGENTRT_ERR_MEM_READ
#define AGENTRT_COMPAT_EMEMQUERY AGENTRT_ERR_MEM_QUERY
#define AGENTRT_COMPAT_EEVOLVE AGENTRT_ERR_MEM_EVOLVE

/* 安全错误映射 (1061-1070) */
#define AGENTRT_COMPAT_ESECURITY AGENTRT_ERR_ESECURITY
#define AGENTRT_COMPAT_ESANITIZE AGENTRT_ERR_ESANITIZE
#define AGENTRT_COMPAT_EAUDIT AGENTRT_ERR_SEC_AUDIT

/* ==================== 类型定义兼容层 ==================== */

/* 错误严重程度映射 */
typedef agentrt_error_severity_t agentrt_compat_error_severity_t;

/* 错误类别映射 */
typedef enum {
    AGENTRT_COMPAT_ERROR_CAT_SYSTEM = 0,
    AGENTRT_COMPAT_ERROR_CAT_KERNEL = 1,
    AGENTRT_COMPAT_ERROR_CAT_COGNITION = 2,
    AGENTRT_COMPAT_ERROR_CAT_EXECUTION = 3,
    AGENTRT_COMPAT_ERROR_CAT_MEMORY = 4,
    AGENTRT_COMPAT_ERROR_CAT_SECURITY = 5
} agentrt_compat_error_category_t;

/* 结构化错误信息兼容结构 */
typedef struct {
    int code;
    agentrt_compat_error_severity_t severity;
    agentrt_compat_error_category_t category;
    const char *module;
    const char *function;
    const char *file;
    int line;
    char message[512];
    uint64_t timestamp_ns;
    void *context;
} agentrt_compat_error_info_t;

/* 错误上下文兼容结构 */
typedef struct {
    const char *function;
    const char *file;
    int line;
    char message[512];
    void *user_data;
} agentrt_compat_error_context_t;

/* 错误处理回调函数类型 */
typedef void (*agentrt_compat_error_handler_t)(agentrt_error_t err,
                                               const agentrt_compat_error_context_t *context);
typedef void (*agentrt_compat_error_info_handler_t)(const agentrt_compat_error_info_t *info);

/* ==================== 函数接口兼容层 ==================== */

/**
 * @brief 获取错误码的字符串描述（兼容接口）
 * @param err 错误码
 * @return 错误描述字符串
 */
static inline const char *agentrt_compat_error_str(agentrt_error_t err)
{
    return agentrt_error_str(err);
}

/**
 * @brief 获取错误码的严重程度（兼容接口）
 * @param err 错误码
 * @return 严重程度
 */
static inline agentrt_compat_error_severity_t agentrt_compat_error_get_severity(agentrt_error_t err)
{
    return (agentrt_compat_error_severity_t)agentrt_error_get_severity(err);
}

/**
 * @brief 获取错误码的类别（兼容接口）
 * @param err 错误码
 * @return 错误类别
 * @note 这是一个近似映射，因为统一模块使用不同的类别系统
 */
static inline agentrt_compat_error_category_t agentrt_compat_error_get_category(agentrt_error_t err)
{
    /* 简化映射：根据错误码范围判断类别 */
    if (err >= -99 && err <= -1)
        return AGENTRT_COMPAT_ERROR_CAT_SYSTEM;
    else if (err >= -199 && err <= -100)
        return AGENTRT_COMPAT_ERROR_CAT_SYSTEM;
    else if (err >= -299 && err <= -200)
        return AGENTRT_COMPAT_ERROR_CAT_KERNEL;
    else if (err >= -399 && err <= -300)
        return AGENTRT_COMPAT_ERROR_CAT_COGNITION;
    else if (err >= -599 && err <= -400)
        return AGENTRT_COMPAT_ERROR_CAT_EXECUTION;
    else if (err >= -699 && err <= -600)
        return AGENTRT_COMPAT_ERROR_CAT_MEMORY;
    else if (err >= -899 && err <= -700)
        return AGENTRT_COMPAT_ERROR_CAT_SECURITY;
    else
        return AGENTRT_COMPAT_ERROR_CAT_SYSTEM;
}

/**
 * @brief 设置全局错误处理回调（兼容接口）
 * @param handler 错误处理回调函数
 * @note 统一模块使用不同的错误处理机制，此函数提供基本兼容
 */
/* G2.5 统一错误码表：原为 static 全局变量（per-TU 独立副本，跨 TU 不可见），
 * 已改为 extern 声明，定义位于 handler.c，确保全局回调机制跨翻译单元生效。 */
extern agentrt_compat_error_handler_t g_compat_error_handler;
extern agentrt_compat_error_info_handler_t g_compat_error_info_handler;

void agentrt_compat_error_set_handler(agentrt_compat_error_handler_t handler);
void agentrt_compat_error_set_info_handler(agentrt_compat_error_info_handler_t handler);

/**
 * @brief 处理错误并记录日志（兼容接口）
 * @param err 错误码
 * @param file 文件名
 * @param line 行号
 * @param fmt 附加信息格式
 * @param ... 参数
 */
static inline void agentrt_compat_error_handle(agentrt_error_t err, const char *file, int line,
                                               const char *fmt, ...)
{
    /* 使用统一模块的错误推送接口 */
    if (err != AGENTRT_OK) {
        char buffer[512];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buffer, sizeof(buffer), fmt,
                  args); /* flawfinder: ignore - variadic error wrapper with bounded buffer */
        va_end(args);

        agentrt_error_push_ex(err, file, line, "unknown", "%s", buffer);
    }
}

/**
 * @brief 带上下文的错误处理（兼容接口）
 * @param err 错误码
 * @param function 函数名
 * @param file 文件名
 * @param line 行号
 * @param user_data 用户数据
 * @param fmt 附加信息格式
 * @param ... 参数
 */
static inline void agentrt_compat_error_handle_with_context(agentrt_error_t err,
                                                            const char *function, const char *file,
                                                            int line, void *user_data,
                                                            const char *fmt, ...)
{
    if (g_compat_error_info_handler && err != AGENTRT_OK) {
        char buffer[512];
        va_list args2;
        va_start(args2, fmt);
        vsnprintf(buffer, sizeof(buffer), fmt, args2);
        va_end(args2);
        agentrt_compat_error_info_t info;
__builtin_memset(&info, 0, sizeof(info));
        info.code = err;
        info.function = function;
        info.file = file;
        info.line = line;
        info.context = user_data;
        snprintf(info.message, sizeof(info.message), "%s", buffer);
        g_compat_error_info_handler(&info);
    }

    if (err != AGENTRT_OK) {
        char buffer[512];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buffer, sizeof(buffer), fmt,
                  args); /* flawfinder: ignore - variadic error wrapper with bounded buffer */
        va_end(args);

        agentrt_error_push_ex(err, file, line, function, "%s", buffer);
    }
}

/* ==================== 便捷宏兼容层 ==================== */

#define AGENTRT_COMPAT_ERROR_HANDLE(err, fmt, ...) \
    agentrt_compat_error_handle(err, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define AGENTRT_COMPAT_ERROR_HANDLE_CONTEXT(err, user_data, fmt, ...)                           \
    agentrt_compat_error_handle_with_context(err, __func__, __FILE__, __LINE__, user_data, fmt, \
                                             ##__VA_ARGS__)

#define AGENTRT_COMPAT_CHECK_NULL(ptr, err)                             \
    do {                                                                \
        if ((ptr) == NULL) {                                            \
            AGENTRT_COMPAT_ERROR_HANDLE(err, "Null pointer: %s", #ptr); \
            return err;                                                 \
        }                                                               \
    } while (0)

#define AGENTRT_COMPAT_CHECK_ERROR(expr)      \
    do {                                      \
        agentrt_error_t _err = (expr);        \
        if (_err != AGENTRT_COMPAT_SUCCESS) { \
            return _err;                      \
        }                                     \
    } while (0)

/* ==================== 迁移辅助宏 ==================== */

/**
 * 使用这些宏可以逐步迁移代码到统一错误处理模块
 */

/* 将 agentrt/atoms/utils/error/ 的错误码宏名映射到兼容层 */
#ifdef AGENTRT_USE_COMPAT_ERRORS
/* 定义原 agentrt/atoms/utils/error/ 模块的宏名，映射到兼容层 */
#define AGENTRT_SUCCESS AGENTRT_COMPAT_SUCCESS
#define AGENTRT_EUNKNOWN AGENTRT_COMPAT_EUNKNOWN
#define AGENTRT_EINVAL AGENTRT_COMPAT_EINVAL
#define AGENTRT_ENOMEM AGENTRT_COMPAT_ENOMEM
#define AGENTRT_EBUSY AGENTRT_COMPAT_EBUSY
#define AGENTRT_ENOENT AGENTRT_COMPAT_ENOENT
#define AGENTRT_EPERM AGENTRT_COMPAT_EPERM
#define AGENTRT_ETIMEDOUT AGENTRT_COMPAT_ETIMEDOUT
#define AGENTRT_EEXIST AGENTRT_COMPAT_EEXIST
#define AGENTRT_ECANCELED AGENTRT_COMPAT_ECANCELED
#define AGENTRT_ENOTSUP AGENTRT_COMPAT_ENOTSUP
#define AGENTRT_EIO AGENTRT_COMPAT_EIO
#define AGENTRT_EINTR AGENTRT_COMPAT_EINTR
#define AGENTRT_EOVERFLOW AGENTRT_COMPAT_EOVERFLOW
#define AGENTRT_EBADF AGENTRT_COMPAT_EBADF
#define AGENTRT_ENOTINIT AGENTRT_COMPAT_ENOTINIT
#define AGENTRT_ERESOURCE AGENTRT_COMPAT_ERESOURCE
#define AGENTRT_EIPCFAIL AGENTRT_COMPAT_EIPCFAIL
#define AGENTRT_ETASKFAIL AGENTRT_COMPAT_ETASKFAIL
#define AGENTRT_ESYNCFAIL AGENTRT_COMPAT_ESYNCFAIL
#define AGENTRT_ELOCKFAIL AGENTRT_COMPAT_ELOCKFAIL
#define AGENTRT_EPLANFAIL AGENTRT_COMPAT_EPLANFAIL
#define AGENTRT_ECOORDFAIL AGENTRT_COMPAT_ECOORDFAIL
#define AGENTRT_EDISPFAIL AGENTRT_COMPAT_EDISPFAIL
#define AGENTRT_EINTENTFAIL AGENTRT_COMPAT_EINTENTFAIL
#define AGENTRT_EEXECFAIL AGENTRT_COMPAT_EEXECFAIL
#define AGENTRT_ECOMPENSATE AGENTRT_COMPAT_ECOMPENSATE
#define AGENTRT_ERETRYEXCEEDED AGENTRT_COMPAT_ERETRYEXCEEDED
#define AGENTRT_EUNITNOTFOUND AGENTRT_COMPAT_EUNITNOTFOUND
#define AGENTRT_EMEMWRITE AGENTRT_COMPAT_EMEMWRITE
#define AGENTRT_EMEMREAD AGENTRT_COMPAT_EMEMREAD
#define AGENTRT_EMEMQUERY AGENTRT_COMPAT_EMEMQUERY
#define AGENTRT_EEVOLVE AGENTRT_COMPAT_EEVOLVE
#define AGENTRT_ESECURITY AGENTRT_COMPAT_ESECURITY
#define AGENTRT_ESANITIZE AGENTRT_COMPAT_ESANITIZE
#define AGENTRT_EAUDIT AGENTRT_COMPAT_EAUDIT

/* 类型定义映射 */
#define agentrt_error_severity_t agentrt_compat_error_severity_t
#define agentrt_error_category_t agentrt_compat_error_category_t
#define agentrt_error_info_t agentrt_compat_error_info_t
#define agentrt_error_context_t agentrt_compat_error_context_t
#define agentrt_error_handler_t agentrt_compat_error_handler_t
#define agentrt_error_info_handler_t agentrt_compat_error_info_handler_t

/* 函数接口映射 */
#define agentrt_error_str agentrt_compat_error_str
#define agentrt_error_get_severity agentrt_compat_error_get_severity
#define agentrt_error_get_category agentrt_compat_error_get_category
#define agentrt_error_set_handler agentrt_compat_error_set_handler
#define agentrt_error_set_info_handler agentrt_compat_error_set_info_handler
#define agentrt_error_handle agentrt_compat_error_handle
#define agentrt_error_handle_with_context agentrt_compat_error_handle_with_context

/* 宏定义映射 */
#define AGENTRT_ERROR_HANDLE AGENTRT_COMPAT_ERROR_HANDLE
#define AGENTRT_ERROR_HANDLE_CONTEXT AGENTRT_COMPAT_ERROR_HANDLE_CONTEXT
#define AGENTRT_CHECK_NULL AGENTRT_COMPAT_CHECK_NULL
#define AGENTRT_CHECK_ERROR AGENTRT_COMPAT_CHECK_ERROR
#endif

#ifdef __cplusplus
}
#endif

#endif /* AGENTRT_UTILS_ERROR_COMPAT_H */