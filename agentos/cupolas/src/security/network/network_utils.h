/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * network_utils.h - Network Utility Functions Interface
 */

#ifndef CUPOLAS_NETWORK_UTILS_H
#define CUPOLAS_NETWORK_UTILS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CUPOLAS_PROTO_TCP = 0,
    CUPOLAS_PROTO_UDP,
    CUPOLAS_PROTO_HTTP,
    CUPOLAS_PROTO_HTTPS,
    CUPOLAS_PROTO_WEBSOCKET,
    CUPOLAS_PROTO_DNS
} cupolas_protocol_t;

int network_utils_parse_url(const char* url, char* scheme, char* host, 
                           uint16_t* port, char* path);

int network_utils_ip_in_cidr(const char* ip, const char* cidr);

int network_utils_validate_ip(const char* ip);

int network_utils_validate_port(uint16_t port);

const char* network_utils_protocol_string(cupolas_protocol_t protocol);

#ifdef __cplusplus
}
#endif

#endif /* CUPOLAS_NETWORK_UTILS_H */
