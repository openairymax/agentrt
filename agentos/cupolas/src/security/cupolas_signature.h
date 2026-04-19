/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * cupolas_signature.h - Code Signature Verification: Ensure Agent Code Authenticity and Integrity
 */

#ifndef CUPOLAS_SIGNATURE_H
#define CUPOLAS_SIGNATURE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Signature verification result codes
 * 
 * Design principles:
 * - Multi-layer: Verify code signatures from multiple sources
 * - Algorithm support: RSA, ECDSA, Ed25519
 * - Certificate chain: Validate certificate hierarchy
 * - Anti-tampering: Real-time integrity checking
 */
typedef enum {
    CUPOLAS_SIG_OK = 0,               /**< Signature valid */
    CUPOLAS_SIG_INVALID = -1,         /**< Signature invalid */
    CUPOLAS_SIG_EXPIRED = -2,         /**< Signature expired */
    CUPOLAS_SIG_REVOKED = -3,         /**< Signature revoked */
    CUPOLAS_SIG_UNTRUSTED = -4,       /**< Untrusted signer */
    CUPOLAS_SIG_TAMPERED = -5,        /**< Content tampered */
    CUPOLAS_SIG_NO_SIGNATURE = -6,    /**< No signature present */
    CUPOLAS_SIG_CERT_INVALID = -7,    /**< Certificate invalid */
    CUPOLAS_SIG_CERT_EXPIRED = -8,    /**< Certificate expired */
    CUPOLAS_SIG_ALGO_UNSUPPORTED = -9 /**< Algorithm unsupported */
} cupolas_sig_result_t;

/**
 * @brief Signature algorithm types
 */
typedef enum {
    CUPOLAS_SIG_ALGO_RSA_SHA256 = 1,  /**< RSA with SHA-256 */
    CUPOLAS_SIG_ALGO_RSA_SHA384 = 2,  /**< RSA with SHA-384 */
    CUPOLAS_SIG_ALGO_RSA_SHA512 = 3,  /**< RSA with SHA-512 */
    CUPOLAS_SIG_ALGO_ECDSA_P256 = 4,  /**< ECDSA P-256 */
    CUPOLAS_SIG_ALGO_ECDSA_P384 = 5,  /**< ECDSA P-384 */
    CUPOLAS_SIG_ALGO_ED25519 = 6      /**< Ed25519 */
} cupolas_sig_algo_t;

/**
 * @brief Signer information structure - canonical definition from commons/types/
 */
#include "cupolas_signer_info.h"

/**
 * @brief Code signature verification context (opaque)
 */
typedef struct cupolas_signature cupolas_signature_t;

/**
 * @brief Signature verification configuration
 */
typedef struct {
    bool check_cert_chain;          /**< Verify certificate chain */
    bool check_revocation;          /**< Check revocation status */
    bool check_timestamp;           /**< Verify timestamp */
    bool allow_self_signed;         /**< Allow self-signed */
    bool allow_expired_test;        /**< Allow expired for testing */
    const char* trusted_ca_path;    /**< Trusted CA bundle path */
    const char* crl_path;           /**< CRL distribution path */
    uint32_t max_chain_depth;       /**< Maximum chain depth */
} cupolas_sig_config_t;

/**
 * @brief Initialize signature verification module
 * @param[in] config Configuration (NULL for defaults)
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads (initialization only)
 * @reentrant No
 * @ownership config: caller retains ownership
 */
int cupolas_signature_init(const cupolas_sig_config_t* config);

/**
 * @brief Shutdown signature verification module
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other operations)
 * @reentrant No
 */
void cupolas_signature_cleanup(void);

/**
 * @brief Verify file signature
 * @param[in] file_path Path to file
 * @param[in] expected_signer Expected signer CN (optional, NULL to skip)
 * @param[out] result Verification result
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership file_path and expected_signer: caller retains ownership
 * @ownership result: caller provides buffer, function writes to it
 */
int cupolas_signature_verify_file(const char* file_path,
                                 const char* expected_signer,
                                 cupolas_sig_result_t* result);

/**
 * @brief Verify data signature in memory
 * @param[in] data Data pointer
 * @param[in] data_len Data length
 * @param[in] signature Signature bytes
 * @param[in] sig_len Signature length
 * @param[in] algo Signature algorithm
 * @param[in] public_key Public key (PEM format)
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership All parameters: caller retains ownership
 */
int cupolas_signature_verify_data(const uint8_t* data, size_t data_len,
                                 const uint8_t* signature, size_t sig_len,
                                 cupolas_sig_algo_t algo,
                                 const char* public_key);

/**
 * @brief Verify file integrity (hash check)
 * @param[in] file_path Path to file
 * @param[in] expected_hash Expected hash (SHA-256, 32 bytes)
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership file_path and expected_hash: caller retains ownership
 */
int cupolas_signature_verify_integrity(const char* file_path,
                                      const uint8_t* expected_hash);

/**
 * @brief Compute file hash
 * @param[in] file_path Path to file
 * @param[out] hash_out Output buffer (32 bytes)
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership file_path: caller retains ownership
 * @ownership hash_out: caller provides buffer, function writes to it
 */
int cupolas_signature_compute_hash(const char* file_path, uint8_t* hash_out);

/**
 * @brief Get signer information from file
 * @param[in] file_path Path to file
 * @param[out] info Signer info structure
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership file_path: caller retains ownership
 * @ownership info: caller provides buffer, function writes to it
 */
int cupolas_signature_get_signer_info(const char* file_path,
                                     cupolas_signer_info_t* info);

/**
 * @brief Free signer info structure
 * @param[in] info Signer info to free (may be NULL)
 * @note Thread-safe: Safe to call from multiple threads
 * @reentrant No
 * @ownership info: transferred to this function, will be freed
 */
void cupolas_signature_free_signer_info(cupolas_signer_info_t* info);

/**
 * @brief Check if signer is in trusted list
 * @param[in] signer_cn Signer CN
 * @return true if trusted, false otherwise
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership signer_cn: caller retains ownership
 */
bool cupolas_signature_is_trusted_signer(const char* signer_cn);

/**
 * @brief Add signer to trusted list
 * @param[in] signer_cn Signer CN
 * @param[in] public_key Public key (PEM format)
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other operations)
 * @reentrant No
 * @ownership signer_cn and public_key: caller retains ownership
 */
int cupolas_signature_add_trusted_signer(const char* signer_cn,
                                        const char* public_key);

/**
 * @brief Sign a file
 * @param[in] file_path Path to file
 * @param[in] private_key Private key (PEM format)
 * @param[in] algo Signature algorithm
 * @param[out] signature_out Output buffer
 * @param[in,out] sig_len Buffer size / actual length
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other operations)
 * @reentrant No
 * @ownership file_path and private_key: caller retains ownership
 * @ownership signature_out: caller provides buffer, function writes to it
 */
int cupolas_signature_sign_file(const char* file_path,
                               const char* private_key,
                               cupolas_sig_algo_t algo,
                               uint8_t* signature_out,
                               size_t* sig_len);

/**
 * @brief Sign data in memory
 * @param[in] data Data pointer
 * @param[in] data_len Data length
 * @param[in] private_key Private key (PEM format)
 * @param[in] algo Signature algorithm
 * @param[out] signature_out Output buffer
 * @param[in,out] sig_len Buffer size / actual length
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other operations)
 * @reentrant No
 * @ownership data and private_key: caller retains ownership
 * @ownership signature_out: caller provides buffer, function writes to it
 */
int cupolas_signature_sign_data(const uint8_t* data, size_t data_len,
                               const char* private_key,
                               cupolas_sig_algo_t algo,
                               uint8_t* signature_out,
                               size_t* sig_len);

/**
 * @brief Get result code string
 * @param[in] result Result code
 * @return Result description string (static, do not free)
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 */
const char* cupolas_signature_result_string(cupolas_sig_result_t result);

/**
 * @brief Get algorithm name string
 * @param[in] algo Algorithm type
 * @return Algorithm name string (static, do not free)
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 */
const char* cupolas_signature_algo_string(cupolas_sig_algo_t algo);

/**
 * @brief Get current timestamp
 * @return Unix timestamp in seconds
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 */
uint64_t cupolas_signature_get_timestamp(void);

/**
 * @brief Check certificate validity period
 * @param[in] not_before Validity start
 * @param[in] not_after Validity end
 * @return 0 if valid, negative if invalid
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 */
int cupolas_signature_check_validity(uint64_t not_before, uint64_t not_after);

#ifdef __cplusplus
}
#endif

#endif /* CUPOLAS_SIGNATURE_H */
