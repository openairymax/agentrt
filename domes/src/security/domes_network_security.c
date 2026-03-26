/**
 * @file domes_network_security.c
 * @brief 网络安全 - TLS 强制、网络过滤实现
 * @author Spharx
 * @date 2026
 */

#include "domes_network_security.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <pthread.h>
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

#define DOMES_MAX_FILTER_RULES 512
#define DOMES_MAX_CONNECTIONS 1024
#define DOMES_MAX_URL_LEN 2048

typedef struct {
    domes_net_filter_rule_t rule;
    int active;
} filter_rule_entry_t;

static struct {
    int initialized;
    domes_net_security_config_t config;
    
    filter_rule_entry_t filter_rules[DOMES_MAX_FILTER_RULES];
    size_t filter_rule_count;
    
    domes_connection_info_t connections[DOMES_MAX_CONNECTIONS];
    size_t connection_count;
    
    domes_net_stats_t stats;
    
    SSL_CTX* ssl_ctx;
    
    void (*ids_callback)(const char* alert_type, const char* details, const domes_connection_info_t* conn);
    
#ifdef _WIN32
    CRITICAL_SECTION lock;
#else
    pthread_mutex_t lock;
#endif
} g_net_security;

static char* domes_strdup(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char* dup = (char*)malloc(len);
    if (dup) memcpy(dup, s, len);
    return dup;
}

static uint64_t domes_get_time_ms(void) {
#ifdef _WIN32
    return GetTickCount64();
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
#endif
}

static void domes_free_filter_rule(domes_net_filter_rule_t* rule) {
    if (!rule) return;
    free(rule->rule_id);
    free(rule->description);
    free(rule->src_ip_pattern);
    free(rule->dst_ip_pattern);
    free(rule->host_pattern);
    free(rule->url_pattern);
    memset(rule, 0, sizeof(*rule));
}

static void domes_free_connection_info(domes_connection_info_t* info) {
    if (!info) return;
    free(info->local_ip);
    free(info->remote_ip);
    free(info->hostname);
    free(info->cipher_suite);
    memset(info, 0, sizeof(*info));
}

int domes_net_security_init(const domes_net_security_config_t* config) {
    if (g_net_security.initialized) return 0;
    
    memset(&g_net_security, 0, sizeof(g_net_security));
    
#ifdef _WIN32
    InitializeCriticalSection(&g_net_security.lock);
    WSADATA wsa_data;
    WSAStartup(MAKEWORD(2, 2), &wsa_data);
#else
    pthread_mutex_init(&g_net_security.lock, NULL);
#endif
    
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
    
    g_net_security.ssl_ctx = SSL_CTX_new(TLS_client_method());
    if (!g_net_security.ssl_ctx) {
        return -1;
    }
    
    if (config) {
        g_net_security.config = *config;
    } else {
        g_net_security.config.tls.min_version = DOMES_TLS_1_2;
        g_net_security.config.tls.max_version = DOMES_TLS_1_3;
        g_net_security.config.tls.require_cert_verify = true;
        g_net_security.config.http.enforce_https = true;
        g_net_security.config.http.hsts_enabled = true;
        g_net_security.config.http.hsts_max_age = 31536000;
        g_net_security.config.dns.enable_dnssec = true;
        g_net_security.config.enable_logging = true;
        g_net_security.config.enable_audit = true;
    }
    
    SSL_CTX_set_min_proto_version(g_net_security.ssl_ctx, TLS1_2_VERSION);
    SSL_CTX_set_max_proto_version(g_net_security.ssl_ctx, TLS1_3_VERSION);
    
    g_net_security.initialized = 1;
    return 0;
}

void domes_net_security_cleanup(void) {
    if (!g_net_security.initialized) return;
    
    for (size_t i = 0; i < g_net_security.filter_rule_count; i++) {
        domes_free_filter_rule(&g_net_security.filter_rules[i].rule);
    }
    
    for (size_t i = 0; i < g_net_security.connection_count; i++) {
        domes_free_connection_info(&g_net_security.connections[i]);
    }
    
    if (g_net_security.ssl_ctx) {
        SSL_CTX_free(g_net_security.ssl_ctx);
    }
    
    EVP_cleanup();
    ERR_free_strings();
    
#ifdef _WIN32
    DeleteCriticalSection(&g_net_security.lock);
    WSACleanup();
#else
    pthread_mutex_destroy(&g_net_security.lock);
#endif
    
    memset(&g_net_security, 0, sizeof(g_net_security));
}

int domes_net_security_get_config(domes_net_security_config_t* config) {
    if (!config) return -1;
    *config = g_net_security.config;
    return 0;
}

int domes_tls_configure(const domes_tls_config_t* config) {
    if (!config) return -1;
    
    g_net_security.config.tls = *config;
    
    if (g_net_security.ssl_ctx) {
        int min_ver = TLS1_2_VERSION;
        int max_ver = TLS1_3_VERSION;
        
        switch (config->min_version) {
            case DOMES_TLS_1_0: min_ver = TLS1_VERSION; break;
            case DOMES_TLS_1_1: min_ver = TLS1_1_VERSION; break;
            case DOMES_TLS_1_2: min_ver = TLS1_2_VERSION; break;
            case DOMES_TLS_1_3: min_ver = TLS1_3_VERSION; break;
        }
        
        switch (config->max_version) {
            case DOMES_TLS_1_0: max_ver = TLS1_VERSION; break;
            case DOMES_TLS_1_1: max_ver = TLS1_1_VERSION; break;
            case DOMES_TLS_1_2: max_ver = TLS1_2_VERSION; break;
            case DOMES_TLS_1_3: max_ver = TLS1_3_VERSION; break;
        }
        
        SSL_CTX_set_min_proto_version(g_net_security.ssl_ctx, min_ver);
        SSL_CTX_set_max_proto_version(g_net_security.ssl_ctx, max_ver);
        
        if (config->ca_bundle_path) {
            SSL_CTX_load_verify_locations(g_net_security.ssl_ctx, config->ca_bundle_path, NULL);
        }
        
        if (config->client_cert_path && config->client_key_path) {
            SSL_CTX_use_certificate_file(g_net_security.ssl_ctx, config->client_cert_path, SSL_FILETYPE_PEM);
            SSL_CTX_use_PrivateKey_file(g_net_security.ssl_ctx, config->client_key_path, SSL_FILETYPE_PEM);
        }
    }
    
    return 0;
}

int domes_tls_verify_cert(const char* cert_path, const char* hostname, domes_cert_result_t* result) {
    if (!cert_path || !result) return -1;
    
    *result = DOMES_CERT_OK;
    
    FILE* f = fopen(cert_path, "r");
    if (!f) {
        *result = DOMES_CERT_INVALID;
        return -1;
    }
    
    X509* cert = PEM_read_X509(f, NULL, NULL, NULL);
    fclose(f);
    
    if (!cert) {
        *result = DOMES_CERT_INVALID;
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
        *result = DOMES_CERT_EXPIRED;
        X509_free(cert);
        return 0;
    }
    
    if (after_cmp < 0) {
        *result = DOMES_CERT_EXPIRED;
        X509_free(cert);
        return 0;
    }
    
    if (hostname) {
        char* hostname_dup = domes_strdup(hostname);
        int match = X509_check_host(cert, hostname_dup, strlen(hostname_dup), 0, NULL);
        free(hostname_dup);
        
        if (match != 1) {
            *result = DOMES_CERT_HOST_MISMATCH;
            X509_free(cert);
            return 0;
        }
    }
    
    X509_free(cert);
    return 0;
}

int domes_tls_verify_cert_chain(const char* cert_chain, size_t chain_len, domes_cert_result_t* result) {
    if (!cert_chain || !result) return -1;
    
    *result = DOMES_CERT_OK;
    
    BIO* bio = BIO_new_mem_buf(cert_chain, (int)chain_len);
    if (!bio) {
        *result = DOMES_CERT_INVALID;
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
        *result = DOMES_CERT_INVALID;
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
                *result = DOMES_CERT_EXPIRED;
                break;
            case X509_V_ERR_CERT_REVOKED:
                *result = DOMES_CERT_REVOKED;
                break;
            case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
            case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
                *result = DOMES_CERT_SELF_SIGNED;
                break;
            default:
                *result = DOMES_CERT_UNTRUSTED;
                break;
        }
    }
    
    X509_STORE_CTX_free(ctx);
    X509_STORE_free(store);
    sk_X509_pop_free(certs, X509_free);
    
    return 0;
}

int domes_tls_check_connection(const char* hostname, uint16_t port, domes_cert_result_t* result) {
    if (!hostname || !result) return -1;
    
    *result = DOMES_CERT_OK;
    
    struct hostent* host = gethostbyname(hostname);
    if (!host) {
        *result = DOMES_CERT_INVALID;
        return -1;
    }
    
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        *result = DOMES_CERT_INVALID;
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
        *result = DOMES_CERT_INVALID;
        return -1;
    }
    
    SSL* ssl = SSL_new(g_net_security.ssl_ctx);
    if (!ssl) {
#ifdef _WIN32
        closesocket(sock);
#else
        close(sock);
#endif
        *result = DOMES_CERT_INVALID;
        return -1;
    }
    
    SSL_set_fd(ssl, sock);
    SSL_set_tlsext_host_name(ssl, hostname);
    
    int ssl_result = SSL_connect(ssl);
    if (ssl_result != 1) {
        int err = SSL_get_error(ssl, ssl_result);
        switch (err) {
            case SSL_ERROR_SSL:
                *result = DOMES_CERT_INVALID;
                break;
            case SSL_ERROR_SYSCALL:
                *result = DOMES_CERT_INVALID;
                break;
            default:
                *result = DOMES_CERT_INVALID;
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
                *result = DOMES_CERT_EXPIRED;
                break;
            case X509_V_ERR_CERT_REVOKED:
                *result = DOMES_CERT_REVOKED;
                break;
            case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
            case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
                *result = DOMES_CERT_SELF_SIGNED;
                break;
            default:
                *result = DOMES_CERT_UNTRUSTED;
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

int domes_tls_get_cipher_suites(char*** suites, size_t* count) {
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
        (*suites)[i] = domes_strdup(default_suites[i]);
    }
    
    return 0;
}

int domes_tls_is_cipher_secure(const char* suite) {
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

int domes_net_add_rule(const domes_net_filter_rule_t* rule) {
    if (!rule) return -1;
    if (g_net_security.filter_rule_count >= DOMES_MAX_FILTER_RULES) return -1;
    
    filter_rule_entry_t* entry = &g_net_security.filter_rules[g_net_security.filter_rule_count];
    memset(entry, 0, sizeof(*entry));
    
    entry->rule.rule_id = domes_strdup(rule->rule_id);
    entry->rule.description = domes_strdup(rule->description);
    entry->rule.src_ip_pattern = domes_strdup(rule->src_ip_pattern);
    entry->rule.dst_ip_pattern = domes_strdup(rule->dst_ip_pattern);
    entry->rule.src_port_start = rule->src_port_start;
    entry->rule.src_port_end = rule->src_port_end;
    entry->rule.dst_port_start = rule->dst_port_start;
    entry->rule.dst_port_end = rule->dst_port_end;
    entry->rule.protocol = rule->protocol;
    entry->rule.host_pattern = domes_strdup(rule->host_pattern);
    entry->rule.url_pattern = domes_strdup(rule->url_pattern);
    entry->rule.action = rule->action;
    entry->rule.priority = rule->priority;
    entry->rule.enabled = rule->enabled;
    entry->rule.rate_limit = rule->rate_limit;
    entry->rule.burst_limit = rule->burst_limit;
    entry->active = 1;
    
    g_net_security.filter_rule_count++;
    return 0;
}

int domes_net_remove_rule(const char* rule_id) {
    if (!rule_id) return -1;
    
    for (size_t i = 0; i < g_net_security.filter_rule_count; i++) {
        if (g_net_security.filter_rules[i].rule.rule_id &&
            strcmp(g_net_security.filter_rules[i].rule.rule_id, rule_id) == 0) {
            domes_free_filter_rule(&g_net_security.filter_rules[i].rule);
            
            for (size_t j = i; j < g_net_security.filter_rule_count - 1; j++) {
                g_net_security.filter_rules[j] = g_net_security.filter_rules[j + 1];
            }
            g_net_security.filter_rule_count--;
            return 0;
        }
    }
    
    return -1;
}

int domes_net_update_rule(const char* rule_id, const domes_net_filter_rule_t* rule) {
    if (!rule_id || !rule) return -1;
    
    for (size_t i = 0; i < g_net_security.filter_rule_count; i++) {
        if (g_net_security.filter_rules[i].rule.rule_id &&
            strcmp(g_net_security.filter_rules[i].rule.rule_id, rule_id) == 0) {
            domes_free_filter_rule(&g_net_security.filter_rules[i].rule);
            
            g_net_security.filter_rules[i].rule.rule_id = domes_strdup(rule->rule_id);
            g_net_security.filter_rules[i].rule.description = domes_strdup(rule->description);
            g_net_security.filter_rules[i].rule.src_ip_pattern = domes_strdup(rule->src_ip_pattern);
            g_net_security.filter_rules[i].rule.dst_ip_pattern = domes_strdup(rule->dst_ip_pattern);
            g_net_security.filter_rules[i].rule.src_port_start = rule->src_port_start;
            g_net_security.filter_rules[i].rule.src_port_end = rule->src_port_end;
            g_net_security.filter_rules[i].rule.dst_port_start = rule->dst_port_start;
            g_net_security.filter_rules[i].rule.dst_port_end = rule->dst_port_end;
            g_net_security.filter_rules[i].rule.protocol = rule->protocol;
            g_net_security.filter_rules[i].rule.host_pattern = domes_strdup(rule->host_pattern);
            g_net_security.filter_rules[i].rule.url_pattern = domes_strdup(rule->url_pattern);
            g_net_security.filter_rules[i].rule.action = rule->action;
            g_net_security.filter_rules[i].rule.priority = rule->priority;
            g_net_security.filter_rules[i].rule.enabled = rule->enabled;
            g_net_security.filter_rules[i].rule.rate_limit = rule->rate_limit;
            g_net_security.filter_rules[i].rule.burst_limit = rule->burst_limit;
            
            return 0;
        }
    }
    
    return -1;
}

int domes_net_get_rule(const char* rule_id, domes_net_filter_rule_t* rule) {
    if (!rule_id || !rule) return -1;
    
    for (size_t i = 0; i < g_net_security.filter_rule_count; i++) {
        if (g_net_security.filter_rules[i].rule.rule_id &&
            strcmp(g_net_security.filter_rules[i].rule.rule_id, rule_id) == 0) {
            *rule = g_net_security.filter_rules[i].rule;
            return 0;
        }
    }
    
    return -1;
}

int domes_net_list_rules(domes_net_filter_rule_t** rules, size_t* count) {
    if (!rules || !count) return -1;
    
    *count = g_net_security.filter_rule_count;
    *rules = (domes_net_filter_rule_t*)malloc(*count * sizeof(domes_net_filter_rule_t));
    if (!*rules) return -1;
    
    for (size_t i = 0; i < *count; i++) {
        (*rules)[i] = g_net_security.filter_rules[i].rule;
    }
    
    return 0;
}

static int domes_match_host_pattern(const char* pattern, const char* host) {
    if (!pattern || !host) return 0;
    
    if (strcmp(pattern, "*") == 0) return 1;
    
    if (pattern[0] == '*' && pattern[1] == '.') {
        const char* suffix = pattern + 1;
        size_t host_len = strlen(host);
        size_t suffix_len = strlen(suffix);
        
        if (host_len >= suffix_len) {
            return strcmp(host + host_len - suffix_len, suffix) == 0;
        }
        return 0;
    }
    
    return strcmp(pattern, host) == 0;
}

static int domes_match_url_pattern(const char* pattern, const char* url) {
    if (!pattern || !url) return 0;
    
    if (strcmp(pattern, "*") == 0) return 1;
    
    return strstr(url, pattern) != NULL;
}

int domes_net_check_access(const char* host, uint16_t port, domes_protocol_t protocol, const char* direction) {
    if (!host) return 0;
    
    g_net_security.stats.total_connections++;
    
    if (g_net_security.config.http.enforce_https && protocol == DOMES_PROTO_HTTP) {
        g_net_security.stats.plaintext_blocked++;
        return 0;
    }
    
    for (size_t i = 0; i < g_net_security.filter_rule_count; i++) {
        domes_net_filter_rule_t* rule = &g_net_security.filter_rules[i].rule;
        
        if (!g_net_security.filter_rules[i].active || !rule->enabled) continue;
        
        int host_match = domes_match_host_pattern(rule->host_pattern, host);
        int port_match = (rule->dst_port_start == 0 && rule->dst_port_end == 0) ||
                        (port >= rule->dst_port_start && port <= rule->dst_port_end);
        int proto_match = rule->protocol == 0 || rule->protocol == protocol;
        
        if (host_match && port_match && proto_match) {
            switch (rule->action) {
                case DOMES_NET_ALLOW:
                    return 1;
                case DOMES_NET_DENY:
                    g_net_security.stats.blocked_connections++;
                    return 0;
                case DOMES_NET_LOG:
                    return 1;
                case DOMES_NET_RATE_LIMIT:
                    return 1;
            }
        }
    }
    
    return 1;
}

int domes_net_check_url(const char* url, const char* method) {
    if (!url) return 0;
    
    g_net_security.stats.http_requests++;
    
    if (g_net_security.config.http.enforce_https) {
        if (strncmp(url, "https://", 8) != 0) {
            g_net_security.stats.plaintext_blocked++;
            return 0;
        }
        g_net_security.stats.https_requests++;
    }
    
    for (size_t i = 0; i < g_net_security.filter_rule_count; i++) {
        domes_net_filter_rule_t* rule = &g_net_security.filter_rules[i].rule;
        
        if (!g_net_security.filter_rules[i].active || !rule->enabled) continue;
        
        if (rule->url_pattern && domes_match_url_pattern(rule->url_pattern, url)) {
            switch (rule->action) {
                case DOMES_NET_ALLOW:
                    return 1;
                case DOMES_NET_DENY:
                    g_net_security.stats.blocked_connections++;
                    return 0;
                default:
                    return 1;
            }
        }
    }
    
    if (g_net_security.config.http.allowed_methods) {
        int method_allowed = 0;
        for (size_t i = 0; i < g_net_security.config.http.method_count; i++) {
            if (strcmp(g_net_security.config.http.allowed_methods[i], method) == 0) {
                method_allowed = 1;
                break;
            }
        }
        if (!method_allowed) return 0;
    }
    
    return 1;
}

int domes_http_configure(const domes_http_security_config_t* config) {
    if (!config) return -1;
    g_net_security.config.http = *config;
    return 0;
}

int domes_http_validate_request(const char* method, const char* url, const char** headers,
                                size_t header_count, size_t body_size) {
    if (!method || !url) return -1;
    
    if (g_net_security.config.http.max_url_length > 0) {
        if (strlen(url) > g_net_security.config.http.max_url_length) {
            return -1;
        }
    }
    
    if (g_net_security.config.http.max_body_size > 0) {
        if (body_size > g_net_security.config.http.max_body_size) {
            return -1;
        }
    }
    
    if (g_net_security.config.http.allowed_methods) {
        int method_allowed = 0;
        for (size_t i = 0; i < g_net_security.config.http.method_count; i++) {
            if (strcmp(g_net_security.config.http.allowed_methods[i], method) == 0) {
                method_allowed = 1;
                break;
            }
        }
        if (!method_allowed) return -1;
    }
    
    if (g_net_security.config.http.forbidden_headers && headers) {
        for (size_t i = 0; i < header_count; i++) {
            for (size_t j = 0; j < g_net_security.config.http.forbidden_count; j++) {
                if (strncmp(headers[i], g_net_security.config.http.forbidden_headers[j],
                           strlen(g_net_security.config.http.forbidden_headers[j])) == 0) {
                    return -1;
                }
            }
        }
    }
    
    return 0;
}

int domes_http_add_security_headers(const char** headers, size_t header_count, size_t max_headers) {
    if (!headers) return -1;
    
    static const char* security_headers[] = {
        "Strict-Transport-Security: max-age=31536000; includeSubDomains",
        "X-Content-Type-Options: nosniff",
        "X-Frame-Options: DENY",
        "X-XSS-Protection: 1; mode=block",
        "Content-Security-Policy: default-src 'self'"
    };
    
    size_t num_sec_headers = sizeof(security_headers) / sizeof(security_headers[0]);
    size_t total = header_count + num_sec_headers;
    
    if (total > max_headers) {
        return -1;
    }
    
    for (size_t i = 0; i < num_sec_headers; i++) {
        ((char**)headers)[header_count + i] = domes_strdup(security_headers[i]);
    }
    
    return 0;
}

int domes_http_is_url_safe(const char* url) {
    if (!url) return 0;
    
    const char* dangerous_patterns[] = {
        "..",
        "//",
        "\\",
        "%00",
        "%0a",
        "%0d",
        "javascript:",
        "data:",
        "vbscript:"
    };
    
    for (size_t i = 0; i < sizeof(dangerous_patterns) / sizeof(dangerous_patterns[0]); i++) {
        if (strstr(url, dangerous_patterns[i]) != NULL) {
            return 0;
        }
    }
    
    return 1;
}

int domes_dns_configure(const domes_dns_security_config_t* config) {
    if (!config) return -1;
    g_net_security.config.dns = *config;
    return 0;
}

int domes_dns_resolve(const char* hostname, char* ip_out, size_t ip_len) {
    if (!hostname || !ip_out || ip_len == 0) return -1;
    
    g_net_security.stats.dns_queries++;
    
    if (g_net_security.config.dns.blocked_domains) {
        for (size_t i = 0; i < g_net_security.config.dns.blocked_count; i++) {
            if (strstr(hostname, g_net_security.config.dns.blocked_domains[i]) != NULL) {
                g_net_security.stats.dns_blocked++;
                return -1;
            }
        }
    }
    
    struct hostent* host = gethostbyname(hostname);
    if (!host) return -1;
    
    const char* ip = inet_ntoa(*(struct in_addr*)host->h_addr);
    if (!ip) return -1;
    
    strncpy(ip_out, ip, ip_len - 1);
    ip_out[ip_len - 1] = '\0';
    
    return 0;
}

int domes_dns_is_domain_allowed(const char* domain) {
    if (!domain) return 0;
    
    if (g_net_security.config.dns.blocked_domains) {
        for (size_t i = 0; i < g_net_security.config.dns.blocked_count; i++) {
            if (strstr(domain, g_net_security.config.dns.blocked_domains[i]) != NULL) {
                return 0;
            }
        }
    }
    
    if (g_net_security.config.dns.allowed_domains && g_net_security.config.dns.domain_count > 0) {
        for (size_t i = 0; i < g_net_security.config.dns.domain_count; i++) {
            if (strcmp(domain, g_net_security.config.dns.allowed_domains[i]) == 0) {
                return 1;
            }
        }
        return 0;
    }
    
    return 1;
}

int domes_dns_verify_dnssec(const char* domain) {
    if (!domain) return 0;
    
    (void)domain;
    
    return g_net_security.config.dns.enable_dnssec ? 1 : 0;
}

int domes_net_get_connections(domes_connection_info_t** connections, size_t* count) {
    if (!connections || !count) return -1;
    
    *count = g_net_security.connection_count;
    *connections = (domes_connection_info_t*)malloc(*count * sizeof(domes_connection_info_t));
    if (!*connections) return -1;
    
    for (size_t i = 0; i < *count; i++) {
        (*connections)[i] = g_net_security.connections[i];
    }
    
    return 0;
}

int domes_net_close_connection(const char* local_ip, uint16_t local_port,
                               const char* remote_ip, uint16_t remote_port) {
    if (!local_ip || !remote_ip) return -1;
    
    for (size_t i = 0; i < g_net_security.connection_count; i++) {
        domes_connection_info_t* conn = &g_net_security.connections[i];
        
        if (conn->local_port == local_port && conn->remote_port == remote_port &&
            strcmp(conn->local_ip, local_ip) == 0 && strcmp(conn->remote_ip, remote_ip) == 0) {
            domes_free_connection_info(conn);
            
            for (size_t j = i; j < g_net_security.connection_count - 1; j++) {
                g_net_security.connections[j] = g_net_security.connections[j + 1];
            }
            g_net_security.connection_count--;
            g_net_security.stats.active_connections--;
            return 0;
        }
    }
    
    return -1;
}

int domes_net_get_stats(domes_net_stats_t* stats) {
    if (!stats) return -1;
    *stats = g_net_security.stats;
    return 0;
}

void domes_net_reset_stats(void) {
    memset(&g_net_security.stats, 0, sizeof(g_net_security.stats));
}

int domes_net_ids_enable(bool enabled) {
    g_net_security.config.enable_ids = enabled;
    return 0;
}

int domes_net_detect_anomaly(const domes_connection_info_t* connection) {
    if (!connection) return 0;
    
    int anomaly_detected = 0;
    const char* alert_type = NULL;
    const char* details = NULL;
    
    if (connection->bytes_sent > 100 * 1024 * 1024) {
        anomaly_detected = 1;
        alert_type = "HIGH_BANDWIDTH";
        details = "Unusually high bandwidth usage detected";
    }
    
    if (!connection->is_encrypted && g_net_security.config.http.enforce_https) {
        anomaly_detected = 1;
        alert_type = "UNENCRYPTED_CONNECTION";
        details = "Unencrypted connection detected";
    }
    
    if (anomaly_detected && g_net_security.ids_callback) {
        g_net_security.ids_callback(alert_type, details, connection);
    }
    
    return anomaly_detected;
}

int domes_net_ids_set_callback(void (*callback)(const char* alert_type, const char* details,
                                                const domes_connection_info_t* conn)) {
    g_net_security.ids_callback = callback;
    return 0;
}

const char* domes_tls_version_string(domes_tls_version_t version) {
    switch (version) {
        case DOMES_TLS_1_0: return "TLS 1.0";
        case DOMES_TLS_1_1: return "TLS 1.1";
        case DOMES_TLS_1_2: return "TLS 1.2";
        case DOMES_TLS_1_3: return "TLS 1.3";
        default: return "Unknown";
    }
}

const char* domes_protocol_string(domes_protocol_t protocol) {
    switch (protocol) {
        case DOMES_PROTO_TCP: return "TCP";
        case DOMES_PROTO_UDP: return "UDP";
        case DOMES_PROTO_HTTP: return "HTTP";
        case DOMES_PROTO_HTTPS: return "HTTPS";
        case DOMES_PROTO_WEBSOCKET: return "WebSocket";
        case DOMES_PROTO_DNS: return "DNS";
        default: return "Unknown";
    }
}

const char* domes_cert_result_string(domes_cert_result_t result) {
    switch (result) {
        case DOMES_CERT_OK: return "Certificate valid";
        case DOMES_CERT_INVALID: return "Certificate invalid";
        case DOMES_CERT_EXPIRED: return "Certificate expired";
        case DOMES_CERT_REVOKED: return "Certificate revoked";
        case DOMES_CERT_UNTRUSTED: return "Certificate untrusted";
        case DOMES_CERT_HOST_MISMATCH: return "Hostname mismatch";
        case DOMES_CERT_SELF_SIGNED: return "Self-signed certificate";
        default: return "Unknown";
    }
}

int domes_net_parse_url(const char* url, char* scheme, char* host, uint16_t* port, char* path) {
    if (!url) return -1;
    
    const char* p = url;
    
    const char* colon = strstr(p, "://");
    if (colon && scheme) {
        size_t scheme_len = colon - p;
        strncpy(scheme, p, scheme_len);
        scheme[scheme_len] = '\0';
        p = colon + 3;
    }
    
    const char* slash = strchr(p, '/');
    const char* port_colon = strchr(p, ':');
    
    if (host) {
        size_t host_len;
        if (port_colon && (!slash || port_colon < slash)) {
            host_len = port_colon - p;
        } else if (slash) {
            host_len = slash - p;
        } else {
            host_len = strlen(p);
        }
        strncpy(host, p, host_len);
        host[host_len] = '\0';
    }
    
    if (port_colon && (!slash || port_colon < slash)) {
        if (port) {
            *port = (uint16_t)atoi(port_colon + 1);
        }
        p = port_colon + 1;
        while (*p && *p != '/') p++;
    } else {
        if (port) {
            if (scheme && strcmp(scheme, "https") == 0) {
                *port = 443;
            } else if (scheme && strcmp(scheme, "http") == 0) {
                *port = 80;
            } else {
                *port = 0;
            }
        }
    }
    
    if (path) {
        if (slash) {
            strcpy(path, slash);
        } else {
            strcpy(path, "/");
        }
    }
    
    return 0;
}

int domes_net_ip_in_cidr(const char* ip, const char* cidr) {
    if (!ip || !cidr) return 0;
    
    char cidr_copy[64];
    strncpy(cidr_copy, cidr, sizeof(cidr_copy) - 1);
    cidr_copy[sizeof(cidr_copy) - 1] = '\0';
    
    char* slash = strchr(cidr_copy, '/');
    if (!slash) return strcmp(ip, cidr_copy) == 0 ? 1 : 0;
    
    *slash = '\0';
    int prefix_len = atoi(slash + 1);
    
    struct in_addr ip_addr, cidr_addr;
    if (inet_pton(AF_INET, ip, &ip_addr) != 1) return 0;
    if (inet_pton(AF_INET, cidr_copy, &cidr_addr) != 1) return 0;
    
    uint32_t mask = prefix_len == 0 ? 0 : (~0U << (32 - prefix_len));
    uint32_t ip_net = ntohl(ip_addr.s_addr) & mask;
    uint32_t cidr_net = ntohl(cidr_addr.s_addr) & mask;
    
    return ip_net == cidr_net ? 1 : 0;
}

int domes_net_validate_ip(const char* ip) {
    if (!ip) return 0;
    
    struct in_addr addr;
    return inet_pton(AF_INET, ip, &addr) == 1 ? 1 : 0;
}

int domes_net_validate_port(uint16_t port) {
    (void)port;
    return 1;
}
