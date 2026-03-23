/**
 * @file audit_rotator.h
 * @brief 审计日志轮转器内部接口
 * @author Spharx
 * @date 2024
 */

#ifndef DOMAIN_AUDIT_ROTATOR_H
#define DOMAIN_AUDIT_ROTATOR_H

#include "../platform/platform.h"
#include "audit_queue.h"
#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 审计日志轮转器句柄 */
typedef struct audit_rotator audit_rotator_t;

/**
 * @brief 创建日志轮转器
 * @param log_dir 日志目录
 * @param log_prefix 日志文件前缀
 * @param max_file_size 单文件最大大小
 * @param max_files 最大文件数
 * @return 轮转器句柄，失败返回 NULL
 */
audit_rotator_t* audit_rotator_create(const char* log_dir, const char* log_prefix,
                                       size_t max_file_size, int max_files);

/**
 * @brief 销毁日志轮转器
 * @param rotator 轮转器句柄
 */
void audit_rotator_destroy(audit_rotator_t* rotator);

/**
 * @brief 写入审计条目
 * @param rotator 轮转器句柄
 * @param entry 审计条目
 * @return 0 成功，其他失败
 */
int audit_rotator_write(audit_rotator_t* rotator, const audit_entry_t* entry);

/**
 * @brief 强制轮转
 * @param rotator 轮转器句柄
 * @return 0 成功，其他失败
 */
int audit_rotator_rotate(audit_rotator_t* rotator);

/**
 * @brief 获取当前文件大小
 * @param rotator 轮转器句柄
 * @return 当前文件大小
 */
size_t audit_rotator_current_size(audit_rotator_t* rotator);

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_AUDIT_ROTATOR_H */
