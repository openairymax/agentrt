/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * network_filter.h - Network Filter and Access Control Module Interface
 */

#ifndef CUPOLAS_NETWORK_FILTER_H
#define CUPOLAS_NETWORK_FILTER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CUPOLAS_NET_ALLOW = 0,
    CUPOLAS_NET_DENY,
    CUPOLAS_NET_LOG,
    CUPOLAS_NET_RATE_LIMIT
} cupolas_net_action_t;

typedef enum {
    CUPOLAS_PROTO_ANY = 0,
    CUPOLAS_PROTO_TCP,
    CUPOLAS_PROTO_UDP,
    CUPOLAS_PROTO_HTTP,
    CUPOLAS_PROTO_HTTPS,
    CUPOLAS_PROTO_WEBSOCKET,
    CUPOLAS_PROTO_DNS
} cupolas_protocol_t;

typedef struct {
    char* rule_id;
    char* description;
    char* src_ip_pattern;
    char* dst_ip_pattern;
    uint16_t src_port_start;
    uint16_t src_port_end;
    uint16_t dst_port_start;
    uint16_t dst_port_end;
    cupolas_protocol_t protocol;
    char* host_pattern;
    char* url_pattern;
    cupolas_net_action_t action;
    uint32_t priority;
    bool enabled;
    uint32_t rate_limit;
    uint32_t burst_limit;
} cupolas_net_filter_rule_t;

typedef struct {
    char* local_ip;
    uint16_t local_port;
    char* remote_ip;
    uint16_t remote_port;
    char* hostname;
    char* cipher_suite;
    bool is_encrypted;
    uint64_t bytes_sent;
    uint64_t bytes_received;
    uint64_t start_time;
} cupolas_connection_info_t;

typedef struct {
    uint64_t total_connections;
    uint64_t active_connections;
    uint64_t blocked_connections;
    uint64_t plaintext_blocked;
    uint64_t https_requests;
    uint64_t http_requests;
    uint64_t dns_queries;
    uint64_t dns_blocked;
} cupolas_net_stats_t;

int network_filter_init(void);
void network_filter_cleanup(void);

int network_filter_add_rule(const cupolas_net_filter_rule_t* rule);
int network_filter_remove_rule(const char* rule_id);
int network_filter_update_rule(const char* rule_id, const cupolas_net_filter_rule_t* rule);
int network_filter_get_rule(const char* rule_id, cupolas_net_filter_rule_t* rule);
int network_filter_list_rules(cupolas_net_filter_rule_t** rules, size_t* count);

int network_filter_check_access(const char* host, uint16_t port, 
                                cupolas_protocol_t protocol, const char* direction);
int network_filter_check_url(const char* url, const char* method);

int network_filter_get_connections(cupolas_connection_info_t** connections, size_t* count);
int network_filter_close_connection(const char* local_ip, uint16_t local_port,
                                   const char* remote_ip, uint16_t remote_port);

int network_filter_get_stats(cupolas_net_stats_t* stats);
void network_filter_reset_stats(void);

#ifdef __cplusplus
}
#endif

#endif /* CUPOLAS_NETWORK_FILTER_H */
