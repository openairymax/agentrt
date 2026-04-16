/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * http_security.c - HTTP Security Module Implementation
 */

#include "http_security.h"
#include "utils/cupolas_utils.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static struct {
    int initialized;
    cupolas_http_security_config_t config;
} g_http = {0};

int http_security_init(const cupolas_http_security_config_t* config) {
    if (g_http.initialized) return 0;
    
    memset(&g_http, 0, sizeof(g_http));
    
    if (config) {
        g_http.config = *config;
    } else {
        g_http.config.enforce_https = true;
        g_http.config.hsts_enabled = true;
        g_http.config.hsts_max_age = 31536000;
    }
    
    g_http.initialized = 1;
    return 0;
}

void http_security_cleanup(void) {
    if (!g_http.initialized) return;
    
    if (g_http.config.allowed_methods) {
        for (size_t i = 0; i < g_http.config.method_count; i++) {
            free(g_http.config.allowed_methods[i]);
        }
        free(g_http.config.allowed_methods);
    }
    
    if (g_http.config.forbidden_headers) {
        for (size_t i = 0; i < g_http.config.forbidden_count; i++) {
            free(g_http.config.forbidden_headers[i]);
        }
        free(g_http.config.forbidden_headers);
    }
    
    memset(&g_http, 0, sizeof(g_http));
}

int http_configure(const cupolas_http_security_config_t* config) {
    if (!config) return -1;
    g_http.config = *config;
    return 0;
}

int http_validate_request(const char* method, const char* url, const char** headers,
                         size_t header_count, size_t body_size) {
    if (!method || !url) return -1;
    
    if (g_http.config.max_url_length > 0) {
        if (strlen(url) > g_http.config.max_url_length) {
            return -1;
        }
    }
    
    if (g_http.config.max_body_size > 0) {
        if (body_size > g_http.config.max_body_size) {
            return -1;
        }
    }
    
    if (g_http.config.allowed_methods) {
        int method_allowed = 0;
        for (size_t i = 0; i < g_http.config.method_count; i++) {
            if (strcmp(g_http.config.allowed_methods[i], method) == 0) {
                method_allowed = 1;
                break;
            }
        }
        if (!method_allowed) return -1;
    }
    
    if (g_http.config.forbidden_headers && headers) {
        for (size_t i = 0; i < header_count; i++) {
            for (size_t j = 0; j < g_http.config.forbidden_count; j++) {
                if (strncmp(headers[i], g_http.config.forbidden_headers[j],
                           strlen(g_http.config.forbidden_headers[j])) == 0) {
                    return -1;
                }
            }
        }
    }
    
    return 0;
}

int http_add_security_headers(const char** headers, size_t header_count, size_t max_headers) {
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
        ((char**)headers)[header_count + i] = cupolas_strdup(security_headers[i]);
    }
    
    return 0;
}

int http_is_url_safe(const char* url) {
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

int http_check_url(const char* url, const char* method) {
    if (!url) return 0;
    
    if (g_http.config.enforce_https) {
        if (strncmp(url, "https://", 8) != 0) {
            return 0;
        }
    }
    
    if (g_http.config.allowed_methods && method) {
        int method_allowed = 0;
        for (size_t i = 0; i < g_http.config.method_count; i++) {
            if (strcmp(g_http.config.allowed_methods[i], method) == 0) {
                method_allowed = 1;
                break;
            }
        }
        if (!method_allowed) return 0;
    }
    
    return 1;
}
