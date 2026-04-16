/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * dns_security.c - DNS Security Module Implementation
 */

#include "dns_security.h"
#include "utils/cupolas_utils.h"
#include <stdlib.h>
#include <string.h>
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
#endif

static struct {
    int initialized;
    cupolas_dns_security_config_t config;
} g_dns = {0};

int dns_security_init(const cupolas_dns_security_config_t* config) {
    if (g_dns.initialized) return 0;
    
    memset(&g_dns, 0, sizeof(g_dns));
    
    if (config) {
        g_dns.config = *config;
    } else {
        g_dns.config.enable_dnssec = true;
    }
    
    g_dns.initialized = 1;
    return 0;
}

void dns_security_cleanup(void) {
    if (!g_dns.initialized) return;
    
    if (g_dns.config.allowed_domains) {
        for (size_t i = 0; i < g_dns.config.domain_count; i++) {
            free(g_dns.config.allowed_domains[i]);
        }
        free(g_dns.config.allowed_domains);
    }
    
    if (g_dns.config.blocked_domains) {
        for (size_t i = 0; i < g_dns.config.blocked_count; i++) {
            free(g_dns.config.blocked_domains[i]);
        }
        free(g_dns.config.blocked_domains);
    }
    
    if (g_dns.config.dns_servers) {
        for (size_t i = 0; i < g_dns.config.dns_server_count; i++) {
            free(g_dns.config.dns_servers[i]);
        }
        free(g_dns.config.dns_servers);
    }
    
    memset(&g_dns, 0, sizeof(g_dns));
}

int dns_configure(const cupolas_dns_security_config_t* config) {
    if (!config) return -1;
    g_dns.config = *config;
    return 0;
}

int dns_resolve(const char* hostname, char* ip_out, size_t ip_len) {
    if (!hostname || !ip_out || ip_len == 0) return -1;
    
    if (g_dns.config.blocked_domains) {
        for (size_t i = 0; i < g_dns.config.blocked_count; i++) {
            if (strstr(hostname, g_dns.config.blocked_domains[i]) != NULL) {
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

int dns_is_domain_allowed(const char* domain) {
    if (!domain) return 0;
    
    if (g_dns.config.blocked_domains) {
        for (size_t i = 0; i < g_dns.config.blocked_count; i++) {
            if (strstr(domain, g_dns.config.blocked_domains[i]) != NULL) {
                return 0;
            }
        }
    }
    
    if (g_dns.config.allowed_domains && g_dns.config.domain_count > 0) {
        for (size_t i = 0; i < g_dns.config.domain_count; i++) {
            if (strcmp(domain, g_dns.config.allowed_domains[i]) == 0) {
                return 1;
            }
        }
        return 0;
    }
    
    return 1;
}

int dns_verify_dnssec(const char* domain) {
    if (!domain) return 0;
    
    (void)domain;
    
    return g_dns.config.enable_dnssec ? 1 : 0;
}
