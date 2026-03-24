/**
 * @file error.h
 * @brief 统一错误处理
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_UTILS_ERROR_H
#define AGENTOS_UTILS_ERROR_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 错误码定义 ==================== */

typedef int32_t agentos_error_t;

/* 错误码基础值 */
#define AGENTOS_ERRNO_BASE          1000

/* 成功 */
#define AGENTOS_SUCCESS             0

/* 通用错误 (1001-1010) */
#define AGENTOS_EUNKNOWN            (AGENTOS_ERRNO_BASE + 1)    /**< 未知错误 */
#define AGENTOS_EINVAL              (AGENTOS_ERRNO_BASE + 2)    /**< 无效参数 */
#define AGENTOS_ENOMEM              (AGENTOS_ERRNO_BASE + 3)    /**< 内存不足 */
#define AGENTOS_EBUSY               (AGENTOS_ERRNO_BASE + 4)    /**< 资源忙碌 */
#define AGENTOS_ENOENT              (AGENTOS_ERRNO_BASE + 5)    /**< 不存在 */
#define AGENTOS_EPERM               (AGENTOS_ERRNO_BASE + 6)    /**< 权限不足 */
#define AGENTOS_ETIMEDOUT           (AGENTOS_ERRNO_BASE + 7)    /**< 超时 */
#define AGENTOS_EEXIST              (AGENTOS_ERRNO_BASE + 8)    /**< 已存在 */
#define AGENTOS_ECANCELED           (AGENTOS_ERRNO_BASE + 9)    /**< 已取消 */
#define AGENTOS_ENOTSUP             (AGENTOS_ERRNO_BASE + 10)   /**< 不支持 */

/* 系统错误 (1011-1020) */
#define AGENTOS_EIO                 (AGENTOS_ERRNO_BASE + 11)   /**< I/O错误 */
#define AGENTOS_EINTR               (AGENTOS_ERRNO_BASE + 12)   /**< 被中断 */
#define AGENTOS_EOVERFLOW           (AGENTOS_ERRNO_BASE + 13)   /**< 溢出 */
#define AGENTOS_EBADF               (AGENTOS_ERRNO_BASE + 14)   /**< 错误的文件描述符 */
#define AGENTOS_ENOTINIT            (AGENTOS_ERRNO_BASE + 15)   /**< 未初始化 */
#define AGENTOS_ERESOURCE           (AGENTOS_ERRNO_BASE + 16)   /**< 资源不足 */

/* 内核错误 (1021-1030) */
#define AGENTOS_EIPCFAIL            (AGENTOS_ERRNO_BASE + 21)   /**< IPC通信失败 */
#define AGENTOS_ETASKFAIL           (AGENTOS_ERRNO_BASE + 22)   /**< 任务失败 */
#define AGENTOS_ESYNCFAIL           (AGENTOS_ERRNO_BASE + 23)   /**< 同步失败 */
#define AGENTOS_ELOCKFAIL           (AGENTOS_ERRNO_BASE + 24)   /**< 锁失败 */

/* 认知层错误 (1031-1040) */
#define AGENTOS_EPLANFAIL           (AGENTOS_ERRNO_BASE + 31)   /**< 规划失败 */
#define AGENTOS_ECOORDFAIL          (AGENTOS_ERRNO_BASE + 32)   /**< 协调失败 */
#define AGENTOS_EDISPFAIL           (AGENTOS_ERRNO_BASE + 33)   /**< 调度失败 */
#define AGENTOS_EINTENTFAIL         (AGENTOS_ERRNO_BASE + 34)   /**< 意图理解失败 */

/* 执行层错误 (1041-1050) */
#define AGENTOS_EEXECFAIL           (AGENTOS_ERRNO_BASE + 41)   /**< 执行失败 */
#define AGENTOS_ECOMPENSATE         (AGENTOS_ERRNO_BASE + 42)   /**< 补偿失败 */
#define AGENTOS_ERETRYEXCEEDED      (AGENTOS_ERRNO_BASE + 43)   /**< 重试次数超限 */
#define AGENTOS_EUNITNOTFOUND       (AGENTOS_ERRNO_BASE + 44)   /**< 执行单元未找到 */

/* 记忆层错误 (1051-1060) */
#define AGENTOS_EMEMWRITE           (AGENTOS_ERRNO_BASE + 51)   /**< 记忆写入失败 */
#define AGENTOS_EMEMREAD            (AGENTOS_ERRNO_BASE + 52)   /**< 记忆读取失败 */
#define AGENTOS_EMEMQUERY           (AGENTOS_ERRNO_BASE + 53)   /**< 记忆查询失败 */
#define AGENTOS_EEVOLVE             (AGENTOS_ERRNO_BASE + 54)   /**< 演化失败 */

/* 安全错误 (1061-1070) */
#define AGENTOS_ESECURITY           (AGENTOS_ERRNO_BASE + 61)   /**< 安全违规 */
#define AGENTOS_ESANITIZE           (AGENTOS_ERRNO_BASE + 62)   /**< 输入净化失败 */
#define AGENTOS_EAUDIT              (AGENTOS_ERRNO_BASE + 63)   /**< 审计失败 */

/* ==================== 结构化错误信息 ==================== */

/**
 * @brief 错误严重程度
 */
typedef enum {
    AGENTOS_ERROR_SEVERITY_INFO = 0,     /**< 信息 */
    AGENTOS_ERROR_SEVERITY_WARNING = 1,  /**< 警告 */
    AGENTOS_ERROR_SEVERITY_ERROR = 2,    /**< 错误 */
    AGENTOS_ERROR_SEVERITY_CRITICAL = 3  /**< 严重 */
} agentos_error_severity_t;

/**
 * @brief 错误类别
 */
typedef enum {
    AGENTOS_ERROR_CAT_SYSTEM = 0,        /**< 系统错误 */
    AGENTOS_ERROR_CAT_KERNEL = 1,        /**< 内核错误 */
    AGENTOS_ERROR_CAT_COGNITION = 2,     /**< 认知层错误 */
    AGENTOS_ERROR_CAT_EXECUTION = 3,     /**< 执行层错误 */
    AGENTOS_ERROR_CAT_MEMORY = 4,        /**< 记忆层错误 */
    AGENTOS_ERROR_CAT_SECURITY = 5       /**< 安全错误 */
} agentos_error_category_t;

/**
 * @brief 结构化错误信息
 */
typedef struct {
    int code;                              /**< 错误码 */
    agentos_error_severity_t severity;     /**< 严重程度 */
    agentos_error_category_t category;     /**< 错误类别 */
    const char* module;                    /**< 模块名 */
    const char* function;                  /**< 函数名 */
    const char* file;                      /**< 文件名 */
    int line;                              /**< 行号 */
    char message[512];                     /**< 可读消息 */
    uint64_t timestamp_ns;                 /**< 时间戳（纳秒） */
    void* context;                         /**< 错误上下文 */
} agentos_error_info_t;

/* ==================== 错误上下文 ==================== */

/**
 * @brief 错误上下文结构（向后兼容）
 */
typedef struct {
    const char* function;
    const char* file;
    int line;
    char message[512];
    void* user_data;
} agentos_error_context_t;

/* ==================== 错误处理回调 ==================== */

/**
 * @brief 错误处理回调函数类型
 */
typedef void (*agentos_error_handler_t)(agentos_error_t err, const agentos_error_context_t* context);

/**
 * @brief 结构化错误处理回调函数类型
 */
typedef void (*agentos_error_info_handler_t)(const agentos_error_info_t* info);

/* ==================== 核心接口 ==================== */

/**
 * @brief 获取错误码的字符串描述
 * @param err 错误码
 * @return 错误描述字符串
 */
const char* agentos_error_str(agentos_error_t err);

/**
 * @brief 获取错误码的严重程度
 * @param err 错误码
 * @return 严重程度
 */
agentos_error_severity_t agentos_error_get_severity(agentos_error_t err);

/**
 * @brief 获取错误码的类别
 * @param err 错误码
 * @return 错误类别
 */
agentos_error_category_t agentos_error_get_category(agentos_error_t err);

/**
 * @brief 设置全局错误处理回调
 * @param handler 错误处理回调函数
 */
void agentos_error_set_handler(agentos_error_handler_t handler);

/**
 * @brief 设置结构化错误处理回调
 * @param handler 结构化错误处理回调函数
 */
void agentos_error_set_info_handler(agentos_error_info_handler_t handler);

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

/**
 * @brief 创建结构化错误信息
 * @param err 错误码
 * @param module 模块名
 * @param function 函数名
 * @param file 文件名
 * @param line 行号
 * @param message 附加消息
 * @param out_info 输出错误信息结构
 */
void agentos_error_create_info(
    agentos_error_t err,
    const char* module,
    const char* function,
    const char* file,
    int line,
    const char* message,
    agentos_error_info_t* out_info);

/* ==================== 便捷宏定义 ==================== */

#define AGENTOS_ERROR_HANDLE(err, fmt, ...) \
    agentos_error_handle(err, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define AGENTOS_ERROR_HANDLE_CONTEXT(err, user_data, fmt, ...) \
    agentos_error_handle_with_context(err, __func__, __FILE__, __LINE__, user_data, fmt, ##__VA_ARGS__)

#define AGENTOS_ERROR_CREATE_INFO(err, module, message, out_info) \
    agentos_error_create_info(err, module, __func__, __FILE__, __LINE__, message, out_info)

/* 错误检查宏 */
#define AGENTOS_CHECK_NULL(ptr, err) \
    do { \
        if ((ptr) == NULL) { \
            AGENTOS_ERROR_HANDLE(err, "Null pointer: %s", #ptr); \
            return err; \
        } \
    } while(0)

#define AGENTOS_CHECK_ERROR(expr) \
    do { \
        agentos_error_t _err = (expr); \
        if (_err != AGENTOS_SUCCESS) { \
            return _err; \
        } \
    } while(0)

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_UTILS_ERROR_H */