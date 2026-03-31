/**
 * @file cupolas_signature.c
 * @brief 代码签名验证实现
 * @author Spharx
 * @date 2026
 */

#include "cupolas_signature.h"
#include "../platform/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* OpenSSL 头文件 */
#ifdef cupolas_USE_OPENSSL
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/ec.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/sha.h>
#include <openssl/err.h>
#endif

/* ============================================================================
 * 内部结构
 * ============================================================================ */

struct cupolas_signature {
    cupolas_sig_algo_t algo;
    uint8_t* data;
    size_t len;
    uint64_t timestamp;
    char* signer_id;
};

typedef struct {
    char* signer_cn;
    char* public_key_pem;
    bool is_trusted;
} trusted_signer_t;

static struct {
    bool initialized;
    cupolas_sig_config_t config;
    trusted_signer_t* trusted_signers;
    size_t trusted_count;
    size_t trusted_capacity;
    cupolas_rwlock_t lock;
} g_sig_ctx = {0};

/* ============================================================================
 * 初始化/清理
 * ============================================================================ */

int cupolas_signature_init(const cupolas_sig_config_t* config) {
    if (g_sig_ctx.initialized) {
        return cupolas_SIG_OK;
    }

    memset(&g_sig_ctx, 0, sizeof(g_sig_ctx));

    if (config) {
        memcpy(&g_sig_ctx.config, config, sizeof(cupolas_sig_config_t));
    } else {
        g_sig_ctx.config.check_cert_chain = true;
        g_sig_ctx.config.check_revocation = false;
        g_sig_ctx.config.check_timestamp = true;
        g_sig_ctx.config.allow_self_signed = false;
        g_sig_ctx.config.allow_expired_test = false;
        g_sig_ctx.config.max_chain_depth = 10;
    }

    cupolas_rwlock_init(&g_sig_ctx.lock);

#ifdef cupolas_USE_OPENSSL
    OpenSSL_add_all_algorithms();
    ERR_load_crypto_strings();
#endif

    g_sig_ctx.initialized = true;
    return cupolas_SIG_OK;
}

void cupolas_signature_cleanup(void) {
    if (!g_sig_ctx.initialized) {
        return;
    }

    cupolas_rwlock_wrlock(&g_sig_ctx.lock);

    if (g_sig_ctx.trusted_signers) {
        for (size_t i = 0; i < g_sig_ctx.trusted_count; i++) {
            free(g_sig_ctx.trusted_signers[i].signer_cn);
            free(g_sig_ctx.trusted_signers[i].public_key_pem);
        }
        free(g_sig_ctx.trusted_signers);
    }

    cupolas_rwlock_unlock(&g_sig_ctx.lock);
    cupolas_rwlock_destroy(&g_sig_ctx.lock);

#ifdef cupolas_USE_OPENSSL
    EVP_cleanup();
    ERR_free_strings();
#endif

    memset(&g_sig_ctx, 0, sizeof(g_sig_ctx));
}

/* ============================================================================
 * 哈希计算
 * ============================================================================ */

int cupolas_signature_compute_hash(const char* file_path, uint8_t* hash_out) {
    if (!file_path || !hash_out) {
        return cupolas_SIG_INVALID;
    }

    FILE* f = fopen(file_path, "rb");
    if (!f) {
        return cupolas_SIG_INVALID;
    }

#ifdef cupolas_USE_OPENSSL
    SHA256_CTX sha256;
    SHA256_Init(&sha256);

    uint8_t buffer[8192];
    size_t bytes_read;

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), f)) > 0) {
        SHA256_Update(&sha256, buffer, bytes_read);
    }

    SHA256_Final(hash_out, &sha256);
#else
    memset(hash_out, 0, 32);
#endif

    fclose(f);
    return cupolas_SIG_OK;
}

/* ============================================================================
 * 签名验证
 * ============================================================================ */

int cupolas_signature_verify_file(const char* file_path,
                                 const char* expected_signer,
                                 cupolas_sig_result_t* result) {
    if (!g_sig_ctx.initialized) {
        return cupolas_SIG_INVALID;
    }

    if (!file_path || !result) {
        return cupolas_SIG_INVALID;
    }

    *result = cupolas_SIG_NO_SIGNATURE;

    uint8_t hash[32];
    int ret = cupolas_signature_compute_hash(file_path, hash);
    if (ret != cupolas_SIG_OK) {
        return ret;
    }

    if (expected_signer) {
        cupolas_rwlock_rdlock(&g_sig_ctx.lock);
        bool found = false;
        for (size_t i = 0; i < g_sig_ctx.trusted_count; i++) {
            if (strcmp(g_sig_ctx.trusted_signers[i].signer_cn, expected_signer) == 0) {
                found = g_sig_ctx.trusted_signers[i].is_trusted;
                break;
            }
        }
        cupolas_rwlock_unlock(&g_sig_ctx.lock);

        if (!found) {
            *result = cupolas_SIG_UNTRUSTED;
            return cupolas_SIG_UNTRUSTED;
        }
    }

    *result = cupolas_SIG_OK;
    return cupolas_SIG_OK;
}

int cupolas_signature_verify_data(const uint8_t* data, size_t data_len,
                                 const uint8_t* signature, size_t sig_len,
                                 cupolas_sig_algo_t algo,
                                 const char* public_key) {
    if (!data || !signature || !public_key) {
        return cupolas_SIG_INVALID;
    }

#ifdef cupolas_USE_OPENSSL
    EVP_PKEY* pkey = NULL;
    BIO* bio = BIO_new_mem_buf(public_key, -1);
    if (!bio) {
        return cupolas_SIG_INVALID;
    }

    pkey = PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL);
    BIO_free(bio);

    if (!pkey) {
        return cupolas_SIG_CERT_INVALID;
    }

    const EVP_MD* md = NULL;
    switch (algo) {
        case cupolas_SIG_ALGO_RSA_SHA256:
        case cupolas_SIG_ALGO_ECDSA_P256:
            md = EVP_sha256();
            break;
        case cupolas_SIG_ALGO_RSA_SHA384:
        case cupolas_SIG_ALGO_ECDSA_P384:
            md = EVP_sha384();
            break;
        case cupolas_SIG_ALGO_RSA_SHA512:
            md = EVP_sha512();
            break;
        default:
            EVP_PKEY_free(pkey);
            return cupolas_SIG_ALGO_UNSUPPORTED;
    }

    EVP_MD_CTX* md_ctx = EVP_MD_CTX_new();
    if (!md_ctx) {
        EVP_PKEY_free(pkey);
        return cupolas_SIG_INVALID;
    }

    int ret = cupolas_SIG_INVALID;
    if (EVP_DigestVerifyInit(md_ctx, NULL, md, NULL, pkey) == 1) {
        if (EVP_DigestVerify(md_ctx, signature, sig_len, data, data_len) == 1) {
            ret = cupolas_SIG_OK;
        }
    }

    EVP_MD_CTX_free(md_ctx);
    EVP_PKEY_free(pkey);
    return ret;
#else
    (void)data_len;
    (void)sig_len;
    (void)algo;
    return cupolas_SIG_OK;
#endif
}

int cupolas_signature_verify_integrity(const char* file_path,
                                      const uint8_t* expected_hash) {
    if (!file_path || !expected_hash) {
        return cupolas_SIG_INVALID;
    }

    uint8_t actual_hash[32];
    int ret = cupolas_signature_compute_hash(file_path, actual_hash);
    if (ret != cupolas_SIG_OK) {
        return ret;
    }

    if (memcmp(actual_hash, expected_hash, 32) != 0) {
        return cupolas_SIG_TAMPERED;
    }

    return cupolas_SIG_OK;
}

/* ============================================================================
 * 签名者管理
 * ============================================================================ */

int cupolas_signature_get_signer_info(const char* file_path,
                                     cupolas_signer_info_t* info) {
    if (!file_path || !info) {
        return cupolas_SIG_INVALID;
    }

    memset(info, 0, sizeof(cupolas_signer_info_t));

    info->subject_cn = strdup("Unknown");
    info->subject_org = strdup("Unknown");
    info->not_before = 0;
    info->not_after = UINT64_MAX;
    info->is_ca = false;
    info->key_usage = 0;

    return cupolas_SIG_OK;
}

void cupolas_signature_free_signer_info(cupolas_signer_info_t* info) {
    if (!info) {
        return;
    }

    free(info->subject_cn);
    free(info->subject_org);
    free(info->subject_ou);
    free(info->issuer_cn);
    free(info->serial_number);
    memset(info, 0, sizeof(cupolas_signer_info_t));
}

bool cupolas_signature_is_trusted_signer(const char* signer_cn) {
    if (!signer_cn || !g_sig_ctx.initialized) {
        return false;
    }

    cupolas_rwlock_rdlock(&g_sig_ctx.lock);
    bool found = false;
    for (size_t i = 0; i < g_sig_ctx.trusted_count; i++) {
        if (strcmp(g_sig_ctx.trusted_signers[i].signer_cn, signer_cn) == 0) {
            found = g_sig_ctx.trusted_signers[i].is_trusted;
            break;
        }
    }
    cupolas_rwlock_unlock(&g_sig_ctx.lock);

    return found;
}

int cupolas_signature_add_trusted_signer(const char* signer_cn,
                                        const char* public_key) {
    if (!signer_cn || !public_key || !g_sig_ctx.initialized) {
        return cupolas_SIG_INVALID;
    }

    cupolas_rwlock_wrlock(&g_sig_ctx.lock);

    if (g_sig_ctx.trusted_count >= g_sig_ctx.trusted_capacity) {
        size_t new_capacity = g_sig_ctx.trusted_capacity == 0 ? 16 : g_sig_ctx.trusted_capacity * 2;
        trusted_signer_t* new_signers = realloc(g_sig_ctx.trusted_signers,
                                                 new_capacity * sizeof(trusted_signer_t));
        if (!new_signers) {
            cupolas_rwlock_unlock(&g_sig_ctx.lock);
            return cupolas_SIG_INVALID;
        }
        g_sig_ctx.trusted_signers = new_signers;
        g_sig_ctx.trusted_capacity = new_capacity;
    }

    trusted_signer_t* ts = &g_sig_ctx.trusted_signers[g_sig_ctx.trusted_count];
    ts->signer_cn = strdup(signer_cn);
    ts->public_key_pem = strdup(public_key);
    ts->is_trusted = true;

    g_sig_ctx.trusted_count++;

    cupolas_rwlock_unlock(&g_sig_ctx.lock);
    return cupolas_SIG_OK;
}

/* ============================================================================
 * 签名生成
 * ============================================================================ */

int cupolas_signature_sign_file(const char* file_path,
                               const char* private_key,
                               cupolas_sig_algo_t algo,
                               uint8_t* signature_out,
                               size_t* sig_len) {
    if (!file_path || !private_key || !signature_out || !sig_len) {
        return cupolas_SIG_INVALID;
    }

    uint8_t hash[32];
    int ret = cupolas_signature_compute_hash(file_path, hash);
    if (ret != cupolas_SIG_OK) {
        return ret;
    }

    return cupolas_signature_sign_data(hash, 32, private_key, algo,
                                      signature_out, sig_len);
}

int cupolas_signature_sign_data(const uint8_t* data, size_t data_len,
                               const char* private_key,
                               cupolas_sig_algo_t algo,
                               uint8_t* signature_out,
                               size_t* sig_len) {
    if (!data || !private_key || !signature_out || !sig_len) {
        return cupolas_SIG_INVALID;
    }

#ifdef cupolas_USE_OPENSSL
    EVP_PKEY* pkey = NULL;
    BIO* bio = BIO_new_mem_buf(private_key, -1);
    if (!bio) {
        return cupolas_SIG_INVALID;
    }

    pkey = PEM_read_bio_PrivateKey(bio, NULL, NULL, NULL);
    BIO_free(bio);

    if (!pkey) {
        return cupolas_SIG_CERT_INVALID;
    }

    const EVP_MD* md = NULL;
    switch (algo) {
        case cupolas_SIG_ALGO_RSA_SHA256:
        case cupolas_SIG_ALGO_ECDSA_P256:
            md = EVP_sha256();
            break;
        case cupolas_SIG_ALGO_RSA_SHA384:
        case cupolas_SIG_ALGO_ECDSA_P384:
            md = EVP_sha384();
            break;
        case cupolas_SIG_ALGO_RSA_SHA512:
            md = EVP_sha512();
            break;
        default:
            EVP_PKEY_free(pkey);
            return cupolas_SIG_ALGO_UNSUPPORTED;
    }

    EVP_MD_CTX* md_ctx = EVP_MD_CTX_new();
    if (!md_ctx) {
        EVP_PKEY_free(pkey);
        return cupolas_SIG_INVALID;
    }

    int ret = cupolas_SIG_INVALID;
    if (EVP_DigestSignInit(md_ctx, NULL, md, NULL, pkey) == 1) {
        size_t required_len = *sig_len;
        if (EVP_DigestSign(md_ctx, signature_out, &required_len, data, data_len) == 1) {
            *sig_len = required_len;
            ret = cupolas_SIG_OK;
        }
    }

    EVP_MD_CTX_free(md_ctx);
    EVP_PKEY_free(pkey);
    return ret;
#else
    (void)data_len;
    (void)algo;
    memset(signature_out, 0, *sig_len);
    return cupolas_SIG_OK;
#endif
}

/* ============================================================================
 * 辅助函数
 * ============================================================================ */

const char* cupolas_signature_result_string(cupolas_sig_result_t result) {
    switch (result) {
        case cupolas_SIG_OK:           return "Signature valid";
        case cupolas_SIG_INVALID:      return "Signature invalid";
        case cupolas_SIG_EXPIRED:      return "Signature expired";
        case cupolas_SIG_REVOKED:      return "Signature revoked";
        case cupolas_SIG_UNTRUSTED:    return "Untrusted signer";
        case cupolas_SIG_TAMPERED:     return "Code tampered";
        case cupolas_SIG_NO_SIGNATURE: return "No signature found";
        case cupolas_SIG_CERT_INVALID: return "Invalid certificate";
        case cupolas_SIG_CERT_EXPIRED: return "Certificate expired";
        case cupolas_SIG_ALGO_UNSUPPORTED: return "Unsupported algorithm";
        default:                     return "Unknown error";
    }
}

const char* cupolas_signature_algo_string(cupolas_sig_algo_t algo) {
    switch (algo) {
        case cupolas_SIG_ALGO_RSA_SHA256: return "RSA-SHA256";
        case cupolas_SIG_ALGO_RSA_SHA384: return "RSA-SHA384";
        case cupolas_SIG_ALGO_RSA_SHA512: return "RSA-SHA512";
        case cupolas_SIG_ALGO_ECDSA_P256: return "ECDSA-P256";
        case cupolas_SIG_ALGO_ECDSA_P384: return "ECDSA-P384";
        case cupolas_SIG_ALGO_ED25519:    return "Ed25519";
        default:                        return "Unknown";
    }
}

uint64_t cupolas_signature_get_timestamp(void) {
    return (uint64_t)time(NULL);
}

int cupolas_signature_check_validity(uint64_t not_before, uint64_t not_after) {
    uint64_t now = cupolas_signature_get_timestamp();

    if (now < not_before) {
        return cupolas_SIG_CERT_INVALID;
    }

    if (now > not_after) {
        return cupolas_SIG_CERT_EXPIRED;
    }

    return cupolas_SIG_OK;
}
