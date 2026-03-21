/**
 * @file audit.h
 * @brief 审计模块公共接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#ifndef DOMAIN_AUDIT_H
#define DOMAIN_AUDIT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 前向声明 */
typedef struct audit_logger audit_logger_t;

/**
 * @brief 审计事件结构
 */
typedef struct audit_event {
    uint64_t    timestamp;          /**< 时间戳（秒） */
    char*       agent_id;            /**< Agent ID */
    char*       tool_name;           /**< 工具名称 */
    char*       input;               /**< 输入（JSON） */
    char*       output;              /**< 输出（JSON） */
    uint32_t    duration_ms;         /**< 耗时（毫秒） */
    int         success;             /**< 是否成功（1/0） */
    char*       error_msg;           /**< 错误信息 */
} audit_event_t;

/**
 * @brief 审计日志器配置
 */
typedef struct audit_config {
    const char* log_path;           /**< 日志文件路径（如 "/var/log/agentos/audit.log"） */
    uint64_t    max_size_bytes;      /**< 单个文件最大字节数，0表示不轮转 */
    uint32_t    max_files;           /**< 最大保留文件数（仅当 max_size>0 时有效） */
    const char* format;              /**< 输出格式："json" 或 "csv" */
    uint32_t    queue_size;          /**< 异步队列大小 */
} audit_config_t;

/**
 * @brief 创建审计日志器
 * @param config 配置（若为 NULL 使用默认值）
 * @return 日志器句柄，失败返回 NULL
 */
audit_logger_t* audit_logger_create(const audit_config_t* config);

/**
 * @brief 销毁审计日志器
 * @param logger 日志器句柄
 */
void audit_logger_destroy(audit_logger_t* logger);

/**
 * @brief 记录一条审计事件（异步）
 * @param logger 日志器
 * @param event 审计事件（内部会复制数据）
 * @return 0 成功，-1 失败（队列满等）
 */
int audit_logger_record(audit_logger_t* logger, const audit_event_t* event);

/**
 * @brief 查询审计日志（同步）
 * @param logger 日志器
 * @param agent_id Agent ID（可为 NULL 表示所有）
 * @param start_time 起始时间（0表示不限）
 * @param end_time 结束时间（0表示不限）
 * @param limit 最大条数
 * @param out_events 输出事件数组（JSON 字符串数组，需调用者 free 每个元素及数组）
 * @param out_count 输出数量
 * @return 0 成功，-1 失败
 */
int audit_logger_query(audit_logger_t* logger,
                       const char* agent_id,
                       uint64_t start_time,
                       uint64_t end_time,
                       uint32_t limit,
                       char*** out_events,
                       size_t* out_count);

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_AUDIT_H */