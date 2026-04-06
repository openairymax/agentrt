/**
 * @file utils.h
 * @brief AgentOS heapstore 公共工具函数接口
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * "From data intelligence emerges."
 */

#ifndef heapstore_UTILS_H
#define heapstore_UTILS_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 确保目录存在，必要时创建嵌套目录
 *
 * @param path [in] 目录路径
 * @return bool 成功返回 true，失败返回 false
 *
 * @ownership 调用者负责 path 的生命周期
 * @threadsafe 是
 * @reentrant 是
 *
 * @note 支持创建多级嵌套目录
 */
bool heapstore_ensure_directory(const char* path);

/**
 * @brief 计算目录的总大小和文件数量
 *
 * @param path [in] 目录路径
 * @param out_size [out] 输出总大小（字节）
 * @param out_count [out] 输出文件数量
 * @return bool 成功返回 true，失败返回 false
 *
 * @ownership 调用者负责 path 的生命周期
 * @threadsafe 是
 * @reentrant 是
 *
 * @note 递归计算子目录大小
 * @since v1.0.0
 */
bool heapstore_calculate_directory_size(const char* path, uint64_t* out_size, uint32_t* out_count);

/**
 * @brief 净化路径组件，防止路径遍历和注入攻击
 *
 * 此函数用于净化用户输入的服务名称、通道ID等标识符，
 * 防止路径遍历攻击（如 "../../../etc/passwd"）和其他注入攻击。
 *
 * @param output [out] 输出缓冲区，存储净化后的字符串
 * @param input [in] 待净化的输入字符串
 * @param size [in] 输出缓冲区大小（字节）
 * @return int 成功返回 0，输入非法返回 -1
 *
 * @ownership 调用者负责 output 和 input 的生命周期
 * @threadsafe 是
 * @reentrant 是
 *
 * @note
 * - 拒绝包含 ".." 的输入（路径遍历）
 * - 拒绝包含 "/" 或 "\\" 的输入（目录分隔符）
 * - 拒绝包含空字节的输入（空字节注入）
 * - 只允许安全字符：字母数字、下划线(_)、连字符(-)、点号(.)
 * - 危险字符会被替换为下划线(_)
 * - 输入长度不能超过 size-1
 *
 * @warning 此函数应在所有文件路径构造前调用
 *
 * @see heapstore_ensure_directory()
 *
 * @since v1.0.0.7
 *
 * @example
 * @code
 * char safe_name[256];
 * if (heapstore_sanitize_path_component(safe_name, "../../../etc/passwd", sizeof(safe_name)) != 0) {
 *     // 输入被拒绝，包含路径遍历攻击
 *     return ERROR_INVALID_PARAM;
 * }
 * // safe_name 现在是安全的，可用于路径构造
 * @endcode
 */
int heapstore_sanitize_path_component(char* output, const char* input, size_t size);

/**
 * @brief 验证标识符是否安全（不包含路径遍历等危险模式）
 *
 * 此函数是 heapstore_sanitize_path_component 的轻量级版本，
 * 仅检查输入是否安全，不进行净化处理。
 *
 * @param input [in] 待验证的输入字符串
 * @return bool 安全返回 true，包含危险模式返回 false
 *
 * @ownership 调用者负责 input 的生命周期
 * @threadsafe 是
 * @reentrant 是
 *
 * @note
 * - 检查 ".." 路径遍历模式
 * - 检查 "/" 和 "\\" 目录分隔符
 * - 检查空字节注入
 * - 检查是否只包含安全字符
 *
 * @see heapstore_sanitize_path_component()
 *
 * @since v1.0.0.7
 */
bool heapstore_is_safe_identifier(const char* input);

#ifdef __cplusplus
}
#endif

#endif /* heapstore_UTILS_H */
