/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * network_filter.c - Network Filter and Access Control Module Implementation
 */

#include "network_filter.h"
#include "utils/cupolas_utils.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_FILTER_RULES 512
#define MAX_CONNECTIONS 1024

typedef struct {
    cupolas_net_filter_rule_t rule;
    int active;
} filter_rule_entry_t;

static struct {
    int initialized;
    filter_rule_entry_t filter_rules[MAX_FILTER_RULES];
    size_t filter_rule_count;
    cupolas_connection_info_t connections[MAX_CONNECTIONS];
    size_t connection_count;
    cupolas_net_stats_t stats;
} g_filter = {0};

int network_filter_init(void) {
    if (g_filter.initialized) return 0;
    
    memset(&g_filter, 0, sizeof(g_filter));
    g_filter.initialized = 1;
    return 0;
}

void network_filter_cleanup(void) {
    if (!g_filter.initialized) return;
    
    for (size_t i = 0; i < g_filter.filter_rule_count; i++) {
        cupolas_net_filter_rule_t* rule = &g_filter.filter_rules[i].rule;
        free(rule->rule_id);
        free(rule->description);
        free(rule->src_ip_pattern);
        free(rule->dst_ip_pattern);
        free(rule->host_pattern);
        free(rule->url_pattern);
    }
    
    for (size_t i = 0; i < g_filter.connection_count; i++) {
        cupolas_connection_info_t* conn = &g_filter.connections[i];
        free(conn->local_ip);
        free(conn->remote_ip);
        free(conn->hostname);
        free(conn->cipher_suite);
    }
    
    memset(&g_filter, 0, sizeof(g_filter));
}

static int match_host_pattern(const char* pattern, const char* host) {
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

static int match_url_pattern(const char* pattern, const char* url) {
    if (!pattern || !url) return 0;
    
    if (strcmp(pattern, "*") == 0) return 1;
    
    return strstr(url, pattern) != NULL;
}

int network_filter_add_rule(const cupolas_net_filter_rule_t* rule) {
    if (!rule) return -1;
    if (g_filter.filter_rule_count >= MAX_FILTER_RULES) return -1;
    
    filter_rule_entry_t* entry = &g_filter.filter_rules[g_filter.filter_rule_count];
    memset(entry, 0, sizeof(*entry));
    
    entry->rule.rule_id = cupolas_strdup(rule->rule_id);
    entry->rule.description = cupolas_strdup(rule->description);
    entry->rule.src_ip_pattern = cupolas_strdup(rule->src_ip_pattern);
    entry->rule.dst_ip_pattern = cupolas_strdup(rule->dst_ip_pattern);
    entry->rule.src_port_start = rule->src_port_start;
    entry->rule.src_port_end = rule->src_port_end;
    entry->rule.dst_port_start = rule->dst_port_start;
    entry->rule.dst_port_end = rule->dst_port_end;
    entry->rule.protocol = rule->protocol;
    entry->rule.host_pattern = cupolas_strdup(rule->host_pattern);
    entry->rule.url_pattern = cupolas_strdup(rule->url_pattern);
    entry->rule.action = rule->action;
    entry->rule.priority = rule->priority;
    entry->rule.enabled = rule->enabled;
    entry->rule.rate_limit = rule->rate_limit;
    entry->rule.burst_limit = rule->burst_limit;
    entry->active = 1;
    
    g_filter.filter_rule_count++;
    return 0;
}

int network_filter_remove_rule(const char* rule_id) {
    if (!rule_id) return -1;
    
    for (size_t i = 0; i < g_filter.filter_rule_count; i++) {
        if (g_filter.filter_rules[i].rule.rule_id &&
            strcmp(g_filter.filter_rules[i].rule.rule_id, rule_id) == 0) {
            cupolas_net_filter_rule_t* rule = &g_filter.filter_rules[i].rule;
            free(rule->rule_id);
            free(rule->description);
            free(rule->src_ip_pattern);
            free(rule->dst_ip_pattern);
            free(rule->host_pattern);
            free(rule->url_pattern);
            
            for (size_t j = i; j < g_filter.filter_rule_count - 1; j++) {
                g_filter.filter_rules[j] = g_filter.filter_rules[j + 1];
            }
            g_filter.filter_rule_count--;
            return 0;
        }
    }
    
    return -1;
}

int network_filter_update_rule(const char* rule_id, const cupolas_net_filter_rule_t* rule) {
    if (!rule_id || !rule) return -1;
    
    for (size_t i = 0; i < g_filter.filter_rule_count; i++) {
        if (g_filter.filter_rules[i].rule.rule_id &&
            strcmp(g_filter.filter_rules[i].rule.rule_id, rule_id) == 0) {
            cupolas_net_filter_rule_t* old_rule = &g_filter.filter_rules[i].rule;
            free(old_rule->rule_id);
            free(old_rule->description);
            free(old_rule->src_ip_pattern);
            free(old_rule->dst_ip_pattern);
            free(old_rule->host_pattern);
            free(old_rule->url_pattern);
            
            old_rule->rule_id = cupolas_strdup(rule->rule_id);
            old_rule->description = cupolas_strdup(rule->description);
            old_rule->src_ip_pattern = cupolas_strdup(rule->src_ip_pattern);
            old_rule->dst_ip_pattern = cupolas_strdup(rule->dst_ip_pattern);
            old_rule->src_port_start = rule->src_port_start;
            old_rule->src_port_end = rule->src_port_end;
            old_rule->dst_port_start = rule->dst_port_start;
            old_rule->dst_port_end = rule->dst_port_end;
            old_rule->protocol = rule->protocol;
            old_rule->host_pattern = cupolas_strdup(rule->host_pattern);
            old_rule->url_pattern = cupolas_strdup(rule->url_pattern);
            old_rule->action = rule->action;
            old_rule->priority = rule->priority;
            old_rule->enabled = rule->enabled;
            old_rule->rate_limit = rule->rate_limit;
            old_rule->burst_limit = rule->burst_limit;
            
            return 0;
        }
    }
    
    return -1;
}

int network_filter_get_rule(const char* rule_id, cupolas_net_filter_rule_t* rule) {
    if (!rule_id || !rule) return -1;
    
    for (size_t i = 0; i < g_filter.filter_rule_count; i++) {
        if (g_filter.filter_rules[i].rule.rule_id &&
            strcmp(g_filter.filter_rules[i].rule.rule_id, rule_id) == 0) {
            *rule = g_filter.filter_rules[i].rule;
            return 0;
        }
    }
    
    return -1;
}

int network_filter_list_rules(cupolas_net_filter_rule_t** rules, size_t* count) {
    if (!rules || !count) return -1;
    
    *count = g_filter.filter_rule_count;
    *rules = (cupolas_net_filter_rule_t*)malloc(*count * sizeof(cupolas_net_filter_rule_t));
    if (!*rules) return -1;
    
    for (size_t i = 0; i < *count; i++) {
        (*rules)[i] = g_filter.filter_rules[i].rule;
    }
    
    return 0;
}

int network_filter_check_access(const char* host, uint16_t port, 
                               cupolas_protocol_t protocol, const char* direction) {
    if (!host) return 0;
    
    g_filter.stats.total_connections++;
    
    for (size_t i = 0; i < g_filter.filter_rule_count; i++) {
        cupolas_net_filter_rule_t* rule = &g_filter.filter_rules[i].rule;
        
        if (!g_filter.filter_rules[i].active || !rule->enabled) continue;
        
        int host_match = match_host_pattern(rule->host_pattern, host);
        int port_match = (rule->dst_port_start == 0 && rule->dst_port_end == 0) ||
                        (port >= rule->dst_port_start && port <= rule->dst_port_end);
        int proto_match = rule->protocol == 0 || rule->protocol == protocol;
        
        if (host_match && port_match && proto_match) {
            switch (rule->action) {
                case CUPOLAS_NET_ALLOW:
                    return 1;
                case CUPOLAS_NET_DENY:
                    g_filter.stats.blocked_connections++;
                    return 0;
                case CUPOLAS_NET_LOG:
                    return 1;
                case CUPOLAS_NET_RATE_LIMIT:
                    return 1;
            }
        }
    }
    
    return 1;
}

int network_filter_check_url(const char* url, const char* method) {
    if (!url) return 0;
    
    g_filter.stats.http_requests++;
    
    if (strncmp(url, "https://", 8) != 0) {
        g_filter.stats.plaintext_blocked++;
        return 0;
    }
    g_filter.stats.https_requests++;
    
    for (size_t i = 0; i < g_filter.filter_rule_count; i++) {
        cupolas_net_filter_rule_t* rule = &g_filter.filter_rules[i].rule;
        
        if (!g_filter.filter_rules[i].active || !rule->enabled) continue;
        
        if (rule->url_pattern && match_url_pattern(rule->url_pattern, url)) {
            switch (rule->action) {
                case CUPOLAS_NET_ALLOW:
                    return 1;
                case CUPOLAS_NET_DENY:
                    g_filter.stats.blocked_connections++;
                    return 0;
                default:
                    return 1;
            }
        }
    }
    
    return 1;
}

int network_filter_get_connections(cupolas_connection_info_t** connections, size_t* count) {
    if (!connections || !count) return -1;
    
    *count = g_filter.connection_count;
    *connections = (cupolas_connection_info_t*)malloc(*count * sizeof(cupolas_connection_info_t));
    if (!*connections) return -1;
    
    for (size_t i = 0; i < *count; i++) {
        (*connections)[i] = g_filter.connections[i];
    }
    
    return 0;
}

int network_filter_close_connection(const char* local_ip, uint16_t local_port,
                                   const char* remote_ip, uint16_t remote_port) {
    if (!local_ip || !remote_ip) return -1;
    
    for (size_t i = 0; i < g_filter.connection_count; i++) {
        cupolas_connection_info_t* conn = &g_filter.connections[i];
        
        if (conn->local_port == local_port && conn->remote_port == remote_port &&
            strcmp(conn->local_ip, local_ip) == 0 && strcmp(conn->remote_ip, remote_ip) == 0) {
            free(conn->local_ip);
            free(conn->remote_ip);
            free(conn->hostname);
            free(conn->cipher_suite);
            memset(conn, 0, sizeof(*conn));
            
            for (size_t j = i; j < g_filter.connection_count - 1; j++) {
                g_filter.connections[j] = g_filter.connections[j + 1];
            }
            g_filter.connection_count--;
            g_filter.stats.active_connections--;
            return 0;
        }
    }
    
    return -1;
}

int network_filter_get_stats(cupolas_net_stats_t* stats) {
    if (!stats) return -1;
    *stats = g_filter.stats;
    return 0;
}

void network_filter_reset_stats(void) {
    memset(&g_filter.stats, 0, sizeof(g_filter.stats));
}
