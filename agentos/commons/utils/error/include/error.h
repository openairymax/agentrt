/*
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 * 
 * @file error.h
 * @brief 统一错误处理框架
 * 
 * 设计原则：
 * 1. 所有错误码为负值，成功为0
 * 2. 错误码分段管理，避免冲突
 * 3. 支持错误链追踪
 * 4. 线程安全的错误信息存储
 * 5. 支持结构化错误上下文
 * 
 * @author Spharx AgentOS Team
 * @date 2026-03-30
 * @version 2.0
 * 
 * @note 线程安全：所有公共接口均为线程安全
 * @see ARCHITECTURAL_PRINCIPLES.md E-6 错误可追溯原则
 */

#ifndef AGENTOS_UTILS_ERROR_H
#define AGENTOS_UTILS_ERROR_H

#include <stdint.h>
#include <stddef.h>
#include "../types/include/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 错误码类型 ==================== */

typedef int32_t agentos_error_t;

/* ==================== 成功/失败基础 ==================== */

#define AGENTOS_OK                      0
#define AGENTOS_ERR_UNKNOWN            (-1)

/* ==================== 向后兼容别名 ==================== */
/* 兼容旧的 AGENTOS_E* 命名 */
/* 使用 #ifndef 防止与 types.h 中的定义冲突 */
#ifndef AGENTOS_SUCCESS
#define AGENTOS_SUCCESS                AGENTOS_OK
#endif
#ifndef AGENTOS_EUNKNOWN
#define AGENTOS_EUNKNOWN               AGENTOS_ERR_UNKNOWN
#endif
#ifndef AGENTOS_EINVAL
#define AGENTOS_EINVAL                AGENTOS_ERR_INVALID_PARAM
#endif
#ifndef AGENTOS_ENOMEM
#define AGENTOS_ENOMEM                AGENTOS_ERR_OUT_OF_MEMORY
#endif
#ifndef AGENTOS_EBUSY
#define AGENTOS_EBUSY                 AGENTOS_ERR_BUSY
#endif
#ifndef AGENTOS_ENOENT
#define AGENTOS_ENOENT                AGENTOS_ERR_NOT_FOUND
#endif
#ifndef AGENTOS_EPERM
#define AGENTOS_EPERM                 AGENTOS_ERR_PERMISSION_DENIED
#endif
#ifndef AGENTOS_ETIMEDOUT
#define AGENTOS_ETIMEDOUT             AGENTOS_ERR_TIMEOUT
#endif
#ifndef AGENTOS_EEXIST
#define AGENTOS_EEXIST                AGENTOS_ERR_ALREADY_EXISTS
#endif
#ifndef AGENTOS_ECANCELED
#define AGENTOS_ECANCELED             AGENTOS_ERR_CANCELED
#endif
#ifndef AGENTOS_ENOTSUP
#define AGENTOS_ENOTSUP               AGENTOS_ERR_NOT_SUPPORTED
#endif
#ifndef AGENTOS_EIO
#define AGENTOS_EIO                   AGENTOS_ERR_IO
#endif
#ifndef AGENTOS_EINTR
#define AGENTOS_EINTR                 AGENTOS_ERR_INTERRUPTED
#endif
#ifndef AGENTOS_EOVERFLOW
#define AGENTOS_EOVERFLOW             AGENTOS_ERR_OVERFLOW
#endif
#ifndef AGENTOS_EBADF
#define AGENTOS_EBADF                 AGENTOS_ERR_SYS_FILE
#endif
#ifndef AGENTOS_ENOTINIT
#define AGENTOS_ENOTINIT              AGENTOS_ERR_SYS_NOT_INIT
#endif
#ifndef AGENTOS_ERESOURCE
#define AGENTOS_ERESOURCE             AGENTOS_ERR_SYS_RESOURCE
#endif
#ifndef AGENTOS_ESECURITY
#define AGENTOS_ESECURITY             AGENTOS_ERR_ESECURITY
#endif
#ifndef AGENTOS_ESANITIZE
#define AGENTOS_ESANITIZE             AGENTOS_ERR_ESANITIZE
#endif

#ifndef AGENTOS_EACCES
#define AGENTOS_EACCES                AGENTOS_ERR_PERMISSION_DENIED
#endif
#ifndef AGENTOS_ECONNREFUSED
#define AGENTOS_ECONNREFUSED          (-20)
#endif
#ifndef AGENTOS_ECONNRESET
#define AGENTOS_ECONNRESET            (-21)
#endif
#ifndef AGENTOS_ENOTCONN
#define AGENTOS_ENOTCONN              (-22)
#endif
#ifndef AGENTOS_EPROTO
#define AGENTOS_EPROTO                (-23)
#endif
#ifndef AGENTOS_EMSGSIZE
#define AGENTOS_EMSGSIZE              (-24)
#endif
#ifndef AGENTOS_ENOSPC
#define AGENTOS_ENOSPC                (-25)
#endif
#ifndef AGENTOS_ERANGE
#define AGENTOS_ERANGE                (-26)
#endif
#ifndef AGENTOS_EDEADLK
#define AGENTOS_EDEADLK               AGENTOS_ERR_SYS_DEADLOCK
#endif
#ifndef AGENTOS_EAGAIN
#define AGENTOS_EAGAIN                AGENTOS_ERR_WOULD_BLOCK
#endif
#ifndef AGENTOS_E2BIG
#define AGENTOS_E2BIG                 (-27)
#endif

/* ==================== 错误码分段 ==================== */
/*
 * 错误码分段规划：
 *   -1 到 -99:      通用基础错误
 *   -100 到 -999:   系统与平台错误
 *   -1000 到 -1999: 内核层错误
 *   -2000 到 -2999: 服务层错误
 *   -3000 到 -3999: LLM/AI服务错误
 *   -4000 到 -4999: 执行/工具错误
 *   -5000 到 -5999: 调度错误
 *   -6000 到 -6999: 记忆/存储错误
 *   -7000 到 -7999: 安全/沙箱错误
 */

/* 通用基础错误 (-1 到 -99) */
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
#define AGENTOS_ERR_CANCELED           (-16)
#define AGENTOS_ERR_BUSY               (-17)
#define AGENTOS_ERR_WOULD_BLOCK        (-18)
#define AGENTOS_ERR_INTERRUPTED        (-19)

/* 系统与平台错误 (-100 到 -199) */
#define AGENTOS_ERR_SYS_BASE           (-100)
#define AGENTOS_ERR_SYS_NOT_INIT       (-101)
#define AGENTOS_ERR_SYS_RESOURCE       (-102)
#define AGENTOS_ERR_SYS_DEADLOCK       (-103)
#define AGENTOS_ERR_SYS_THREAD         (-104)
#define AGENTOS_ERR_SYS_MUTEX          (-105)
#define AGENTOS_ERR_SYS_SEMAPHORE      (-106)
#define AGENTOS_ERR_SYS_CONDITION      (-107)
#define AGENTOS_ERR_SYS_ATOMIC         (-108)
#define AGENTOS_ERR_SYS_SOCKET         (-109)
#define AGENTOS_ERR_SYS_PIPE            (-110)
#define AGENTOS_ERR_SYS_PROCESS         (-111)
#define AGENTOS_ERR_SYS_FILE           (-112)
#define AGENTOS_ERR_SYS_TIME           (-113)

/* 内核层错误 (-200 到 -299) */
#define AGENTOS_ERR_KERN_BASE          (-200)
#define AGENTOS_ERR_KERN_IPC           (-201)
#define AGENTOS_ERR_KERN_TASK          (-202)
#define AGENTOS_ERR_KERN_SYNC          (-203)
#define AGENTOS_ERR_KERN_LOCK          (-204)
#define AGENTOS_ERR_KERN_MEM           (-205)
#define AGENTOS_ERR_KERN_SCHED         (-206)
#define AGENTOS_ERR_KERN_TIMER         (-207)
#define AGENTOS_ERR_KERN_INTERRUPT     (-208)

/* 服务层错误 (-300 到 -399) */
#define AGENTOS_ERR_SVC_BASE           (-300)
#define AGENTOS_ERR_SVC_NOT_READY      (-301)
#define AGENTOS_ERR_SVC_BUSY           (-302)
#define AGENTOS_ERR_SVC_STOPPED        (-303)
#define AGENTOS_ERR_SVC_CONFIG         (-304)
#define AGENTOS_ERR_SVC_DEPENDENCY     (-305)
#define AGENTOS_ERR_SVC_HEALTH         (-306)
#define AGENTOS_ERR_SVC_LOADBALANCE    (-307)

/* LLM/AI服务错误 (-400 到 -499) */
#define AGENTOS_ERR_LLM_BASE           (-400)
#define AGENTOS_ERR_LLM_NO_PROVIDER    (-401)
#define AGENTOS_ERR_LLM_PROVIDER_FAIL  (-402)
#define AGENTOS_ERR_LLM_RATE_LIMIT     (-403)
#define AGENTOS_ERR_LLM_CONTEXT_LEN    (-404)
#define AGENTOS_ERR_LLM_INVALID_MODEL  (-405)
#define AGENTOS_ERR_LLM_AUTH_FAIL      (-406)
#define AGENTOS_ERR_LLM_TOKEN_LIMIT    (-407)
#define AGENTOS_ERR_LLM_PARSE_RESP     (-408)
#define AGENTOS_ERR_LLM_EMPTY_RESP     (-409)
#define AGENTOS_ERR_LLM_COST_EXCEED    (-410)

/* 执行/工具错误 (-500 到 -599) */
#define AGENTOS_ERR_EXEC_BASE           (-500)
#define AGENTOS_ERR_EXEC_NOT_FOUND      (-501)
#define AGENTOS_ERR_EXEC_FAIL           (-502)
#define AGENTOS_ERR_EXEC_TIMEOUT        (-503)
#define AGENTOS_ERR_EXEC_VALIDATION     (-504)
#define AGENTOS_ERR_EXEC_SANDBOX       (-505)
#define AGENTOS_ERR_EXEC_PERMISSION     (-506)
#define AGENTOS_ERR_EXEC_ARGS          (-507)
#define AGENTOS_ERR_EXEC_ENV           (-508)

/* 记忆/存储错误 (-600 到 -699) */
#define AGENTOS_ERR_MEM_BASE           (-600)
#define AGENTOS_ERR_MEM_WRITE          (-601)
#define AGENTOS_ERR_MEM_READ           (-602)
#define AGENTOS_ERR_MEM_QUERY          (-603)
#define AGENTOS_ERR_MEM_EVOLVE          (-604)
#define AGENTOS_ERR_MEM_FULL           (-605)
#define AGENTOS_ERR_MEM_CORRUPT        (-606)
#define AGENTOS_ERR_MEM_NOT_INIT       (-607)

/* 安全/沙箱错误 (-700 到 -799) */
#define AGENTOS_ERR_SEC_BASE           (-700)
#define AGENTOS_ERR_SEC_VIOLATION      (-701)
#define AGENTOS_ERR_SEC_SANITIZE       (-702)
#define AGENTOS_ERR_SEC_AUDIT          (-703)
#define AGENTOS_ERR_SEC_PERMISSION      (-704)
#define AGENTOS_ERR_SEC_VALIDATION     (-705)
#define AGENTOS_ERR_SEC_QUOTA          (-706)
#define AGENTOS_ERR_SEC_TEMP_DIR        (-707)
#define AGENTOS_ERR_SEC_SYMLINK        (-708)
#define AGENTOS_ERR_SEC_PATH_TRAV      (-709)
#define AGENTOS_ERR_ESECURITY          (-710)
#define AGENTOS_ERR_ESANITIZE          (-711)

/* 协调/规划错误 (-800 到 -899) */
#define AGENTOS_ERR_COORD_BASE         (-800)
#define AGENTOS_ERR_COORD_PLAN_FAIL    (-801)
#define AGENTOS_ERR_COORD_SYNC_FAIL    (-802)
#define AGENTOS_ERR_COORD_DISPATCH     (-803)
#define AGENTOS_ERR_COORD_INTENT       (-804)
#define AGENTOS_ERR_COORD_COMPENSATE   (-805)
#define AGENTOS_ERR_COORD_RETRY_EXCEED (-806)

/* ==================== 错误上下文 ==================== */

/**
 * @brief 错误上下文最大深度
 */
#define AGENTOS_ERROR_CONTEXT_MAX_DEPTH 16

/**
 * @brief 错误严重程度
 */
typedef enum {
    AGENTOS_ERR_SEVERITY_INFO = 0,
    AGENTOS_ERR_SEVERITY_WARNING = 1,
    AGENTOS_ERR_SEVERITY_ERROR = 2,
    AGENTOS_ERR_SEVERITY_CRITICAL = 3
} agentos_error_severity_t;

/**
 * @brief 错误上下文条目
 */
typedef struct {
    const char* file;
    int line;
    const char* function;
    const char* message;
    agentos_error_t error_code;
    uint64_t timestamp_ns;
} agentos_error_context_entry_t;

/**
 * @brief 错误链结构
 */
typedef struct {
    agentos_error_t code;
    int depth;
    agentos_error_context_entry_t contexts[AGENTOS_ERROR_CONTEXT_MAX_DEPTH];
} agentos_error_chain_t;

/* ==================== 错误处理接口 ==================== */

/**
 * @brief 获取错误码的可读描述
 * @param code 错误码
 * @return 错误描述字符串
 */
const char* agentos_error_str(agentos_error_t code);

/**
 * @brief 获取错误严重程度
 * @param code 错误码
 * @return 严重程度
 */
agentos_error_severity_t agentos_error_get_severity(agentos_error_t code);

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

/* ==================== 便捷宏 ==================== */

/**
 * @brief 设置错误并返回
 */
#define AGENTOS_ERROR(code, msg) \
    do { \
        agentos_error_push_ex((code), __FILE__, __LINE__, __func__, "%s", (msg)); \
        return (code); \
    } while (0)

/**
 * @brief 设置格式化错误并返回
 */
#define AGENTOS_ERROR_FMT(code, fmt, ...) \
    do { \
        agentos_error_push_ex((code), __FILE__, __LINE__, __func__, (fmt), __VA_ARGS__); \
        return (code); \
    } while (0)

/**
 * @brief 条件检查，失败时返回错误
 */
#define AGENTOS_CHECK(cond, code, msg) \
    do { \
        if (!(cond)) { \
            AGENTOS_ERROR((code), (msg)); \
        } \
    } while (0)

/**
 * @brief 空指针检查
 */
#define AGENTOS_CHECK_NULL(ptr, name) \
    AGENTOS_CHECK((ptr) != NULL, AGENTOS_ERR_NULL_POINTER, name " is NULL")

/**
 * @brief 内存分配检查
 */
#define AGENTOS_CHECK_ALLOC(ptr) \
    AGENTOS_CHECK((ptr) != NULL, AGENTOS_ERR_OUT_OF_MEMORY, "Memory allocation failed")

/**
 * @brief 错误传播宏
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
 * @brief 错误检查宏（返回错误码而非直接返回）
 */
#define AGENTOS_TRY(expr) \
    do { \
        agentos_error_t __err = (expr); \
        if (__err != AGENTOS_OK) { \
            return __err; \
        } \
    } while (0)

/* ==================== 向后兼容接口（已废弃） ==================== */

#ifndef AGENTOS_ERROR_CONTEXT_T_DEFINED
#define AGENTOS_ERROR_CONTEXT_T_DEFINED
/**
 * @brief 错误上下文结构（兼容旧代码）
 * @deprecated 请使用 agentos_error_chain_t 或 atoms/coreloopthree/include/error_utils.h 中的完整版定义
 */
typedef struct {
    const char* file;
    int line;
    const char* function;
    const char* message;
    agentos_error_t error_code;
} agentos_error_context_t;
#endif /* AGENTOS_ERROR_CONTEXT_T_DEFINED */

/**
 * @brief 错误处理回调函数类型
 * @deprecated 请使用新的错误链接口
 */
typedef void (*agentos_error_handler_t)(agentos_error_t err, const agentos_error_context_t* context);

/**
 * @brief 设置错误处理回调（兼容旧代码）
 * @deprecated
 */
void agentos_error_set_handler(agentos_error_handler_t handler);

/**
 * @brief 兼容旧代码的错误处理宏
 * @deprecated 请使用 AGENTOS_ERROR
 */
#define AGENTOS_ERROR_HANDLE(code, msg) \
    do { \
        agentos_error_push_ex((code), __FILE__, __LINE__, __func__, "%s", (msg)); \
    } while (0)

/**
 * @brief 兼容旧代码的错误处理宏（带上下文）
 * @deprecated
 */
#define AGENTOS_ERROR_HANDLE_CONTEXT(code, user_data, msg) \
    do { \
        agentos_error_push_ex((code), __FILE__, __LINE__, __func__, "%s", (msg)); \
        (void)(user_data); \
    } while (0)

/* ==================== 错误统计 ==================== */

/**
 * @brief 错误统计信息
 */
typedef struct {
    uint64_t total_errors;
    uint64_t errors_by_code[32];
    uint64_t last_error_time;
    agentos_error_t last_error;
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

/* ==================== 多语言支持 ==================== */

/**
 * @brief 支持的语言
 */
typedef enum {
    AGENTOS_LANG_EN_US = 0,      /**< 英语（美国） */
    AGENTOS_LANG_ZH_CN = 1,      /**< 简体中文 */
    AGENTOS_LANG_ZH_TW = 2,      /**< 繁体中文 */
    AGENTOS_LANG_JA_JP = 3,      /**< 日语 */
    AGENTOS_LANG_KO_KR = 4,      /**< 韩语 */
    AGENTOS_LANG_DE_DE = 5,      /**< 德语 */
    AGENTOS_LANG_FR_FR = 6,      /**< 法语 */
    AGENTOS_LANG_ES_ES = 7       /**< 西班牙语 */
} agentos_language_t;

/**
 * @brief 多语言错误描述结构
 */
typedef struct {
    agentos_error_t error_code;  /**< 错误码 */
    const char* descriptions[8]; /**< 各语言描述（按agentos_language_t顺序） */
} agentos_error_i18n_entry_t;

/**
 * @brief 设置当前语言环境
 * 
 * @param[in] lang 语言
 * @return 成功返回AGENTOS_OK，失败返回错误码
 */
agentos_error_t agentos_error_set_language(agentos_language_t lang);

/**
 * @brief 获取当前语言环境
 * 
 * @return 当前语言
 */
agentos_language_t agentos_error_get_language(void);

/**
 * @brief 获取错误码的本地化描述
 * 
 * @param[in] code 错误码
 * @param[in] lang 语言（如果为-1，使用当前语言环境）
 * @return 本地化错误描述字符串
 */
const char* agentos_error_str_i18n(agentos_error_t code, agentos_language_t lang);

/**
 * @brief 注册自定义错误码的本地化描述
 * 
 * @param[in] entries 错误描述条目数组
 * @param[in] count 条目数量
 * @return 成功返回AGENTOS_OK，失败返回错误码
 */
agentos_error_t agentos_error_register_i18n(
    const agentos_error_i18n_entry_t* entries,
    size_t count);

/**
 * @brief 获取错误链的本地化JSON表示
 * 
 * @param[in] chain 错误链
 * @param[in] lang 语言（如果为-1，使用当前语言环境）
 * @return JSON字符串（需调用者释放）
 */
char* agentos_error_chain_to_json_i18n(
    const agentos_error_chain_t* chain,
    agentos_language_t lang);

/* ==================== 错误链增强功能 ==================== */

/**
 * @brief 错误链迭代器
 */
typedef struct {
    const agentos_error_chain_t* chain; /**< 错误链 */
    size_t current_index;               /**< 当前索引 */
} agentos_error_chain_iterator_t;

/**
 * @brief 初始化错误链迭代器
 * 
 * @param[in] chain 错误链
 * @param[out] iter 迭代器
 */
void agentos_error_chain_iter_init(
    const agentos_error_chain_t* chain,
    agentos_error_chain_iterator_t* iter);

/**
 * @brief 获取下一个错误上下文条目
 * 
 * @param[inout] iter 迭代器
 * @return 下一个条目指针，如果没有更多条目返回NULL
 */
const agentos_error_context_entry_t* agentos_error_chain_iter_next(
    agentos_error_chain_iterator_t* iter);

/**
 * @brief 重置错误链迭代器
 * 
 * @param[inout] iter 迭代器
 */
void agentos_error_chain_iter_reset(agentos_error_chain_iterator_t* iter);

/**
 * @brief 获取错误链深度
 * 
 * @param[in] chain 错误链
 * @return 链深度
 */
int agentos_error_chain_get_depth(const agentos_error_chain_t* chain);

/**
 * @brief 获取错误链中最早的错误码
 * 
 * @param[in] chain 错误链
 * @return 最早的错误码
 */
agentos_error_t agentos_error_chain_get_root_error(const agentos_error_chain_t* chain);

/**
 * @brief 获取错误链中最新的错误码
 * 
 * @param[in] chain 错误链
 * @return 最新的错误码
 */
agentos_error_t agentos_error_chain_get_latest_error(const agentos_error_chain_t* chain);

/**
 * @brief 将错误链格式化为可读字符串
 * 
 * @param[in] chain 错误链
 * @param[in] lang 语言（如果为-1，使用当前语言环境）
 * @return 格式化字符串（需调用者释放）
 */
char* agentos_error_chain_format(
    const agentos_error_chain_t* chain,
    agentos_language_t lang);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_UTILS_ERROR_H */
