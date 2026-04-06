/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * tls_security.h - TLS/SSL Security Module Interface
 */

#ifndef CUPOLAS_TLS_SECURITY_H
#define CUPOLAS_TLS_SECURITY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CUPOLAS_TLS_AUTO = 0,
    CUPOLAS_TLS_1_0 = 1,
    CUPOLAS_TLS_1_1 = 2,
    CUPOLAS_TLS_1_2 = 3,
    CUPOLAS_TLS_1_3 = 4
} cupolas_tls_version_t;

typedef enum {
    CUPOLAS_CERT_OK = 0,
    CUPOLAS_CERT_INVALID,
    CUPOLAS_CERT_EXPIRED,
    CUPOLAS_CERT_REVOKED,
    CUPOLAS_CERT_UNTRUSTED,
    CUPOLAS_CERT_HOST_MISMATCH,
    CUPOLAS_CERT_SELF_SIGNED
} cupolas_cert_result_t;

typedef struct {
    cupolas_tls_version_t min_version;
    cupolas_tls_version_t max_version;
    
    const char* ca_bundle_path;
    const char* client_cert_path;
    const char* client_key_path;
    
    bool require_cert_verify;
    bool verify_hostname;
    
    uint32_t handshake_timeout_ms;
} cupolas_tls_config_t;

int tls_security_init(const cupolas_tls_config_t* config);
void tls_security_cleanup(void);

int tls_configure(const cupolas_tls_config_t* config);

int tls_verify_cert(const char* cert_path, const char* hostname, cupolas_cert_result_t* result);

int tls_verify_cert_chain(const char* cert_chain, size_t chain_len, cupolas_cert_result_t* result);

int tls_check_connection(const char* hostname, uint16_t port, cupolas_cert_result_t* result);

int tls_get_cipher_suites(char*** suites, size_t* count);

int tls_is_cipher_secure(const char* suite);

const char* tls_version_string(cupolas_tls_version_t version);

const char* cert_result_string(cupolas_cert_result_t result);

#ifdef __cplusplus
}
#endif

#endif /* CUPOLAS_TLS_SECURITY_H */
