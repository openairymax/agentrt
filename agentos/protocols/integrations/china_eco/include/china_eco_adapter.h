// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: Apache-2.0
/**
 * @file china_eco_adapter.h
 * @brief China Domestic Ecosystem Protocol Adapter for AgentOS
 *
 * 国内生态协议兼容适配器，提供：
 * 1. LLM Provider Bridge — 百炼(Bailian)/文心(Wenxin)/DashScope等国内平台OpenAI兼容层
 * 2. Object Storage Bridge — 阿里云OSS / 腾讯云COS / 百度云BOS的统一对象存储适配
 * 3. SM Crypto Suite — 国产加密算法SM2/SM3/SM4的OpenSSL delegate支持
 * 4. Message Queue Bridge — RocketMQ / Pulsar 消息队列协议映射
 *
 * @since 3.0.0
 * @see unified_protocol.h
 */

#ifndef AGENTOS_CHINA_ECO_ADAPTER_H
#define AGENTOS_CHINA_ECO_ADAPTER_H

#include "unified_protocol.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CHINA_ECO_VERSION                "0.1.0"
#define CHINA_ECO_PROTOCOL_NAME          "china-eco"
#define CHINA_ECO_MAX_PROVIDERS          16
#define CHINA_ECO_MAX_ENDPOINTS          64
#define CHINA_ECO_MAX_MESSAGE_SIZE       (16 * 1024 * 1024)
#define CHINA_ECO_DEFAULT_TIMEOUT_MS     30000

#define CHINA_ECO_SM2_KEY_SIZE           64
#define CHINA_ECO_SM3_HASH_SIZE          32
#define CHINA_ECO_SM3_DIGEST_SIZE        32
#define CHINA_ECO_SM4_BLOCK_SIZE         16
#define CHINA_ECO_SM4_KEY_SIZE           16
#define CHINA_ECO_SM4_IV_SIZE            16

typedef enum {
    CHINA_ECO_PROVIDER_BAILIAN    = 0,
    CHINA_ECO_PROVIDER_WENXIN     = 1,
    CHINA_ECO_PROVIDER_DASHSCOPE  = 2,
    CHINA_ECO_PROVIDER_ZHIPU      = 3,
    CHINA_ECO_PROVIDER_MINIMAX    = 4,
    CHINA_ECO_PROVIDER_MOONSHOT   = 5,
    CHINA_ECO_PROVIDER_DEEPSEEK   = 6,
    CHINA_ECO_PROVIDER_QWEN       = 7
} china_eco_provider_type_t;

typedef enum {
    CHINA_ECO_CAP_LLM_BRIDGE      = 0x01,
    CHINA_ECO_CAP_OBJECT_STORAGE   = 0x02,
    CHINA_ECO_CAP_SM_CRYPTO       = 0x04,
    CHINA_ECO_CAP_MESSAGE_QUEUE   = 0x08,
    CHINA_ECO_CAP_CONTENT_AUDIT   = 0x10
} china_eco_capability_t;

typedef enum {
    CHINA_ECO_OSS_ALIYUN  = 0,
    CHINA_ECO_OSS_TENCENT = 1,
    CHINA_ECO_OSS_BAIDU   = 2,
    CHINA_ECO_OSS_HUAWEI  = 3
} china_eco_storage_type_t;

typedef struct {
    china_eco_provider_type_t provider_type;
    char provider_id[32];
    char api_base_url[512];
    char api_key[256];
    char model_id[128];
    bool enabled;
    uint64_t rate_rpm;
    uint64_t rate_tpm;
} china_eco_llm_provider_t;

typedef struct {
    china_eco_storage_type_t storage_type;
    char endpoint_url[512];
    char access_key_id[128];
    char access_key_secret[256];
    char bucket_name[128];
    char region[64];
    bool enabled;
} china_eco_storage_bridge_t;

typedef struct {
    uint8_t public_key[CHINA_ECO_SM2_KEY_SIZE];
    uint8_t private_key[CHINA_ECO_SM2_KEY_SIZE];
    bool has_key_pair;
} china_eco_sm2_context_t;

typedef struct {
    uint8_t key[CHINA_ECO_SM4_KEY_SIZE];
    uint8_t iv[CHINA_ECO_SM4_IV_SIZE];
    bool initialized;
} china_eco_sm4_context_t;

typedef struct {
    china_eco_llm_provider_t llm_providers[CHINA_ECO_MAX_PROVIDERS];
    size_t llm_provider_count;
    china_eco_storage_bridge_t storage_bridges[CHINA_ECO_MAX_ENDPOINTS];
    size_t storage_bridge_count;
    china_eco_sm2_context_t sm2_ctx;
    china_eco_sm4_context_t sm4_ctx;
    uint64_t request_counter;
    uint64_t token_total;
    bool initialized;
} china_eco_handle_t;

int china_eco_create(china_eco_handle_t** handle);
void china_eco_destroy(china_eco_handle_t* handle);

int china_eco_add_llm_provider(china_eco_handle_t* h,
                                const china_eco_llm_provider_t* provider);
int china_eco_remove_llm_provider(china_eco_handle_t* h,
                                   china_eco_provider_type_t type);

int china_eco_llm_chat(china_eco_handle_t* h, china_eco_provider_type_t provider,
                       const char* messages_json,
                       const char* model_id,
                       char* response, size_t* resp_size);

int china_eco_add_storage_bridge(china_eco_handle_t* h,
                                  const china_eco_storage_bridge_t* bridge);
int china_eco_storage_upload(china_eco_handle_t* h,
                              china_eco_storage_type_t storage_type,
                              const void* data, size_t size,
                              const char* object_key,
                              char* result_url, size_t* url_size);
int china_eco_storage_download(china_eco_handle_t* h,
                                china_eco_storage_type_t storage_type,
                                const char* object_key,
                                void** data, size_t* size);

int china_eco_sm3_hash(const void* data, size_t size,
                       uint8_t digest[CHINA_ECO_SM3_DIGEST_SIZE]);
int china_eco_sm4_encrypt(china_eco_sm4_context_t* ctx,
                           const void* plaintext, size_t pt_size,
                           void* ciphertext, size_t* ct_size);
int china_eco_sm4_decrypt(china_eco_sm4_context_t* ctx,
                           const void* ciphertext, size_t ct_size,
                           void* plaintext, size_t* pt_size);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_CHINA_ECO_ADAPTER_H */
