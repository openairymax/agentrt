/**
 * @file logger.h
 * @brief 日志接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#ifndef AGENTOS_UTILS_LOGGER_H
#define AGENTOS_UTILS_LOGGER_H

#ifdef __cplusplus
extern "C" {
#endif

#define AGENTOS_LOG_LEVEL_ERROR 1
#define AGENTOS_LOG_LEVEL_WARN  2
#define AGENTOS_LOG_LEVEL_INFO  3
#define AGENTOS_LOG_LEVEL_DEBUG 4

#ifndef AGENTOS_LOG_LEVEL
#define AGENTOS_LOG_LEVEL AGENTOS_LOG_LEVEL_INFO
#endif

/**
 * @brief 设置当前线程的追踪ID
 * @param trace_id 追踪ID，若为NULL则自动生成
 * @return 实际设置的追踪ID（静态内存，无需释放）
 */
const char* agentos_log_set_trace_id(const char* trace_id);

/**
 * @brief 获取当前线程的追踪ID
 * @return 追踪ID，可能为NULL
 */
const char* agentos_log_get_trace_id(void);

/**
 * @brief 记录日志
 * @param level 日志级别
 * @param file 文件名（通常用 __FILE__）
 * @param line 行号
 * @param fmt 格式字符串
 * @param ... 参数
 */
void agentos_log_write(int level, const char* file, int line, const char* fmt, ...);

#define AGENTOS_LOG_ERROR(fmt, ...) agentos_log_write(AGENTOS_LOG_LEVEL_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define AGENTOS_LOG_WARN(fmt, ...)  agentos_log_write(AGENTOS_LOG_LEVEL_WARN, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define AGENTOS_LOG_INFO(fmt, ...)  agentos_log_write(AGENTOS_LOG_LEVEL_INFO, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define AGENTOS_LOG_DEBUG(fmt, ...) agentos_log_write(AGENTOS_LOG_LEVEL_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_UTILS_LOGGER_H */