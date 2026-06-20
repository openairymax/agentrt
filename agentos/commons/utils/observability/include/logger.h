/**
 * @file logger.h
 * @brief AgentRT 统一日志接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 日志级别值定义（与 logging.h 统一）：
 *   DEBUG=0, INFO=1, WARN=2, ERROR=3, FATAL=4
 * 值越大越严重，与 syslog/Linux 内核惯例一致。
 */

#ifndef AGENTOS_UTILS_LOGGER_H
#define AGENTOS_UTILS_LOGGER_H

#ifdef __cplusplus
extern "C" {
#endif

/* 统一日志级别常量 — 值越大越严重 */
#ifndef AGENTOS_LOG_LEVEL_DEBUG_DEFINED
#define AGENTOS_LOG_LEVEL_DEBUG_DEFINED
#define AGENTOS_LOG_LEVEL_DEBUG 0
#define AGENTOS_LOG_LEVEL_INFO 1
#define AGENTOS_LOG_LEVEL_WARN 2
#define AGENTOS_LOG_LEVEL_ERROR 3
#define AGENTOS_LOG_LEVEL_FATAL 4
#endif

#ifndef AGENTOS_LOG_LEVEL
#define AGENTOS_LOG_LEVEL AGENTOS_LOG_LEVEL_INFO
#endif

const char *agentos_log_set_trace_id(const char *trace_id);
const char *agentos_log_get_trace_id(void);
void agentos_log_write(int level, const char *file, int line, const char *fmt, ...);

#ifndef AGENTOS_LOG_ERROR
#define AGENTOS_LOG_ERROR(fmt, ...) \
    agentos_log_write(AGENTOS_LOG_LEVEL_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#endif
#ifndef AGENTOS_LOG_WARN
#define AGENTOS_LOG_WARN(fmt, ...) \
    agentos_log_write(AGENTOS_LOG_LEVEL_WARN, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#endif
#ifndef AGENTOS_LOG_INFO
#define AGENTOS_LOG_INFO(fmt, ...) \
    agentos_log_write(AGENTOS_LOG_LEVEL_INFO, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#endif

#ifndef AGENTOS_LOG_DEBUG
#ifdef AGENTOS_DEBUG
#define AGENTOS_LOG_DEBUG(fmt, ...) \
    agentos_log_write(AGENTOS_LOG_LEVEL_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#else
#define AGENTOS_LOG_DEBUG(fmt, ...) ((void)0)
#endif
#endif

#ifndef AGENTOS_LOG_FATAL
#define AGENTOS_LOG_FATAL(fmt, ...)                                                         \
    do {                                                                                    \
        agentos_log_write(AGENTOS_LOG_LEVEL_FATAL, __FILE__, __LINE__, fmt, ##__VA_ARGS__); \
        abort();                                                                            \
    } while (0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_UTILS_LOGGER_H */
