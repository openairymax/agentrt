/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file hash.c
 * @brief 通用哈希函数工具实现
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "hash.h"
#include <string.h>

/* ==================== FNV-1a 64位哈希 ==================== */

uint64_t hash_fnv1a_64(const void* data, size_t len) {
    const uint8_t* bytes = (const uint8_t*)data;
    uint64_t hash = HASH_FNV_OFFSET_BASIS_64;

    for (size_t i = 0; i < len; i++) {
        hash ^= bytes[i];
        hash *= HASH_FNV_PRIME_64;
    }

    return hash;
}

uint64_t hash_fnv1a_string(const char* str) {
    if (!str) {
        return 0;
    }
    return hash_fnv1a_64(str, strlen(str));
}

size_t hash_fnv1a_64_mod(const void* data, size_t len, size_t modulus) {
    if (modulus == 0) {
        return 0;
    }
    uint64_t hash = hash_fnv1a_64(data, len);
    return (size_t)(hash % modulus);
}

size_t hash_string_mod(const char* str, size_t bucket_count) {
    if (!str || bucket_count == 0) {
        return 0;
    }
    return hash_fnv1a_64_mod(str, strlen(str), bucket_count);
}

/* ==================== djb2 哈希 ==================== */

uint32_t hash_djb2(const char* str) {
    if (!str) {
        return 0;
    }

    uint32_t hash = HASH_DJB2_INIT;
    int c;

    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;  /* hash * 33 + c */
    }

    return hash;
}

/* ==================== MurmurHash 混淆 ==================== */

uint64_t hash_murmur_finalize(uint64_t key) {
    key ^= key >> 33;
    key *= 0xff51afd7ed558ccdULL;
    key ^= key >> 33;
    key *= 0xc4ceb9fe1a85ec53ULL;
    key ^= key >> 33;
    return key;
}
