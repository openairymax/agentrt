/**
 * @file api.c
 * @brief API Call Execution Unit Implementation
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "execution.h"
#include "agentos.h"
#include <stdlib.h>

#include "memory_compat.h"
#include "string_compat.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef int socklen_t;
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#define SOCKET int
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define closesocket close
#endif

#define MAX_URL_LENGTH 2048
#define MAX_HEADERS 16
#define MAX_BODY_SIZE 4096
#define DEFAULT_TIMEOUT_MS 30000

typedef struct api_config {
    char base_url[256];
    char api_key[128];
    int timeout_ms;
    int max_retries;
    float retry_delay_ms;
} api_config_t;

typedef struct api_unit_data {
    char* id;
    char* description;
    api_config_t config;
    char** cached_responses;
    size_t cached_count;
    size_t cache_capacity;
    agentos_mutex_t* lock;
} api_unit_data_t;

static int parse_url(const char* url, char* host, size_t host_size,
                     int* port, char* path, size_t path_size) {
    if (!url || !host || !port || !path) return -1;

    const char* p = url;
    if (strncmp(p, "https://", 8) == 0) {
        p += 8;
        *port = 443;
    } else if (strncmp(p, "http://", 7) == 0) {
        p += 7;
        *port = 80;
    } else {
        return -1;
    }

    const char* slash = strchr(p, '/');
    const char* colon = strchr(p, ':');
    if (colon && (!slash || colon < slash)) {
        size_t hlen = (size_t)(colon - p);
        if (hlen >= host_size) hlen = host_size - 1;
        memcpy(host, p, hlen);
        host[hlen] = '\0';
        *port = atoi(colon + 1);
        if (*port <= 0) *port = 80;
    } else {
        size_t hlen = slash ? (size_t)(slash - p) : strlen(p);
        if (hlen >= host_size) hlen = host_size - 1;
        memcpy(host, p, hlen);
        host[hlen] = '\0';
    }

    if (slash) {
        strncpy(path, slash, path_size - 1);
        path[path_size - 1] = '\0';
    } else {
        strncpy(path, "/", path_size - 1);
        path[path_size - 1] = '\0';
    }

    return 0;
}

static SOCKET connect_to_host(const char* host, int port, int timeout_ms) {
    struct addrinfo hints, *result, *rp;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);

    int rc = getaddrinfo(host, port_str, &hints, &result);
    if (rc != 0) return INVALID_SOCKET;

    SOCKET sock = INVALID_SOCKET;
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sock == INVALID_SOCKET) continue;

        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));

        if (connect(sock, rp->ai_addr, (socklen_t)rp->ai_addrlen) == 0) {
            break;
        }
        closesocket(sock);
        sock = INVALID_SOCKET;
    }

    freeaddrinfo(result);
    return sock;
}

static char* http_request(SOCKET sock, const char* method, const char* host,
                           const char* path, const char* api_key,
                           const char* body) {
    size_t req_cap = 4096;
    char* request = (char*)AGENTOS_MALLOC(req_cap);
    if (!request) return NULL;

    int len = snprintf(request, req_cap,
        "%s %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Connection: close\r\n"
        "Accept: application/json\r\n",
        method, path, host);

    if (api_key && api_key[0]) {
        len += snprintf(request + len, req_cap - len,
            "Authorization: Bearer %s\r\n", api_key);
    }

    if (body && body[0]) {
        size_t body_len = strlen(body);
        len += snprintf(request + len, req_cap - len,
            "Content-Type: application/json\r\n"
            "Content-Length: %zu\r\n", body_len);
        if ((size_t)len + body_len + 4 >= req_cap) {
            req_cap = (size_t)len + body_len + 64;
            char* new_req = (char*)AGENTOS_REALLOC(request, req_cap);
            if (!new_req) { AGENTOS_FREE(request); return NULL; }
            request = new_req;
        }
        memcpy(request + len, "\r\n", 2);
        len += 2;
        memcpy(request + len, body, body_len);
        len += (int)body_len;
        memcpy(request + len, "\r\n", 2);
        len += 2;
    } else {
        len += snprintf(request + len, req_cap - len, "\r\n");
    }

    size_t total_sent = 0;
    while (total_sent < (size_t)len) {
        int sent = send(sock, request + total_sent, (int)((size_t)len - total_sent), 0);
        if (sent <= 0) { AGENTOS_FREE(request); return NULL; }
        total_sent += (size_t)sent;
    }
    AGENTOS_FREE(request);

    size_t resp_cap = 8192;
    char* response = (char*)AGENTOS_MALLOC(resp_cap);
    if (!response) return NULL;

    size_t total_recv = 0;
    for (;;) {
        if (total_recv + 1024 > resp_cap) {
            resp_cap *= 2;
            char* new_resp = (char*)AGENTOS_REALLOC(response, resp_cap);
            if (!new_resp) { AGENTOS_FREE(response); return NULL; }
            response = new_resp;
        }
        int n = recv(sock, response + total_recv, 1024, 0);
        if (n <= 0) break;
        total_recv += (size_t)n;
    }
    response[total_recv] = '\0';

    return response;
}

static char* extract_body(const char* response) {
    if (!response) return NULL;

    const char* header_end = strstr(response, "\r\n\r\n");
    if (!header_end) header_end = strstr(response, "\n\n");
    if (!header_end) return AGENTOS_STRDUP(response);

    const char* body = header_end + 4;

    const char* cl = strstr(response, "Content-Length:");
    if (!cl) cl = strstr(response, "content-length:");
    if (cl) {
        size_t content_len = (size_t)atol(cl + 15);
        if (content_len > 0) {
            char* result = (char*)AGENTOS_MALLOC(content_len + 1);
            if (result) {
                memcpy(result, body, content_len);
                result[content_len] = '\0';
            }
            return result;
        }
    }

    return AGENTOS_STRDUP(body);
}

static agentos_error_t api_execute(agentos_execution_unit_t* unit,
                                    const void* input,
                                    void** out_output) {
    if (!unit || !unit->execution_unit_data || !input || !out_output) {
        return AGENTOS_EINVAL;
    }

    api_unit_data_t* data = (api_unit_data_t*)unit->execution_unit_data;
    const char* request_str = (const char*)input;

    char method[16] = "GET";
    char url[MAX_URL_LENGTH] = {0};
    char body[MAX_BODY_SIZE] = {0};

    if (sscanf(request_str, "method=%15[^&]&url=%2047[^&]&body=%4095[^\n]", method, url, body) < 2) {
        if (sscanf(request_str, "%2047s", url) != 1) {
            return AGENTOS_EINVAL;
        }
        strncpy(method, "GET", sizeof(method) - 1);
    }

    char host[256] = {0};
    int port = 80;
    char path[MAX_URL_LENGTH] = {0};

    if (parse_url(url, host, sizeof(host), &port, path, sizeof(path)) != 0) {
        return AGENTOS_EINVAL;
    }

    agentos_mutex_lock(data->lock);

    for (size_t i = 0; i < data->cached_count; i++) {
        if (data->cached_responses[i] && strstr(data->cached_responses[i], url)) {
            *out_output = AGENTOS_STRDUP(data->cached_responses[i]);
            agentos_mutex_unlock(data->lock);
            return *out_output ? AGENTOS_SUCCESS : AGENTOS_ENOMEM;
        }
    }
    agentos_mutex_unlock(data->lock);

    SOCKET sock = connect_to_host(host, port, data->config.timeout_ms > 0 ? data->config.timeout_ms : DEFAULT_TIMEOUT_MS);
    if (sock == INVALID_SOCKET) {
        return AGENTOS_ECONNREFUSED;
    }

    char* raw_response = http_request(sock, method, host, path,
                                       data->config.api_key[0] ? data->config.api_key : NULL,
                                       body[0] ? body : NULL);
    closesocket(sock);

    if (!raw_response) {
        return AGENTOS_EIO;
    }

    char* response_body = extract_body(raw_response);
    AGENTOS_FREE(raw_response);

    if (!response_body) {
        return AGENTOS_EIO;
    }

    agentos_mutex_lock(data->lock);
    if (data->cached_count < data->cache_capacity) {
        size_t entry_size = strlen(url) + strlen(response_body) + 4;
        char* cache_entry = (char*)AGENTOS_MALLOC(entry_size);
        if (cache_entry) {
            snprintf(cache_entry, entry_size, "%s\n%s", url, response_body);
            data->cached_responses[data->cached_count++] = cache_entry;
        }
    }
    agentos_mutex_unlock(data->lock);

    *out_output = response_body;
    return AGENTOS_SUCCESS;
}

static void api_destroy(agentos_execution_unit_t* unit) {
    if (!unit) return;

    api_unit_data_t* data = (api_unit_data_t*)unit->execution_unit_data;
    if (data) {
        if (data->id) AGENTOS_FREE(data->id);
        if (data->description) AGENTOS_FREE(data->description);

        if (data->cached_responses) {
            for (size_t i = 0; i < data->cached_count; i++) {
                if (data->cached_responses[i]) {
                    AGENTOS_FREE(data->cached_responses[i]);
                }
            }
            AGENTOS_FREE(data->cached_responses);
        }

        if (data->lock) agentos_mutex_destroy(data->lock);
        AGENTOS_FREE(data);
    }

    AGENTOS_FREE(unit);
}

static const char* api_get_metadata(agentos_execution_unit_t* unit) {
    if (!unit || !unit->execution_unit_data) return "api";
    api_unit_data_t* data = (api_unit_data_t*)unit->execution_unit_data;
    return data->description ? data->description : "API Execution Unit v1.0";
}

agentos_error_t agentos_unit_api_create(
    const api_config_t* config,
    agentos_execution_unit_t** out_unit) {
    if (!config || !out_unit) {
        return AGENTOS_EINVAL;
    }

    agentos_execution_unit_t* unit = (agentos_execution_unit_t*)AGENTOS_CALLOC(1, sizeof(agentos_execution_unit_t));
    if (!unit) {
        return AGENTOS_ENOMEM;
    }

    api_unit_data_t* data = (api_unit_data_t*)AGENTOS_CALLOC(1, sizeof(api_unit_data_t));
    if (!data) {
        AGENTOS_FREE(unit);
        return AGENTOS_ENOMEM;
    }

    data->id = AGENTOS_STRDUP("api");
    data->description = AGENTOS_STRDUP("Execute HTTP/HTTPS API calls");
    if (!data->id || !data->description) {
        if (data->id) AGENTOS_FREE(data->id);
        if (data->description) AGENTOS_FREE(data->description);
        AGENTOS_FREE(data);
        AGENTOS_FREE(unit);
        return AGENTOS_ENOMEM;
    }

    memcpy(&data->config, config, sizeof(api_config_t));

    data->cached_responses = (char**)AGENTOS_CALLOC(10, sizeof(char*));
    if (!data->cached_responses) {
        AGENTOS_FREE(data->id);
        AGENTOS_FREE(data->description);
        AGENTOS_FREE(data);
        AGENTOS_FREE(unit);
        return AGENTOS_ENOMEM;
    }
    data->cache_capacity = 10;
    data->cached_count = 0;

    data->lock = agentos_mutex_create();
    if (!data->lock) {
        AGENTOS_FREE(data->id);
        AGENTOS_FREE(data->description);
        AGENTOS_FREE(data->cached_responses);
        AGENTOS_FREE(data);
        AGENTOS_FREE(unit);
        return AGENTOS_ENOMEM;
    }

    unit->execution_unit_data = data;
    unit->execution_unit_execute = api_execute;
    unit->execution_unit_destroy = api_destroy;
    unit->execution_unit_get_metadata = api_get_metadata;

    *out_unit = unit;
    return AGENTOS_SUCCESS;
}
