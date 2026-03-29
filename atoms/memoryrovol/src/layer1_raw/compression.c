/**
 * @file compression.c
 * @brief L1 原始卷数据压�?解压支持（可选）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "layer1_raw.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../../../commons/utils/memory/include/memory_compat.h"
#include "../../../commons/utils/string/include/string_compat.h"
#include <string.h>
#include <zlib.h>

/**
 * @brief 压缩数据（使�?zlib�?
 * @param in 输入数据
 * @param in_len 输入长度
 * @param out 输出压缩数据（需调用者释放）
 * @param out_len 输出长度
 * @return agentos_error_t
 */
agentos_error_t agentos_layer1_raw_compress(
    const void* in,
    size_t in_len,
    void** out,
    size_t* out_len) {

// From data intelligence emerges. by spharx
    if (!in || in_len == 0 || !out || !out_len) return AGENTOS_EINVAL;

    // 预估压缩后最大大�?
    uLongf max_dest = compressBound(in_len);
    Bytef* dest = (Bytef*)AGENTOS_MALLOC(max_dest);
    if (!dest) return AGENTOS_ENOMEM;

    uLongf dest_len = max_dest;
    int ret = compress2(dest, &dest_len, (const Bytef*)in, in_len, Z_BEST_SPEED);
    if (ret != Z_OK) {
        AGENTOS_FREE(dest);
        return AGENTOS_EIO;
    }

    *out = dest;
    *out_len = dest_len;
    return AGENTOS_SUCCESS;
}

/**
 * @brief 解压数据
 * @param in 压缩数据
 * @param in_len 压缩数据长度
 * @param out 输出解压数据（需调用者释放）
 * @param out_len 输出长度（解压后大小�?
 * @return agentos_error_t
 */
agentos_error_t agentos_layer1_raw_decompress(
    const void* in,
    size_t in_len,
    void** out,
    size_t out_len) {

    if (!in || in_len == 0 || !out || out_len == 0) return AGENTOS_EINVAL;

    Bytef* dest = (Bytef*)AGENTOS_MALLOC(out_len);
    if (!dest) return AGENTOS_ENOMEM;

    uLongf dest_len = out_len;
    int ret = uncompress(dest, &dest_len, (const Bytef*)in, in_len);
    if (ret != Z_OK) {
        AGENTOS_FREE(dest);
        return AGENTOS_EIO;
    }

    *out = dest;
    return AGENTOS_SUCCESS;
}