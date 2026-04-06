/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * network_utils.c - Network Utility Functions Implementation
 */

#include "network_utils.h"
#include "../../utils/cupolas_utils.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

int network_utils_parse_url(const char* url, char* scheme, char* host, 
                           uint16_t* port, char* path) {
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
            snprintf(path, 256, "%s", slash);
        } else {
            snprintf(path, 256, "%s", "/");
        }
    }
    
    return 0;
}

int network_utils_ip_in_cidr(const char* ip, const char* cidr) {
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

int network_utils_validate_ip(const char* ip) {
    if (!ip) return 0;
    
    struct in_addr addr;
    return inet_pton(AF_INET, ip, &addr) == 1 ? 1 : 0;
}

int network_utils_validate_port(uint16_t port) {
    (void)port;
    return 1;
}

const char* network_utils_protocol_string(cupolas_protocol_t protocol) {
    switch (protocol) {
        case CUPOLAS_PROTO_TCP: return "TCP";
        case CUPOLAS_PROTO_UDP: return "UDP";
        case CUPOLAS_PROTO_HTTP: return "HTTP";
        case CUPOLAS_PROTO_HTTPS: return "HTTPS";
        case CUPOLAS_PROTO_WEBSOCKET: return "WebSocket";
        case CUPOLAS_PROTO_DNS: return "DNS";
        default: return "Unknown";
    }
}
