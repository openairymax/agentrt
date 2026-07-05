// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
/**
 * @file svc_logger.h
 * @brief 日志服务兼容层
 *
 * 本文件是 agentrt/commons/utils/logging 的兼容层，提供向后兼容的 API。
 * 新代码应直接使用 #include <logging.h>
 *
 * @see agentrt/commons/utils/logging/include/logging.h
 */

#ifndef AGENTRT_DAEMON_COMMON_SVC_LOGGER_H
#define AGENTRT_DAEMON_COMMON_SVC_LOGGER_H

/* 包含 commons 的统一日志系统 */
#include "error.h"
#include "platform.h"

#include <logging.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 日志级别兼容 ==================== */

/**
 * @brief 日志级别枚举（兼容层）
 * @note 映射到 commons 的 log_level_t
 */
#ifndef AGENTRT_LOG_LEVEL_T_DEFINED
#define AGENTRT_LOG_LEVEL_T_DEFINED
typedef log_level_t agentrt_log_level_t;
#endif

/* 兼容旧日志级别名称
 *
 * 注意：AGENTRT_LOG_DEBUG / INFO / WARN / ERROR / FATAL 已移除值宏定义，
 * 因为它们与 logging_compat.h 和 observability/logger.h 的函数式宏同名冲突。
 * 文件中需要日志级别常量时，请直接使用 LOG_LEVEL_* 枚举值。
 * 文件中需要日志打印时，请使用 AGENTRT_LOG_*("fmt", ...) 函数式宏
 * （来自 logging_compat.h 或 observability/logger.h）或 SVC_LOG_* 宏。
 */
#ifndef AGENTRT_LOG_TRACE
#define AGENTRT_LOG_TRACE LOG_LEVEL_DEBUG
#endif
#ifndef AGENTRT_LOG_OFF
#define AGENTRT_LOG_OFF LOG_LEVEL_COUNT
#endif

/* ==================== 追踪上下文 ==================== */

/**
 * @brief 追踪上下文结构
 * @note 用于分布式追踪，支持 TraceID/SpanID/SessionID
 */
typedef struct {
    char trace_id[36];       /**< 追踪ID (UUID格式) */
    char span_id[16];        /**< 跨度ID */
    char session_id[36];     /**< 会话ID */
    char parent_span_id[16]; /**< 父跨度ID */
} agentrt_trace_context_t;

/* ==================== 日志记录器类型 ==================== */

/**
 * @brief 日志记录器句柄类型
 * @note 兼容层使用单例模式
 */
typedef struct agentrt_logger_s *agentrt_logger_t;

/* ==================== 日志输出目标 ==================== */

/**
 * @brief 日志输出目标类型
 */
typedef log_output_t agentrt_log_target_type_t;

#define AGENTRT_LOG_TARGET_FILE LOG_OUTPUT_FILE
#define AGENTRT_LOG_TARGET_STDOUT LOG_OUTPUT_CONSOLE
#define AGENTRT_LOG_TARGET_STDERR LOG_OUTPUT_CONSOLE
#define AGENTRT_LOG_TARGET_SYSLOG LOG_OUTPUT_SYSLOG
#define AGENTRT_LOG_TARGET_CALLBACK LOG_OUTPUT_NETWORK

/**
 * @brief 日志回调函数类型
 */
typedef void (*agentrt_log_callback_t)(agentrt_log_level_t level, const char *timestamp,
                                       const char *logger_name,
                                       const agentrt_trace_context_t *trace_ctx,
                                       const char *message, const char *file, int line,
                                       void *user_data);

/**
 * @brief 日志输出目标配置
 */
typedef struct {
    agentrt_log_target_type_t type;
    union {
        struct {
            char path[260];
            uint64_t max_size;
            int max_files;
        } file;
        struct {
            agentrt_log_callback_t callback;
            void *user_data;
        } callback;
    } config;
} agentrt_log_target_t;

/* ==================== 日志记录器配置 ==================== */

/**
 * @brief 日志记录器配置
 */
typedef struct {
    char name[64];
    agentrt_log_level_t level;
    agentrt_log_target_t *targets;
    int target_count;
    bool include_source;
    bool include_trace;
    bool json_format;
} agentrt_logger_config_t;

/* ==================== 全局日志接口 ==================== */

/**
 * @brief 初始化日志系统
 * @param config [in] 全局配置（可为NULL使用默认值）
 * @return 0成功，非0失败
 */
static inline int agentrt_log_init(const agentrt_logger_config_t *config)
{
    log_config_t log_cfg = {0};

    if (config) {
        log_cfg.level = (log_level_t)config->level;
        log_cfg.format = config->json_format ? LOG_FORMAT_JSON : LOG_FORMAT_TEXT;
        log_cfg.outputs = (1 << LOG_OUTPUT_CONSOLE); /* 默认控制台输出 */

        /* 查找文件输出目标 */
        for (int i = 0; i < config->target_count; i++) {
            if (config->targets[i].type == AGENTRT_LOG_TARGET_FILE) {
                log_cfg.outputs |= (1 << LOG_OUTPUT_FILE);
                log_cfg.file_path = config->targets[i].config.file.path;
                log_cfg.max_file_size = config->targets[i].config.file.max_size;
                log_cfg.max_backup_count = config->targets[i].config.file.max_files;
            }
        }
    }

    return log_init(&log_cfg);
}

/**
 * @brief 关闭日志系统
 */
static inline void agentrt_log_shutdown(void)
{
    log_cleanup();
}

/**
 * @brief 设置全局日志级别
 * @param level [in] 日志级别
 */
static inline void agentrt_log_set_level(agentrt_log_level_t level)
{
    (void)level;
}

/**
 * @brief 获取全局日志级别
 * @return 当前日志级别
 */
static inline agentrt_log_level_t agentrt_log_get_level(void)
{
    return (agentrt_log_level_t)LOG_LEVEL_INFO;
}

/* ==================== 追踪上下文接口 ==================== */

/**
 * @brief 生成新的追踪上下文
 * @param ctx [out] 追踪上下文输出
 */
static inline void agentrt_trace_new(agentrt_trace_context_t *ctx)
{
    if (!ctx)
        return;

    /* 生成 UUID 格式的 trace_id */
    snprintf(ctx->trace_id, sizeof(ctx->trace_id), "%08x-%04x-%04x-%04x-%012llx",
             (uint32_t)agentrt_time_ns(), (uint16_t)(agentrt_time_ns() >> 16),
             (uint16_t)(agentrt_time_ns() >> 32), (uint16_t)(agentrt_time_ns() >> 48),
             (unsigned long long)agentrt_thread_id());

    ctx->span_id[0] = '\0';
    ctx->session_id[0] = '\0';
    ctx->parent_span_id[0] = '\0';
}

/**
 * @brief 生成新的 SpanID
 * @param ctx [in,out] 追踪上下文
 */
static inline void agentrt_trace_new_span(agentrt_trace_context_t *ctx)
{
    if (!ctx)
        return;
    snprintf(ctx->span_id, sizeof(ctx->span_id), "%llx", (unsigned long long)agentrt_time_ns());
}

/**
 * @brief 获取当前线程的追踪上下文
 * @return 追踪上下文指针（线程本地存储）
 */
static inline agentrt_trace_context_t *agentrt_trace_current(void)
{
    static AGENTRT_THREAD_LOCAL agentrt_trace_context_t tls_trace = {0};
    return &tls_trace;
}

/**
 * @brief 设置当前线程的追踪上下文
 * @param ctx [in] 追踪上下文
 */
static inline void agentrt_trace_set_current(const agentrt_trace_context_t *ctx)
{
    if (ctx) {
        agentrt_trace_context_t *current = agentrt_trace_current();
        if (current) {
            *current = *ctx;
        }
    }
}

/**
 * @brief 设置当前会话ID
 * @param session_id [in] 会话ID
 */
static inline void agentrt_trace_set_session_id(const char *session_id)
{
    if (session_id) {
        agentrt_trace_context_t *ctx = agentrt_trace_current();
        if (ctx) {
            AGENTRT_STRNCPY_TERM(ctx->session_id, session_id, sizeof(ctx->session_id));
        }
    }
}

/**
 * @brief 获取当前会话ID
 * @return 会话ID字符串
 */
static inline const char *agentrt_trace_get_session_id(void)
{
    agentrt_trace_context_t *ctx = agentrt_trace_current();
    return ctx ? ctx->session_id : "";
}

/* ==================== 日志记录器接口 ==================== */

/**
 * @brief 获取默认日志记录器
 * @return 默认日志记录器句柄
 */
static inline agentrt_logger_t agentrt_logger_default(void)
{
    return (agentrt_logger_t)1; /* 非NULL表示使用默认日志器 */
}

/**
 * @brief 创建日志记录器
 * @param config [in] 配置
 * @return 日志记录器句柄
 */
static inline agentrt_logger_t agentrt_logger_create(const agentrt_logger_config_t *config)
{
    (void)config;
    return agentrt_logger_default();
}

/**
 * @brief 销毁日志记录器
 * @param logger [in] 日志记录器句柄
 */
static inline void agentrt_logger_destroy(agentrt_logger_t logger)
{
    (void)logger;
}

/**
 * @brief 设置日志记录器级别
 * @param logger [in] 日志记录器句柄
 * @param level [in] 日志级别
 */
static inline void agentrt_logger_set_level(agentrt_logger_t logger, agentrt_log_level_t level)
{
    (void)logger;
    (void)level;
}

/**
 * @brief 写入日志
 * @param logger [in] 日志记录器句柄
 * @param level [in] 日志级别
 * @param file [in] 源文件名
 * @param line [in] 行号
 * @param func [in] 函数名
 * @param fmt [in] 格式化消息
 * @param ... [in] 可变参数
 */
static inline void agentrt_logger_log(agentrt_logger_t logger, agentrt_log_level_t level,
                                      const char *file, int line, const char *func, const char *fmt,
                                      ...)
{
    (void)logger;
    (void)func;
    va_list args;
    va_start(args, fmt);
    log_write_va((log_level_t)level, file, line, fmt, args);
    va_end(args);
}

/**
 * @brief 写入带追踪上下文的日志
 */
static inline void agentrt_logger_log_with_trace(agentrt_logger_t logger, agentrt_log_level_t level,
                                                 const agentrt_trace_context_t *trace_ctx,
                                                 const char *file, int line, const char *func,
                                                 const char *fmt, ...)
{
    (void)logger;
    (void)func;
    /* 设置 trace_id */
    if (trace_ctx && trace_ctx->trace_id[0]) {
        log_set_trace_id(trace_ctx->trace_id);
    }

    va_list args;
    va_start(args, fmt);
    log_write_va((log_level_t)level, file, line, fmt, args);
    va_end(args);
}

/* ==================== 便捷日志宏 ==================== */

/**
 * @brief 日志宏（内部使用）
 */
#define AGENTRT_LOG_IMPL(logger, level, ...)                 \
    do {                                                     \
        (void)(logger);                                      \
        log_write((level), __FILE__, __LINE__, __VA_ARGS__); \
    } while (0)

/**
 * @brief 带追踪上下文的日志宏（内部使用）
 */
#define AGENTRT_LOG_TRACE_IMPL(logger, level, trace_ctx, ...)  \
    do {                                                       \
        (void)(logger);                                        \
        if ((trace_ctx) != NULL && (trace_ctx)->trace_id[0]) { \
            log_set_trace_id((trace_ctx)->trace_id);           \
        }                                                      \
        log_write((level), __FILE__, __LINE__, __VA_ARGS__);   \
    } while (0)

/* ==================== 日志级别宏 ==================== */

/**
 * @brief 跟踪级别日志
 */
#ifndef LOG_TRACE
#define LOG_TRACE(...) AGENTRT_LOG_IMPL(agentrt_logger_default(), LOG_LEVEL_DEBUG, __VA_ARGS__)
#endif

#ifndef LOG_DEBUG
#define LOG_DEBUG(...) AGENTRT_LOG_IMPL(agentrt_logger_default(), LOG_LEVEL_DEBUG, __VA_ARGS__)
#endif

#ifndef LOG_INFO
#define LOG_INFO(...) AGENTRT_LOG_IMPL(agentrt_logger_default(), LOG_LEVEL_INFO, __VA_ARGS__)
#endif

#ifndef LOG_WARN
#define LOG_WARN(...) AGENTRT_LOG_IMPL(agentrt_logger_default(), LOG_LEVEL_WARN, __VA_ARGS__)
#endif

#ifndef LOG_ERROR
#define LOG_ERROR(...) AGENTRT_LOG_IMPL(agentrt_logger_default(), LOG_LEVEL_ERROR, __VA_ARGS__)
#endif

#ifndef LOG_FATAL
#define LOG_FATAL(...) AGENTRT_LOG_IMPL(agentrt_logger_default(), LOG_LEVEL_FATAL, __VA_ARGS__)
#endif

/* ==================== 带追踪上下文的日志宏 ==================== */

/**
 * @brief 带追踪的跟踪级别日志
 */
#define LOG_TRACE_T(ctx, ...) \
    AGENTRT_LOG_TRACE_IMPL(agentrt_logger_default(), LOG_LEVEL_DEBUG, (ctx), __VA_ARGS__)

/**
 * @brief 带追踪的调试级别日志
 */
#define LOG_DEBUG_T(ctx, ...) \
    AGENTRT_LOG_TRACE_IMPL(agentrt_logger_default(), LOG_LEVEL_DEBUG, (ctx), __VA_ARGS__)

/**
 * @brief 带追踪的信息级别日志
 */
#define LOG_INFO_T(ctx, ...) \
    AGENTRT_LOG_TRACE_IMPL(agentrt_logger_default(), LOG_LEVEL_INFO, (ctx), __VA_ARGS__)

/**
 * @brief 带追踪的警告级别日志
 */
#define LOG_WARN_T(ctx, ...) \
    AGENTRT_LOG_TRACE_IMPL(agentrt_logger_default(), LOG_LEVEL_WARN, (ctx), __VA_ARGS__)

/**
 * @brief 带追踪的错误级别日志
 */
#define LOG_ERROR_T(ctx, ...) \
    AGENTRT_LOG_TRACE_IMPL(agentrt_logger_default(), LOG_LEVEL_ERROR, (ctx), __VA_ARGS__)

/**
 * @brief 带追踪的致命级别日志
 */
#define LOG_FATAL_T(ctx, ...) \
    AGENTRT_LOG_TRACE_IMPL(agentrt_logger_default(), LOG_LEVEL_FATAL, (ctx), __VA_ARGS__)

/* ==================== 错误日志辅助宏 ==================== */

/**
 * @brief 记录错误并返回错误码
 */
#define LOG_ERROR_RETURN(code, ...) \
    do {                            \
        LOG_ERROR(__VA_ARGS__);     \
        return (code);              \
    } while (0)

/**
 * @brief 条件检查日志
 */
#define LOG_CHECK(cond, level, ...)                                           \
    do {                                                                      \
        if (!(cond)) {                                                        \
            AGENTRT_LOG_IMPL(agentrt_logger_default(), (level), __VA_ARGS__); \
        }                                                                     \
    } while (0)

/* ==================== SVC_ 前缀日志宏（Daemon 服务层专用） ==================== */

/**
 * @brief SVC_ 前缀日志宏 - 与 LOG_* 完全等价
 * @note Daemon 服务层统一使用 SVC_ 前缀，避免与其他模块命名冲突
 *       这些宏直接映射到 commons 的 log_write() 函数
 */

/** SVC 跟踪级别日志（调试追踪） */
#define SVC_LOG_TRACE(...) LOG_DEBUG(__VA_ARGS__)

/** SVC 调试级别日志 */
#define SVC_LOG_DEBUG(...) LOG_DEBUG(__VA_ARGS__)

/** SVC 信息级别日志 */
#define SVC_LOG_INFO(...) LOG_INFO(__VA_ARGS__)

/** SVC 警告级别日志 */
#define SVC_LOG_WARN(...) LOG_WARN(__VA_ARGS__)

/** SVC 错误级别日志 */
#define SVC_LOG_ERROR(...) LOG_ERROR(__VA_ARGS__)

/** SVC 致命错误级别日志 */
#define SVC_LOG_FATAL(...) LOG_FATAL(__VA_ARGS__)

/* ==================== 日志级别字符串转换 ==================== */

/**
 * @brief 日志级别转字符串
 */
static inline const char *agentrt_log_level_to_string(agentrt_log_level_t level)
{
    return log_level_to_string((log_level_t)level);
}

/**
 * @brief 字符串转日志级别
 */
static inline agentrt_log_level_t agentrt_log_level_from_string(const char *str)
{
    return (agentrt_log_level_t)log_level_from_string(str);
}

#ifdef __cplusplus
}
#endif

#endif /* AGENTRT_DAEMON_COMMON_SVC_LOGGER_H */
