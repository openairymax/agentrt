/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file secure_mem.h
 * @brief 安全内存操作接口
 *
 * 提供防止编译器优化的安全内存操作函数，
 * 用于处理敏感数据（如密码、密钥、令牌等）的内存操作。
 *
 * @security 安全等级: CRITICAL
 * @security 威胁模型: 编译器优化可能将敏感数据保留在内存中
 * @security 防护措施: 使用 volatile 指针和内存屏障防止优化
 *
 * @compliance 符合标准:
 *   - CWE-14: Compiler Removal of Code to Clear Buffers
 *   - OWASP Cryptographic Storage Cheat Sheet
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef gateway_SECURE_MEM_H
#define gateway_SECURE_MEM_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 安全内存设置（防编译器优化）
 *
 * 使用 volatile 指针和内存屏障确保编译器不会优化掉内存操作，
 * 防止敏感数据残留在寄存器或CPU缓存中。
 *
 * @param ptr 内存指针
 * @param value 设置值（通常为0）
 * @param n 字节数
 *
 * @example
 * ```c
 * char password[64];
 * use_password(password);
 * secure_memset(password, 0, sizeof(password));
 * ```
 */
void secure_memset(void* ptr, uint8_t value, size_t n);

/**
 * @brief 安全内存释放
 *
 * 先将内存内容清零，再释放内存。
 * 结合 secure_memset 使用，确保敏感数据不会残留在内存中。
 *
 * @param ptr 内存指针
 * @param size 内存大小（字节）
 *
 * @example
 * ```c
 * char* secret = malloc(256);
 * use_secret(secret);
 * secure_free(secret, 256);
 * secret = NULL;
 * ```
 */
void secure_free(void* ptr, size_t size);

/**
 * @brief 安全内存比较
 *
 * 比较两个内存区域的内容，使用常量时间算法。
 * 防止时序攻击（timing attack）泄露信息。
 *
 * @param s1 内存区域1
 * @param s2 内存区域2
 * @param n 比较字节数
 * @return 0 相等，非0 不相等
 *
 * @security 防护措施: 常量时间比较，防止时序攻击
 *
 * @example
 * ```c
 * if (secure_memcmp(stored_hash, computed_hash, 32) != 0) {
 *     return AUTH_FAILED;
 * }
 * ```
 */
int secure_memcmp(const void* s1, const void* s2, size_t n);

/**
 * @brief 安全内存复制
 *
 * 复制内存内容，并在复制完成后清零源缓冲区。
 * 适用于处理敏感数据（如密钥）的移动操作。
 *
 * @param dest 目标缓冲区
 * @param src 源缓冲区
 * @param n 复制字节数
 *
 * @security 防护措施: 复制后清零源缓冲区
 *
 * @example
 * ```c
 * char session_key[32];
 * secure_memcpy(session_key, temp_key, 32);
 * // temp_key 已被清零
 * ```
 */
void secure_memcpy(void* dest, const void* src, size_t n);

/**
 * @brief 检查内存是否全为零
 *
 * 使用常量时间算法检查内存区域是否全为零。
 * 防止时序攻击判断密钥长度等信息。
 *
 * @param ptr 内存指针
 * @param n 检查字节数
 * @return true 全为零，false 包含非零字节
 *
 * @security 防护措施: 常量时间检查
 *
 * @example
 * ```c
 * if (is_mem_zero(encrypted_key, key_length)) {
 *     // 密钥未初始化
 * }
 * ```
 */
bool is_mem_zero(const void* ptr, size_t n);

/**
 * @brief 安全字符串比较
 *
 * 比较两个字符串，使用常量时间算法。
 * 防止时序攻击泄露密码、令牌等信息。
 *
 * @param s1 字符串1
 * @param s2 字符串2
 * @return 0 相等，非0 不相等
 *
 * @security 防护措施: 常量时间比较
 *
 * @example
 * ```c
 * if (secure_strcmp(input_password, stored_password) != 0) {
 *     return AUTH_FAILED;
 * }
 * ```
 */
int secure_strcmp(const char* s1, const char* s2);

#endif /* gateway_SECURE_MEM_H */
