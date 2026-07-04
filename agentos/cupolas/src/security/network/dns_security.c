/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * dns_security.c - DNS Security Module Implementation
 */

#include "dns_security.h"

#include "memory_compat.h"
#include "utils/cupolas_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include "error.h"

static struct {
    int initialized;
    cupolas_dns_security_config_t config;
} g_dns = {0};

int dns_security_init(const cupolas_dns_security_config_t *config)
{
    if (g_dns.initialized)
        return 0;

    __builtin_memset(&g_dns, 0, sizeof(g_dns));

    if (config) {
        g_dns.config = *config;
    } else {
        g_dns.config.enable_dnssec = true;
    }

    g_dns.initialized = 1;
    return 0;
}

void dns_security_cleanup(void)
{
    if (!g_dns.initialized)
        return;

    if (g_dns.config.allowed_domains) {
        for (size_t i = 0; i < g_dns.config.domain_count; i++) {
            AGENTOS_FREE(g_dns.config.allowed_domains[i]);
        }
        AGENTOS_FREE(g_dns.config.allowed_domains);
    }

    if (g_dns.config.blocked_domains) {
        for (size_t i = 0; i < g_dns.config.blocked_count; i++) {
            AGENTOS_FREE(g_dns.config.blocked_domains[i]);
        }
        AGENTOS_FREE(g_dns.config.blocked_domains);
    }

    if (g_dns.config.dns_servers) {
        for (size_t i = 0; i < g_dns.config.dns_server_count; i++) {
            AGENTOS_FREE(g_dns.config.dns_servers[i]);
        }
        AGENTOS_FREE(g_dns.config.dns_servers);
    }

    __builtin_memset(&g_dns, 0, sizeof(g_dns));
}

int dns_configure(const cupolas_dns_security_config_t *config)
{
    if (!config)
        return AGENTOS_EINVAL;
    g_dns.config = *config;
    return 0;
}

int dns_resolve(const char *hostname, char *ip_out, size_t ip_len)
{
    if (!hostname || !ip_out || ip_len == 0)
        return AGENTOS_EINVAL;

    if (g_dns.config.blocked_domains) {
        for (size_t i = 0; i < g_dns.config.blocked_count; i++) {
            if (strstr(hostname, g_dns.config.blocked_domains[i]) != NULL) {
                return AGENTOS_EINVAL;
            }
        }
    }

    struct hostent *host = gethostbyname(hostname);
    if (!host)
        return AGENTOS_EINVAL;

    const char *ip = inet_ntoa(*(struct in_addr *)host->h_addr);
    if (!ip)
        return AGENTOS_EINVAL;

    AGENTOS_STRNCPY_TERM(ip_out, ip, ip_len);

    return 0;
}

int dns_is_domain_allowed(const char *domain)
{
    if (!domain)
        return 0;

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

int dns_verify_dnssec(const char *domain)
{
    if (!domain)
        return 0;

    (void)domain;

    return g_dns.config.enable_dnssec ? 1 : 0;
}
