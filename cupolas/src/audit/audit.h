/**
 * @file audit.h
 * @brief 审计日志系统公共接口
 * @author Spharx
 * @date 2024
 * 
 * 设计原则：
 * - 异步写入：后台线程批量写入，不阻塞主线程
 * - 日志轮转：自动轮转和压缩
 * - 结构化：JSON 格式输出
 */

#ifndef CUPOLAS_AUDIT_H
#define CUPOLAS_AUDIT_H

#include "../platform/platform.h"
#include "audit_queue.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 审计日志记录器句柄 */
typedef struct audit_logger audit_logger_t;

/**
 * @brief 创建审计日志记录器
 * @param log_dir 日志目录
 * @param log_prefix 日志文件前缀
 * @param max_file_size 单文件最大大小（字节）
 * @param max_files 最大文件数
 * @return 记录器句柄，失败返回 NULL
 */
audit_logger_t* audit_logger_create(const char* log_dir, const char* log_prefix,
                                     size_t max_file_size, int max_files);

/**
 * @brief 销毁审计日志记录器
 * @param logger 记录器句柄
 */
void audit_logger_destroy(audit_logger_t* logger);

/**
 * @brief 记录审计日志
 * @param logger 记录器句柄
 * @param type 事件类型
 * @param agent_id Agent ID
 * @param action 操作
 * @param resource 资源
 * @param detail 详情
 * @param result 结果
 * @return 0 成功，其他失败
 */
int audit_logger_log(audit_logger_t* logger, audit_event_type_t type,
                      const char* agent_id, const char* action,
                      const char* resource, const char* detail, int result);

/**
 * @brief 记录权限审计日志
 * @param logger 记录器句柄
 * @param agent_id Agent ID
 * @param action 操作
 * @param resource 资源
 * @param allowed 是否允许
 * @return 0 成功，其他失败
 */
int audit_logger_log_permission(audit_logger_t* logger, const char* agent_id,
                                 const char* action, const char* resource, int allowed);

/**
 * @brief 记录净化审计日志
 * @param logger 记录器句柄
 * @param agent_id Agent ID
 * @param input 输入
 * @param output 输出
 * @param passed 是否通过
 * @return 0 成功，其他失败
 */
int audit_logger_log_sanitizer(audit_logger_t* logger, const char* agent_id,
                                const char* input, const char* output, int passed);

/**
 * @brief 记录工位审计日志
 * @param logger 记录器句柄
 * @param agent_id Agent ID
 * @param command 命令
 * @param exit_code 退出码
 * @return 0 成功，其他失败
 */
int audit_logger_log_workbench(audit_logger_t* logger, const char* agent_id,
                                const char* command, int exit_code);

/**
 * @brief 刷新日志缓冲区
 * @param logger 记录器句柄
 */
void audit_logger_flush(audit_logger_t* logger);

/**
 * @brief 获取日志统计信息
 * @param logger 记录器句柄
 * @param total_logged 总记录数
 * @param total_failed 总失败数
 */
void audit_logger_stats(audit_logger_t* logger, uint64_t* total_logged, uint64_t* total_failed);

#ifdef __cplusplus
}
#endif

#endif /* CUPOLAS_AUDIT_H */
