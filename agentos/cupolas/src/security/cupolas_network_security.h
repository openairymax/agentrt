/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * cupolas_network_security.h - Network Security: TLS, Firewall, and Network Access Control
 */

#ifndef CUPOLAS_NETWORK_SECURITY_H
#define CUPOLAS_NETWORK_SECURITY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief TLS version enumeration
 * 
 * Design principles:
 * - Strong encryption: Only modern TLS versions
 * - Perfect forward secrecy: Ephemeral key exchange
 * - Certificate validation: Strict chain verification
 * - Cipher suite control: Only strong ciphers allowed
 */
typedef enum {
    CUPOLAS_TLS_AUTO = 0,             /**< Auto-negotiate best version */
    CUPOLAS_TLS_1_2 = 1,              /**< TLS 1.2 */
    CUPOLAS_TLS_1_3 = 2               /**< TLS 1.3 */
} cupolas_tls_version_t;

/**
 * @brief Certificate validation mode
 */
typedef enum {
    CUPOLAS_CERT_NONE = 0,            /**< No validation */
    CUPOLAS_CERT_OPTIONAL,            /**< Optional client cert */
    CUPOLAS_CERT_REQUIRED             /**< Required client cert */
} cupolas_cert_mode_t;

/**
 * @brief Firewall action
 */
typedef enum {
    CUPOLAS_FW_ALLOW = 0,             /**< Allow connection */
    CUPOLAS_FW_DENY,                  /**< Deny connection */
    CUPOLAS_FW_LOG,                   /**< Log only */
    CUPOLAS_FW_RATE_LIMIT             /**< Rate limit */
} cupolas_fw_action_t;

/**
 * @brief Network protocol
 */
typedef enum {
    CUPOLAS_PROTO_ANY = 0,            /**< Any protocol */
    CUPOLAS_PROTO_TCP,                /**< TCP */
    CUPOLAS_PROTO_UDP,                /**< UDP */
    CUPOLAS_PROTO_ICMP                /**< ICMP */
} cupolas_proto_t;

/**
 * @brief Connection direction
 */
typedef enum {
    CUPOLAS_DIR_ANY = 0,              /**< Any direction */
    CUPOLAS_DIR_INBOUND,              /**< Inbound */
    CUPOLAS_DIR_OUTBOUND              /**< Outbound */
} cupolas_direction_t;

/**
 * @brief TLS configuration
 */
typedef struct {
    cupolas_tls_version_t min_version;    /**< Minimum TLS version */
    cupolas_tls_version_t max_version;    /**< Maximum TLS version */
    
    char** cipher_suites;                 /**< Allowed cipher suites */
    size_t cipher_count;                  /**< Number of ciphers */
    
    const char* ca_file;                  /**< CA certificate file */
    const char* ca_path;                  /**< CA certificate path */
    const char* cert_file;                /**< Certificate file */
    const char* key_file;                 /**< Private key file */
    
    cupolas_cert_mode_t verify_mode;      /**< Verification mode */
    bool verify_hostname;                 /**< Verify hostname */
    bool verify_depth;                    /**< Verification depth */
    
    bool enable_ocsp_stapling;            /**< Enable OCSP stapling */
    bool enable_sct;                      /**< Enable SCT */
    bool enable_session_tickets;          /**< Enable session tickets */
    uint32_t session_cache_size;          /**< Session cache size */
    bool enable_logging;                  /**< Enable logging */
    bool enable_audit;                    /**< Enable audit */
    char* ca_bundle_path;                 /**< CA bundle path */
    char* client_cert_path;               /**< Client certificate path */
    char* client_key_path;                /**< Client key path */
    struct {
        cupolas_tls_version_t min_version;
        cupolas_tls_version_t max_version;
    } tls;
    
    uint32_t handshake_timeout_ms;        /**< Handshake timeout */
    uint32_t read_timeout_ms;             /**< Read timeout */
    uint32_t write_timeout_ms;            /**< Write timeout */

    bool enable_ids;                      /**< Enable IDS */
    struct {
        bool enforce_https;
        bool enable_dnssec;
        char dns_server[64];
        char** blocked_domains;
        size_t blocked_count;
        char** allowed_domains;
        size_t domain_count;
        size_t max_url_length;
        size_t max_body_size;
        char** allowed_methods;
        size_t method_count;
        char** forbidden_headers;
        size_t forbidden_count;
        bool hsts_enabled;
        uint32_t hsts_max_age;
    } http;
    struct {
        bool enable_dnssec;
        char upstream_server[64];
        char** blocked_domains;
        size_t blocked_count;
        char** allowed_domains;
        size_t domain_count;
    } dns;
} cupolas_tls_config_t;

/**
 * @brief Firewall rule structure
 */
typedef struct {
    char* rule_id;                       /**< Rule identifier */
    cupolas_proto_t protocol;             /**< Protocol */
    cupolas_direction_t direction;        /**< Direction */
    
    char* src_ip;                       /**< Source IP/CIDR */
    char* src_port;                     /**< Source port range */
    char* dst_ip;                       /**< Destination IP/CIDR */
    char* dst_port;                     /**< Destination port range */
    
    cupolas_fw_action_t action;           /**< Action */
    bool log;                           /**< Enable logging */
    uint32_t rate_limit;                /**< Rate limit (connections/sec) */
    
    uint64_t valid_from;                /**< Valid from timestamp */
    uint64_t valid_until;               /**< Valid until timestamp */
    
    char* description;                  /**< Rule description */
    char* src_ip_pattern;               /**< Source IP pattern */
    char* dst_ip_pattern;               /**< Destination IP pattern */
    char* host_pattern;                 /**< Host pattern */
    char* url_pattern;                  /**< URL pattern */
    bool enabled;                       /**< Rule enabled */
    uint16_t dst_port_start;            /**< Destination port range start */
    uint16_t dst_port_end;              /**< Destination port range end */
    uint16_t src_port_start;            /**< Source port range start */
    uint16_t src_port_end;              /**< Source port range end */
    int priority;                       /**< Rule priority */
    uint32_t burst_limit;               /**< Burst limit for rate limiting */
} cupolas_fw_rule_t;

typedef struct {
    char local_ip[64];
    uint16_t local_port;
    char remote_ip[64];
    uint16_t remote_port;
    cupolas_proto_t protocol;
    bool is_encrypted;
    uint64_t bytes_sent;
    uint64_t bytes_received;
    uint64_t connect_time;
    char hostname[256];
    char cipher_suite[128];
    struct {
        bool enforce_https;
    } http;
} cupolas_connection_info_t;

/**
 * @brief Firewall configuration
 */
typedef struct {
    bool enable;                        /**< Enable firewall */
    cupolas_fw_action_t default_inbound;  /**< Default inbound action */
    cupolas_fw_action_t default_outbound; /**< Default outbound action */
    
    cupolas_fw_rule_t* rules;           /**< Rules array */
    size_t rule_count;                  /**< Number of rules */
    
    bool enable_logging;                /**< Enable logging */
    bool enable_rate_limiting;          /**< Enable rate limiting */
    
    uint32_t max_connections;           /**< Maximum connections */
    uint32_t connection_timeout_ms;     /**< Connection timeout */
} cupolas_firewall_config_t;

/**
 * @brief Network security statistics
 */
typedef struct {
    uint64_t total_connections;
    uint64_t active_connections;
    uint64_t tls_handshakes;
    uint64_t tls_failures;
    uint64_t firewall_blocks;
    uint64_t rate_limit_hits;
    uint64_t cert_errors;
    uint64_t hostname_mismatches;
    uint64_t dns_queries;
    uint64_t dns_blocked;
    uint64_t http_requests;
    uint64_t https_requests;
    uint64_t plaintext_blocked;
    uint64_t blocked_connections;
} cupolas_net_stats_t;

typedef cupolas_fw_rule_t cupolas_net_filter_rule_t;
typedef struct {
    bool enable_ids;
    cupolas_tls_config_t manager;
} cupolas_net_security_config_t;
typedef struct {
    bool enable_ids;
} cupolas_ids_config_t;
typedef struct {
    bool enable_dnssec;
    char upstream_server[64];
} cupolas_dns_security_config_t;
typedef struct {
    bool enforce_https;
    size_t max_url_length;
    size_t max_body_size;
    char** allowed_methods;
    size_t method_count;
    char** forbidden_headers;
    size_t forbidden_count;
} cupolas_http_security_config_t;

/**
 * @brief Initialize network security module
 * @param[in] config Configuration (NULL for defaults)
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads (initialization only)
 * @reentrant No
 * @ownership config: caller retains ownership
 */
int cupolas_network_security_init(const cupolas_tls_config_t* config);

/**
 * @brief Shutdown network security module
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other operations)
 * @reentrant No
 */
void cupolas_network_security_cleanup(void);

/**
 * @brief Initialize TLS context
 * @param[in] config TLS configuration
 * @return TLS context handle, NULL on failure
 * @note Thread-safe: Safe to call from multiple threads (initialization only)
 * @reentrant No
 * @ownership config: caller retains ownership
 */
void* cupolas_tls_context_create(const cupolas_tls_config_t* config);

/**
 * @brief Free TLS context
 * @param[in] ctx TLS context (may be NULL)
 * @note Thread-safe: Safe to call from multiple threads
 * @reentrant No
 * @ownership ctx: transferred to this function, will be freed
 */
void cupolas_tls_context_free(void* ctx);

/**
 * @brief Create TLS client connection
 * @param[in] ctx TLS context
 * @param[in] host Hostname
 * @param[in] port Port number
 * @param[out] error Error output (may be NULL)
 * @return TLS connection handle, NULL on failure
 * @note Thread-safe: Safe to call from multiple threads
 * @reentrant No
 * @ownership ctx, host: caller retains ownership
 * @ownership error: caller provides buffer, function writes to it
 */
void* cupolas_tls_client_connect(void* ctx, const char* host, uint16_t port, int* error);

/**
 * @brief Create TLS server connection (accept)
 * @param[in] ctx TLS context
 * @param[in] socket_fd Server socket file descriptor
 * @param[out] error Error output (may be NULL)
 * @return TLS connection handle, NULL on failure
 * @note Thread-safe: Safe to call from multiple threads
 * @reentrant No
 * @ownership ctx: caller retains ownership
 * @ownership error: caller provides buffer, function writes to it
 */
void* cupolas_tls_server_accept(void* ctx, int socket_fd, int* error);

/**
 * @brief Close TLS connection
 * @param[in] conn TLS connection (may be NULL)
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads
 * @reentrant No
 * @ownership conn: transferred to this function, will be closed
 */
int cupolas_tls_close(void* conn);

/**
 * @brief Read from TLS connection
 * @param[in] conn TLS connection
 * @param[out] buf Output buffer
 * @param[in] len Buffer length
 * @return Bytes read, negative on error
 * @note Thread-safe: Safe to call from multiple threads
 * @reentrant No
 * @ownership conn: caller retains ownership
 * @ownership buf: caller provides buffer, function writes to it
 */
int cupolas_tls_read(void* conn, char* buf, size_t len);

/**
 * @brief Write to TLS connection
 * @param[in] conn TLS connection
 * @param[in] buf Input buffer
 * @param[in] len Buffer length
 * @return Bytes written, negative on error
 * @note Thread-safe: Safe to call from multiple threads
 * @reentrant No
 * @ownership conn and buf: caller retains ownership
 */
int cupolas_tls_write(void* conn, const char* buf, size_t len);

/**
 * @brief Get peer certificate
 * @param[in] conn TLS connection
 * @param[out] cert_out Certificate output (PEM format)
 * @param[in,out] cert_len Buffer size / actual length
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership conn: caller retains ownership
 * @ownership cert_out: caller provides buffer, function writes to it
 */
int cupolas_tls_get_peer_cert(void* conn, char* cert_out, size_t* cert_len);

/**
 * @brief Verify peer certificate
 * @param[in] conn TLS connection
 * @param[in] hostname Expected hostname
 * @return 0 if valid, negative if invalid
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership conn and hostname: caller retains ownership
 */
int cupolas_tls_verify_peer(void* conn, const char* hostname);

/**
 * @brief Get TLS version
 * @param[in] conn TLS connection
 * @return TLS version string (static, do not free)
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 */
const char* cupolas_tls_get_version(void* conn);

/**
 * @brief Get cipher suite
 * @param[in] conn TLS connection
 * @return Cipher suite name (static, do not free)
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 */
const char* cupolas_tls_get_cipher(void* conn);

/**
 * @brief Enable firewall
 * @param[in] config Firewall configuration
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other operations)
 * @reentrant No
 * @ownership config: caller retains ownership
 */
int cupolas_firewall_enable(const cupolas_firewall_config_t* config);

/**
 * @brief Disable firewall
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other operations)
 * @reentrant No
 */
int cupolas_firewall_disable(void);

/**
 * @brief Add firewall rule
 * @param[in] rule Firewall rule
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other operations)
 * @reentrant No
 * @ownership rule: caller retains ownership
 */
int cupolas_firewall_add_rule(const cupolas_fw_rule_t* rule);

/**
 * @brief Remove firewall rule
 * @param[in] rule_id Rule identifier
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other operations)
 * @reentrant No
 */
int cupolas_firewall_remove_rule(const char* rule_id);

/**
 * @brief Check if connection is allowed
 * @param[in] protocol Protocol
 * @param[in] direction Direction
 * @param[in] src_ip Source IP
 * @param[in] src_port Source port
 * @param[in] dst_ip Destination IP
 * @param[in] dst_port Destination port
 * @return CUPOLAS_FW_ALLOW if allowed, other action if denied
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership src_ip and dst_ip: caller retains ownership
 */
cupolas_fw_action_t cupolas_firewall_check(const cupolas_proto_t protocol,
                                          const cupolas_direction_t direction,
                                          const char* src_ip,
                                          uint16_t src_port,
                                          const char* dst_ip,
                                          uint16_t dst_port);

/**
 * @brief Get firewall rules
 * @param[out] rules Rules array output
 * @param[out] count Number of rules
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership rules: caller provides buffer, function writes to it
 * @ownership count: caller provides buffer, function writes to it
 */
int cupolas_firewall_get_rules(cupolas_fw_rule_t** rules, size_t* count);

/**
 * @brief Clear all firewall rules
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other operations)
 * @reentrant No
 */
int cupolas_firewall_clear_rules(void);

/**
 * @brief Get firewall statistics
 * @param[out] stats Statistics output
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership stats: caller provides buffer, function writes to it
 */
int cupolas_firewall_get_stats(cupolas_net_stats_t* stats);

/**
 * @brief Get network security statistics
 * @param[out] stats Statistics output
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership stats: caller provides buffer, function writes to it
 */
int cupolas_network_security_get_stats(cupolas_net_stats_t* stats);

/**
 * @brief Validate certificate
 * @param[in] cert_pem Certificate in PEM format
 * @param[in] ca_file CA bundle file
 * @param[out] error Error message output (may be NULL)
 * @return 0 if valid, negative if invalid
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership cert_pem, ca_file, and error: caller retains ownership
 */
int cupolas_cert_validate(const char* cert_pem, const char* ca_file, char* error);

/**
 * @brief Extract certificate subject
 * @param[in] cert_pem Certificate in PEM format
 * @param[out] subject Subject output buffer
 * @param[in] subject_len Buffer length
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership cert_pem: caller retains ownership
 * @ownership subject: caller provides buffer, function writes to it
 */
int cupolas_cert_get_subject(const char* cert_pem, char* subject, size_t subject_len);

/**
 * @brief Extract certificate issuer
 * @param[in] cert_pem Certificate in PEM format
 * @param[out] issuer Issuer output buffer
 * @param[in] issuer_len Buffer length
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership cert_pem: caller retains ownership
 * @ownership issuer: caller provides buffer, function writes to it
 */
int cupolas_cert_get_issuer(const char* cert_pem, char* issuer, size_t issuer_len);

/**
 * @brief Get certificate validity period
 * @param[in] cert_pem Certificate in PEM format
 * @param[out] not_before Validity start timestamp
 * @param[out] not_after Validity end timestamp
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership cert_pem: caller retains ownership
 * @ownership not_before and not_after: caller provides buffers, function writes to them
 */
int cupolas_cert_get_validity(const char* cert_pem, uint64_t* not_before, uint64_t* not_after);

/**
 * @brief Check if certificate is expired
 * @param[in] cert_pem Certificate in PEM format
 * @return 1 if expired, 0 if valid
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership cert_pem: caller retains ownership
 */
int cupolas_cert_is_expired(const char* cert_pem);

/**
 * @brief Verify hostname against certificate
 * @param[in] cert_pem Certificate in PEM format
 * @param[in] hostname Hostname to verify
 * @return 0 if matches, negative if mismatch
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership cert_pem and hostname: caller retains ownership
 */
int cupolas_cert_verify_hostname(const char* cert_pem, const char* hostname);

/**
 * @brief Load certificate from file
 * @param[in] cert_path Certificate file path
 * @param[out] cert_pem Certificate in PEM format output
 * @param[in,out] pem_len Buffer size / actual length
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership cert_path: caller retains ownership
 * @ownership cert_pem: caller provides buffer, function writes to it
 */
int cupolas_cert_load_file(const char* cert_path, char* cert_pem, size_t* pem_len);

/**
 * @brief Save certificate to file
 * @param[in] cert_pem Certificate in PEM format
 * @param[in] cert_path Output file path
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership cert_pem and cert_path: caller retains ownership
 */
int cupolas_cert_save_file(const char* cert_pem, const char* cert_path);

#ifdef __cplusplus
}
#endif

#endif /* CUPOLAS_NETWORK_SECURITY_H */
