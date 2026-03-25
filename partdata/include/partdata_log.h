/**
 * @file partdata_log.h
 * @brief AgentOS 数据分区日志管理接口
 *
 * Copyright (c) 2026 SPHARX. All Rights Reserved.
 * "From data intelligence emerges."
 */

#ifndef AGENTOS_PARTDATA_LOG_H
#define AGENTOS_PARTDATA_LOG_H

#include "partdata.h"

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 日志级别
 */
typedef enum {
    PARTDATA_LOG_ERROR = 0,
    PARTDATA_LOG_WARN = 1,
    PARTDATA_LOG_INFO = 2,
    PARTDATA_LOG_DEBUG = 3
} partdata_log_level_t;

/**
 * @brief 日志处理器类型
 */
typedef enum {
    PARTDATA_LOG_HANDLER_FILE = 0,
    PARTDATA_LOG_HANDLER_STDOUT = 1,
    PARTDATA_LOG_HANDLER_STDERR = 2
} partdata_log_handler_type_t;

/**
 * @brief 日志文件信息
 */
typedef struct {
    char path[512];
    uint64_t size_bytes;
    uint32_t line_count;
    time_t created_at;
    time_t modified_at;
} partdata_log_file_info_t;

/**
 * @brief 初始化日志系统
 *
 * @return partdata_error_t 错误码
 */
partdata_error_t partdata_log_init(void);

/**
 * @brief 关闭日志系统
 */
void partdata_log_shutdown(void);

/**
 * @brief 写入日志
 *
 * @param level 日志级别
 * @param service 服务名称
 * @param trace_id 追踪 ID（可为空）
 * @param file 文件名
 * @param line 行号
 * @param format 格式化字符串
 * @param ... 可变参数
 */
void partdata_log_write(partdata_log_level_t level, const char* service, const char* trace_id, const char* file, int line, const char* format, ...);

/**
 * @brief 写入日志（va_list 版本）
 *
 * @param level 日志级别
 * @param service 服务名称
 * @param trace_id 追踪 ID（可为空）
 * @param file 文件名
 * @param line 行号
 * @param format 格式化字符串
 * @param args va_list
 */
void partdata_log_writev(partdata_log_level_t level, const char* service, const char* trace_id, const char* file, int line, const char* format, va_list args);

/**
 * @brief 获取当前日志级别
 *
 * @return partdata_log_level_t 当前日志级别
 */
partdata_log_level_t partdata_log_get_level(void);

/**
 * @brief 设置日志级别
 *
 * @param level 日志级别
 */
void partdata_log_set_level(partdata_log_level_t level);

/**
 * @brief 获取服务日志路径
 *
 * @param service 服务名称
 * @param buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 * @return partdata_error_t 错误码
 */
partdata_error_t partdata_log_get_service_path(const char* service, char* buffer, size_t buffer_size);

/**
 * @brief 执行日志轮转
 *
 * @return partdata_error_t 错误码
 */
partdata_error_t partdata_log_rotate(void);

/**
 * @brief 清理过期日志文件
 *
 * @param days_to_keep 保留天数
 * @param freed_bytes 释放的字节数（输出）
 * @return partdata_error_t 错误码
 */
partdata_error_t partdata_log_cleanup(int days_to_keep, uint64_t* freed_bytes);

/**
 * @brief 获取日志文件信息
 *
 * @param service 服务名称（NULL 表示主日志）
 * @param info 输出文件信息
 * @return partdata_error_t 错误码
 */
partdata_error_t partdata_log_get_file_info(const char* service, partdata_log_file_info_t* info);

/**
 * @brief 获取日志统计信息
 *
 * @param total_files 总文件数
 * @param total_size_bytes 总大小
 * @param oldest_timestamp 最旧日志时间戳
 * @return partdata_error_t 错误码
 */
partdata_error_t partdata_log_get_stats(uint32_t* total_files, uint64_t* total_size_bytes, time_t* oldest_timestamp);

#define PARTDATA_LOG_ERROR(service, trace_id, fmt, ...) \
    partdata_log_write(PARTDATA_LOG_ERROR, service, trace_id, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define PARTDATA_LOG_WARN(service, trace_id, fmt, ...) \
    partdata_log_write(PARTDATA_LOG_WARN, service, trace_id, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define PARTDATA_LOG_INFO(service, trace_id, fmt, ...) \
    partdata_log_write(PARTDATA_LOG_INFO, service, trace_id, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define PARTDATA_LOG_DEBUG(service, trace_id, fmt, ...) \
    partdata_log_write(PARTDATA_LOG_DEBUG, service, trace_id, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_PARTDATA_LOG_H */
