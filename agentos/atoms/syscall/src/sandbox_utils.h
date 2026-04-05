/**
 * @file sandbox_utils.h
 * @brief 沙箱工具函数接口 - 哈希、审计日志、资源管理
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_SANDBOX_UTILS_H
#define AGENTOS_SANDBOX_UTILS_H

#include "agentos.h"
#include <stdint.h>
#include <stddef.h>

/* 前向声明 */
typedef struct agentos_sandbox agentos_sandbox_t;
typedef struct audit_entry audit_entry_t;

/**
 * @brief 计算字符串的简单哈希
 * @param str 输入字符串
 * @return 哈希值
 */
uint64_t sandbox_simple_hash(const char* str);

/**
 * @brief 添加审计日志条目
 * @param sandbox 沙箱句柄
 * @param syscall_num 系统调用号
 * @param caller_id 调用者 ID
 * @param result_code 结果码
 * @param duration_ns 执行时长（纳秒）
 * @param details 详细信息
 * @return AGENTOS_SUCCESS 成功，其他为错误码
 */
agentos_error_t sandbox_add_audit_entry(agentos_sandbox_t* sandbox, int syscall_num,
                                       const char* caller_id, int result_code,
                                       uint64_t duration_ns, const char* details);

/**
 * @brief 释放沙箱资源
 * @param sandbox 沙箱句柄
 * @param syscall_num 系统调用号
 * @param args 参数
 * @param result 结果
 */
void sandbox_release_resource(agentos_sandbox_t* sandbox, int syscall_num,
                             void* args, int result);

/**
 * @brief 生成参数哈希
 * @param args 参数
 * @return 参数字符串哈希
 */
char* sandbox_generate_args_hash(void* args);

#endif /* AGENTOS_SANDBOX_UTILS_H */
