/**
 * @file id_utils.h
 * @brief ID生成工具函数
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#ifndef AGENTOS_ID_UTILS_H
#define AGENTOS_ID_UTILS_H

#include "agentos.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 生成唯一任务ID
 * 
 * @param prefix ID前缀（如"task"、"plan"等）
 * @param buf 输出缓冲区
 * @param len 缓冲区长度
 */
void agentos_generate_task_id(const char* prefix, char* buf, size_t len);

/**
 * @brief 生成唯一计划ID
 * 
 * @param buf 输出缓冲区
 * @param len 缓冲区长度
 */
void agentos_generate_plan_id(char* buf, size_t len);

/**
 * @brief 生成唯一记录ID（用于记忆系统）
 * 
 * @param buf 输出缓冲区
 * @param len 缓冲区长度
 */
void agentos_generate_record_id(char* buf, size_t len);

/**
 * @brief 生成唯一会话ID
 * 
 * @param buf 输出缓冲区
 * @param len 缓冲区长度
 */
void agentos_generate_session_id(char* buf, size_t len);

/**
 * @brief 生成UUID格式的字符串
 * 
 * @param buf 输出缓冲区（至少37字节）
 * @return agentos_error_t 错误码
 */
agentos_error_t agentos_generate_uuid(char* buf);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_ID_UTILS_H */