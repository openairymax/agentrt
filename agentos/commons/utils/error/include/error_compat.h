/**
 * @file error_compat.h
 * @brief 错误处理模块向后兼容层
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 本文件提供从其他错误处理模块到统一错误处理模块（agentos/commons/utils/error/）的
 * 向后兼容映射，支持渐进式迁移。
 *
 * 设计原则：
 * 1. 保持现有代码不变，通过映射层实现兼容
 * 2. 提供逐步迁移路径
 * 3. 保持错误处理语义一致性
 */

#ifndef AGENTOS_UTILS_ERROR_COMPAT_H
#define AGENTOS_UTILS_ERROR_COMPAT_H

#include "error.h"  /* 包含统一错误处理模块 */

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== agentos/atoms/utils/error/ 模块兼容层 ==================== */
/**
 * 为 agentos/atoms/utils/error/include/error.h 提供的兼容层。
 * 该模块使用正数错误码（1000+），需要映射到统一模块的负数错误码。
 */

/* 错误码映射表 */
#define AGENTOS_COMPAT_ERRNO_BASE          1000

/* 成功码映射 */
#define AGENTOS_COMPAT_SUCCESS             AGENTOS_OK

/* 通用错误映射 (1001-1010) */
#define AGENTOS_COMPAT_EUNKNOWN            AGENTOS_ERR_UNKNOWN
#define AGENTOS_COMPAT_EINVAL              AGENTOS_ERR_INVALID_PARAM
#define AGENTOS_COMPAT_ENOMEM              AGENTOS_ERR_OUT_OF_MEMORY
#define AGENTOS_COMPAT_EBUSY               AGENTOS_ERR_BUSY
#define AGENTOS_COMPAT_ENOENT              AGENTOS_ERR_NOT_FOUND
#define AGENTOS_COMPAT_EPERM               AGENTOS_ERR_PERMISSION_DENIED
#define AGENTOS_COMPAT_ETIMEDOUT           AGENTOS_ERR_TIMEOUT
#define AGENTOS_COMPAT_EEXIST              AGENTOS_ERR_ALREADY_EXISTS
#define AGENTOS_COMPAT_ECANCELED           AGENTOS_ERR_CANCELED
#define AGENTOS_COMPAT_ENOTSUP             AGENTOS_ERR_NOT_SUPPORTED

/* 系统错误映射 (1011-1020) */
#define AGENTOS_COMPAT_EIO                 AGENTOS_ERR_IO
#define AGENTOS_COMPAT_EINTR               AGENTOS_ERR_INTERRUPTED
#define AGENTOS_COMPAT_EOVERFLOW           AGENTOS_ERR_OVERFLOW
#define AGENTOS_COMPAT_EBADF               AGENTOS_ERR_SYS_FILE
#define AGENTOS_COMPAT_ENOTINIT            AGENTOS_ERR_SYS_NOT_INIT
#define AGENTOS_COMPAT_ERESOURCE           AGENTOS_ERR_SYS_RESOURCE

/* 内核错误映射 (1021-1030) */
#define AGENTOS_COMPAT_EIPCFAIL            AGENTOS_ERR_KERN_IPC
#define AGENTOS_COMPAT_ETASKFAIL           AGENTOS_ERR_KERN_TASK
#define AGENTOS_COMPAT_ESYNCFAIL           AGENTOS_ERR_KERN_SYNC
#define AGENTOS_COMPAT_ELOCKFAIL           AGENTOS_ERR_KERN_LOCK

/* 认知层错误映射 (1031-1040) */
#define AGENTOS_COMPAT_EPLANFAIL           AGENTOS_ERR_COORD_PLAN_FAIL
#define AGENTOS_COMPAT_ECOORDFAIL          AGENTOS_ERR_COORD_SYNC_FAIL
#define AGENTOS_COMPAT_EDISPFAIL           AGENTOS_ERR_COORD_DISPATCH
#define AGENTOS_COMPAT_EINTENTFAIL         AGENTOS_ERR_COORD_INTENT

/* 执行层错误映射 (1041-1050) */
#define AGENTOS_COMPAT_EEXECFAIL           AGENTOS_ERR_EXEC_FAIL
#define AGENTOS_COMPAT_ECOMPENSATE         AGENTOS_ERR_COORD_COMPENSATE
#define AGENTOS_COMPAT_ERETRYEXCEEDED      AGENTOS_ERR_COORD_RETRY_EXCEED
#define AGENTOS_COMPAT_EUNITNOTFOUND       AGENTOS_ERR_EXEC_NOT_FOUND

/* 记忆层错误映射 (1051-1060) */
#define AGENTOS_COMPAT_EMEMWRITE           AGENTOS_ERR_MEM_WRITE
#define AGENTOS_COMPAT_EMEMREAD            AGENTOS_ERR_MEM_READ
#define AGENTOS_COMPAT_EMEMQUERY           AGENTOS_ERR_MEM_QUERY
#define AGENTOS_COMPAT_EEVOLVE             AGENTOS_ERR_MEM_EVOLVE

/* 安全错误映射 (1061-1070) */
#define AGENTOS_COMPAT_ESECURITY           AGENTOS_ERR_ESECURITY
#define AGENTOS_COMPAT_ESANITIZE           AGENTOS_ERR_ESANITIZE
#define AGENTOS_COMPAT_EAUDIT              AGENTOS_ERR_SEC_AUDIT

/* ==================== 类型定义兼容层 ==================== */

/* 错误严重程度映射 */
typedef agentos_error_severity_t agentos_compat_error_severity_t;

/* 错误类别映射（简化版） */
typedef enum {
    AGENTOS_COMPAT_ERROR_CAT_SYSTEM = 0,
    AGENTOS_COMPAT_ERROR_CAT_KERNEL = 1,
    AGENTOS_COMPAT_ERROR_CAT_COGNITION = 2,
    AGENTOS_COMPAT_ERROR_CAT_EXECUTION = 3,
    AGENTOS_COMPAT_ERROR_CAT_MEMORY = 4,
    AGENTOS_COMPAT_ERROR_CAT_SECURITY = 5
} agentos_compat_error_category_t;

/* 结构化错误信息兼容结构 */
typedef struct {
    int code;
    agentos_compat_error_severity_t severity;
    agentos_compat_error_category_t category;
    const char* module;
    const char* function;
    const char* file;
    int line;
    char message[512];
    uint64_t timestamp_ns;
    void* context;
} agentos_compat_error_info_t;

/* 错误上下文兼容结构 */
typedef struct {
    const char* function;
    const char* file;
    int line;
    char message[512];
    void* user_data;
} agentos_compat_error_context_t;

/* 错误处理回调函数类型 */
typedef void (*agentos_compat_error_handler_t)(agentos_error_t err, const agentos_compat_error_context_t* context);
typedef void (*agentos_compat_error_info_handler_t)(const agentos_compat_error_info_t* info);

/* ==================== 函数接口兼容层 ==================== */

/**
 * @brief 获取错误码的字符串描述（兼容接口）
 * @param err 错误码
 * @return 错误描述字符串
 */
static inline const char* agentos_compat_error_str(agentos_error_t err)
{
    return agentos_error_str(err);
}

/**
 * @brief 获取错误码的严重程度（兼容接口）
 * @param err 错误码
 * @return 严重程度
 */
static inline agentos_compat_error_severity_t agentos_compat_error_get_severity(agentos_error_t err)
{
    return (agentos_compat_error_severity_t)agentos_error_get_severity(err);
}

/**
 * @brief 获取错误码的类别（兼容接口）
 * @param err 错误码
 * @return 错误类别
 * @note 这是一个近似映射，因为统一模块使用不同的类别系统
 */
static inline agentos_compat_error_category_t agentos_compat_error_get_category(agentos_error_t err)
{
    /* 简化映射：根据错误码范围判断类别 */
    if (err >= -99 && err <= -1) return AGENTOS_COMPAT_ERROR_CAT_SYSTEM;
    else if (err >= -199 && err <= -100) return AGENTOS_COMPAT_ERROR_CAT_SYSTEM;
    else if (err >= -299 && err <= -200) return AGENTOS_COMPAT_ERROR_CAT_KERNEL;
    else if (err >= -399 && err <= -300) return AGENTOS_COMPAT_ERROR_CAT_COGNITION;
    else if (err >= -599 && err <= -400) return AGENTOS_COMPAT_ERROR_CAT_EXECUTION;
    else if (err >= -699 && err <= -600) return AGENTOS_COMPAT_ERROR_CAT_MEMORY;
    else if (err >= -899 && err <= -700) return AGENTOS_COMPAT_ERROR_CAT_SECURITY;
    else return AGENTOS_COMPAT_ERROR_CAT_SYSTEM;
}

/**
 * @brief 设置全局错误处理回调（兼容接口）
 * @param handler 错误处理回调函数
 * @note 统一模块使用不同的错误处理机制，此函数提供基本兼容
 */
static inline void agentos_compat_error_set_handler(agentos_compat_error_handler_t handler)
{
    /* 统一模块使用错误链机制，此处记录回调但不直接使用 */
    (void)handler; /* 暂时忽略，后续实现完整映射 */
}

/**
 * @brief 设置结构化错误处理回调（兼容接口）
 * @param handler 结构化错误处理回调函数
 */
static inline void agentos_compat_error_set_info_handler(agentos_compat_error_info_handler_t handler)
{
    (void)handler; /* 暂时忽略，后续实现完整映射 */
}

/**
 * @brief 处理错误并记录日志（兼容接口）
 * @param err 错误码
 * @param file 文件名
 * @param line 行号
 * @param fmt 附加信息格式
 * @param ... 参数
 */
static inline void agentos_compat_error_handle(agentos_error_t err, const char* file, int line, const char* fmt, ...)
{
    /* 使用统一模块的错误推送接口 */
    if (err != AGENTOS_OK) {
        char buffer[512];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buffer, sizeof(buffer), fmt, args);
        va_end(args);
        
        agentos_error_push_ex(err, file, line, "unknown", "%s", buffer);
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
static inline void agentos_compat_error_handle_with_context(
    agentos_error_t err,
    const char* function,
    const char* file,
    int line,
    void* user_data,
    const char* fmt, ...)
{
    (void)user_data; /* 暂时忽略用户数据 */
    
    if (err != AGENTOS_OK) {
        char buffer[512];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buffer, sizeof(buffer), fmt, args);
        va_end(args);
        
        agentos_error_push_ex(err, file, line, function, "%s", buffer);
    }
}

/* ==================== 便捷宏兼容层 ==================== */

#define AGENTOS_COMPAT_ERROR_HANDLE(err, fmt, ...) \
    agentos_compat_error_handle(err, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define AGENTOS_COMPAT_ERROR_HANDLE_CONTEXT(err, user_data, fmt, ...) \
    agentos_compat_error_handle_with_context(err, __func__, __FILE__, __LINE__, user_data, fmt, ##__VA_ARGS__)

#define AGENTOS_COMPAT_CHECK_NULL(ptr, err) \
    do { \
        if ((ptr) == NULL) { \
            AGENTOS_COMPAT_ERROR_HANDLE(err, "Null pointer: %s", #ptr); \
            return err; \
        } \
    } while(0)

#define AGENTOS_COMPAT_CHECK_ERROR(expr) \
    do { \
        agentos_error_t _err = (expr); \
        if (_err != AGENTOS_COMPAT_SUCCESS) { \
            return _err; \
        } \
    } while(0)

/* ==================== 迁移辅助宏 ==================== */

/**
 * 使用这些宏可以逐步迁移代码到统一错误处理模块
 */

/* 将 agentos/atoms/utils/error/ 的错误码宏名映射到兼容层 */
#ifdef AGENTOS_USE_COMPAT_ERRORS
    /* 定义原 agentos/atoms/utils/error/ 模块的宏名，映射到兼容层 */
    #define AGENTOS_SUCCESS              AGENTOS_COMPAT_SUCCESS
    #define AGENTOS_EUNKNOWN             AGENTOS_COMPAT_EUNKNOWN
    #define AGENTOS_EINVAL               AGENTOS_COMPAT_EINVAL
    #define AGENTOS_ENOMEM               AGENTOS_COMPAT_ENOMEM
    #define AGENTOS_EBUSY                AGENTOS_COMPAT_EBUSY
    #define AGENTOS_ENOENT               AGENTOS_COMPAT_ENOENT
    #define AGENTOS_EPERM                AGENTOS_COMPAT_EPERM
    #define AGENTOS_ETIMEDOUT            AGENTOS_COMPAT_ETIMEDOUT
    #define AGENTOS_EEXIST               AGENTOS_COMPAT_EEXIST
    #define AGENTOS_ECANCELED            AGENTOS_COMPAT_ECANCELED
    #define AGENTOS_ENOTSUP              AGENTOS_COMPAT_ENOTSUP
    #define AGENTOS_EIO                  AGENTOS_COMPAT_EIO
    #define AGENTOS_EINTR                AGENTOS_COMPAT_EINTR
    #define AGENTOS_EOVERFLOW            AGENTOS_COMPAT_EOVERFLOW
    #define AGENTOS_EBADF                AGENTOS_COMPAT_EBADF
    #define AGENTOS_ENOTINIT             AGENTOS_COMPAT_ENOTINIT
    #define AGENTOS_ERESOURCE            AGENTOS_COMPAT_ERESOURCE
    #define AGENTOS_EIPCFAIL             AGENTOS_COMPAT_EIPCFAIL
    #define AGENTOS_ETASKFAIL            AGENTOS_COMPAT_ETASKFAIL
    #define AGENTOS_ESYNCFAIL            AGENTOS_COMPAT_ESYNCFAIL
    #define AGENTOS_ELOCKFAIL            AGENTOS_COMPAT_ELOCKFAIL
    #define AGENTOS_EPLANFAIL            AGENTOS_COMPAT_EPLANFAIL
    #define AGENTOS_ECOORDFAIL           AGENTOS_COMPAT_ECOORDFAIL
    #define AGENTOS_EDISPFAIL            AGENTOS_COMPAT_EDISPFAIL
    #define AGENTOS_EINTENTFAIL          AGENTOS_COMPAT_EINTENTFAIL
    #define AGENTOS_EEXECFAIL            AGENTOS_COMPAT_EEXECFAIL
    #define AGENTOS_ECOMPENSATE          AGENTOS_COMPAT_ECOMPENSATE
    #define AGENTOS_ERETRYEXCEEDED       AGENTOS_COMPAT_ERETRYEXCEEDED
    #define AGENTOS_EUNITNOTFOUND        AGENTOS_COMPAT_EUNITNOTFOUND
    #define AGENTOS_EMEMWRITE            AGENTOS_COMPAT_EMEMWRITE
    #define AGENTOS_EMEMREAD             AGENTOS_COMPAT_EMEMREAD
    #define AGENTOS_EMEMQUERY            AGENTOS_COMPAT_EMEMQUERY
    #define AGENTOS_EEVOLVE              AGENTOS_COMPAT_EEVOLVE
    #define AGENTOS_ESECURITY            AGENTOS_COMPAT_ESECURITY
    #define AGENTOS_ESANITIZE            AGENTOS_COMPAT_ESANITIZE
    #define AGENTOS_EAUDIT               AGENTOS_COMPAT_EAUDIT
    
    /* 类型定义映射 */
    #define agentos_error_severity_t     agentos_compat_error_severity_t
    #define agentos_error_category_t     agentos_compat_error_category_t
    #define agentos_error_info_t         agentos_compat_error_info_t
    #define agentos_error_context_t      agentos_compat_error_context_t
    #define agentos_error_handler_t      agentos_compat_error_handler_t
    #define agentos_error_info_handler_t agentos_compat_error_info_handler_t
    
    /* 函数接口映射 */
    #define agentos_error_str            agentos_compat_error_str
    #define agentos_error_get_severity   agentos_compat_error_get_severity
    #define agentos_error_get_category   agentos_compat_error_get_category
    #define agentos_error_set_handler    agentos_compat_error_set_handler
    #define agentos_error_set_info_handler agentos_compat_error_set_info_handler
    #define agentos_error_handle         agentos_compat_error_handle
    #define agentos_error_handle_with_context agentos_compat_error_handle_with_context
    
    /* 宏定义映射 */
    #define AGENTOS_ERROR_HANDLE         AGENTOS_COMPAT_ERROR_HANDLE
    #define AGENTOS_ERROR_HANDLE_CONTEXT AGENTOS_COMPAT_ERROR_HANDLE_CONTEXT
    #define AGENTOS_CHECK_NULL           AGENTOS_COMPAT_CHECK_NULL
    #define AGENTOS_CHECK_ERROR          AGENTOS_COMPAT_CHECK_ERROR
#endif

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_UTILS_ERROR_COMPAT_H */