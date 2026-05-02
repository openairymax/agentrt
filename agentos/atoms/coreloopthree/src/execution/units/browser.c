/**
 * @file browser.c
 * @brief 浏览器控制单元（基于Playwright模拟）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "agentos.h"
#include "execution.h"
#include "memory_compat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#endif

typedef struct browser_unit_data {
    char *metadata_json;
} browser_unit_data_t;

static int is_private_ip(uint32_t ip)
{
    uint8_t a = (ip >> 24) & 0xFF;
    uint8_t b = (ip >> 16) & 0xFF;
    if (a == 127)
        return 1;
    if (a == 10)
        return 1;
    if (a == 0)
        return 1;
    if (a == 172 && b >= 16 && b <= 31)
        return 1;
    if (a == 192 && b == 168)
        return 1;
    if (a == 169 && b == 254)
        return 1;
    if (a == 100 && b >= 64 && b <= 127)
        return 1;
    if (a == 198 && b >= 18 && b <= 19)
        return 1;
    if (a >= 224)
        return 1;
    if (ip == 0xFFFFFFFF)
        return 1;
    return 0;
}

static int is_private_ipv6(const struct in6_addr *addr)
{
    if (addr->s6_addr[0] == 0 && addr->s6_addr[1] == 0 && addr->s6_addr[2] == 0 && addr->s6_addr[3] == 0 &&
        addr->s6_addr[4] == 0 && addr->s6_addr[5] == 0 && addr->s6_addr[6] == 0 && addr->s6_addr[7] == 0 &&
        addr->s6_addr[8] == 0 && addr->s6_addr[9] == 0 && addr->s6_addr[10] == 0 && addr->s6_addr[11] == 0 &&
        addr->s6_addr[12] == 0 && addr->s6_addr[13] == 0 && addr->s6_addr[14] == 0 && addr->s6_addr[15] == 1)
        return 1;
    if (addr->s6_addr[0] == 0xfc || addr->s6_addr[0] == 0xfd)
        return 1;
    if (addr->s6_addr[0] == 0xfe && (addr->s6_addr[1] & 0xc0) == 0x80)
        return 1;
    if (addr->s6_addr[0] == 0xff)
        return 1;
    return 0;
}

static int extract_hostname(const char *url, char *hostname, size_t hostname_size)
{
    const char *start = strstr(url, "://");
    if (!start)
        return -1;
    start += 3;
    const char *at_sign = strchr(start, '@');
    if (at_sign)
        start = at_sign + 1;
    if (*start == '[') {
        start++;
        const char *end = strchr(start, ']');
        if (!end)
            return -1;
        size_t len = (size_t) (end - start);
        if (len >= hostname_size)
            len = hostname_size - 1;
        memcpy(hostname, start, len);
        hostname[len] = '\0';
        return 0;
    }
    const char *end = start;
    while (*end && *end != '/' && *end != ':' && *end != '?' && *end != '#' && *end != ']')
        end++;
    size_t len = (size_t) (end - start);
    if (len >= hostname_size)
        len = hostname_size - 1;
    memcpy(hostname, start, len);
    hostname[len] = '\0';
    return 0;
}

static int has_url_encoding(const char *url)
{
    for (const char *p = url; *p; p++) {
        if (*p == '%') {
            if (p[1] && p[2]) {
                char hex[3] = {p[1], p[2], '\0'};
                char *end;
                long val = strtol(hex, &end, 16);
                if (end != hex && val >= 0 && val <= 255) {
                    char decoded = (char) val;
                    if (decoded == '/' || decoded == '\\' || decoded == '.' || decoded == '@' || decoded == ':' ||
                        decoded == '\0') {
                        return 1;
                    }
                }
            }
        }
    }
    return 0;
}

static int is_safe_url(const char *url)
{
    if (!url)
        return 0;
    if (strncasecmp(url, "https://", 8) != 0 && strncasecmp(url, "about:blank", 11) != 0) {
        return 0;
    }
    if (strstr(url, "javascript:") != NULL)
        return 0;
    if (strstr(url, "data:") != NULL)
        return 0;
    if (strstr(url, "file:") != NULL)
        return 0;
    if (has_url_encoding(url))
        return 0;

    if (strncasecmp(url, "https://", 8) == 0) {
        char hostname[256];
        if (extract_hostname(url, hostname, sizeof(hostname)) == 0) {
            struct addrinfo hints, *res;
            memset(&hints, 0, sizeof(hints));
            hints.ai_family = AF_UNSPEC;
            if (getaddrinfo(hostname, NULL, &hints, &res) == 0 && res) {
                struct addrinfo *rp;
                int unsafe = 0;
                for (rp = res; rp != NULL; rp = rp->ai_next) {
                    if (rp->ai_family == AF_INET) {
                        struct sockaddr_in *addr = (struct sockaddr_in *) rp->ai_addr;
                        uint32_t ip              = ntohl(addr->sin_addr.s_addr);
                        if (is_private_ip(ip)) {
                            unsafe = 1;
                            break;
                        }
                    } else if (rp->ai_family == AF_INET6) {
                        struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *) rp->ai_addr;
                        if (is_private_ipv6(&addr6->sin6_addr)) {
                            unsafe = 1;
                            break;
                        }
                    }
                }
                freeaddrinfo(res);
                if (unsafe)
                    return 0;
            } else {
                return 0;
            }
        } else {
            return 0;
        }
    }
    return 1;
}

static agentos_error_t browser_execute(agentos_execution_unit_t *unit, const void *input, void **out_output)
{
    (void) unit;
    if (!input || !out_output)
        return AGENTOS_EINVAL;

    const char *cmd = (const char *) input;
    char *result_json = NULL;

    if (strstr(cmd, "navigate") != NULL) {
        const char *url_start = strstr(cmd, "http");
        if (!url_start) {
            result_json = AGENTOS_STRDUP("{\"error\":\"no_url_provided\",\"status\":\"failed\"}");
            if (!result_json) return AGENTOS_ENOMEM;
            *out_output = result_json;
            return AGENTOS_EINVAL;
        }
        if (!is_safe_url(url_start)) {
            result_json = AGENTOS_STRDUP("{\"error\":\"unsafe_url\",\"status\":\"denied\"}");
            if (!result_json) return AGENTOS_ENOMEM;
            *out_output = result_json;
            return AGENTOS_EPERM;
        }
        size_t url_len = strlen(url_start);
        char *url_copy = (char *)AGENTOS_MALLOC(url_len + 1);
        if (!url_copy) return AGENTOS_ENOMEM;
        memcpy(url_copy, url_start, url_len + 1);
        for (char *p = url_copy; *p; p++) {
            if (*p == ' ' || *p == '\n' || *p == '\r') { *p = '\0'; break; }
        }
        size_t buf_size = 64 + strlen(url_copy) + 1;
        result_json = (char *)AGENTOS_MALLOC(buf_size);
        if (!result_json) { AGENTOS_FREE(url_copy); return AGENTOS_ENOMEM; }
        snprintf(result_json, buf_size,
                 "{\"status\":\"navigated\",\"url\":\"%s\"}", url_copy);
        AGENTOS_FREE(url_copy);
        *out_output = result_json;
        return AGENTOS_SUCCESS;
    } else if (strstr(cmd, "click") != NULL) {
        const char *selector = strstr(cmd, "selector=");
        if (selector) {
            selector += strlen("selector=");
            size_t sel_len = strlen(selector);
            char *sel_copy = (char *)AGENTOS_MALLOC(sel_len + 1);
            if (!sel_copy) return AGENTOS_ENOMEM;
            memcpy(sel_copy, selector, sel_len + 1);
            for (char *p = sel_copy; *p; p++) {
                if (*p == ' ' || *p == '\n' || *p == '\r') { *p = '\0'; break; }
            }
            size_t buf_size = 64 + strlen(sel_copy) + 1;
            result_json = (char *)AGENTOS_MALLOC(buf_size);
            if (!result_json) { AGENTOS_FREE(sel_copy); return AGENTOS_ENOMEM; }
            snprintf(result_json, buf_size,
                     "{\"status\":\"clicked\",\"selector\":\"%s\"}", sel_copy);
            AGENTOS_FREE(sel_copy);
        } else {
            result_json = AGENTOS_STRDUP("{\"status\":\"clicked\",\"selector\":\"unknown\"}");
            if (!result_json) return AGENTOS_ENOMEM;
        }
        *out_output = result_json;
        return AGENTOS_SUCCESS;
    } else if (strstr(cmd, "screenshot") != NULL) {
        result_json = AGENTOS_STRDUP("{\"status\":\"screenshot_taken\",\"format\":\"png\",\"size_bytes\":0}");
        if (!result_json) return AGENTOS_ENOMEM;
        *out_output = result_json;
        return AGENTOS_SUCCESS;
    } else if (strstr(cmd, "type") != NULL || strstr(cmd, "fill") != NULL) {
        result_json = AGENTOS_STRDUP("{\"status\":\"typed\",\"value\":\"\"}");
        if (!result_json) return AGENTOS_ENOMEM;
        *out_output = result_json;
        return AGENTOS_SUCCESS;
    } else if (strstr(cmd, "wait") != NULL) {
        result_json = AGENTOS_STRDUP("{\"status\":\"waited\",\"timeout_ms\":0}");
        if (!result_json) return AGENTOS_ENOMEM;
        *out_output = result_json;
        return AGENTOS_SUCCESS;
    }

    *out_output = AGENTOS_STRDUP("{\"error\":\"unsupported_command\",\"status\":\"failed\"}");
    return *out_output ? AGENTOS_ENOTSUP : AGENTOS_ENOMEM;
}

static void browser_destroy(agentos_execution_unit_t *unit)
{
    if (!unit)
        return;
    browser_unit_data_t *data = (browser_unit_data_t *) unit->execution_unit_data;
    if (data) {
        if (data->metadata_json)
            AGENTOS_FREE(data->metadata_json);
        AGENTOS_FREE(data);
    }
    AGENTOS_FREE(unit);
}

agentos_execution_unit_t *agentos_browser_unit_create(void)
{
    agentos_execution_unit_t *unit = (agentos_execution_unit_t *) AGENTOS_MALLOC(sizeof(agentos_execution_unit_t));
    if (!unit)
        return NULL;
    memset(unit, 0, sizeof(*unit));

    browser_unit_data_t *data = (browser_unit_data_t *) AGENTOS_MALLOC(sizeof(browser_unit_data_t));
    if (!data) {
        AGENTOS_FREE(unit);
        return NULL;
    }

    char meta[128];
    snprintf(meta, sizeof(meta), "{\"type\":\"browser\"}");
    data->metadata_json = AGENTOS_STRDUP(meta);

    if (!data->metadata_json) {
        AGENTOS_FREE(data);
        AGENTOS_FREE(unit);
        return NULL;
    }

    unit->execution_unit_data    = data;
    unit->execution_unit_execute = browser_execute;
    unit->execution_unit_destroy = browser_destroy;

    return unit;
}
