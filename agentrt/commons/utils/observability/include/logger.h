/**
 * @file logger.h
 * @brief AgentRT 统一日志接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 日志级别值定义（与 logging.h 统一）：
 *   DEBUG=0, INFO=1, WARN=2, ERROR=3, FATAL=4
 * 值越大越严重，与 syslog/Linux 内核惯例一致。
 */

#ifndef AGENTRT_UTILS_LOGGER_H
#define AGENTRT_UTILS_LOGGER_H

#ifdef __cplusplus
extern "C" {
#endif

/* 统一日志级别常量 — 值越大越严重 */
#ifndef AGENTRT_LOG_LEVEL_DEBUG_DEFINED
#define AGENTRT_LOG_LEVEL_DEBUG_DEFINED
#define AGENTRT_LOG_LEVEL_DEBUG 0
#define AGENTRT_LOG_LEVEL_INFO 1
#define AGENTRT_LOG_LEVEL_WARN 2
#define AGENTRT_LOG_LEVEL_ERROR 3
#define AGENTRT_LOG_LEVEL_FATAL 4
#endif

#ifndef AGENTRT_LOG_LEVEL
#define AGENTRT_LOG_LEVEL AGENTRT_LOG_LEVEL_INFO
#endif

const char *agentrt_log_set_trace_id(const char *trace_id);
const char *agentrt_log_get_trace_id(void);
void agentrt_log_write(int level, const char *file, int line, const char *fmt, ...);

#ifndef AGENTRT_LOG_ERROR
#define AGENTRT_LOG_ERROR(fmt, ...) \
    agentrt_log_write(AGENTRT_LOG_LEVEL_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#endif
#ifndef AGENTRT_LOG_WARN
#define AGENTRT_LOG_WARN(fmt, ...) \
    agentrt_log_write(AGENTRT_LOG_LEVEL_WARN, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#endif
#ifndef AGENTRT_LOG_INFO
#define AGENTRT_LOG_INFO(fmt, ...) \
    agentrt_log_write(AGENTRT_LOG_LEVEL_INFO, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#endif

#ifndef AGENTRT_LOG_DEBUG
#ifdef AGENTRT_DEBUG
#define AGENTRT_LOG_DEBUG(fmt, ...) \
    agentrt_log_write(AGENTRT_LOG_LEVEL_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#else
#define AGENTRT_LOG_DEBUG(fmt, ...) ((void)0)
#endif
#endif

#ifndef AGENTRT_LOG_FATAL
#define AGENTRT_LOG_FATAL(fmt, ...)                                                         \
    do {                                                                                    \
        agentrt_log_write(AGENTRT_LOG_LEVEL_FATAL, __FILE__, __LINE__, fmt, ##__VA_ARGS__); \
        abort();                                                                            \
    } while (0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* AGENTRT_UTILS_LOGGER_H */
