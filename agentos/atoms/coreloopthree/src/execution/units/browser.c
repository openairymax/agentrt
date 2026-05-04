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
#include <unistd.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#endif

#define BROWSER_MAX_CONTEXTS 64
#define BROWSER_MAX_CDP_CONNS 16
#define BROWSER_CDP_PORT_FIRST 9222
#define BROWSER_LAUNCH_TIMEOUT_MS 15000
#define BROWSER_CDP_CONNECT_TIMEOUT_MS 5000

typedef enum {
    BROWSER_STATE_STOPPED = 0,
    BROWSER_STATE_LAUNCHING,
    BROWSER_STATE_RUNNING,
    BROWSER_STATE_STOPPING,
    BROWSER_STATE_ERROR
} browser_state_t;

typedef struct {
    int socket_fd;
    int in_use;
    uint32_t last_active_ms;
    char agent_id[128];
    char endpoint_url[256];
} cdp_connection_t;

typedef struct {
    char browser_path[256];
    char user_data_dir[256];
    char remote_debugging_url[256];
    int remote_debugging_port;
    uint32_t headless;
    pid_t browser_pid;
    browser_state_t state;
    uint32_t launch_time_ms;

    cdp_connection_t connections[BROWSER_MAX_CDP_CONNS];
    size_t connection_count;

    agentos_mutex_t pool_lock;
    agentos_mutex_t browser_lock;

    struct {
        char context_id[64];
        char agent_id[128];
        int active;
    } sandbox_contexts[BROWSER_MAX_CONTEXTS];
    size_t context_count;
} browser_manager_t;

static browser_manager_t g_browser_mgr;
static int g_browser_mgr_initialized = 0;

static int browser_mgr_init(void);
static void browser_mgr_shutdown(void);

static uint32_t browser_get_time_ms(void)
{
    return (uint32_t)(agentos_time_ms() & 0xFFFFFFFF);
}

static int browser_mgr_init(void)
{
    if (g_browser_mgr_initialized)
        return 0;

    memset(&g_browser_mgr, 0, sizeof(g_browser_mgr));
    g_browser_mgr.state = BROWSER_STATE_STOPPED;
    g_browser_mgr.remote_debugging_port = BROWSER_CDP_PORT_FIRST;
    g_browser_mgr.headless = 1;
    snprintf(g_browser_mgr.user_data_dir, sizeof(g_browser_mgr.user_data_dir), "%s/browser_data",
             ".");
    snprintf(g_browser_mgr.browser_path, sizeof(g_browser_mgr.browser_path), "chromium");

    agentos_mutex_init(&g_browser_mgr.pool_lock);
    agentos_mutex_init(&g_browser_mgr.browser_lock);
    g_browser_mgr_initialized = 1;
    return 0;
}

static void browser_mgr_shutdown(void)
{
    if (!g_browser_mgr_initialized)
        return;

    agentos_mutex_lock(&g_browser_mgr.browser_lock);
    if (g_browser_mgr.state == BROWSER_STATE_RUNNING) {
        g_browser_mgr.state = BROWSER_STATE_STOPPING;
        for (size_t i = 0; i < g_browser_mgr.connection_count; i++) {
            if (g_browser_mgr.connections[i].socket_fd > 0) {
#ifdef _WIN32
                closesocket(g_browser_mgr.connections[i].socket_fd);
#else
                close(g_browser_mgr.connections[i].socket_fd);
#endif
            }
        }
        g_browser_mgr.state = BROWSER_STATE_STOPPED;
    }
    agentos_mutex_unlock(&g_browser_mgr.browser_lock);

    agentos_mutex_destroy(&g_browser_mgr.pool_lock);
    agentos_mutex_destroy(&g_browser_mgr.browser_lock);
    g_browser_mgr_initialized = 0;
}

int agentos_browser_launch(const char *browser_path, int port, int headless,
                           const char *user_data_dir)
{
    browser_mgr_init();

    agentos_mutex_lock(&g_browser_mgr.browser_lock);
    if (g_browser_mgr.state == BROWSER_STATE_RUNNING ||
        g_browser_mgr.state == BROWSER_STATE_LAUNCHING) {
        agentos_mutex_unlock(&g_browser_mgr.browser_lock);
        return -1;
    }

    if (browser_path && browser_path[0])
        snprintf(g_browser_mgr.browser_path, sizeof(g_browser_mgr.browser_path), "%s",
                 browser_path);
    if (port > 0)
        g_browser_mgr.remote_debugging_port = port;
    g_browser_mgr.headless = (uint32_t)(headless ? 1 : 0);
    if (user_data_dir && user_data_dir[0])
        snprintf(g_browser_mgr.user_data_dir, sizeof(g_browser_mgr.user_data_dir), "%s",
                 user_data_dir);

    g_browser_mgr.state = BROWSER_STATE_LAUNCHING;
    g_browser_mgr.launch_time_ms = browser_get_time_ms();

    snprintf(g_browser_mgr.remote_debugging_url, sizeof(g_browser_mgr.remote_debugging_url),
             "ws://127.0.0.1:%d/devtools/browser", g_browser_mgr.remote_debugging_port);

    g_browser_mgr.state = BROWSER_STATE_RUNNING;
    agentos_mutex_unlock(&g_browser_mgr.browser_lock);
    return 0;
}

int agentos_browser_attach(const char *debugging_url)
{
    browser_mgr_init();

    if (!debugging_url || !debugging_url[0])
        return -1;

    agentos_mutex_lock(&g_browser_mgr.browser_lock);
    if (g_browser_mgr.state == BROWSER_STATE_RUNNING) {
        agentos_mutex_unlock(&g_browser_mgr.browser_lock);
        return -2;
    }

    snprintf(g_browser_mgr.remote_debugging_url, sizeof(g_browser_mgr.remote_debugging_url), "%s",
             debugging_url);
    g_browser_mgr.state = BROWSER_STATE_RUNNING;
    agentos_mutex_unlock(&g_browser_mgr.browser_lock);
    return 0;
}

int agentos_browser_close(void)
{
    if (!g_browser_mgr_initialized)
        return -1;

    agentos_mutex_lock(&g_browser_mgr.browser_lock);
    if (g_browser_mgr.state != BROWSER_STATE_RUNNING) {
        agentos_mutex_unlock(&g_browser_mgr.browser_lock);
        return -2;
    }

    g_browser_mgr.state = BROWSER_STATE_STOPPING;

    agentos_mutex_lock(&g_browser_mgr.pool_lock);
    for (size_t i = 0; i < g_browser_mgr.connection_count; i++) {
        if (g_browser_mgr.connections[i].socket_fd > 0) {
#ifdef _WIN32
            closesocket(g_browser_mgr.connections[i].socket_fd);
#else
            close(g_browser_mgr.connections[i].socket_fd);
#endif
        }
        g_browser_mgr.connections[i].socket_fd = -1;
        g_browser_mgr.connections[i].in_use = 0;
    }
    g_browser_mgr.connection_count = 0;

    for (size_t i = 0; i < g_browser_mgr.context_count; i++) {
        g_browser_mgr.sandbox_contexts[i].active = 0;
    }
    g_browser_mgr.context_count = 0;
    agentos_mutex_unlock(&g_browser_mgr.pool_lock);

    g_browser_mgr.state = BROWSER_STATE_STOPPED;
    agentos_mutex_unlock(&g_browser_mgr.browser_lock);
    browser_mgr_shutdown();
    return 0;
}

int agentos_browser_get_state(void)
{
    if (!g_browser_mgr_initialized)
        return BROWSER_STATE_STOPPED;
    return (int)g_browser_mgr.state;
}

static int cdp_pool_acquire(const char *agent_id, const char *endpoint, cdp_connection_t **out_conn)
{
    if (!g_browser_mgr_initialized || !agent_id || !out_conn)
        return -1;

    agentos_mutex_lock(&g_browser_mgr.pool_lock);

    for (size_t i = 0; i < g_browser_mgr.connection_count; i++) {
        if (!g_browser_mgr.connections[i].in_use &&
            strcmp(g_browser_mgr.connections[i].agent_id, agent_id) == 0) {
            g_browser_mgr.connections[i].in_use = 1;
            g_browser_mgr.connections[i].last_active_ms = browser_get_time_ms();
            *out_conn = &g_browser_mgr.connections[i];
            agentos_mutex_unlock(&g_browser_mgr.pool_lock);
            return 0;
        }
    }

    if (g_browser_mgr.connection_count >= BROWSER_MAX_CDP_CONNS) {
        agentos_mutex_unlock(&g_browser_mgr.pool_lock);
        return -2;
    }

    size_t idx = g_browser_mgr.connection_count;

    snprintf(g_browser_mgr.connections[idx].agent_id,
             sizeof(g_browser_mgr.connections[idx].agent_id), "%s", agent_id);
    g_browser_mgr.connections[idx].socket_fd = -1;
    g_browser_mgr.connections[idx].in_use = 1;
    g_browser_mgr.connections[idx].last_active_ms = browser_get_time_ms();

    if (endpoint && endpoint[0]) {
        snprintf(g_browser_mgr.connections[idx].endpoint_url,
                 sizeof(g_browser_mgr.connections[idx].endpoint_url), "%s", endpoint);
    }

    g_browser_mgr.connection_count++;
    *out_conn = &g_browser_mgr.connections[idx];

    agentos_mutex_unlock(&g_browser_mgr.pool_lock);
    return 0;
}

static void cdp_pool_release(cdp_connection_t *conn)
{
    if (!g_browser_mgr_initialized || !conn)
        return;

    agentos_mutex_lock(&g_browser_mgr.pool_lock);
    conn->in_use = 0;
    conn->last_active_ms = browser_get_time_ms();
    agentos_mutex_unlock(&g_browser_mgr.pool_lock);
}

int agentos_browser_create_context(const char *agent_id, char *out_context_id, size_t ctx_size)
{
    if (!g_browser_mgr_initialized || !agent_id || !out_context_id || ctx_size == 0)
        return -1;

    agentos_mutex_lock(&g_browser_mgr.browser_lock);
    if (g_browser_mgr.state != BROWSER_STATE_RUNNING) {
        agentos_mutex_unlock(&g_browser_mgr.browser_lock);
        return -2;
    }

    if (g_browser_mgr.context_count >= BROWSER_MAX_CONTEXTS) {
        agentos_mutex_unlock(&g_browser_mgr.browser_lock);
        return -3;
    }

    size_t idx = g_browser_mgr.context_count;
    snprintf(g_browser_mgr.sandbox_contexts[idx].context_id,
             sizeof(g_browser_mgr.sandbox_contexts[idx].context_id), "ctx-%zu-%u", idx,
             (unsigned int)browser_get_time_ms());
    snprintf(g_browser_mgr.sandbox_contexts[idx].agent_id,
             sizeof(g_browser_mgr.sandbox_contexts[idx].agent_id), "%s", agent_id);
    g_browser_mgr.sandbox_contexts[idx].active = 1;

    snprintf(out_context_id, ctx_size, "%s", g_browser_mgr.sandbox_contexts[idx].context_id);
    g_browser_mgr.context_count++;

    agentos_mutex_unlock(&g_browser_mgr.browser_lock);
    return 0;
}

int agentos_browser_destroy_context(const char *context_id)
{
    if (!g_browser_mgr_initialized || !context_id)
        return -1;

    agentos_mutex_lock(&g_browser_mgr.browser_lock);
    for (size_t i = 0; i < g_browser_mgr.context_count; i++) {
        if (g_browser_mgr.sandbox_contexts[i].active &&
            strcmp(g_browser_mgr.sandbox_contexts[i].context_id, context_id) == 0) {
            g_browser_mgr.sandbox_contexts[i].active = 0;

            agentos_mutex_lock(&g_browser_mgr.pool_lock);
            for (size_t j = 0; j < g_browser_mgr.connection_count; j++) {
                if (strcmp(g_browser_mgr.connections[j].agent_id,
                           g_browser_mgr.sandbox_contexts[i].agent_id) == 0) {
                    if (g_browser_mgr.connections[j].socket_fd > 0) {
#ifdef _WIN32
                        closesocket(g_browser_mgr.connections[j].socket_fd);
#else
                        close(g_browser_mgr.connections[j].socket_fd);
#endif
                    }
                    g_browser_mgr.connections[j].socket_fd = -1;
                    g_browser_mgr.connections[j].in_use = 0;
                }
            }
            agentos_mutex_unlock(&g_browser_mgr.pool_lock);

            agentos_mutex_unlock(&g_browser_mgr.browser_lock);
            return 0;
        }
    }
    agentos_mutex_unlock(&g_browser_mgr.browser_lock);
    return -2;
}

int agentos_browser_get_context_count(void)
{
    if (!g_browser_mgr_initialized)
        return 0;

    agentos_mutex_lock(&g_browser_mgr.browser_lock);
    int count = 0;
    for (size_t i = 0; i < g_browser_mgr.context_count; i++) {
        if (g_browser_mgr.sandbox_contexts[i].active)
            count++;
    }
    agentos_mutex_unlock(&g_browser_mgr.browser_lock);
    return count;
}

typedef struct browser_unit_data {
    char agent_id[128];
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
    if (addr->s6_addr[0] == 0 && addr->s6_addr[1] == 0 && addr->s6_addr[2] == 0 &&
        addr->s6_addr[3] == 0 && addr->s6_addr[4] == 0 && addr->s6_addr[5] == 0 &&
        addr->s6_addr[6] == 0 && addr->s6_addr[7] == 0 && addr->s6_addr[8] == 0 &&
        addr->s6_addr[9] == 0 && addr->s6_addr[10] == 0 && addr->s6_addr[11] == 0 &&
        addr->s6_addr[12] == 0 && addr->s6_addr[13] == 0 && addr->s6_addr[14] == 0 &&
        addr->s6_addr[15] == 1)
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
        size_t len = (size_t)(end - start);
        if (len >= hostname_size)
            len = hostname_size - 1;
        memcpy(hostname, start, len);
        hostname[len] = '\0';
        return 0;
    }
    const char *end = start;
    while (*end && *end != '/' && *end != ':' && *end != '?' && *end != '#' && *end != ']')
        end++;
    size_t len = (size_t)(end - start);
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
                    char decoded = (char)val;
                    if (decoded == '/' || decoded == '\\' || decoded == '.' || decoded == '@' ||
                        decoded == ':' || decoded == '\0') {
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
                        struct sockaddr_in *addr = (struct sockaddr_in *)rp->ai_addr;
                        uint32_t ip = ntohl(addr->sin_addr.s_addr);
                        if (is_private_ip(ip)) {
                            unsafe = 1;
                            break;
                        }
                    } else if (rp->ai_family == AF_INET6) {
                        struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)rp->ai_addr;
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

static int ws_send_frame(int fd, const char *payload, size_t payload_len)
{
    uint8_t header[14];
    size_t header_size = 2;

    header[0] = 0x81;
    header[1] = 0x80;

    if (payload_len <= 125) {
        header[1] |= (uint8_t)payload_len;
    } else if (payload_len <= 65535) {
        header[1] |= 126;
        header[2] = (uint8_t)((payload_len >> 8) & 0xFF);
        header[3] = (uint8_t)(payload_len & 0xFF);
        header_size = 4;
    } else {
        header[1] |= 127;
        for (int i = 0; i < 8; i++) {
            header[2 + i] = (uint8_t)((payload_len >> (56 - i * 8)) & 0xFF);
        }
        header_size = 10;
    }

    uint8_t mask_key[4];
    mask_key[0] = (uint8_t)(rand() & 0xFF);
    mask_key[1] = (uint8_t)(rand() & 0xFF);
    mask_key[2] = (uint8_t)(rand() & 0xFF);
    mask_key[3] = (uint8_t)(rand() & 0xFF);
    memcpy(&header[header_size], mask_key, 4);
    header_size += 4;

    if (send(fd, header, header_size, 0) != (ssize_t)header_size) {
        return -1;
    }

    if (payload_len == 0)
        return 0;

    uint8_t *masked = (uint8_t *)AGENTOS_MALLOC(payload_len);
    if (!masked)
        return -1;

    for (size_t i = 0; i < payload_len; i++) {
        masked[i] = ((const uint8_t *)payload)[i] ^ mask_key[i % 4];
    }

    ssize_t sent = send(fd, masked, payload_len, 0);
    AGENTOS_FREE(masked);

    if (sent != (ssize_t)payload_len)
        return -1;
    return 0;
}

static int ws_recv_frame(int fd, char **out_payload, size_t *out_len, uint32_t timeout_ms)
{
    struct timeval tv;
    tv.tv_sec = (time_t)(timeout_ms / 1000);
    tv.tv_usec = (suseconds_t)((timeout_ms % 1000) * 1000);
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
        return -1;

    uint8_t header[2];
    ssize_t n = recv(fd, header, 2, 0);
    if (n != 2)
        return -1;

    uint8_t opcode = header[0] & 0x0F;
    uint8_t has_mask = header[1] & 0x80;
    uint64_t payload_len = header[1] & 0x7F;

    if (payload_len == 126) {
        uint8_t ext[2];
        if (recv(fd, ext, 2, 0) != 2)
            return -1;
        payload_len = ((uint64_t)ext[0] << 8) | ext[1];
    } else if (payload_len == 127) {
        uint8_t ext[8];
        if (recv(fd, ext, 8, 0) != 8)
            return -1;
        payload_len = 0;
        for (int i = 0; i < 8; i++) {
            payload_len = (payload_len << 8) | ext[i];
        }
    }

    if (payload_len > 10 * 1024 * 1024)
        return -1;

    uint8_t mask_key[4] = {0, 0, 0, 0};
    if (has_mask) {
        if (recv(fd, mask_key, 4, 0) != 4)
            return -1;
    }

    char *payload = (char *)AGENTOS_MALLOC((size_t)payload_len + 1);
    if (!payload)
        return -1;

    size_t total_read = 0;
    while (total_read < (size_t)payload_len) {
        n = recv(fd, payload + total_read, (size_t)payload_len - total_read, 0);
        if (n <= 0) {
            AGENTOS_FREE(payload);
            return -1;
        }
        total_read += (size_t)n;
    }
    payload[(size_t)payload_len] = '\0';

    if (has_mask) {
        for (size_t i = 0; i < (size_t)payload_len; i++) {
            payload[i] ^= mask_key[i % 4];
        }
    }

    if (opcode == 0x08) {
        AGENTOS_FREE(payload);
        return -2;
    }

    *out_payload = payload;
    if (out_len)
        *out_len = (size_t)payload_len;
    return 0;
}

static int cdp_get_id(void)
{
    static int s_cdp_id = 0;
    s_cdp_id++;
    if (s_cdp_id > 999999)
        s_cdp_id = 1;
    return s_cdp_id;
}

static char *js_escape(const char *src, size_t src_len)
{
    if (!src || src_len == 0)
        return AGENTOS_STRDUP("");

    size_t est = src_len * 2 + 4;
    char *dst = (char *)AGENTOS_MALLOC(est);
    if (!dst)
        return NULL;

    size_t dp = 0;
    for (size_t i = 0; i < src_len; i++) {
        char ch = src[i];
        if (ch == '\\') {
            if (dp + 2 >= est) {
                est *= 2;
                char *n = (char *)AGENTOS_MALLOC(est);
                if (!n) {
                    AGENTOS_FREE(dst);
                    return NULL;
                }
                memcpy(n, dst, dp);
                AGENTOS_FREE(dst);
                dst = n;
            }
            dst[dp++] = '\\';
            dst[dp++] = '\\';
        } else if (ch == '\'') {
            if (dp + 2 >= est) {
                est *= 2;
                char *n = (char *)AGENTOS_MALLOC(est);
                if (!n) {
                    AGENTOS_FREE(dst);
                    return NULL;
                }
                memcpy(n, dst, dp);
                AGENTOS_FREE(dst);
                dst = n;
            }
            dst[dp++] = '\\';
            dst[dp++] = '\'';
        } else if (ch == '"') {
            if (dp + 2 >= est) {
                est *= 2;
                char *n = (char *)AGENTOS_MALLOC(est);
                if (!n) {
                    AGENTOS_FREE(dst);
                    return NULL;
                }
                memcpy(n, dst, dp);
                AGENTOS_FREE(dst);
                dst = n;
            }
            dst[dp++] = '\\';
            dst[dp++] = '"';
        } else if (ch == '\n') {
            if (dp + 2 >= est) {
                est *= 2;
                char *n = (char *)AGENTOS_MALLOC(est);
                if (!n) {
                    AGENTOS_FREE(dst);
                    return NULL;
                }
                memcpy(n, dst, dp);
                AGENTOS_FREE(dst);
                dst = n;
            }
            dst[dp++] = '\\';
            dst[dp++] = 'n';
        } else if (ch == '\r') {
            if (dp + 2 >= est) {
                est *= 2;
                char *n = (char *)AGENTOS_MALLOC(est);
                if (!n) {
                    AGENTOS_FREE(dst);
                    return NULL;
                }
                memcpy(n, dst, dp);
                AGENTOS_FREE(dst);
                dst = n;
            }
            dst[dp++] = '\\';
            dst[dp++] = 'r';
        } else if (ch == '\t') {
            if (dp + 2 >= est) {
                est *= 2;
                char *n = (char *)AGENTOS_MALLOC(est);
                if (!n) {
                    AGENTOS_FREE(dst);
                    return NULL;
                }
                memcpy(n, dst, dp);
                AGENTOS_FREE(dst);
                dst = n;
            }
            dst[dp++] = '\\';
            dst[dp++] = 't';
        } else {
            if (dp + 1 >= est) {
                est *= 2;
                char *n = (char *)AGENTOS_MALLOC(est);
                if (!n) {
                    AGENTOS_FREE(dst);
                    return NULL;
                }
                memcpy(n, dst, dp);
                AGENTOS_FREE(dst);
                dst = n;
            }
            dst[dp++] = ch;
        }
    }
    dst[dp] = '\0';
    return dst;
}

static agentos_error_t browser_execute(agentos_execution_unit_t *unit, const void *input,
                                       void **out_output)
{
    agentos_error_t ret = AGENTOS_SUCCESS;
    char *result_json = NULL;
    cdp_connection_t *conn = NULL;
    int has_cdp = 0;

    if (!input || !out_output)
        return AGENTOS_EINVAL;

    const char *cmd = (const char *)input;

    browser_unit_data_t *bdata = NULL;
    if (unit && unit->execution_unit_data) {
        bdata = (browser_unit_data_t *)unit->execution_unit_data;
    }

    if (bdata && bdata->agent_id[0]) {
        if (cdp_pool_acquire(bdata->agent_id, NULL, &conn) == 0 && conn && conn->socket_fd >= 0) {
            has_cdp = 1;
        } else if (conn) {
            cdp_pool_release(conn);
            conn = NULL;
        }
    }

    if (strstr(cmd, "navigate") != NULL) {
        const char *url_start = strstr(cmd, "http");
        if (!url_start) {
            ret = AGENTOS_EINVAL;
            result_json = AGENTOS_STRDUP("{\"error\":\"no_url_provided\",\"status\":\"failed\"}");
            if (!result_json) {
                ret = AGENTOS_ENOMEM;
                goto cleanup;
            }
            *out_output = result_json;
            goto cleanup;
        }
        if (!is_safe_url(url_start)) {
            ret = AGENTOS_EPERM;
            result_json = AGENTOS_STRDUP("{\"error\":\"unsafe_url\",\"status\":\"denied\"}");
            if (!result_json) {
                ret = AGENTOS_ENOMEM;
                goto cleanup;
            }
            *out_output = result_json;
            goto cleanup;
        }

        size_t url_len = strlen(url_start);
        char *url_copy = (char *)AGENTOS_MALLOC(url_len + 1);
        if (!url_copy) {
            ret = AGENTOS_ENOMEM;
            goto cleanup;
        }
        memcpy(url_copy, url_start, url_len + 1);
        for (char *p = url_copy; *p; p++) {
            if (*p == ' ' || *p == '\n' || *p == '\r') {
                *p = '\0';
                break;
            }
        }

        int cdp_ok = 0;
        if (has_cdp) {
            int cdp_id = cdp_get_id();
            char cdp_json[8192];
            int written = snprintf(cdp_json, sizeof(cdp_json),
                                   "{\"id\":%d,\"method\":\"Page.navigate\","
                                   "\"params\":{\"url\":\"%s\"}}",
                                   cdp_id, url_copy);
            if (written > 0 && (size_t)written < sizeof(cdp_json)) {
                if (ws_send_frame(conn->socket_fd, cdp_json, strlen(cdp_json)) == 0) {
                    char *resp = NULL;
                    if (ws_recv_frame(conn->socket_fd, &resp, NULL, 5000) == 0 && resp) {
                        if (strstr(resp, "\"result\"")) {
                            cdp_ok = 1;
                            size_t buf_size = 96 + url_len + 1;
                            result_json = (char *)AGENTOS_MALLOC(buf_size);
                            if (result_json) {
                                snprintf(result_json, buf_size,
                                         "{\"status\":\"navigated\",\"url\":\"%s\","
                                         "\"cdp\":true}",
                                         url_copy);
                            }
                        }
                        AGENTOS_FREE(resp);
                    }
                }
            }
        }

        if (!cdp_ok || !result_json) {
            size_t buf_size = 96 + url_len + 1;
            result_json = (char *)AGENTOS_MALLOC(buf_size);
            if (!result_json) {
                AGENTOS_FREE(url_copy);
                ret = AGENTOS_ENOMEM;
                goto cleanup;
            }
            snprintf(result_json, buf_size,
                     "{\"status\":\"navigated\",\"url\":\"%s\",\"simulated\":true}", url_copy);
        }
        AGENTOS_FREE(url_copy);
        if (!result_json) {
            ret = AGENTOS_ENOMEM;
            goto cleanup;
        }
        *out_output = result_json;
        goto cleanup;
    } else if (strstr(cmd, "evaluate") != NULL || strstr(cmd, "exec") != NULL) {
        const char *script = strstr(cmd, "script=");
        if (!script)
            script = strstr(cmd, "expr=");
        if (!script) {
            ret = AGENTOS_EINVAL;
            *out_output = AGENTOS_STRDUP("{\"error\":\"missing_script\",\"status\":\"failed\"}");
            if (!*out_output)
                ret = AGENTOS_ENOMEM;
            goto cleanup;
        }

        script += strstr(cmd, "script=") ? strlen("script=") : strlen("expr=");
        const char *dangerous[] = {
            "document.cookie", "localStorage", "sessionStorage", "eval(",   "Function(",
            "XMLHttpRequest",  "fetch(",       "WebSocket",      "import(", "require("};
        for (size_t d = 0; d < sizeof(dangerous) / sizeof(dangerous[0]); d++) {
            if (strstr(script, dangerous[d])) {
                ret = AGENTOS_SUCCESS;
                size_t buf_size = 160 + strlen(dangerous[d]) + 1;
                result_json = (char *)AGENTOS_MALLOC(buf_size);
                if (!result_json) {
                    ret = AGENTOS_ENOMEM;
                    goto cleanup;
                }
                snprintf(result_json, buf_size,
                         "{\"error\":\"blocked_pattern\",\"pattern\":\"%s\","
                         "\"status\":\"rejected\"}",
                         dangerous[d]);
                *out_output = result_json;
                goto cleanup;
            }
        }

        size_t script_len = strlen(script);
        while (script_len > 0 && (script[script_len - 1] == '\n' || script[script_len - 1] == '\r'))
            script_len--;

        int cdp_ok = 0;
        if (has_cdp && script_len > 0) {
            int cdp_id = cdp_get_id();
            char *escaped_script = js_escape(script, script_len);
            if (escaped_script) {
                char cdp_json[16384];
                int written = snprintf(cdp_json, sizeof(cdp_json),
                                       "{\"id\":%d,\"method\":\"Runtime.evaluate\",\"params\":"
                                       "{\"expression\":\"%s\",\"returnByValue\":true}}",
                                       cdp_id, escaped_script);
                AGENTOS_FREE(escaped_script);

                if (written > 0 && (size_t)written < sizeof(cdp_json)) {
                    if (ws_send_frame(conn->socket_fd, cdp_json, strlen(cdp_json)) == 0) {
                        char *resp = NULL;
                        if (ws_recv_frame(conn->socket_fd, &resp, NULL, 5000) == 0 && resp) {
                            if (strstr(resp, "\"result\"")) {
                                cdp_ok = 1;
                                size_t buf_size = 256 + script_len + 1;
                                result_json = (char *)AGENTOS_MALLOC(buf_size);
                                if (result_json) {
                                    snprintf(result_json, buf_size,
                                             "{\"status\":\"evaluated\","
                                             "\"script_length\":%zu,\"cdp\":true}",
                                             script_len);
                                }
                            }
                            AGENTOS_FREE(resp);
                        }
                    }
                }
            }
        }

        if (!cdp_ok || !result_json) {
            size_t buf_size = 256 + script_len + 1;
            result_json = (char *)AGENTOS_MALLOC(buf_size);
            if (!result_json) {
                ret = AGENTOS_ENOMEM;
                goto cleanup;
            }
            snprintf(result_json, buf_size,
                     "{\"status\":\"evaluated\",\"result\":null,\"script_length\":%zu,"
                     "\"simulated\":true}",
                     script_len);
        }
        *out_output = result_json;
        goto cleanup;
    } else if (strstr(cmd, "click") != NULL) {
        const char *selector = strstr(cmd, "selector=");
        char *sel_copy = NULL;

        if (selector) {
            selector += strlen("selector=");
            size_t sel_len = strlen(selector);
            sel_copy = (char *)AGENTOS_MALLOC(sel_len + 1);
            if (!sel_copy) {
                ret = AGENTOS_ENOMEM;
                goto cleanup;
            }
            memcpy(sel_copy, selector, sel_len + 1);
            for (char *p = sel_copy; *p; p++) {
                if (*p == ' ' || *p == '\n' || *p == '\r') {
                    *p = '\0';
                    break;
                }
            }
        }

        int cdp_ok = 0;
        if (has_cdp && sel_copy) {
            int cdp_id = cdp_get_id();
            char *escaped_sel = js_escape(sel_copy, strlen(sel_copy));
            if (escaped_sel) {
                char cdp_script[4096];
                int written = snprintf(cdp_script, sizeof(cdp_script),
                                       "document.querySelector('%s').click()", escaped_sel);
                AGENTOS_FREE(escaped_sel);
                if (written > 0 && (size_t)written < sizeof(cdp_script)) {
                    char cdp_json[8192];
                    written = snprintf(cdp_json, sizeof(cdp_json),
                                       "{\"id\":%d,\"method\":\"Runtime.evaluate\","
                                       "\"params\":{\"expression\":\"%s\","
                                       "\"returnByValue\":true}}",
                                       cdp_id, cdp_script);
                    if (written > 0 && (size_t)written < sizeof(cdp_json)) {
                        if (ws_send_frame(conn->socket_fd, cdp_json, strlen(cdp_json)) == 0) {
                            char *resp = NULL;
                            if (ws_recv_frame(conn->socket_fd, &resp, NULL, 5000) == 0 && resp) {
                                if (strstr(resp, "\"result\"")) {
                                    cdp_ok = 1;
                                    size_t buf_size = 128 + strlen(sel_copy) + 1;
                                    result_json = (char *)AGENTOS_MALLOC(buf_size);
                                    if (result_json) {
                                        snprintf(result_json, buf_size,
                                                 "{\"status\":\"clicked\",\"selector\":"
                                                 "\"%s\",\"cdp\":true}",
                                                 sel_copy);
                                    }
                                }
                                AGENTOS_FREE(resp);
                            }
                        }
                    }
                }
            }
        }

        if (!cdp_ok || !result_json) {
            if (sel_copy) {
                size_t buf_size = 128 + strlen(sel_copy) + 1;
                result_json = (char *)AGENTOS_MALLOC(buf_size);
                if (!result_json) {
                    AGENTOS_FREE(sel_copy);
                    ret = AGENTOS_ENOMEM;
                    goto cleanup;
                }
                snprintf(result_json, buf_size,
                         "{\"status\":\"clicked\",\"selector\":\"%s\","
                         "\"simulated\":true}",
                         sel_copy);
            } else {
                result_json = AGENTOS_STRDUP("{\"status\":\"clicked\",\"selector\":\"unknown\","
                                             "\"simulated\":true}");
                if (!result_json) {
                    AGENTOS_FREE(sel_copy);
                    ret = AGENTOS_ENOMEM;
                    goto cleanup;
                }
            }
        }
        AGENTOS_FREE(sel_copy);
        *out_output = result_json;
        goto cleanup;
    } else if (strstr(cmd, "screenshot") != NULL) {
        const char *fmt = strstr(cmd, "format=");
        char fmt_buf[16] = "png";

        if (fmt) {
            fmt += strlen("format=");
            size_t f_len = 0;
            while (fmt[f_len] && fmt[f_len] != ' ' && fmt[f_len] != ',' && fmt[f_len] != '\n' &&
                   f_len < 15) {
                fmt_buf[f_len] = fmt[f_len];
                f_len++;
            }
            fmt_buf[f_len] = '\0';
        }

        int cdp_ok = 0;
        if (has_cdp) {
            int cdp_id = cdp_get_id();
            char cdp_json[1024];
            int written = snprintf(cdp_json, sizeof(cdp_json),
                                   "{\"id\":%d,\"method\":\"Page.captureScreenshot\","
                                   "\"params\":{\"format\":\"%s\"}}",
                                   cdp_id, fmt_buf);
            if (written > 0 && (size_t)written < sizeof(cdp_json)) {
                if (ws_send_frame(conn->socket_fd, cdp_json, strlen(cdp_json)) == 0) {
                    char *resp = NULL;
                    if (ws_recv_frame(conn->socket_fd, &resp, NULL, 10000) == 0 && resp) {
                        const char *data_field = strstr(resp, "\"data\":");
                        size_t data_len = 0;
                        if (data_field)
                            data_len = strlen(resp);
                        if (strstr(resp, "\"result\"") && data_field) {
                            cdp_ok = 1;
                            size_t buf_size = 256;
                            result_json = (char *)AGENTOS_MALLOC(buf_size);
                            if (result_json) {
                                snprintf(result_json, buf_size,
                                         "{\"status\":\"screenshot_taken\","
                                         "\"format\":\"%s\",\"cdp\":true,"
                                         "\"response_size\":%zu}",
                                         fmt_buf, data_len);
                            }
                        }
                        AGENTOS_FREE(resp);
                    }
                }
            }
        }

        if (!cdp_ok || !result_json) {
            size_t buf_size = 192;
            result_json = (char *)AGENTOS_MALLOC(buf_size);
            if (!result_json) {
                ret = AGENTOS_ENOMEM;
                goto cleanup;
            }
            snprintf(result_json, buf_size,
                     "{\"status\":\"screenshot_taken\",\"format\":\"%s\","
                     "\"size_bytes\":0,\"simulated\":true}",
                     fmt_buf);
        }
        *out_output = result_json;
        goto cleanup;
    } else if (strstr(cmd, "type") != NULL || strstr(cmd, "fill") != NULL) {
        const char *selector = strstr(cmd, "selector=");
        const char *value = strstr(cmd, "value=");
        char sel_buf[128] = "unknown";
        char val_buf[256] = "";
        if (selector) {
            selector += strlen("selector=");
            size_t s_len = 0;
            while (selector[s_len] && selector[s_len] != ' ' && selector[s_len] != ',' &&
                   selector[s_len] != '\n' && s_len < 127) {
                sel_buf[s_len] = selector[s_len];
                s_len++;
            }
            sel_buf[s_len] = '\0';
        }
        if (value) {
            value += strlen("value=");
            size_t v_len = 0;
            while (value[v_len] && value[v_len] != '\n' && value[v_len] != '\r' && v_len < 255) {
                val_buf[v_len] = value[v_len];
                v_len++;
            }
            val_buf[v_len] = '\0';
        }

        int cdp_ok = 0;
        if (has_cdp) {
            char *escaped_sel = js_escape(sel_buf, strlen(sel_buf));
            if (escaped_sel) {
                int focus_id = cdp_get_id();
                char focus_json[8192];
                int fw = snprintf(focus_json, sizeof(focus_json),
                                  "{\"id\":%d,\"method\":\"Runtime.evaluate\","
                                  "\"params\":{\"expression\":"
                                  "\"document.querySelector('%s').focus()\","
                                  "\"returnByValue\":true}}",
                                  focus_id, escaped_sel);
                if (fw > 0 && (size_t)fw < sizeof(focus_json)) {
                    if (ws_send_frame(conn->socket_fd, focus_json, strlen(focus_json)) == 0) {
                        char *focus_resp = NULL;
                        if (ws_recv_frame(conn->socket_fd, &focus_resp, NULL, 3000) == 0 &&
                            focus_resp) {
                            AGENTOS_FREE(focus_resp);
                        }
                    }
                }
                AGENTOS_FREE(escaped_sel);
            }

            if (strstr(cmd, "fill") != NULL) {
                int insert_id = cdp_get_id();
                char *escaped_val = js_escape(val_buf, strlen(val_buf));
                if (escaped_val) {
                    char insert_json[8192];
                    int iw = snprintf(insert_json, sizeof(insert_json),
                                      "{\"id\":%d,\"method\":\"Input.insertText\","
                                      "\"params\":{\"text\":\"%s\"}}",
                                      insert_id, escaped_val);
                    AGENTOS_FREE(escaped_val);
                    if (iw > 0 && (size_t)iw < sizeof(insert_json)) {
                        if (ws_send_frame(conn->socket_fd, insert_json, strlen(insert_json)) == 0) {
                            char *ins_resp = NULL;
                            if (ws_recv_frame(conn->socket_fd, &ins_resp, NULL, 3000) == 0 &&
                                ins_resp) {
                                if (strstr(ins_resp, "\"result\"")) {
                                    cdp_ok = 1;
                                    size_t buf_size = 256 + strlen(sel_buf) + strlen(val_buf) + 1;
                                    result_json = (char *)AGENTOS_MALLOC(buf_size);
                                    if (result_json) {
                                        snprintf(result_json, buf_size,
                                                 "{\"status\":\"filled\","
                                                 "\"selector\":\"%s\","
                                                 "\"value\":\"%s\","
                                                 "\"cdp_method\":\"Input.insertText\","
                                                 "\"cdp\":true}",
                                                 sel_buf, val_buf);
                                    }
                                }
                                AGENTOS_FREE(ins_resp);
                            }
                        }
                    }
                }
            } else {
                size_t vlen = strlen(val_buf);
                int all_sent = 1;
                for (size_t ci = 0; ci < vlen && all_sent; ci++) {
                    char key_char = val_buf[ci];
                    char key_str[8];
                    if (key_char >= 'a' && key_char <= 'z')
                        snprintf(key_str, sizeof(key_str), "%c", key_char);
                    else if (key_char >= 'A' && key_char <= 'Z')
                        snprintf(key_str, sizeof(key_str), "%c", key_char);
                    else if (key_char >= '0' && key_char <= '9')
                        snprintf(key_str, sizeof(key_str), "%c", key_char);
                    else if (key_char == ' ')
                        snprintf(key_str, sizeof(key_str), " ");
                    else if (key_char == '\n')
                        snprintf(key_str, sizeof(key_str), "Enter");
                    else if (key_char == '\t')
                        snprintf(key_str, sizeof(key_str), "Tab");
                    else if (key_char == '\b')
                        snprintf(key_str, sizeof(key_str), "Backspace");
                    else if (key_char == '\x1b')
                        snprintf(key_str, sizeof(key_str), "Escape");
                    else
                        snprintf(key_str, sizeof(key_str), "%c", key_char);

                    int kid = cdp_get_id();
                    char kd_json[4096];
                    int kw = snprintf(kd_json, sizeof(kd_json),
                                      "{\"id\":%d,\"method\":\"Input.dispatchKeyEvent\","
                                      "\"params\":{\"type\":\"keyDown\","
                                      "\"key\":\"%s\","
                                      "\"text\":\"%c\"}}",
                                      kid, key_str, key_char);
                    if (kw <= 0 || (size_t)kw >= sizeof(kd_json)) {
                        all_sent = 0;
                        break;
                    }
                    if (ws_send_frame(conn->socket_fd, kd_json, strlen(kd_json)) != 0) {
                        all_sent = 0;
                        break;
                    }
                    char *kd_resp = NULL;
                    if (ws_recv_frame(conn->socket_fd, &kd_resp, NULL, 1000) == 0 && kd_resp)
                        AGENTOS_FREE(kd_resp);

                    int kid2 = cdp_get_id();
                    char ku_json[2048];
                    kw = snprintf(ku_json, sizeof(ku_json),
                                  "{\"id\":%d,\"method\":\"Input.dispatchKeyEvent\","
                                  "\"params\":{\"type\":\"keyUp\","
                                  "\"key\":\"%s\"}}",
                                  kid2, key_str);
                    if (kw <= 0 || (size_t)kw >= sizeof(ku_json)) {
                        all_sent = 0;
                        break;
                    }
                    if (ws_send_frame(conn->socket_fd, ku_json, strlen(ku_json)) != 0) {
                        all_sent = 0;
                        break;
                    }
                    char *ku_resp = NULL;
                    if (ws_recv_frame(conn->socket_fd, &ku_resp, NULL, 1000) == 0 && ku_resp)
                        AGENTOS_FREE(ku_resp);
                }

                if (all_sent && vlen > 0) {
                    cdp_ok = 1;
                    size_t buf_size = 256 + strlen(sel_buf) + strlen(val_buf) + 1;
                    result_json = (char *)AGENTOS_MALLOC(buf_size);
                    if (result_json) {
                        snprintf(result_json, buf_size,
                                 "{\"status\":\"typed\","
                                 "\"selector\":\"%s\","
                                 "\"value\":\"%s\","
                                 "\"cdp_method\":\"Input.dispatchKeyEvent\","
                                 "\"cdp\":true}",
                                 sel_buf, val_buf);
                    }
                }
            }
        }

        if (!cdp_ok || !result_json) {
            size_t buf_size = 256 + strlen(sel_buf) + strlen(val_buf) + 1;
            result_json = (char *)AGENTOS_MALLOC(buf_size);
            if (!result_json) {
                ret = AGENTOS_ENOMEM;
                goto cleanup;
            }
            snprintf(result_json, buf_size,
                     "{\"status\":\"typed\",\"selector\":\"%s\","
                     "\"value\":\"%s\",\"simulated\":true}",
                     sel_buf, val_buf);
        }
        *out_output = result_json;
        goto cleanup;
    } else if (strstr(cmd, "wait") != NULL) {
        const char *timeout = strstr(cmd, "timeout=");
        const char *wait_selector = strstr(cmd, "selector=");
        uint32_t timeout_ms = 5000;
        if (timeout) {
            timeout += strlen("timeout=");
            timeout_ms = (uint32_t)strtoul(timeout, NULL, 10);
            if (timeout_ms == 0)
                timeout_ms = 5000;
        }

        int cdp_ok = 0;
        if (has_cdp) {
            int cdp_id = cdp_get_id();
            char cdp_js[4096];
            int written;

            if (wait_selector) {
                wait_selector += strlen("selector=");
                size_t sel_len = strlen(wait_selector);
                char *sel_copy = (char *)AGENTOS_MALLOC(sel_len + 1);
                if (!sel_copy) {
                    ret = AGENTOS_ENOMEM;
                    goto cleanup;
                }
                memcpy(sel_copy, wait_selector, sel_len + 1);
                for (char *p = sel_copy; *p; p++) {
                    if (*p == ' ' || *p == '\n' || *p == '\r' || *p == ',') {
                        *p = '\0';
                        break;
                    }
                }
                char *escaped_sel = js_escape(sel_copy, strlen(sel_copy));
                AGENTOS_FREE(sel_copy);
                if (escaped_sel) {
                    written = snprintf(cdp_js, sizeof(cdp_js),
                                       "(function(){var s='%s';var t=%u;"
                                       "return new Promise(function(r){"
                                       "var st=Date.now();"
                                       "var chk=function(){"
                                       "if(document.querySelector(s)){r('found');}"
                                       "else if(Date.now()-st>t){r('timeout');}"
                                       "else{setTimeout(chk,100);}};"
                                       "chk();})})()",
                                       escaped_sel, timeout_ms);
                    AGENTOS_FREE(escaped_sel);
                } else {
                    goto cdpskip;
                }
            } else {
                written = snprintf(cdp_js, sizeof(cdp_js),
                                   "new Promise(function(r){"
                                   "setTimeout(function(){r('waited');},%u);})",
                                   timeout_ms);
            }

            if (written > 0 && (size_t)written < sizeof(cdp_js)) {
                char cdp_json[8192];
                int w2 = snprintf(cdp_json, sizeof(cdp_json),
                                  "{\"id\":%d,\"method\":\"Runtime.evaluate\","
                                  "\"params\":{\"expression\":\"%s\","
                                  "\"returnByValue\":true,\"awaitPromise\":true}}",
                                  cdp_id, cdp_js);
                if (w2 > 0 && (size_t)w2 < sizeof(cdp_json)) {
                    if (ws_send_frame(conn->socket_fd, cdp_json, strlen(cdp_json)) == 0) {
                        char *resp = NULL;
                        uint32_t recv_to = timeout_ms + 10000;
                        if (ws_recv_frame(conn->socket_fd, &resp, NULL, recv_to) == 0 && resp) {
                            if (strstr(resp, "\"result\"")) {
                                cdp_ok = 1;
                            }
                            AGENTOS_FREE(resp);
                        }
                    }
                }
            }
        }

    cdpskip:
        if (cdp_ok) {
            if (wait_selector) {
                wait_selector += strlen("selector=");
                size_t sel_len = strlen(wait_selector);
                char *sel_copy = (char *)AGENTOS_MALLOC(sel_len + 1);
                if (!sel_copy) {
                    ret = AGENTOS_ENOMEM;
                    goto cleanup;
                }
                memcpy(sel_copy, wait_selector, sel_len + 1);
                for (char *p = sel_copy; *p; p++) {
                    if (*p == ' ' || *p == '\n' || *p == '\r' || *p == ',') {
                        *p = '\0';
                        break;
                    }
                }
                size_t buf_size = 160 + strlen(sel_copy) + 1;
                result_json = (char *)AGENTOS_MALLOC(buf_size);
                if (!result_json) {
                    AGENTOS_FREE(sel_copy);
                    ret = AGENTOS_ENOMEM;
                    goto cleanup;
                }
                snprintf(result_json, buf_size,
                         "{\"status\":\"waited\",\"selector\":\"%s\","
                         "\"timeout_ms\":%u,\"cdp\":true}",
                         sel_copy, timeout_ms);
                AGENTOS_FREE(sel_copy);
            } else {
                size_t buf_size = 160;
                result_json = (char *)AGENTOS_MALLOC(buf_size);
                if (!result_json) {
                    ret = AGENTOS_ENOMEM;
                    goto cleanup;
                }
                snprintf(result_json, buf_size,
                         "{\"status\":\"waited\",\"timeout_ms\":%u,"
                         "\"cdp\":true}",
                         timeout_ms);
            }
            *out_output = result_json;
            goto cleanup;
        }

        if (wait_selector) {
            wait_selector += strlen("selector=");
            size_t sel_len = strlen(wait_selector);
            char *sel_copy = (char *)AGENTOS_MALLOC(sel_len + 1);
            if (!sel_copy) {
                ret = AGENTOS_ENOMEM;
                goto cleanup;
            }
            memcpy(sel_copy, wait_selector, sel_len + 1);
            for (char *p = sel_copy; *p; p++) {
                if (*p == ' ' || *p == '\n' || *p == '\r' || *p == ',') {
                    *p = '\0';
                    break;
                }
            }
            size_t buf_size = 160 + strlen(sel_copy) + 1;
            result_json = (char *)AGENTOS_MALLOC(buf_size);
            if (!result_json) {
                AGENTOS_FREE(sel_copy);
                ret = AGENTOS_ENOMEM;
                goto cleanup;
            }
            snprintf(result_json, buf_size,
                     "{\"status\":\"waited\",\"selector\":\"%s\","
                     "\"timeout_ms\":%u,\"simulated\":true}",
                     sel_copy, timeout_ms);
            AGENTOS_FREE(sel_copy);
        } else {
            size_t buf_size = 160;
            result_json = (char *)AGENTOS_MALLOC(buf_size);
            if (!result_json) {
                ret = AGENTOS_ENOMEM;
                goto cleanup;
            }
            snprintf(result_json, buf_size,
                     "{\"status\":\"waited\",\"timeout_ms\":%u,"
                     "\"simulated\":true}",
                     timeout_ms);
        }
        *out_output = result_json;
        goto cleanup;
    }

    ret = AGENTOS_EPROTONOSUPPORT;
    *out_output = AGENTOS_STRDUP("{\"error\":\"unsupported_command\",\"status\":\"failed\"}");
    if (!*out_output)
        ret = AGENTOS_ENOMEM;

cleanup:
    if (conn)
        cdp_pool_release(conn);
    return ret;
}

static void browser_destroy(agentos_execution_unit_t *unit)
{
    if (!unit)
        return;
    browser_unit_data_t *data = (browser_unit_data_t *)unit->execution_unit_data;
    if (data) {
        if (data->metadata_json)
            AGENTOS_FREE(data->metadata_json);
        AGENTOS_FREE(data);
    }
    AGENTOS_FREE(unit);
}

agentos_execution_unit_t *agentos_browser_unit_create(void)
{
    agentos_execution_unit_t *unit =
        (agentos_execution_unit_t *)AGENTOS_MALLOC(sizeof(agentos_execution_unit_t));
    if (!unit)
        return NULL;
    memset(unit, 0, sizeof(*unit));

    browser_unit_data_t *data = (browser_unit_data_t *)AGENTOS_MALLOC(sizeof(browser_unit_data_t));
    if (!data) {
        AGENTOS_FREE(unit);
        return NULL;
    }
    memset(data, 0, sizeof(*data));

    char meta[128];
    snprintf(meta, sizeof(meta), "{\"type\":\"browser\"}");
    data->metadata_json = AGENTOS_STRDUP(meta);

    if (!data->metadata_json) {
        AGENTOS_FREE(data);
        AGENTOS_FREE(unit);
        return NULL;
    }

    unit->execution_unit_data = data;
    unit->execution_unit_execute = browser_execute;
    unit->execution_unit_destroy = browser_destroy;

    return unit;
}

int agentos_browser_unit_set_agent(agentos_execution_unit_t *unit, const char *agent_id)
{
    if (!unit || !unit->execution_unit_data || !agent_id)
        return -1;
    browser_unit_data_t *data = (browser_unit_data_t *)unit->execution_unit_data;
    strncpy(data->agent_id, agent_id, sizeof(data->agent_id) - 1);
    data->agent_id[sizeof(data->agent_id) - 1] = '\0';
    return 0;
}
