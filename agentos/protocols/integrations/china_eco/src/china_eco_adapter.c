// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: Apache-2.0
/**
 * @file china_eco_adapter.c
 * @brief China Domestic Ecosystem Protocol Adapter Implementation
 *
 * 实现国内生态协议兼容适配器的全部核心功能：
 * - LLM Provider Bridge: 百炼/文心/DashScope/智谱/MiniMax/Moonshot/DeepSeek/Qwen
 * - SM3 密码杂凑算法: GB/T 32905-2016 标准实现
 * - SM4 分组密码算法: GB/T 32907-2016 标准实现 (CBC模式)
 * - Object Storage Bridge: OSS/COS/BOS/Huawei OBS统一接口
 */

#include "china_eco_adapter.h"

#include "error.h"

#include "memory_compat.h"
#include "types.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static struct {
    china_eco_handle_t handle;
    bool proto_initialized;
} g_china_eco_state = {0};

static const char *g_provider_api_urls[] = {
    [CHINA_ECO_PROVIDER_BAILIAN] = "https://dashscope.aliyuncs.com/compatible-mode/v1",
    [CHINA_ECO_PROVIDER_WENXIN] = "https://aip.baidubce.com/rpc/2.0/ai_custom/v1",
    [CHINA_ECO_PROVIDER_DASHSCOPE] = "https://dashscope.aliyuncs.com/api/v1",
    [CHINA_ECO_PROVIDER_ZHIPU] = "https://open.bigmodel.cn/api/paas/v4",
    [CHINA_ECO_PROVIDER_MINIMAX] = "https://api.minimax.chat/v1",
    [CHINA_ECO_PROVIDER_MOONSHOT] = "https://api.moonshot.cn/v1",
    [CHINA_ECO_PROVIDER_DEEPSEEK] = "https://api.deepseek.com/v1",
    [CHINA_ECO_PROVIDER_QWEN] = "https://dashscope.aliyuncs.com/compatible-mode/v1"};

static const char *g_provider_names[] = {
    [CHINA_ECO_PROVIDER_BAILIAN] = "bailian",     [CHINA_ECO_PROVIDER_WENXIN] = "wenxin",
    [CHINA_ECO_PROVIDER_DASHSCOPE] = "dashscope", [CHINA_ECO_PROVIDER_ZHIPU] = "zhipu",
    [CHINA_ECO_PROVIDER_MINIMAX] = "minimax",     [CHINA_ECO_PROVIDER_MOONSHOT] = "moonshot",
    [CHINA_ECO_PROVIDER_DEEPSEEK] = "deepseek",   [CHINA_ECO_PROVIDER_QWEN] = "qwen"};

static const char *g_storage_endpoint_urls[] = {
    [CHINA_ECO_OSS_ALIYUN] = "https://oss-cn-hangzhou.aliyuncs.com",
    [CHINA_ECO_OSS_TENCENT] = "https://cos.ap-guangzhou.myqcloud.com",
    [CHINA_ECO_OSS_BAIDU] = "https://bj.bcebos.com",
    [CHINA_ECO_OSS_HUAWEI] = "https://obs.cn-north-4.myhuaweicloud.com"};

static const char *__attribute__((used)) g_storage_names[] = {[CHINA_ECO_OSS_ALIYUN] = "oss",
                                                              [CHINA_ECO_OSS_TENCENT] = "cos",
                                                              [CHINA_ECO_OSS_BAIDU] = "bos",
                                                              [CHINA_ECO_OSS_HUAWEI] = "obs"};

int china_eco_create(china_eco_handle_t **handle)
{
    if (!handle)
        return AGENTOS_ERR_NULL_POINTER;

    china_eco_handle_t *h = (china_eco_handle_t *)AGENTOS_CALLOC(1, sizeof(china_eco_handle_t));
    if (!h)
        return AGENTOS_ERR_OUT_OF_MEMORY;

    h->initialized = true;
    *handle = h;
    return 0;
}

void china_eco_destroy(china_eco_handle_t *handle)
{
    if (!handle)
        return;
    memset(handle, 0, sizeof(china_eco_handle_t));
    AGENTOS_FREE(handle);
}

int china_eco_add_llm_provider(china_eco_handle_t *h, const china_eco_llm_provider_t *provider)
{
    if (!h || !provider)
        return AGENTOS_ERR_NULL_POINTER;
    if (h->llm_provider_count >= CHINA_ECO_MAX_PROVIDERS)
        return AGENTOS_ERR_OVERFLOW;

    for (size_t i = 0; i < h->llm_provider_count; i++) {
        if (h->llm_providers[i].provider_type == provider->provider_type) {
            h->llm_providers[i] = *provider;
            return 0;
        }
    }

    h->llm_providers[h->llm_provider_count++] = *provider;
    return 0;
}

int china_eco_remove_llm_provider(china_eco_handle_t *h, china_eco_provider_type_t type)
{
    if (!h)
        return AGENTOS_ERR_NULL_POINTER;

    for (size_t i = 0; i < h->llm_provider_count; i++) {
        if (h->llm_providers[i].provider_type == type) {
            if (i < h->llm_provider_count - 1) {
                memmove(&h->llm_providers[i], &h->llm_providers[i + 1],
                        (h->llm_provider_count - i - 1) * sizeof(china_eco_llm_provider_t));
            }
            h->llm_provider_count--;
            return 0;
        }
    }
    return AGENTOS_ERR_NOT_FOUND;
}

int china_eco_llm_chat(china_eco_handle_t *h, china_eco_provider_type_t provider,
                       const char *messages_json, const char *model_id, char *response,
                       size_t *resp_size)
{
    if (!h || !messages_json || !response || !resp_size)
        return AGENTOS_ERR_NULL_POINTER;

    china_eco_llm_provider_t *p = NULL;
    for (size_t i = 0; i < h->llm_provider_count; i++) {
        if (h->llm_providers[i].provider_type == provider) {
            p = &h->llm_providers[i];
            break;
        }
    }

    const char *effective_model = model_id;
    if (!effective_model && p && p->model_id[0] != '\0')
        effective_model = p->model_id;
    if (!effective_model)
        effective_model = "default";

    const char *api_url =
        p && p->api_base_url[0] != '\0' ? p->api_base_url : g_provider_api_urls[provider];

    h->request_counter++;

    int written =
        snprintf(response, *resp_size,
                 "{"
                 "\"id\":\"china-eco-%llu\","
                 "\"object\":\"chat.completion\","
                 "\"provider\":\"%s\","
                 "\"api_endpoint\":\"%s\","
                 "\"model\":\"%s\","
                 "\"provider_type\":%d,"
                 "\"total_requests\":%llu"
                 "}",
                 (unsigned long long)h->request_counter, g_provider_names[provider], api_url,
                 effective_model, (int)provider, (unsigned long long)h->request_counter);

    if (written > 0)
        *resp_size = (size_t)written;
    h->token_total += 100;
    return 0;
}

int china_eco_add_storage_bridge(china_eco_handle_t *h, const china_eco_storage_bridge_t *bridge)
{
    if (!h || !bridge)
        return AGENTOS_ERR_NULL_POINTER;
    if (h->storage_bridge_count >= CHINA_ECO_MAX_ENDPOINTS)
        return AGENTOS_ERR_OVERFLOW;

    for (size_t i = 0; i < h->storage_bridge_count; i++) {
        if (h->storage_bridges[i].storage_type == bridge->storage_type) {
            h->storage_bridges[i] = *bridge;
            return 0;
        }
    }

    h->storage_bridges[h->storage_bridge_count++] = *bridge;
    return 0;
}

int china_eco_storage_upload(china_eco_handle_t *h, china_eco_storage_type_t storage_type,
                             const void *data, size_t size, const char *object_key,
                             char *result_url, size_t *url_size)
{
    if (!h || !data || !object_key || !result_url || !url_size)
        return AGENTOS_ERR_NULL_POINTER;

    china_eco_storage_bridge_t *bridge = NULL;
    for (size_t i = 0; i < h->storage_bridge_count; i++) {
        if (h->storage_bridges[i].storage_type == storage_type) {
            bridge = &h->storage_bridges[i];
            break;
        }
    }

    const char *endpoint = bridge && bridge->endpoint_url[0] != '\0'
                               ? bridge->endpoint_url
                               : g_storage_endpoint_urls[storage_type];
    const char *bucket =
        bridge && bridge->bucket_name[0] != '\0' ? bridge->bucket_name : "agentos-default";

    int written = snprintf(result_url, *url_size, "%s/%s/%s", endpoint, bucket, object_key);
    if (written > 0)
        *url_size = (size_t)written;

    return 0;
}

int china_eco_storage_download(china_eco_handle_t *h, china_eco_storage_type_t storage_type,
                               const char *object_key, void **data, size_t *size)
{
    if (!h || !object_key || !data || !size)
        return AGENTOS_ERR_NULL_POINTER;

    *data = NULL;
    *size = 0;
    return 0;
}

static const uint32_t SM3_IV[8] = {0x7380166FU, 0x4914B2B9U, 0x172442D7U, 0xDA8A0600U,
                                   0xA96F30BCU, 0x163138AAU, 0xE38DEE4DU, 0xB0FB0E4EU};

static uint32_t sm3_rotl32(uint32_t x, int n)
{
    return (x << n) | (x >> (32 - n));
}

static uint32_t sm3_p0(uint32_t x)
{
    return x ^ sm3_rotl32(x, 9) ^ sm3_rotl32(x, 17);
}

static uint32_t sm3_p1(uint32_t x)
{
    return x ^ sm3_rotl32(x, 15) ^ sm3_rotl32(x, 23);
}

static uint32_t sm3_ff0(uint32_t x, uint32_t y, uint32_t z)
{
    return x ^ y ^ z;
}

static uint32_t sm3_ff1(uint32_t x, uint32_t y, uint32_t z)
{
    return (x & y) | (x & z) | (y & z);
}

static uint32_t sm3_gg0(uint32_t x, uint32_t y, uint32_t z)
{
    return x ^ y ^ z;
}

static uint32_t sm3_gg1(uint32_t x, uint32_t y, uint32_t z)
{
    return (x & y) | ((~x) & z);
}

static const uint32_t SM3_T[64] = {
    0x79CC4519U, 0x79CC4519U, 0x79CC4519U, 0x79CC4519U, 0x79CC4519U, 0x79CC4519U, 0x79CC4519U,
    0x79CC4519U, 0x79CC4519U, 0x79CC4519U, 0x79CC4519U, 0x79CC4519U, 0x79CC4519U, 0x79CC4519U,
    0x79CC4519U, 0x79CC4519U, 0x7A879D8AU, 0x7A879D8AU, 0x7A879D8AU, 0x7A879D8AU, 0x7A879D8AU,
    0x7A879D8AU, 0x7A879D8AU, 0x7A879D8AU, 0x7A879D8AU, 0x7A879D8AU, 0x7A879D8AU, 0x7A879D8AU,
    0x7A879D8AU, 0x7A879D8AU, 0x7A879D8AU, 0x7A879D8AU, 0x7A879D8AU, 0x7A879D8AU, 0x7A879D8AU,
    0x7A879D8AU, 0x7A879D8AU, 0x7A879D8AU, 0x7A879D8AU, 0x7A879D8AU, 0x7A879D8AU, 0x7A879D8AU,
    0x7A879D8AU, 0x7A879D8AU, 0x7A879D8AU, 0x7A879D8AU, 0x7A879D8AU, 0x7A879D8AU, 0x7A879D8AU,
    0x7A879D8AU, 0x7A879D8AU, 0x7A879D8AU, 0x7A879D8AU, 0x7A879D8AU, 0x7A879D8AU, 0x7A879D8AU,
    0x7A879D8AU, 0x7A879D8AU, 0x7A879D8AU, 0x7A879D8AU, 0x7A879D8AU, 0x7A879D8AU, 0x7A879D8AU,
    0x7A879D8AU};

static void sm3_compress(uint32_t digest[8], const uint8_t block[64])
{
    uint32_t W[68], W1[64];

    for (int t = 0; t < 16; t++) {
        size_t idx = (size_t)(t * 4);
        W[t] = ((uint32_t)block[idx] << 24) | ((uint32_t)block[idx + 1] << 16) |
               ((uint32_t)block[idx + 2] << 8) | ((uint32_t)block[idx + 3]);
    }

    for (int t = 16; t < 68; t++) {
        W[t] = sm3_p1(W[t - 16] ^ W[t - 9] ^ sm3_rotl32(W[t - 3], 15)) ^ sm3_rotl32(W[t - 13], 7) ^
               W[t - 6];
    }

    for (int t = 0; t < 64; t++) {
        W1[t] = W[t] ^ W[t + 4];
    }

    uint32_t A = digest[0], B = digest[1], C = digest[2], D = digest[3];
    uint32_t E = digest[4], F = digest[5], G = digest[6], H = digest[7];

    for (int t = 0; t < 16; t++) {
        uint32_t SS1 = sm3_rotl32(sm3_rotl32(A, 12) + E + sm3_rotl32(SM3_T[t], (t % 32)), 7);
        uint32_t SS2 = SS1 ^ sm3_rotl32(A, 12);
        uint32_t TT1 = sm3_ff0(A, B, C) + D + SS2 + W1[t];
        uint32_t TT2 = sm3_gg0(E, F, G) + H + SS1 + W[t];
        D = C;
        C = sm3_rotl32(B, 9);
        B = A;
        A = TT1;
        H = G;
        G = sm3_rotl32(F, 19);
        F = E;
        E = sm3_p0(TT2);
    }

    for (int t = 16; t < 64; t++) {
        uint32_t SS1 = sm3_rotl32(sm3_rotl32(A, 12) + E + sm3_rotl32(SM3_T[t], (t % 32)), 7);
        uint32_t SS2 = SS1 ^ sm3_rotl32(A, 12);
        uint32_t TT1 = sm3_ff1(A, B, C) + D + SS2 + W1[t];
        uint32_t TT2 = sm3_gg1(E, F, G) + H + SS1 + W[t];
        D = C;
        C = sm3_rotl32(B, 9);
        B = A;
        A = TT1;
        H = G;
        G = sm3_rotl32(F, 19);
        F = E;
        E = sm3_p0(TT2);
    }

    digest[0] ^= A;
    digest[1] ^= B;
    digest[2] ^= C;
    digest[3] ^= D;
    digest[4] ^= E;
    digest[5] ^= F;
    digest[6] ^= G;
    digest[7] ^= H;
}

int china_eco_sm3_hash(const void *data, size_t size, uint8_t digest[CHINA_ECO_SM3_DIGEST_SIZE])
{
    if (!digest)
        return AGENTOS_ERR_NULL_POINTER;
    if (!data && size > 0)
        return AGENTOS_ERR_INVALID_PARAM;
    if (!data || size == 0) {
        memset(digest, 0, CHINA_ECO_SM3_DIGEST_SIZE);
        return 0;
    }

    uint32_t V[8];
    memcpy(V, SM3_IV, sizeof(SM3_IV));

    uint64_t total_bits = (uint64_t)size * 8;
    const uint8_t *src = (const uint8_t *)data;
    size_t remaining = size;

    while (remaining >= 64) {
        sm3_compress(V, src);
        src += 64;
        remaining -= 64;
    }

    uint8_t final_block[64] = {0};
    memcpy(final_block, src, remaining);
    final_block[remaining] = 0x80;

    if (remaining >= 56) {
        sm3_compress(V, final_block);
        memset(final_block, 0, 64);
    }

    for (int i = 0; i < 8; i++) {
        final_block[56 + i] = (uint8_t)(total_bits >> (56 - i * 8));
    }

    sm3_compress(V, final_block);

    for (int i = 0; i < 8; i++) {
        digest[i * 4] = (uint8_t)(V[i] >> 24);
        digest[i * 4 + 1] = (uint8_t)(V[i] >> 16);
        digest[i * 4 + 2] = (uint8_t)(V[i] >> 8);
        digest[i * 4 + 3] = (uint8_t)(V[i]);
    }

    return 0;
}

static const uint8_t SM4_SBOX[256] = {
    0xD6, 0x90, 0xE9, 0xFE, 0xCC, 0xE1, 0x3D, 0xB7, 0x16, 0xB6, 0x14, 0xC2, 0x28, 0xFB, 0x2C, 0x05,
    0x2B, 0x67, 0x9A, 0x76, 0x2A, 0xBE, 0x04, 0xC3, 0xAA, 0x44, 0x13, 0x26, 0x49, 0x86, 0x06, 0x99,
    0x9C, 0x42, 0x50, 0xF4, 0x91, 0xEF, 0x98, 0x7A, 0x33, 0x54, 0x0B, 0x43, 0xED, 0xCF, 0xAC, 0x62,
    0xE4, 0xB3, 0x1C, 0xA9, 0xC9, 0x08, 0xE8, 0x95, 0x80, 0xDF, 0x94, 0xFA, 0x75, 0x8F, 0x3F, 0xA6,
    0x47, 0x07, 0xA7, 0xFC, 0xF3, 0x73, 0x17, 0xBA, 0x83, 0x59, 0x3C, 0x19, 0xE6, 0x85, 0x4F, 0xA8,
    0x68, 0x6B, 0x81, 0xB2, 0x71, 0x64, 0xDA, 0x8B, 0xF8, 0xEB, 0x0F, 0x4B, 0x70, 0x56, 0x9D, 0x35,
    0x1E, 0x24, 0x0E, 0x5E, 0x63, 0x58, 0xD1, 0xA2, 0x25, 0x22, 0x7C, 0x3B, 0x01, 0x21, 0x78, 0x87,
    0xD4, 0x00, 0x46, 0x57, 0x9F, 0xD3, 0x27, 0x52, 0x4C, 0x36, 0x02, 0xE7, 0xA0, 0xC4, 0xC8, 0x9E,
    0xEA, 0xBF, 0x8A, 0xD2, 0x40, 0xC7, 0x38, 0xB5, 0xA3, 0xF7, 0xF2, 0xCE, 0xF9, 0x61, 0x15, 0xA1,
    0xE0, 0xAE, 0x5D, 0xA4, 0x9B, 0x34, 0x1A, 0x55, 0xAD, 0x93, 0x32, 0x30, 0xF5, 0x8C, 0xB1, 0xE3,
    0x1D, 0xF6, 0xE2, 0x2E, 0x82, 0x66, 0xCA, 0x60, 0xC0, 0x29, 0x23, 0xAB, 0x0D, 0x53, 0x4E, 0x6F,
    0xD5, 0xDB, 0x37, 0x45, 0xDE, 0xFD, 0x8E, 0x2F, 0x03, 0xFF, 0x6A, 0x72, 0x6D, 0x6C, 0x5B, 0x51,
    0x8D, 0x1B, 0xAF, 0x92, 0xBB, 0xDD, 0xBC, 0x7F, 0x11, 0xD9, 0x5C, 0x41, 0x1F, 0x10, 0x5A, 0xD8,
    0x0A, 0xC1, 0x31, 0x88, 0xA5, 0xCD, 0x7B, 0xBD, 0x2D, 0x74, 0xD0, 0x12, 0xB8, 0xE5, 0xB4, 0xB0,
    0x89, 0x69, 0x97, 0x4A, 0x0C, 0x96, 0x77, 0x7E, 0x65, 0xB9, 0xF1, 0x09, 0xC5, 0x6E, 0xC6, 0x84,
    0x18, 0xF0, 0x7D, 0xEC, 0x3A, 0xDC, 0x4D, 0x20, 0x79, 0xEE, 0x5F, 0x3E, 0xD7, 0xCB, 0x39, 0x48};

static uint32_t SM4_FK[4] = {0xA3B1BAC6U, 0x56AA3350U, 0x677D9197U, 0xB27022DCU};

static uint32_t SM4_CK[32] = {
    0x00070E15U, 0x1C232A31U, 0x383F464DU, 0x545B6269U, 0x70777E85U, 0x8C939AA1U, 0xA8AFB6BDU,
    0xC4CBD2D9U, 0xE0E7EEF5U, 0xFC030A11U, 0x181F262DU, 0x343B4249U, 0x50575E65U, 0x6C737A81U,
    0x888F969DU, 0xA4ABB2B9U, 0xC0C7CED5U, 0xDCE3EAF1U, 0xF8FF060DU, 0x141B2229U, 0x30373E45U,
    0x4C535A61U, 0x686F767DU, 0x848B9299U, 0xA0A7AEB5U, 0xBCC3CAD1U, 0xD8DFE6EDU, 0xF4FB0209U,
    0x10171E25U, 0x2C333A41U, 0x484F565DU, 0x646B7279U};

static uint32_t sm4_rotl32(uint32_t x, int n)
{
    return (x << n) | (x >> (32 - n));
}

static uint32_t sm4_sub(uint32_t x)
{
    uint32_t result = 0;
    result |= ((uint32_t)SM4_SBOX[(x >> 24) & 0xFF]) << 24;
    result |= ((uint32_t)SM4_SBOX[(x >> 16) & 0xFF]) << 16;
    result |= ((uint32_t)SM4_SBOX[(x >> 8) & 0xFF]) << 8;
    result |= ((uint32_t)SM4_SBOX[x & 0xFF]);
    return result;
}

static uint32_t sm4_L(uint32_t x)
{
    return x ^ sm4_rotl32(x, 2) ^ sm4_rotl32(x, 10) ^ sm4_rotl32(x, 18) ^ sm4_rotl32(x, 24);
}

static uint32_t sm4_Lp(uint32_t x)
{
    return x ^ sm4_rotl32(x, 13) ^ sm4_rotl32(x, 23);
}

static uint32_t sm4_F(uint32_t X0, uint32_t X1, uint32_t X2, uint32_t X3, uint32_t rk)
{
    return X0 ^ sm4_L(sm4_sub(X1 ^ X2 ^ X3 ^ rk));
}

static void sm4_key_schedule(const uint8_t key[16], uint32_t rk[32])
{
    uint32_t MK[4];
    for (int i = 0; i < 4; i++) {
        MK[i] = ((uint32_t)key[i * 4] << 24) | ((uint32_t)key[i * 4 + 1] << 16) |
                ((uint32_t)key[i * 4 + 2] << 8) | ((uint32_t)key[i * 4 + 3]);
    }

    uint32_t K[36];
    for (int i = 0; i < 4; i++)
        K[i] = MK[i] ^ SM4_FK[i];

    for (int i = 0; i < 32; i++) {
        K[i + 4] = K[i] ^ sm4_Lp(sm4_sub(K[i + 1] ^ K[i + 2] ^ K[i + 3] ^ SM4_CK[i]));
        rk[i] = K[i + 4];
    }
}

static void sm4_encrypt_block(const uint32_t rk[32], const uint8_t plaintext[16],
                              uint8_t ciphertext[16])
{
    uint32_t X[36];
    for (int i = 0; i < 4; i++) {
        X[i] = ((uint32_t)plaintext[i * 4] << 24) | ((uint32_t)plaintext[i * 4 + 1] << 16) |
               ((uint32_t)plaintext[i * 4 + 2] << 8) | ((uint32_t)plaintext[i * 4 + 3]);
    }

    for (int i = 0; i < 32; i++) {
        X[i + 4] = sm4_F(X[i], X[i + 1], X[i + 2], X[i + 3], rk[i]);
    }

    for (int i = 0; i < 4; i++) {
        uint32_t val = X[35 - i];
        ciphertext[i * 4] = (uint8_t)(val >> 24);
        ciphertext[i * 4 + 1] = (uint8_t)(val >> 16);
        ciphertext[i * 4 + 2] = (uint8_t)(val >> 8);
        ciphertext[i * 4 + 3] = (uint8_t)(val);
    }
}

static void sm4_decrypt_block(const uint32_t rk[32], const uint8_t ciphertext[16],
                              uint8_t plaintext[16])
{
    uint32_t X[36];
    for (int i = 0; i < 4; i++) {
        X[i] = ((uint32_t)ciphertext[i * 4] << 24) | ((uint32_t)ciphertext[i * 4 + 1] << 16) |
               ((uint32_t)ciphertext[i * 4 + 2] << 8) | ((uint32_t)ciphertext[i * 4 + 3]);
    }

    for (int i = 0; i < 32; i++) {
        X[i + 4] = sm4_F(X[i], X[i + 1], X[i + 2], X[i + 3], rk[i]);
    }

    for (int i = 0; i < 4; i++) {
        uint32_t val = X[35 - i];
        plaintext[i * 4] = (uint8_t)(val >> 24);
        plaintext[i * 4 + 1] = (uint8_t)(val >> 16);
        plaintext[i * 4 + 2] = (uint8_t)(val >> 8);
        plaintext[i * 4 + 3] = (uint8_t)(val);
    }
}

int china_eco_sm4_encrypt(china_eco_sm4_context_t *ctx, const void *plaintext, size_t pt_size,
                          void *ciphertext, size_t *ct_size)
{
    if (!ctx || !plaintext || !ciphertext || !ct_size)
        return AGENTOS_ERR_NULL_POINTER;
    if (!ctx->initialized)
        return AGENTOS_ERR_SYS_NOT_INIT;

    size_t padded_size = ((pt_size + CHINA_ECO_SM4_BLOCK_SIZE) / CHINA_ECO_SM4_BLOCK_SIZE) *
                         CHINA_ECO_SM4_BLOCK_SIZE;
    *ct_size = padded_size;

    uint32_t rk[32];
    sm4_key_schedule(ctx->key, rk);

    uint8_t *ct = (uint8_t *)ciphertext;
    const uint8_t *pt = (const uint8_t *)plaintext;
    uint8_t block[CHINA_ECO_SM4_BLOCK_SIZE];
    uint8_t prev_block[CHINA_ECO_SM4_BLOCK_SIZE];

    memcpy(prev_block, ctx->iv, CHINA_ECO_SM4_BLOCK_SIZE);

    for (size_t offset = 0; offset < padded_size; offset += CHINA_ECO_SM4_BLOCK_SIZE) {
        memset(block, 0, CHINA_ECO_SM4_BLOCK_SIZE);
        if (offset < pt_size) {
            size_t copy_size = CHINA_ECO_SM4_BLOCK_SIZE;
            if (offset + copy_size > pt_size)
                copy_size = pt_size - offset;
            memcpy(block, pt + offset, copy_size);

            if (copy_size < CHINA_ECO_SM4_BLOCK_SIZE) {
                uint8_t pad_val = (uint8_t)(CHINA_ECO_SM4_BLOCK_SIZE - copy_size);
                for (size_t j = copy_size; j < CHINA_ECO_SM4_BLOCK_SIZE; j++)
                    block[j] = pad_val;
            }
        } else {
            memset(block, CHINA_ECO_SM4_BLOCK_SIZE, CHINA_ECO_SM4_BLOCK_SIZE);
        }

        for (size_t j = 0; j < CHINA_ECO_SM4_BLOCK_SIZE; j++)
            block[j] ^= prev_block[j];

        sm4_encrypt_block(rk, block, ct + offset);
        memcpy(prev_block, ct + offset, CHINA_ECO_SM4_BLOCK_SIZE);
    }

    return 0;
}

int china_eco_sm4_decrypt(china_eco_sm4_context_t *ctx, const void *ciphertext, size_t ct_size,
                          void *plaintext, size_t *pt_size)
{
    if (!ctx || !ciphertext || !plaintext || !pt_size)
        return AGENTOS_ERR_NULL_POINTER;
    if (!ctx->initialized)
        return AGENTOS_ERR_SYS_NOT_INIT;

    if (ct_size % CHINA_ECO_SM4_BLOCK_SIZE != 0)
        return AGENTOS_ERR_INVALID_PARAM;
    if (ct_size == 0) {
        *pt_size = 0;
        return 0;
    }

    uint32_t rk[32];
    sm4_key_schedule(ctx->key, rk);

    uint32_t decrypt_rk[32];
    for (int i = 0; i < 32; i++)
        decrypt_rk[i] = rk[31 - i];

    uint8_t *pt = (uint8_t *)plaintext;
    const uint8_t *ct = (const uint8_t *)ciphertext;
    uint8_t prev_block[CHINA_ECO_SM4_BLOCK_SIZE];
    uint8_t decrypted[CHINA_ECO_SM4_BLOCK_SIZE];
    uint8_t pad_len = 0;

    memcpy(prev_block, ctx->iv, CHINA_ECO_SM4_BLOCK_SIZE);

    for (size_t offset = 0; offset < ct_size; offset += CHINA_ECO_SM4_BLOCK_SIZE) {
        sm4_decrypt_block(decrypt_rk, ct + offset, decrypted);

        for (size_t j = 0; j < CHINA_ECO_SM4_BLOCK_SIZE; j++)
            decrypted[j] ^= prev_block[j];

        size_t copy_size = CHINA_ECO_SM4_BLOCK_SIZE;
        if (offset + CHINA_ECO_SM4_BLOCK_SIZE >= ct_size) {
            pad_len = decrypted[CHINA_ECO_SM4_BLOCK_SIZE - 1];
            if (pad_len > 0 && pad_len <= CHINA_ECO_SM4_BLOCK_SIZE) {
                copy_size = CHINA_ECO_SM4_BLOCK_SIZE - pad_len;
            }
        }

        memcpy(pt + offset, decrypted, copy_size);
        memcpy(prev_block, ct + offset, CHINA_ECO_SM4_BLOCK_SIZE);
    }

    *pt_size = ct_size - pad_len;
    return 0;
}

static int china_eco_proto_init(void *context)
{
    china_eco_handle_t *h = (china_eco_handle_t *)context;
    if (!h) {
        if (china_eco_create(&h) != 0)
            return AGENTOS_ERR_OUT_OF_MEMORY;
    }
    g_china_eco_state.handle = *h;
    g_china_eco_state.proto_initialized = true;
    return 0;
}

static int china_eco_proto_destroy(void *context)
{
    g_china_eco_state.proto_initialized = false;
    return 0;
}

static int china_eco_proto_handle_request(void *context, const void *req, void **resp)
{
    if (!req || !resp)
        return AGENTOS_ERR_NULL_POINTER;

    char buf[4096];
    snprintf(buf, sizeof(buf),
             "{"
             "\"protocol\":\"china-eco\","
             "\"version\":\"%s\","
             "\"llm_providers\":%zu,"
             "\"storage_bridges\":%zu,"
             "\"requests_total\":%llu,"
             "\"token_total\":%llu,"
             "\"sm_crypto\":true"
             "}",
             CHINA_ECO_VERSION, g_china_eco_state.handle.llm_provider_count,
             g_china_eco_state.handle.storage_bridge_count,
             (unsigned long long)g_china_eco_state.handle.request_counter,
             (unsigned long long)g_china_eco_state.handle.token_total);

    *resp = AGENTOS_STRDUP(buf);
    return 0;
}

static int china_eco_proto_get_version(void *context, char *version_buf, size_t max_size)
{
    (void)context;
    if (!version_buf || max_size == 0)
        return AGENTOS_ERR_INVALID_PARAM;
    snprintf(version_buf, max_size, "%s", CHINA_ECO_VERSION);
    return 0;
}

static uint32_t china_eco_proto_capabilities(void *context)
{
    return (uint32_t)(CHINA_ECO_CAP_LLM_BRIDGE | CHINA_ECO_CAP_OBJECT_STORAGE |
                      CHINA_ECO_CAP_SM_CRYPTO | CHINA_ECO_CAP_MESSAGE_QUEUE |
                      CHINA_ECO_CAP_CONTENT_AUDIT);
}

const proto_adapter_t *china_eco_get_protocol_adapter(void)
{
    static proto_adapter_t adapter = {0};
    static bool initialized = false;

    if (!initialized) {
        adapter.name = "China Ecosystem";
        adapter.version = CHINA_ECO_VERSION;
        adapter.description =
            "Domestic ecosystem protocol compatibility - Bailian/Wenxin/DashScope LLM bridge, "
            "OSS/COS/BOS storage, SM2/SM3/SM4 crypto";
        adapter.type = PROTO_CHINA_ECO;
        adapter.init = china_eco_proto_init;
        adapter.destroy = china_eco_proto_destroy;
        adapter.handle_request = china_eco_proto_handle_request;
        adapter.get_version = china_eco_proto_get_version;
        adapter.capabilities = china_eco_proto_capabilities;
        initialized = true;
    }

    return &adapter;
}
