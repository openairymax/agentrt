/*
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 * 
 * @file input_validator.h
 * @brief 输入验证工具库 - 安全内生加固
 * 
 * @details
 * 本模块提供统一的输入验证功能，防止注入攻击、路径遍历、缓冲区溢出等安全问题。
 * 遵循白名单验证原则，只允许已知安全的输入模式。
 * 
 * 安全原则：
 * 1. 永不信任外部输入
 * 2. 白名单优于黑名单
 * 3. 边界检查必须严格
 * 4. 错误时安全失败
 * 
 * @author Spharx AgentOS Team
 * @date 2026-03-30
 * @version 2.0
 * 
 * @note 线程安全：所有公共接口均为线程安全
 * @see ARCHITECTURAL_PRINCIPLES.md E-1 安全内生原则
 * @see C_Cpp_secure_coding_guide.md 安全编码指南
 */

#ifndef AGENTOS_INPUT_VALIDATOR_H
#define AGENTOS_INPUT_VALIDATOR_H

#include <stdint.h>
#include <stddef.h>
#include "../error/include/error.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 验证结果 ==================== */

/**
 * @brief 验证结果结构
 */
typedef struct {
    int is_valid;                         /**< 是否有效 */
    const char* error_message;            /**< 错误消息 */
    int error_code;                       /**< 错误码 */
    const char* error_field;              /**< 错误字段 */
} agentos_validation_result_t;

/* ==================== 字符串验证 ==================== */

/**
 * @brief 验证字符串长度
 * @param str [in] 输入字符串
 * @param min_len 最小长度
 * @param max_len 最大长度
 * @param result [out] 验证结果
 */
void agentos_validate_string_length(
    const char* str,
    size_t min_len,
    size_t max_len,
    agentos_validation_result_t* result);

/**
 * @brief 验证字符串是否只包含安全字符
 * @param str [in] 输入字符串
 * @param allowed_chars [in] 允许的字符集（白名单）
 * @param result [out] 验证结果
 */
void agentos_validate_string_charset(
    const char* str,
    const char* allowed_chars,
    agentos_validation_result_t* result);

/**
 * @brief 验证标识符（字母、数字、下划线）
 * @param str [in] 输入字符串
 * @param max_len 最大长度
 * @param result [out] 验证结果
 */
void agentos_validate_identifier(
    const char* str,
    size_t max_len,
    agentos_validation_result_t* result);

/**
 * @brief 验证JSON字符串
 * @param str [in] 输入字符串
 * @param max_len 最大长度
 * @param result [out] 验证结果
 */
void agentos_validate_json_string(
    const char* str,
    size_t max_len,
    agentos_validation_result_t* result);

/* ==================== 路径验证 ==================== */

/**
 * @brief 验证文件路径安全性
 * @param path [in] 输入路径
 * @param allowed_root [in] 允许的根目录（可为NULL）
 * @param result [out] 验证结果
 * 
 * @details
 * 检测以下安全问题：
 * - 路径遍历攻击（../）
 * - 空字节注入
 * - 符号链接攻击
 * - 绝对路径限制
 */
void agentos_validate_file_path(
    const char* path,
    const char* allowed_root,
    agentos_validation_result_t* result);

/**
 * @brief 规范化路径
 * @param path [in] 输入路径
 * @param out_normalized [out] 输出规范化路径（调用者负责释放）
 * @param out_len 输出长度
 * @return agentos_error_t 错误码
 */
agentos_error_t agentos_normalize_path(
    const char* path,
    char** out_normalized,
    size_t* out_len);

/* ==================== 命令验证 ==================== */

/**
 * @brief 验证Shell命令安全性
 * @param cmd [in] 输入命令
 * @param allowed_commands [in] 允许的命令列表（以NULL结尾）
 * @param result [out] 验证结果
 * 
 * @details
 * 检测以下安全问题：
 * - 命令注入（; | & $ ` 等）
 * - 危险命令（rm -rf, dd, mkfs等）
 * - 环境变量注入
 */
void agentos_validate_shell_command(
    const char* cmd,
    const char** allowed_commands,
    agentos_validation_result_t* result);

/**
 * @brief 净化Shell参数
 * @param param [in] 输入参数
 * @param out_sanitized [out] 输出净化后的参数（调用者负责释放）
 * @return agentos_error_t 错误码
 */
agentos_error_t agentos_sanitize_shell_param(
    const char* param,
    char** out_sanitized);

/* ==================== SQL验证 ==================== */

/**
 * @brief 验证SQL查询安全性
 * @param sql [in] 输入SQL
 * @param result [out] 验证结果
 * 
 * @details
 * 检测以下安全问题：
 * - SQL注入（UNION, OR 1=1, --等）
 * - 危险操作（DROP, TRUNCATE, ALTER等）
 * - 多语句执行
 */
void agentos_validate_sql_query(
    const char* sql,
    agentos_validation_result_t* result);

/**
 * @brief 净化SQL标识符（表名、列名等）
 * @param identifier [in] 输入标识符
 * @param out_sanitized [out] 输出净化后的标识符（调用者负责释放）
 * @return agentos_error_t 错误码
 */
agentos_error_t agentos_sanitize_sql_identifier(
    const char* identifier,
    char** out_sanitized);

/* ==================== URL验证 ==================== */

/**
 * @brief 验证URL安全性
 * @param url [in] 输入URL
 * @param allowed_schemes [in] 允许的协议列表（如 {"http", "https", NULL}）
 * @param result [out] 验证结果
 * 
 * @details
 * 检测以下安全问题：
 * - 协议注入（javascript:, data:等）
 * - SSRF攻击（内网IP、localhost等）
 * - 凭据泄露
 */
void agentos_validate_url(
    const char* url,
    const char** allowed_schemes,
    agentos_validation_result_t* result);

/**
 * @brief 解析URL组件
 * @param url [in] 输入URL
 * @param out_scheme [out] 输出协议（调用者负责释放）
 * @param out_host [out] 输出主机名（调用者负责释放）
 * @param out_port [out] 输出端口
 * @param out_path [out] 输出路径（调用者负责释放）
 * @return agentos_error_t 错误码
 */
agentos_error_t agentos_parse_url(
    const char* url,
    char** out_scheme,
    char** out_host,
    uint16_t* out_port,
    char** out_path);

/* ==================== 数值验证 ==================== */

/**
 * @brief 验证整数范围
 * @param value 输入值
 * @param min_val 最小值
 * @param max_val 最大值
 * @param result [out] 验证结果
 */
void agentos_validate_int_range(
    int64_t value,
    int64_t min_val,
    int64_t max_val,
    agentos_validation_result_t* result);

/**
 * @brief 验证浮点数范围
 * @param value 输入值
 * @param min_val 最小值
 * @param max_val 最大值
 * @param result [out] 验证结果
 */
void agentos_validate_float_range(
    double value,
    double min_val,
    double max_val,
    agentos_validation_result_t* result);

/* ==================== 缓冲区验证 ==================== */

/**
 * @brief 安全内存复制
 * @param dest [out] 目标缓冲区
 * @param dest_size 目标缓冲区大小
 * @param src [in] 源数据
 * @param src_size 源数据大小
 * @return agentos_error_t 错误码
 */
agentos_error_t agentos_safe_memcpy(
    void* dest,
    size_t dest_size,
    const void* src,
    size_t src_size);

/**
 * @brief 安全字符串复制
 * @param dest [out] 目标缓冲区
 * @param dest_size 目标缓冲区大小
 * @param src [in] 源字符串
 * @return agentos_error_t 错误码
 */
agentos_error_t agentos_safe_strcpy(
    char* dest,
    size_t dest_size,
    const char* src);

/**
 * @brief 安全字符串拼接
 * @param dest [in,out] 目标缓冲区
 * @param dest_size 目标缓冲区大小
 * @param src [in] 源字符串
 * @return agentos_error_t 错误码
 */
agentos_error_t agentos_safe_strcat(
    char* dest,
    size_t dest_size,
    const char* src);

/* ==================== 便捷宏定义 ==================== */

/**
 * @brief 验证并返回错误
 */
#define AGENTOS_VALIDATE_OR_RETURN(result, error_code) \
    do { \
        if (!(result).is_valid) { \
            return error_code; \
        } \
    } while(0)

/**
 * @brief 验证并跳转到错误处理
 */
#define AGENTOS_VALIDATE_OR_GOTO(result, label, error_code) \
    do { \
        if (!(result).is_valid) { \
            err = error_code; \
            goto label; \
        } \
    } while(0)

/**
 * @brief 安全字符串复制宏
 */
#define AGENTOS_SAFE_STRCPY(dest, src) \
    agentos_safe_strcpy(dest, sizeof(dest), src)

/**
 * @brief 安全字符串拼接宏
 */
#define AGENTOS_SAFE_STRCAT(dest, src) \
    agentos_safe_strcat(dest, sizeof(dest), src)

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_INPUT_VALIDATOR_H */
