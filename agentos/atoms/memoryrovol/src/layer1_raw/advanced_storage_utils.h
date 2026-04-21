/**
 * @file advanced_storage_utils.h
 * @brief L1 增强存储工具函数 - 压缩、加密、完整性校验
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_ADVANCED_STORAGE_UTILS_H
#define AGENTOS_ADVANCED_STORAGE_UTILS_H

#include "../../../atoms/corekern/include/agentos.h"
#include <stdint.h>
#include <stddef.h>

/**
 * @brief 压缩算法类型
 */
typedef enum {
    COMPRESSION_NONE = 0,           /**< 无压缩 */
    COMPRESSION_LZ4,                /**< LZ4 快速压缩 */
    COMPRESSION_SNAPPY,             /**< Snappy 压缩 */
    COMPRESSION_ZSTD,               /**< Zstd 高压缩比 */
    COMPRESSION_ZLIB                /**< Zlib 兼容压缩 */
} compression_algorithm_t;

/**
 * @brief 加密算法类型
 */
typedef enum {
    ENCRYPTION_NONE = 0,            /**< 无加密 */
    ENCRYPTION_AES_128_GCM,         /**< AES-128-GCM */
    ENCRYPTION_AES_256_GCM,         /**< AES-256-GCM */
    ENCRYPTION_CHACHA20_POLY1305    /**< ChaCha20-Poly1305 */
} encryption_algorithm_t;

/**
 * @brief 生成数据完整性哈希
 * @param data 输入数据
 * @param data_len 数据长度
 * @param out_hash 输出哈希（需要调用者释放）
 * @return AGENTOS_SUCCESS 成功，其他为错误码
 */
agentos_error_t advanced_storage_generate_hash(const void* data, size_t data_len, char** out_hash);

/**
 * @brief 验证数据完整性
 * @param data 数据
 * @param data_len 数据长度
 * @param expected_hash 期望的哈希值
 * @return 1 表示验证通过，0 表示失败
 */
int advanced_storage_verify_hash(const void* data, size_t data_len, const char* expected_hash);

/**
 * @brief 压缩数据
 * @param data 输入数据
 * @param data_len 数据长度
 * @param algorithm 压缩算法
 * @param level 压缩级别
 * @param out_compressed 输出压缩数据（需要调用者释放）
 * @param out_compressed_len 输出压缩后长度
 * @return AGENTOS_SUCCESS 成功，其他为错误码
 */
agentos_error_t advanced_storage_compress(const void* data, size_t data_len,
                                         compression_algorithm_t algorithm, int level,
                                         void** out_compressed, size_t* out_compressed_len);

/**
 * @brief 解压数据
 * @param compressed_data 压缩数据
 * @param compressed_len 压缩后长度
 * @param algorithm 压缩算法
 * @param original_len 原始长度（如果已知）
 * @param out_data 输出解压数据（需要调用者释放）
 * @param out_data_len 输出解压后长度
 * @return AGENTOS_SUCCESS 成功，其他为错误码
 */
agentos_error_t advanced_storage_decompress(const void* compressed_data, size_t compressed_len,
                                           compression_algorithm_t algorithm, size_t original_len,
                                           void** out_data, size_t* out_data_len);

/**
 * @brief 加密数据
 * @param plaintext 明文数据
 * @param plaintext_len 明文长度
 * @param algorithm 加密算法
 * @param key 加密密钥
 * @param key_len 密钥长度
 * @param iv 初始化向量
 * @param iv_len IV 长度
 * @param out_ciphertext 输出密文（需要调用者释放）
 * @param out_ciphertext_len 输出密文长度
 * @param out_tag 输出认证标签（需要调用者释放）
 * @param out_tag_len 输出标签长度
 * @return AGENTOS_SUCCESS 成功，其他为错误码
 */
agentos_error_t advanced_storage_encrypt(const void* plaintext, size_t plaintext_len,
                                        encryption_algorithm_t algorithm,
                                        const uint8_t* key, size_t key_len,
                                        const uint8_t* iv, size_t iv_len,
                                        void** out_ciphertext, size_t* out_ciphertext_len,
                                        uint8_t** out_tag, size_t* out_tag_len);

/**
 * @brief 解密数据
 * @param ciphertext 密文数据
 * @param ciphertext_len 密文长度
 * @param algorithm 加密算法
 * @param key 加密密钥
 * @param key_len 密钥长度
 * @param iv 初始化向量
 * @param iv_len IV 长度
 * @param tag 认证标签
 * @param tag_len 标签长度
 * @param out_plaintext 输出明文（需要调用者释放）
 * @param out_plaintext_len 输出明文长度
 * @return AGENTOS_SUCCESS 成功，其他为错误码
 */
agentos_error_t advanced_storage_decrypt(const void* ciphertext, size_t ciphertext_len,
                                        encryption_algorithm_t algorithm,
                                        const uint8_t* key, size_t key_len,
                                        const uint8_t* iv, size_t iv_len,
                                        const uint8_t* tag, size_t tag_len,
                                        void** out_plaintext, size_t* out_plaintext_len);

#endif /* AGENTOS_ADVANCED_STORAGE_UTILS_H */
