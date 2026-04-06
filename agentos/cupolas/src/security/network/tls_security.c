/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * tls_security.c - TLS/SSL Security Module Implementation
 */

#include "tls_security.h"
#include "../../utils/cupolas_utils.h"
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#endif

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

static struct {
    int initialized;
    cupolas_tls_config_t config;
    SSL_CTX* ssl_ctx;
    CUPOLAS_MUTEX_TYPE lock;
} g_tls = {0};

int tls_security_init(const cupolas_tls_config_t* config) {
    if (g_tls.initialized) return 0;
    
    memset(&g_tls, 0, sizeof(g_tls));
    CUPOLAS_MUTEX_INIT(&g_tls.lock);
    
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
    
    g_tls.ssl_ctx = SSL_CTX_new(TLS_client_method());
    if (!g_tls.ssl_ctx) {
        return -1;
    }
    
    if (config) {
        g_tls.config = *config;
    } else {
        g_tls.config.min_version = CUPOLAS_TLS_1_2;
        g_tls.config.max_version = CUPOLAS_TLS_1_3;
        g_tls.config.require_cert_verify = true;
    }
    
    SSL_CTX_set_min_proto_version(g_tls.ssl_ctx, TLS1_2_VERSION);
    SSL_CTX_set_max_proto_version(g_tls.ssl_ctx, TLS1_3_VERSION);
    
    g_tls.initialized = 1;
    return 0;
}

void tls_security_cleanup(void) {
    if (!g_tls.initialized) return;
    
    if (g_tls.ssl_ctx) {
        SSL_CTX_free(g_tls.ssl_ctx);
        g_tls.ssl_ctx = NULL;
    }
    
    EVP_cleanup();
    ERR_free_strings();
    
    CUPOLAS_MUTEX_DESTROY(&g_tls.lock);
    memset(&g_tls, 0, sizeof(g_tls));
}

int tls_configure(const cupolas_tls_config_t* config) {
    if (!config) return -1;
    
    g_tls.config = *config;
    
    if (g_tls.ssl_ctx) {
        int min_ver = TLS1_2_VERSION;
        int max_ver = TLS1_3_VERSION;
        
        switch (config->min_version) {
            case CUPOLAS_TLS_1_0: min_ver = TLS1_VERSION; break;
            case CUPOLAS_TLS_1_1: min_ver = TLS1_1_VERSION; break;
            case CUPOLAS_TLS_1_2: min_ver = TLS1_2_VERSION; break;
            case CUPOLAS_TLS_1_3: min_ver = TLS1_3_VERSION; break;
            default: break;
        }
        
        switch (config->max_version) {
            case CUPOLAS_TLS_1_0: max_ver = TLS1_VERSION; break;
            case CUPOLAS_TLS_1_1: max_ver = TLS1_1_VERSION; break;
            case CUPOLAS_TLS_1_2: max_ver = TLS1_2_VERSION; break;
            case CUPOLAS_TLS_1_3: max_ver = TLS1_3_VERSION; break;
            default: break;
        }
        
        SSL_CTX_set_min_proto_version(g_tls.ssl_ctx, min_ver);
        SSL_CTX_set_max_proto_version(g_tls.ssl_ctx, max_ver);
        
        if (config->ca_bundle_path) {
            SSL_CTX_load_verify_locations(g_tls.ssl_ctx, config->ca_bundle_path, NULL);
        }
        
        if (config->client_cert_path && config->client_key_path) {
            SSL_CTX_use_certificate_file(g_tls.ssl_ctx, config->client_cert_path, SSL_FILETYPE_PEM);
            SSL_CTX_use_PrivateKey_file(g_tls.ssl_ctx, config->client_key_path, SSL_FILETYPE_PEM);
        }
    }
    
    return 0;
}

int tls_verify_cert(const char* cert_path, const char* hostname, cupolas_cert_result_t* result) {
    if (!cert_path || !result) return -1;
    
    *result = CUPOLAS_CERT_OK;
    
    FILE* f = fopen(cert_path, "r");
    if (!f) {
        *result = CUPOLAS_CERT_INVALID;
        return -1;
    }
    
    X509* cert = PEM_read_X509(f, NULL, NULL, NULL);
    fclose(f);
    
    if (!cert) {
        *result = CUPOLAS_CERT_INVALID;
        return -1;
    }
    
    const ASN1_TIME* not_before = X509_get0_notBefore(cert);
    const ASN1_TIME* not_after = X509_get0_notAfter(cert);
    
    time_t now = time(NULL);
    ASN1_TIME* now_asn1 = ASN1_TIME_adj(NULL, now, 0, 0);
    
    int before_cmp = X509_cmp_time(not_before, now_asn1);
    int after_cmp = X509_cmp_time(not_after, now_asn1);
    
    ASN1_TIME_free(now_asn1);
    
    if (before_cmp > 0) {
        *result = CUPOLAS_CERT_EXPIRED;
        X509_free(cert);
        return 0;
    }
    
    if (after_cmp < 0) {
        *result = CUPOLAS_CERT_EXPIRED;
        X509_free(cert);
        return 0;
    }
    
    if (hostname) {
        char* hostname_dup = cupolas_strdup(hostname);
        int match = X509_check_host(cert, hostname_dup, strlen(hostname_dup), 0, NULL);
        free(hostname_dup);
        
        if (match != 1) {
            *result = CUPOLAS_CERT_HOST_MISMATCH;
            X509_free(cert);
            return 0;
        }
    }
    
    X509_free(cert);
    return 0;
}

int tls_verify_cert_chain(const char* cert_chain, size_t chain_len, cupolas_cert_result_t* result) {
    if (!cert_chain || !result) return -1;
    
    *result = CUPOLAS_CERT_OK;
    
    BIO* bio = BIO_new_mem_buf(cert_chain, (int)chain_len);
    if (!bio) {
        *result = CUPOLAS_CERT_INVALID;
        return -1;
    }
    
    X509_STORE* store = X509_STORE_new();
    X509_STORE_CTX* ctx = X509_STORE_CTX_new();
    
    STACK_OF(X509)* certs = sk_X509_new_null();
    X509* cert = NULL;
    
    while ((cert = PEM_read_bio_X509(bio, NULL, NULL, NULL)) != NULL) {
        sk_X509_push(certs, cert);
    }
    
    BIO_free(bio);
    
    if (sk_X509_num(certs) == 0) {
        *result = CUPOLAS_CERT_INVALID;
        sk_X509_free(certs);
        X509_STORE_CTX_free(ctx);
        X509_STORE_free(store);
        return -1;
    }
    
    X509_STORE_CTX_init(ctx, store, sk_X509_value(certs, 0), certs);
    
    int verify_result = X509_verify_cert(ctx);
    if (verify_result != 1) {
        int err = X509_STORE_CTX_get_error(ctx);
        switch (err) {
            case X509_V_ERR_CERT_HAS_EXPIRED:
                *result = CUPOLAS_CERT_EXPIRED;
                break;
            case X509_V_ERR_CERT_REVOKED:
                *result = CUPOLAS_CERT_REVOKED;
                break;
            case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
            case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
                *result = CUPOLAS_CERT_SELF_SIGNED;
                break;
            default:
                *result = CUPOLAS_CERT_UNTRUSTED;
                break;
        }
    }
    
    X509_STORE_CTX_free(ctx);
    X509_STORE_free(store);
    sk_X509_pop_free(certs, X509_free);
    
    return 0;
}

int tls_check_connection(const char* hostname, uint16_t port, cupolas_cert_result_t* result) {
    if (!hostname || !result) return -1;
    
    *result = CUPOLAS_CERT_OK;
    
    struct hostent* host = gethostbyname(hostname);
    if (!host) {
        *result = CUPOLAS_CERT_INVALID;
        return -1;
    }
    
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        *result = CUPOLAS_CERT_INVALID;
        return -1;
    }
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    memcpy(&addr.sin_addr, host->h_addr, host->h_length);
    
    int connect_result = connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    if (connect_result != 0) {
#ifdef _WIN32
        closesocket(sock);
#else
        close(sock);
#endif
        *result = CUPOLAS_CERT_INVALID;
        return -1;
    }
    
    SSL* ssl = SSL_new(g_tls.ssl_ctx);
    if (!ssl) {
#ifdef _WIN32
        closesocket(sock);
#else
        close(sock);
#endif
        *result = CUPOLAS_CERT_INVALID;
        return -1;
    }
    
    SSL_set_fd(ssl, sock);
    SSL_set_tlsext_host_name(ssl, hostname);
    
    int ssl_result = SSL_connect(ssl);
    if (ssl_result != 1) {
        int err = SSL_get_error(ssl, ssl_result);
        switch (err) {
            case SSL_ERROR_SSL:
                *result = CUPOLAS_CERT_INVALID;
                break;
            case SSL_ERROR_SYSCALL:
                *result = CUPOLAS_CERT_INVALID;
                break;
            default:
                *result = CUPOLAS_CERT_INVALID;
                break;
        }
        SSL_free(ssl);
#ifdef _WIN32
        closesocket(sock);
#else
        close(sock);
#endif
        return 0;
    }
    
    long verify_result_long = SSL_get_verify_result(ssl);
    if (verify_result_long != X509_V_OK) {
        switch (verify_result_long) {
            case X509_V_ERR_CERT_HAS_EXPIRED:
                *result = CUPOLAS_CERT_EXPIRED;
                break;
            case X509_V_ERR_CERT_REVOKED:
                *result = CUPOLAS_CERT_REVOKED;
                break;
            case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
            case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
                *result = CUPOLAS_CERT_SELF_SIGNED;
                break;
            default:
                *result = CUPOLAS_CERT_UNTRUSTED;
                break;
        }
    }
    
    SSL_free(ssl);
#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif
    
    return 0;
}

int tls_get_cipher_suites(char*** suites, size_t* count) {
    if (!suites || !count) return -1;
    
    static const char* default_suites[] = {
        "TLS_AES_256_GCM_SHA384",
        "TLS_CHACHA20_POLY1305_SHA256",
        "TLS_AES_128_GCM_SHA256",
        "TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384",
        "TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384",
        "TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256",
        "TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256"
    };
    
    *count = sizeof(default_suites) / sizeof(default_suites[0]);
    *suites = (char**)malloc(*count * sizeof(char*));
    if (!*suites) return -1;
    
    for (size_t i = 0; i < *count; i++) {
        (*suites)[i] = cupolas_strdup(default_suites[i]);
    }
    
    return 0;
}

int tls_is_cipher_secure(const char* suite) {
    if (!suite) return 0;
    
    const char* secure_suites[] = {
        "TLS_AES_256_GCM_SHA384",
        "TLS_CHACHA20_POLY1305_SHA256",
        "TLS_AES_128_GCM_SHA256",
        "TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384",
        "TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384",
        "TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256",
        "TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256",
        "TLS_DHE_RSA_WITH_AES_256_GCM_SHA384",
        "TLS_DHE_RSA_WITH_AES_128_GCM_SHA256"
    };
    
    for (size_t i = 0; i < sizeof(secure_suites) / sizeof(secure_suites[0]); i++) {
        if (strcmp(suite, secure_suites[i]) == 0) {
            return 1;
        }
    }
    
    return 0;
}

const char* tls_version_string(cupolas_tls_version_t version) {
    switch (version) {
        case CUPOLAS_TLS_1_0: return "TLS 1.0";
        case CUPOLAS_TLS_1_1: return "TLS 1.1";
        case CUPOLAS_TLS_1_2: return "TLS 1.2";
        case CUPOLAS_TLS_1_3: return "TLS 1.3";
        default: return "Unknown";
    }
}

const char* cert_result_string(cupolas_cert_result_t result) {
    switch (result) {
        case CUPOLAS_CERT_OK: return "Certificate valid";
        case CUPOLAS_CERT_INVALID: return "Certificate invalid";
        case CUPOLAS_CERT_EXPIRED: return "Certificate expired";
        case CUPOLAS_CERT_REVOKED: return "Certificate revoked";
        case CUPOLAS_CERT_UNTRUSTED: return "Certificate untrusted";
        case CUPOLAS_CERT_HOST_MISMATCH: return "Hostname mismatch";
        case CUPOLAS_CERT_SELF_SIGNED: return "Self-signed certificate";
        default: return "Unknown";
    }
}
