/**
 * @file error.h
 * @brief 统一错误处理框架
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * 
 * 设计原则：
 * 1. 所有错误码为负值，成功为0
 * 2. 支持错误链追踪
 * 3. 线程安全的错误信息存储
 * 4. 支持错误上下文信息
 */

#ifndef AGENTOS_ERROR_H
#define AGENTOS_ERROR_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 错误码定义 ==================== */

/**
 * @brief 错误码类型
 */
typedef int agentos_error_t;

/* 成功 */
#define AGENTOS_OK                     0

/* 通用错误 (1-999) */
#define AGENTOS_ERR_UNKNOWN            (-1)
#define AGENTOS_ERR_INVALID_PARAM      (-2)
#define AGENTOS_ERR_NULL_POINTER       (-3)
#define AGENTOS_ERR_OUT_OF_MEMORY      (-4)
#define AGENTOS_ERR_BUFFER_TOO_SMALL   (-5)
#define AGENTOS_ERR_NOT_FOUND          (-6)
#define AGENTOS_ERR_ALREADY_EXISTS     (-7)
#define AGENTOS_ERR_TIMEOUT            (-8)
#define AGENTOS_ERR_NOT_SUPPORTED      (-9)
#define AGENTOS_ERR_PERMISSION_DENIED  (-10)
#define AGENTOS_ERR_IO                 (-11)
#define AGENTOS_ERR_PARSE_ERROR        (-12)
#define AGENTOS_ERR_STATE_ERROR        (-13)
#define AGENTOS_ERR_OVERFLOW           (-14)
#define AGENTOS_ERR_UNDERFLOW          (-15)

/* 服务层错误 (1000-1999) */
#define AGENTOS_ERR_SERVICE_BASE       (-1000)
#define AGENTOS_ERR_SERVICE_NOT_READY  (-1001)
#define AGENTOS_ERR_SERVICE_BUSY       (-1002)
#define AGENTOS_ERR_SERVICE_STOPPED    (-1003)
#define AGENTOS_ERR_SERVICE_CONFIG     (-1004)

/* IPC 错误 (2000-2999) */
#define AGENTOS_ERR_IPC_BASE           (-2000)
#define AGENTOS_ERR_IPC_CONNECT        (-2001)
#define AGENTOS_ERR_IPC_DISCONNECT     (-2002)
#define AGENTOS_ERR_IPC_TIMEOUT        (-2003)
#define AGENTOS_ERR_IPC_INVALID_MSG    (-2004)
#define AGENTOS_ERR_IPC_BUFFER_FULL    (-2005)

/* LLM 服务错误 (3000-3999) */
#define AGENTOS_ERR_LLM_BASE           (-3000)
#define AGENTOS_ERR_LLM_NO_PROVIDER    (-3001)
#define AGENTOS_ERR_LLM_PROVIDER_FAIL  (-3002)
#define AGENTOS_ERR_LLM_RATE_LIMIT     (-3003)
#define AGENTOS_ERR_LLM_CONTEXT_LEN    (-3004)
#define AGENTOS_ERR_LLM_INVALID_MODEL  (-3005)
#define AGENTOS_ERR_LLM_AUTH_FAIL      (-3006)

/* 工具服务错误 (4000-4999) */
#define AGENTOS_ERR_TOOL_BASE          (-4000)
#define AGENTOS_ERR_TOOL_NOT_FOUND     (-4001)
#define AGENTOS_ERR_TOOL_EXEC_FAIL     (-4002)
#define AGENTOS_ERR_TOOL_TIMEOUT       (-4003)
#define AGENTOS_ERR_TOOL_VALIDATION    (-4004)
#define AGENTOS_ERR_TOOL_SANDBOX       (-4005)

/* 调度服务错误 (5000-5999) */
#define AGENTOS_ERR_SCHED_BASE         (-5000)
#define AGENTOS_ERR_SCHED_NO_AGENT     (-5001)
#define AGENTOS_ERR_SCHED_AGENT_BUSY   (-5002)
#define AGENTOS_ERR_SCHED_STRATEGY     (-5003)

/* 市场服务错误 (6000-6999) */
#define AGENTOS_ERR_MARKET_BASE        (-6000)
#define AGENTOS_ERR_MARKET_NOT_FOUND   (-6001)
#define AGENTOS_ERR_MARKET_INSTALL     (-6002)
#define AGENTOS_ERR_MARKET_DEPENDENCY  (-6003)

/* 监控服务错误 (7000-7999) */
#define AGENTOS_ERR_MONITOR_BASE       (-7000)
#define AGENTOS_ERR_MONITOR_METRIC     (-7001)
#define AGENTOS_ERR_MONITOR_TRACE      (-7002)
#define AGENTOS_ERR_MONITOR_ALERT      (-7003)

/* ==================== 错误上下文 ==================== */

/**
 * @brief 错误上下文最大深度
 */
#define AGENTOS_ERROR_CONTEXT_MAX_DEPTH 8

/**
 * @brief 错误上下文条目
 */
typedef struct {
    const char* file;           /**< 源文件名 */
    int line;                   /**< 行号 */
    const char* function;       /**< 函数名 */
    const char* message;        /**< 错误消息 */
    agentos_error_t error_code; /**< 错误码 */
} agentos_error_context_t;

/**
 * @brief 错误链结构
 */
typedef struct {
    agentos_error_t code;                               /**< 主错误码 */
    int depth;                                          /**< 上下文深度 */
    agentos_error_context_t contexts[AGENTOS_ERROR_CONTEXT_MAX_DEPTH];
} agentos_error_chain_t;

/* ==================== 错误处理接口 ==================== */

/**
 * @brief 获取错误码的可读描述
 * @param code 错误码
 * @return 错误描述字符串
 */
const char* agentos_strerror(agentos_error_t code);

/**
 * @brief 获取当前线程的错误链
 * @return 错误链指针
 */
agentos_error_chain_t* agentos_error_get_chain(void);

/**
 * @brief 清除当前线程的错误链
 */
void agentos_error_clear(void);

/**
 * @brief 添加错误上下文
 * @param code 错误码
 * @param file 源文件名
 * @param line 行号
 * @param func 函数名
 * @param fmt 格式化消息
 * @param ... 可变参数
 */
void agentos_error_push_ex(agentos_error_t code, 
                           const char* file, 
                           int line, 
                           const char* func,
                           const char* fmt, ...);

/**
 * @brief 设置错误并返回
 * @param code 错误码
 * @param msg 错误消息
 * @return 错误码
 */
#define AGENTOS_ERROR(code, msg) \
    do { \
        agentos_error_push_ex((code), __FILE__, __LINE__, __func__, "%s", (msg)); \
        return (code); \
    } while (0)

/**
 * @brief 设置格式化错误并返回
 * @param code 错误码
 * @param fmt 格式化消息
 * @param ... 可变参数
 */
#define AGENTOS_ERROR_FMT(code, fmt, ...) \
    do { \
        agentos_error_push_ex((code), __FILE__, __LINE__, __func__, (fmt), __VA_ARGS__); \
        return (code); \
    } while (0)

/**
 * @brief 条件检查，失败时返回错误
 * @param cond 条件
 * @param code 错误码
 * @param msg 错误消息
 */
#define AGENTOS_CHECK(cond, code, msg) \
    do { \
        if (!(cond)) { \
            AGENTOS_ERROR((code), (msg)); \
        } \
    } while (0)

/**
 * @brief 空指针检查
 * @param ptr 指针
 * @param name 参数名
 */
#define AGENTOS_CHECK_NULL(ptr, name) \
    AGENTOS_CHECK((ptr) != NULL, AGENTOS_ERR_NULL_POINTER, name " is NULL")

/**
 * @brief 内存分配检查
 * @param ptr 指针
 */
#define AGENTOS_CHECK_ALLOC(ptr) \
    AGENTOS_CHECK((ptr) != NULL, AGENTOS_ERR_OUT_OF_MEMORY, "Memory allocation failed")

/**
 * @brief 错误传播宏
 * @param expr 表达式
 */
#define AGENTOS_PROPAGATE(expr) \
    do { \
        agentos_error_t __err = (expr); \
        if (__err != AGENTOS_OK) { \
            agentos_error_push_ex(__err, __FILE__, __LINE__, __func__, "Propagated from %s", #expr); \
            return __err; \
        } \
    } while (0)

/**
 * @brief 带清理的错误传播
 * @param expr 表达式
 * @param cleanup 清理代码块
 */
#define AGENTOS_PROPAGATE_CLEANUP(expr, cleanup) \
    do { \
        agentos_error_t __err = (expr); \
        if (__err != AGENTOS_OK) { \
            agentos_error_push_ex(__err, __FILE__, __LINE__, __func__, "Propagated from %s", #expr); \
            { cleanup; } \
            return __err; \
        } \
    } while (0)

/**
 * @brief 打印错误链（用于调试）
 * @param chain 错误链
 */
void agentos_error_print_chain(const agentos_error_chain_t* chain);

/**
 * @brief 将错误链转换为 JSON 字符串
 * @param chain 错误链
 * @return JSON 字符串（需调用者释放）
 */
char* agentos_error_chain_to_json(const agentos_error_chain_t* chain);

/* ==================== 结果包装器 ==================== */

/**
 * @brief 结果包装器（用于返回值或错误）
 * @tparam T 值类型
 */
#define AGENTOS_RESULT_DECL(name, type) \
    typedef struct { \
        agentos_error_t error; \
        type value; \
    } name

/* ==================== 错误统计 ==================== */

/**
 * @brief 错误统计信息
 */
typedef struct {
    uint64_t total_errors;          /**< 总错误数 */
    uint64_t errors_by_code[100];   /**< 按错误码统计 */
    uint64_t last_error_time;       /**< 最后错误时间 */
    agentos_error_t last_error;     /**< 最后错误码 */
} agentos_error_stats_t;

/**
 * @brief 获取错误统计
 * @param stats 统计信息输出
 */
void agentos_error_get_stats(agentos_error_stats_t* stats);

/**
 * @brief 重置错误统计
 */
void agentos_error_reset_stats(void);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_ERROR_H */
