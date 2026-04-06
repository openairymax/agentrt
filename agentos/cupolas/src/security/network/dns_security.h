/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * dns_security.h - DNS Security Module Interface
 */

#ifndef CUPOLAS_DNS_SECURITY_H
#define CUPOLAS_DNS_SECURITY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool enable_dnssec;
    bool enable_doh;
    
    char** allowed_domains;
    size_t domain_count;
    
    char** blocked_domains;
    size_t blocked_count;
    
    char** dns_servers;
    size_t dns_server_count;
} cupolas_dns_security_config_t;

int dns_security_init(const cupolas_dns_security_config_t* config);
void dns_security_cleanup(void);

int dns_configure(const cupolas_dns_security_config_t* config);

int dns_resolve(const char* hostname, char* ip_out, size_t ip_len);

int dns_is_domain_allowed(const char* domain);

int dns_verify_dnssec(const char* domain);

#ifdef __cplusplus
}
#endif

#endif /* CUPOLAS_DNS_SECURITY_H */
