/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * http_security.h - HTTP Security Module Interface
 */

#ifndef CUPOLAS_HTTP_SECURITY_H
#define CUPOLAS_HTTP_SECURITY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool enforce_https;
    bool hsts_enabled;
    uint32_t hsts_max_age;
    
    char** allowed_methods;
    size_t method_count;
    
    char** forbidden_headers;
    size_t forbidden_count;
    
    size_t max_url_length;
    size_t max_body_size;
    size_t max_header_size;
} cupolas_http_security_config_t;

int http_security_init(const cupolas_http_security_config_t* config);
void http_security_cleanup(void);

int http_configure(const cupolas_http_security_config_t* config);

int http_validate_request(const char* method, const char* url, const char** headers,
                         size_t header_count, size_t body_size);

int http_add_security_headers(const char** headers, size_t header_count, size_t max_headers);

int http_is_url_safe(const char* url);

int http_check_url(const char* url, const char* method);

#ifdef __cplusplus
}
#endif

#endif /* CUPOLAS_HTTP_SECURITY_H */
