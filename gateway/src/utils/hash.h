/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file hash.h
 * @brief 通用哈希函数工具
 *
 * 提供 FNV-1a 等标准哈希算法实现
 * 供 session.c 和 ratelimit.c 共同使用
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef GATEWAY_HASH_H
#define GATEWAY_HASH_H

#include <stddef.h>
#include <stdint.h>

/* ==================== FNV-1a 哈希常量 ==================== */

#define HASH_FNV_OFFSET_BASIS_64   14695981039346656037ULL
#define HASH_FNV_PRIME_64          1099511628211ULL

#define HASH_DJB2_INIT             5381

/* ==================== FNV-1a 哈希算法 ==================== */

/**
 * @brief FNV-1a 64位哈希函数
 *
 * Fowler-Noll-Vo 哈希函数的变体，具有以下特点：
 * - 分布均匀，碰撞率低
 * - 计算速度快
 * - 适合字符串哈希
 *
 * @param[in] data 输入数据
 * @param[in] len 数据长度
 * @return 64位哈希值
 */
uint64_t hash_fnv1a_64(const void* data, size_t len);

/**
 * @brief FNV-1a 64位字符串哈希函数
 *
 * @param[in] str 字符串（以 '\0' 结尾）
 * @return 64位哈希值
 */
uint64_t hash_fnv1a_string(const char* str);

/**
 * @brief FNV-1a 64位哈希函数（带取模）
 *
 * @param[in] data 输入数据
 * @param[in] len 数据长度
 * @param[in] modulus 取模值（桶数量）
 * @return 哈希值 % modulus
 */
size_t hash_fnv1a_64_mod(const void* data, size_t len, size_t modulus);

/**
 * @brief FNV-1a 字符串哈希函数（带取模）
 *
 * 适用于哈希表桶索引计算
 *
 * @param[in] str 字符串
 * @param[in] bucket_count 桶数量
 * @return 桶索引（0 到 bucket_count-1）
 */
size_t hash_string_mod(const char* str, size_t bucket_count);

/* ==================== 其他哈希算法 ==================== */

/**
 * @brief djb2 哈希函数
 *
 * 由 Daniel J. Bernstein 设计的简单高效哈希函数
 *
 * @param[in] str 字符串
 * @return 32位哈希值
 */
uint32_t hash_djb2(const char* str);

/**
 * @brief MurmurHash 混淆函数
 *
 * 用于打乱哈希值的位，增加随机性
 *
 * @param[in] key 输入值
 * @return 混淆后的值
 */
uint64_t hash_murmur_finalize(uint64_t key);

/* ==================== 内联函数 ==================== */

/**
 * @brief 内联 FNV-1a 64位哈希函数
 *
 * 用于性能敏感场景，避免函数调用开销
 */
static inline uint64_t hash_fnv1a_64_inline(const char* str) {
    uint64_t hash = HASH_FNV_OFFSET_BASIS_64;
    while (*str) {
        hash ^= (uint8_t)(*str++);
        hash *= HASH_FNV_PRIME_64;
    }
    return hash;
}

#endif /* GATEWAY_HASH_H */
