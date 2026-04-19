/**
 * @file advanced_storage_utils.c
 * @brief L1 增强存储工具函数实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "advanced_storage_utils.h"
#include "agentos.h"
#include "logger.h"
#include <string.h>
#include <stdio.h>

/* 基础库兼容性层 */
#include "include/memory_compat.h"

/* 压缩库 */
#include <zstd.h>

/* 加密库 */
#include <openssl/evp.h>

/* ==================== 哈希函数 ==================== */

agentos_error_t advanced_storage_generate_hash(const void* data, size_t data_len, char** out_hash) {
    if (!data || data_len == 0 || !out_hash) return AGENTOS_EINVAL;

    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    if (!mdctx) {
        AGENTOS_LOG_ERROR("Failed to create EVP_MD_CTX");
        return AGENTOS_ENOMEM;
    }

    if (EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL) != 1) {
        AGENTOS_LOG_ERROR("Failed to initialize digest");
        EVP_MD_CTX_free(mdctx);
        return AGENTOS_EINVAL;
    }

    if (EVP_DigestUpdate(mdctx, data, data_len) != 1) {
        AGENTOS_LOG_ERROR("Failed to update digest");
        EVP_MD_CTX_free(mdctx);
        return AGENTOS_EINVAL;
    }

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len = 0;
    if (EVP_DigestFinal_ex(mdctx, hash, &hash_len) != 1) {
        AGENTOS_LOG_ERROR("Failed to finalize digest");
        EVP_MD_CTX_free(mdctx);
        return AGENTOS_EINVAL;
    }

    EVP_MD_CTX_free(mdctx);

    /* 转换为十六进制字符串 */
    char* hex_hash = (char*)AGENTOS_MALLOC(hash_len * 2 + 1);
    if (!hex_hash) {
        AGENTOS_LOG_ERROR("Failed to allocate hex hash buffer");
        return AGENTOS_ENOMEM;
    }

    for (unsigned int i = 0; i < hash_len; i++) {
        snprintf(hex_hash + i * 2, 3, "%02x", hash[i]);
    }
    hex_hash[hash_len * 2] = '\0';

    *out_hash = hex_hash;
    return AGENTOS_SUCCESS;
}

int advanced_storage_verify_hash(const void* data, size_t data_len, const char* expected_hash) {
    if (!data || !expected_hash) return 0;

    char* actual_hash = NULL;
    agentos_error_t err = advanced_storage_generate_hash(data, data_len, &actual_hash);
    if (err != AGENTOS_SUCCESS) {
        AGENTOS_LOG_ERROR("Failed to generate integrity hash for verification");
        return 0;
    }

    int result = (strcmp(actual_hash, expected_hash) == 0);
    AGENTOS_FREE(actual_hash);

    if (!result) {
        AGENTOS_LOG_WARN("Data integrity verification failed");
    }

    return result;
}

/* ==================== 压缩函数 ==================== */

agentos_error_t advanced_storage_compress(const void* data, size_t data_len,
                                         compression_algorithm_t algorithm, int level,
                                         void** out_compressed, size_t* out_compressed_len) {
    if (!data || data_len == 0 || !out_compressed || !out_compressed_len) {
        return AGENTOS_EINVAL;
    }

    /* 对于小数据，直接复制而不压缩 */
    if (data_len < 128) {
        void* copy = AGENTOS_MALLOC(data_len);
        if (!copy) return AGENTOS_ENOMEM;
        memcpy(copy, data, data_len);
        *out_compressed = copy;
        *out_compressed_len = data_len;
        return AGENTOS_SUCCESS;
    }

    switch (algorithm) {
        case COMPRESSION_ZSTD: {
            size_t max_compressed_size = ZSTD_compressBound(data_len);
            void* compressed = AGENTOS_MALLOC(max_compressed_size);
            if (!compressed) return AGENTOS_ENOMEM;

            size_t compressed_size = ZSTD_compress(compressed, max_compressed_size,
                                                  data, data_len, level);
            if (ZSTD_isError(compressed_size)) {
                AGENTOS_FREE(compressed);
                AGENTOS_LOG_ERROR("ZSTD compression failed: %s", ZSTD_getErrorName(compressed_size));
                return AGENTOS_EINVAL;
            }

            /* 重新分配内存以节省空间 */
            void* optimized = AGENTOS_REALLOC(compressed, compressed_size);
            if (!optimized) {
                AGENTOS_FREE(compressed);
                return AGENTOS_ENOMEM;
            }

            *out_compressed = optimized;
            *out_compressed_len = compressed_size;
            break;
        }

        case COMPRESSION_NONE:
        default: {
            void* copy = AGENTOS_MALLOC(data_len);
            if (!copy) return AGENTOS_ENOMEM;
            memcpy(copy, data, data_len);
            *out_compressed = copy;
            *out_compressed_len = data_len;
            break;
        }
    }

    return AGENTOS_SUCCESS;
}

agentos_error_t advanced_storage_decompress(const void* compressed_data, size_t compressed_len,
                                           compression_algorithm_t algorithm, size_t original_len,
                                           void** out_data, size_t* out_data_len) {
    if (!compressed_data || compressed_len == 0 || !out_data || !out_data_len) {
        return AGENTOS_EINVAL;
    }

    switch (algorithm) {
        case COMPRESSION_ZSTD: {
            size_t decompressed_size = original_len > 0 ? original_len : ZSTD_getFrameContentSize(compressed_data, compressed_len);
            if (decompressed_size == ZSTD_CONTENTSIZE_ERROR || decompressed_size == ZSTD_CONTENTSIZE_UNKNOWN) {
                /* 尝试猜测大小 */
                decompressed_size = compressed_len * 4;  /* 保守估计 */
            }

            void* decompressed = AGENTOS_MALLOC(decompressed_size);
            if (!decompressed) return AGENTOS_ENOMEM;

            size_t actual_size = ZSTD_decompress(decompressed, decompressed_size,
                                                compressed_data, compressed_len);
            if (ZSTD_isError(actual_size)) {
                AGENTOS_FREE(decompressed);
                AGENTOS_LOG_ERROR("ZSTD decompression failed: %s", ZSTD_getErrorName(actual_size));
                return AGENTOS_EINVAL;
            }

            /* 重新分配内存以节省空间 */
            void* optimized = AGENTOS_REALLOC(decompressed, actual_size);
            if (!optimized) {
                AGENTOS_FREE(decompressed);
                return AGENTOS_ENOMEM;
            }

            *out_data = optimized;
            *out_data_len = actual_size;
            break;
        }

        case COMPRESSION_NONE:
        default: {
            void* copy = AGENTOS_MALLOC(compressed_len);
            if (!copy) return AGENTOS_ENOMEM;
            memcpy(copy, compressed_data, compressed_len);
            *out_data = copy;
            *out_data_len = compressed_len;
            break;
        }
    }

    return AGENTOS_SUCCESS;
}

/* ==================== 加密函数 ==================== */

agentos_error_t advanced_storage_encrypt(const void* plaintext, size_t plaintext_len,
                                        encryption_algorithm_t algorithm,
                                        const uint8_t* key, size_t key_len,
                                        const uint8_t* iv, size_t iv_len,
                                        void** out_ciphertext, size_t* out_ciphertext_len,
                                        uint8_t** out_tag, size_t* out_tag_len) {
    if (!plaintext || plaintext_len == 0 || !key || !iv || !out_ciphertext || !out_ciphertext_len) {
        return AGENTOS_EINVAL;
    }

    if (algorithm == ENCRYPTION_NONE) {
        void* copy = AGENTOS_MALLOC(plaintext_len);
        if (!copy) return AGENTOS_ENOMEM;
        memcpy(copy, plaintext, plaintext_len);
        *out_ciphertext = copy;
        *out_ciphertext_len = plaintext_len;
        if (out_tag) *out_tag = NULL;
        if (out_tag_len) *out_tag_len = 0;
        return AGENTOS_SUCCESS;
    }

    if (algorithm == ENCRYPTION_AES_256_GCM) {
        if (key_len != 32) {
            AGENTOS_LOG_ERROR("AES-256-GCM requires 32-byte key");
            return AGENTOS_EINVAL;
        }
        if (iv_len != 12) {
            AGENTOS_LOG_ERROR("AES-256-GCM requires 12-byte IV");
            return AGENTOS_EINVAL;
        }

        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        if (!ctx) {
            AGENTOS_LOG_ERROR("Failed to create EVP_CIPHER_CTX");
            return AGENTOS_ENOMEM;
        }

        /* 初始化加密操作 */
        if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, key, iv) != 1) {
            AGENTOS_LOG_ERROR("Failed to initialize encryption");
            EVP_CIPHER_CTX_free(ctx);
            return AGENTOS_EINVAL;
        }

        /* 分配密文缓冲区（明文长度 + 块大小） */
        size_t ciphertext_len = plaintext_len + EVP_CIPHER_CTX_block_size(ctx);
        void* ciphertext = AGENTOS_MALLOC(ciphertext_len);
        if (!ciphertext) {
            EVP_CIPHER_CTX_free(ctx);
            return AGENTOS_ENOMEM;
        }

        int len = 0;
        if (EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, plaintext_len) != 1) {
            AGENTOS_LOG_ERROR("Failed to encrypt data");
            AGENTOS_FREE(ciphertext);
            EVP_CIPHER_CTX_free(ctx);
            return AGENTOS_EINVAL;
        }
        ciphertext_len = len;

        /* 完成加密 */
        if (EVP_EncryptFinal_ex(ctx, (unsigned char*)ciphertext + len, &len) != 1) {
            AGENTOS_LOG_ERROR("Failed to finalize encryption");
            AGENTOS_FREE(ciphertext);
            EVP_CIPHER_CTX_free(ctx);
            return AGENTOS_EINVAL;
        }
        ciphertext_len += len;

        /* 获取认证标签 */
        uint8_t* tag = NULL;
        size_t tag_len = 16;  /* GCM 标签长度 */
        if (out_tag && out_tag_len) {
            tag = (uint8_t*)AGENTOS_MALLOC(tag_len);
            if (!tag) {
                AGENTOS_FREE(ciphertext);
                EVP_CIPHER_CTX_free(ctx);
                return AGENTOS_ENOMEM;
            }

            if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, tag_len, tag) != 1) {
                AGENTOS_LOG_ERROR("Failed to get authentication tag");
                AGENTOS_FREE(ciphertext);
                AGENTOS_FREE(tag);
                EVP_CIPHER_CTX_free(ctx);
                return AGENTOS_EINVAL;
            }

            *out_tag = tag;
            *out_tag_len = tag_len;
        }

        EVP_CIPHER_CTX_free(ctx);

        /* 重新分配内存以节省空间 */
        void* optimized = AGENTOS_REALLOC(ciphertext, ciphertext_len);
        if (!optimized) {
            AGENTOS_FREE(ciphertext);
            if (tag) AGENTOS_FREE(tag);
            return AGENTOS_ENOMEM;
        }

        *out_ciphertext = optimized;
        *out_ciphertext_len = ciphertext_len;
        return AGENTOS_SUCCESS;
    }

    AGENTOS_LOG_ERROR("Unsupported encryption algorithm: %d", algorithm);
    return AGENTOS_ENOTSUP;
}

agentos_error_t advanced_storage_decrypt(const void* ciphertext, size_t ciphertext_len,
                                        encryption_algorithm_t algorithm,
                                        const uint8_t* key, size_t key_len,
                                        const uint8_t* iv, size_t iv_len,
                                        const uint8_t* tag, size_t tag_len,
                                        void** out_plaintext, size_t* out_plaintext_len) {
    if (!ciphertext || ciphertext_len == 0 || !key || !iv || !out_plaintext || !out_plaintext_len) {
        return AGENTOS_EINVAL;
    }

    if (algorithm == ENCRYPTION_NONE) {
        void* copy = AGENTOS_MALLOC(ciphertext_len);
        if (!copy) return AGENTOS_ENOMEM;
        memcpy(copy, ciphertext, ciphertext_len);
        *out_plaintext = copy;
        *out_plaintext_len = ciphertext_len;
        return AGENTOS_SUCCESS;
    }

    if (algorithm == ENCRYPTION_AES_256_GCM) {
        if (key_len != 32) {
            AGENTOS_LOG_ERROR("AES-256-GCM requires 32-byte key");
            return AGENTOS_EINVAL;
        }
        if (iv_len != 12) {
            AGENTOS_LOG_ERROR("AES-256-GCM requires 12-byte IV");
            return AGENTOS_EINVAL;
        }
        if (!tag || tag_len != 16) {
            AGENTOS_LOG_ERROR("AES-256-GCM requires 16-byte tag");
            return AGENTOS_EINVAL;
        }

        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        if (!ctx) {
            AGENTOS_LOG_ERROR("Failed to create EVP_CIPHER_CTX");
            return AGENTOS_ENOMEM;
        }

        /* 初始化解密操作 */
        if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, key, iv) != 1) {
            AGENTOS_LOG_ERROR("Failed to initialize decryption");
            EVP_CIPHER_CTX_free(ctx);
            return AGENTOS_EINVAL;
        }

        /* 设置认证标签 */
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, tag_len, (void*)tag) != 1) {
            AGENTOS_LOG_ERROR("Failed to set authentication tag");
            EVP_CIPHER_CTX_free(ctx);
            return AGENTOS_EINVAL;
        }

        /* 分配明文缓冲区 */
        size_t plaintext_len = ciphertext_len + EVP_CIPHER_CTX_block_size(ctx);
        void* plaintext = AGENTOS_MALLOC(plaintext_len);
        if (!plaintext) {
            EVP_CIPHER_CTX_free(ctx);
            return AGENTOS_ENOMEM;
        }

        int len = 0;
        if (EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, ciphertext_len) != 1) {
            AGENTOS_LOG_ERROR("Failed to decrypt data");
            AGENTOS_FREE(plaintext);
            EVP_CIPHER_CTX_free(ctx);
            return AGENTOS_EINVAL;
        }
        plaintext_len = len;

        /* 完成解密 */
        int final_len = 0;
        if (EVP_DecryptFinal_ex(ctx, (unsigned char*)plaintext + len, &final_len) != 1) {
            AGENTOS_LOG_ERROR("Failed to finalize decryption or authentication failed");
            AGENTOS_FREE(plaintext);
            EVP_CIPHER_CTX_free(ctx);
            return AGENTOS_EAUTH;  /* 认证失败 */
        }
        plaintext_len += final_len;

        EVP_CIPHER_CTX_free(ctx);

        /* 重新分配内存以节省空间 */
        void* optimized = AGENTOS_REALLOC(plaintext, plaintext_len);
        if (!optimized) {
            AGENTOS_FREE(plaintext);
            return AGENTOS_ENOMEM;
        }

        *out_plaintext = optimized;
        *out_plaintext_len = plaintext_len;
        return AGENTOS_SUCCESS;
    }

    AGENTOS_LOG_ERROR("Unsupported encryption algorithm: %d", algorithm);
    return AGENTOS_ENOTSUP;
}
