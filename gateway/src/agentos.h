/**
 * @file agentos.h
 * @brief AgentOS gateway 模块适配层
 *
 * 本文件为适配层，将 gateway 模块的接口映射到 commons 模块
 * gateway 模块原来直接实现的功能通过 commons 模块统一使用
 *
 * 映射关系：
 * - platform_* 线程/锁 -> agentos_* 线程/锁
 * - AGENTOS_LOG_* 日志 -> 使用 commons/utils/observability/include/logger.h
 * - agentos_error_t -> 使用 commons/utils/error/include/error.h
 * - agentos_time_* -> 使用 commons/platform/platform.h 中的agentos_time_ns/ms
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_GATEWAY_AGENTOS_H
#define AGENTOS_GATEWAY_AGENTOS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 导出宏定义 ==================== */

#ifdef AGENTOS_BUILDING_SHARED
    #if defined(_WIN32) || defined(_WIN64)
        #define AGENTOS_API __declspec(dllexport)
    #elif defined(__GNUC__) || defined(__clang__)
        #define AGENTOS_API __attribute__((visibility("default")))
    #else
        #define AGENTOS_API
    #endif
#else
    #if defined(_WIN32) || defined(_WIN64)
        #define AGENTOS_API __declspec(dllimport)
    #else
        #define AGENTOS_API
    #endif
#endif

#ifndef AGENTOS_API
    #define AGENTOS_API
#endif

/* ==================== 错误码定义（继承 commons/error.h） ==================== */

/*
 * 错误码字段规划：
 *   -1 ~ -99:      通用错误码
 *   -100 ~ -199:   系统平台错误
 *   -200 ~ -299:   内核错误
 *   -300 ~ -399:   网络错误
 *   -400 ~ -499:   LLM/AI相关错误
 *   -500 ~ -599:   执行/调度错误
 *   -600 ~ -699:   记忆/存储错误
 *   -700 ~ -799:   安全/穹顶错误
 *   -800 ~ -899:   协议/网关错误
 */

typedef int32_t agentos_error_t;

#define AGENTOS_OK                      0
#define AGENTOS_SUCCESS                0
#define AGENTOS_ERR_UNKNOWN           (-1)
#define AGENTOS_ERR_INVALID_PARAM      (-2)
#define AGENTOS_ERR_NULL_POINTER       (-3)
#define AGENTOS_ERR_OUT_OF_MEMORY     (-4)
#define AGENTOS_ERR_BUFFER_TOO_SMALL  (-5)
#define AGENTOS_ERR_NOT_FOUND         (-6)
#define AGENTOS_ERR_ALREADY_EXISTS    (-7)
#define AGENTOS_ERR_TIMEOUT           (-8)
#define AGENTOS_ERR_NOT_SUPPORTED     (-9)
#define AGENTOS_ERR_PERMISSION_DENIED (-10)
#define AGENTOS_ERR_IO                (-11)
#define AGENTOS_ERR_BUSY              (-12)
#define AGENTOS_ERR_STATE_ERROR       (-13)
#define AGENTOS_ERR_OVERFLOW          (-14)
#define AGENTOS_ERR_CANCELED          (-15)
#define AGENTOS_ERR_NOT_INITIALIZED   (-16)
#define AGENTOS_ERR_ALREADY_INITIALIZED (-17)
#define AGENTOS_ERR_CONNECTION_FAILED  (-18)
#define AGENTOS_ERR_CONNECTION_CLOSED  (-19)
#define AGENTOS_ERR_PROTOCOL_ERROR     (-20)
#define AGENTOS_ERR_PARSE_ERROR       (-21)

/* 错误码别名（兼容 BSD 风格） */
#define AGENTOS_EINVAL                 AGENTOS_ERR_INVALID_PARAM
#define AGENTOS_ENOMEM                AGENTOS_ERR_OUT_OF_MEMORY
#define AGENTOS_EBUSY                 AGENTOS_ERR_BUSY
#define AGENTOS_ENOENT                AGENTOS_ERR_NOT_FOUND
#define AGENTOS_EPERM                 AGENTOS_ERR_PERMISSION_DENIED
#define AGENTOS_ETIMEDOUT             AGENTOS_ERR_TIMEOUT
#define AGENTOS_EEXIST                AGENTOS_ERR_ALREADY_EXISTS
#define AGENTOS_ECANCELED            AGENTOS_ERR_CANCELED
#define AGENTOS_ENOTSUP               AGENTOS_ERR_NOT_SUPPORTED
#define AGENTOS_EIO                   AGENTOS_ERR_IO
#define AGENTOS_ERROR                 (-100)

/* ==================== 线程类型映射（platform -> agentos） ==================== */

/*
 * gateway 模块使用 platform_* 前缀，commons 模块使用 agentos_* 前缀
 * 通过映射实现兼容
 */

#define platform_thread_t              agentos_thread_t
#define platform_thread_id_t           agentos_thread_id_t
#define platform_mutex_t              agentos_mutex_t
#define platform_cond_t                agentos_cond_t
#define platform_socket_t              agentos_socket_t

#define PLATFORM_THREAD_INVALID        AGENTOS_INVALID_THREAD
#define PLATFORM_MUTEX_INVALID        AGENTOS_INVALID_MUTEX
#define PLATFORM_SOCKET_INVALID       AGENTOS_INVALID_SOCKET

#define platform_mutex_init            agentos_mutex_init
#define platform_mutex_destroy         agentos_mutex_destroy
#define platform_mutex_lock            agentos_mutex_lock
#define platform_mutex_trylock         agentos_mutex_trylock
#define platform_mutex_unlock          agentos_mutex_unlock

#define platform_cond_init             agentos_cond_init
#define platform_cond_destroy          agentos_cond_destroy
#define platform_cond_wait             agentos_cond_wait
#define platform_cond_timedwait        agentos_cond_timedwait
#define platform_cond_signal           agentos_cond_signal
#define platform_cond_broadcast        agentos_cond_broadcast

#define platform_thread_create         agentos_thread_create
#define platform_thread_join           agentos_thread_join
#define platform_thread_self           agentos_thread_id

#define platform_socket_tcp            agentos_socket_tcp
#define platform_socket_unix           agentos_socket_unix
#define platform_socket_close          agentos_socket_close
#define platform_socket_set_nonblock   agentos_socket_set_nonblock
#define platform_socket_set_reuseaddr  agentos_socket_set_reuseaddr

#define platform_network_init          agentos_network_init
#define platform_network_cleanup       agentos_network_cleanup
#define platform_ignore_sigpipe        agentos_ignore_sigpipe

/* ==================== 时间函数映射 ==================== */

/**
 * @brief 获取当前时间（纳秒）- 真实时间
 * @return 当前时间戳（纳秒）
 */
AGENTOS_API uint64_t agentos_time_ns(void);

/**
 * @brief 获取当前时间（毫秒）- 真实时间
 * @return 当前时间戳（毫秒）
 */
AGENTOS_API uint64_t agentos_time_ms(void);

#define agentos_time_monotonic_ns()   agentos_time_ns()
#define agentos_time_monotonic_ms()   agentos_time_ms()

/**
 * @brief 获取当前时间（纳秒）- 墙钟时间
 * @return 当前时间戳（纳秒）
 */
AGENTOS_API uint64_t agentos_time_current_ns(void);

/**
 * @brief 纳秒级睡眠
 * @param ns 睡眠时间（纳秒）
 * @return AGENTOS_SUCCESS 成功
 */
AGENTOS_API agentos_error_t agentos_time_nanosleep(uint64_t ns);

/* ==================== 线程函数类型及映射 ==================== */

/*
 * gateway 模块使用不同线程函数类型定义
 * 通过转换宏兼容
 */

#ifdef _WIN32
    #define platform_thread_func_t     agentos_thread_func_t
#else
    #define platform_thread_func_t      agentos_thread_func_t
#endif

typedef void* (*agentos_thread_func_t)(void* arg);

/* ==================== 日志接口（来自 commons/logger.h） ==================== */

/*
 * 日志级别定义
 */
#define AGENTOS_LOG_LEVEL_ERROR 1
#define AGENTOS_LOG_LEVEL_WARN  2
#define AGENTOS_LOG_LEVEL_INFO  3
#define AGENTOS_LOG_LEVEL_DEBUG 4

#ifndef AGENTOS_LOG_LEVEL
#define AGENTOS_LOG_LEVEL AGENTOS_LOG_LEVEL_INFO
#endif

/*
 * 日志写函数接口（由 commons 模块提供）
 */
AGENTOS_API const char* agentos_log_set_trace_id(const char* trace_id);
AGENTOS_API const char* agentos_log_get_trace_id(void);
AGENTOS_API void agentos_log_write(int level, const char* file, int line, const char* fmt, ...);

#define AGENTOS_LOG_ERROR(fmt, ...) agentos_log_write(AGENTOS_LOG_LEVEL_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define AGENTOS_LOG_WARN(fmt, ...)  agentos_log_write(AGENTOS_LOG_LEVEL_WARN, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define AGENTOS_LOG_INFO(fmt, ...)  agentos_log_write(AGENTOS_LOG_LEVEL_INFO, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define AGENTOS_LOG_DEBUG(fmt, ...) agentos_log_write(AGENTOS_LOG_LEVEL_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

/* ==================== 错误接口（来自 commons/error.h） ==================== */

/*
 * 错误字符串获取接口（由 commons 模块提供）
 */
AGENTOS_API const char* agentos_strerror(agentos_error_t err);

/* ==================== 日志初始化接口（适配层） ==================== */

AGENTOS_API int agentos_logger_init(const char* config_path);
AGENTOS_API void agentos_logger_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_GATEWAY_AGENTOS_H */
