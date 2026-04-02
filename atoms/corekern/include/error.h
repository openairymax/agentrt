/**
 * @file error.h
 * @brief 内核错误码定义
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * "From data intelligence emerges."
 *
 * @note 定义了 AgentOS 内核统一的错误码体系，遵循 POSIX 错误码语义
 */

#ifndef AGENTOS_ATOMS_COREKERN_ERROR_H
#define AGENTOS_ATOMS_COREKERN_ERROR_H

#include <stdint.h>
#include "export.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief API 版本号
 */
#define AGENTOS_COREKERN_API_VERSION 1

/**
 * @brief AgentOS 统一错误类型
 *
 * 使用 int32_t 确保跨平台一致性
 * 正值表示成功，负值表示错误
 */
typedef int32_t agentos_error_t;

/**
 * @brief 成功
 *
 * 操作成功完成，无错误
 */
#define AGENTOS_SUCCESS                0

/**
 * @brief 无效参数
 *
 * 传入的参数无效或超出范围
 */
#define AGENTOS_EINVAL                 -1

/**
 * @brief 内存不足
 *
 * 内存分配失败，系统内存不足
 */
#define AGENTOS_ENOMEM                 -2

/**
 * @brief 资源忙
 *
 * 请求的资源正被其他操作占用
 */
#define AGENTOS_EBUSY                  -3

/**
 * @brief 资源不存在
 *
 * 请求的资源或实体不存在
 */
#define AGENTOS_ENOENT                 -4

/**
 * @brief 权限不足
 *
 * 操作权限不足，无法执行该操作
 */
#define AGENTOS_EPERM                  -5

/**
 * @brief 操作超时
 *
 * 操作在指定时间内未完成
 */
#define AGENTOS_ETIMEDOUT              -6

/**
 * @brief 资源已存在
 *
 * 试图创建已存在的资源
 */
#define AGENTOS_EEXIST                 -7

/**
 * @brief 操作已取消
 *
 * 操作被用户或系统取消
 */
#define AGENTOS_ECANCELED              -8

/**
 * @brief 不支持的操作
 *
 * 请求的操作不被支持
 */
#define AGENTOS_ENOTSUP                -9

/**
 * @brief I/O 错误
 *
 * 输入/输出操作失败
 */
#define AGENTOS_EIO                    -10

/**
 * @brief 操作被中断
 *
 * 操作被信号中断
 */
#define AGENTOS_EINTR                  -11

/**
 * @brief 数值溢出
 *
 * 数值运算结果超出表示范围
 */
#define AGENTOS_EOVERFLOW              -12

/**
 * @brief 错误的文件描述符
 *
 * 文件描述符无效或已关闭
 */
#define AGENTOS_EBADF                  -13

/**
 * @brief 未初始化
 *
 * 使用前未正确初始化
 */
#define AGENTOS_ENOTINIT               -14

/**
 * @brief 资源相关错误
 *
 * 资源耗尽、不足或状态异常
 */
#define AGENTOS_ERESOURCE              -15

/**
 * @brief 功能未实现
 *
 * 请求的功能未在此平台实现
 */
#define AGENTOS_ENOSYS                 -16

/**
 * @brief 获取错误码对应的描述字符串
 *
 * @param err [in] 错误码
 * @return const char* 错误描述字符串
 *
 * @ownership 返回内部字符串，调用者不应释放
 * @threadsafe 是
 * @reentrant 是
 *
 * @note 提供详细的错误描述，包括数值上下文和修复建议
 */
AGENTOS_API const char* agentos_strerror(agentos_error_t err);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_ATOMS_COREKERN_ERROR_H */
