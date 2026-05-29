#include "memory_compat.h"
/*
 * Copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "atomic_compat.h"
#include "error.h"
#include "platform.h"
#include "svc_logger.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef _WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#define NOTIFY_D_DEFAULT_PORT 8084
#define NOTIFY_D_MAX_BUFFER 65536
#define NOTIFY_D_DEFAULT_SOCKET AGENTOS_RUNTIME_DIR "/notify.sock"
#define NOTIFY_D_MAX_PENDING 1024
#define NOTIFY_D_MAX_CLIENTS 128
#define NOTIFY_D_WS_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

typedef enum {
    NOTIFY_CLIENT_SOCKET,
    NOTIFY_CLIENT_WEBSOCKET,
    NOTIFY_CLIENT_SSE
} notify_client_type_t;

typedef struct {
    agentos_socket_t fd;
    notify_client_type_t type;
    char *channel;
    uint64_t connected_at;
    uint64_t last_activity;
    uint64_t messages_sent;
    int active;
    char handshake_done;
} notify_client_t;

typedef struct {
    char *message;
    char *channel;
    char *event_type;
    uint64_t timestamp;
} notify_event_t;

typedef struct {
    agentos_socket_t server_fd;
    agentos_mutex_t lock;
    agentos_thread_t event_thread;
    atomic_int running;
    atomic_int event_running;
    atomic_int force_stop;
    uint64_t start_time;
    uint64_t notified_count;
    uint64_t error_count;
    notify_client_t clients[NOTIFY_D_MAX_CLIENTS];
    size_t client_count;
    notify_event_t *pending[NOTIFY_D_MAX_PENDING];
    size_t pending_head;
    size_t pending_tail;
    size_t pending_count;
    int tcp_port;
    char *socket_path;
} notify_d_service_t;

static notify_d_service_t g_service = {0};
static atomic_int g_shutdown = 0;

static void notify_d_signal_handler(int sig)
{

    atomic_store_explicit(&g_shutdown, 1, memory_order_seq_cst);
}

static int notify_d_compute_ws_accept_key(const char *client_key, char *out_key, size_t out_size)
{
    if (!client_key || !out_key || out_size < 64) {
    AGENTOS_ERROR_HANDLE(AGENTOS_ERR_INVALID_PARAM, "null parameter");
        return AGENTOS_ERR_INVALID_PARAM;
    }

    char combined[256];
    snprintf(combined, sizeof(combined), "%s%s", client_key, NOTIFY_D_WS_GUID);

    unsigned char sha1[20];
    memset(sha1, 0, sizeof(sha1));

    unsigned int h0 = 0x67452301, h1 = 0xEFCDAB89, h2 = 0x98BADCFE;
    unsigned int h3 = 0x10325476, h4 = 0xC3D2E1F0;

    size_t msg_len = strlen(combined);
    size_t padded_len = ((msg_len + 8) / 64 + 1) * 64;
    unsigned char *padded = (unsigned char *)AGENTOS_CALLOC(1, padded_len);
    if (!padded) {
    AGENTOS_ERROR_HANDLE(AGENTOS_ERR_OUT_OF_MEMORY, "calloc failed for SHA1 padded buffer");
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }
    memcpy(padded, combined, msg_len);
    padded[msg_len] = 0x80;

    uint64_t bit_len = (uint64_t)msg_len * 8;
    for (int i = 0; i < 8; i++)
        padded[padded_len - 8 + i] = (unsigned char)(bit_len >> (56 - 8 * i));

    for (size_t off = 0; off < padded_len; off += 64) {
        unsigned int w[80];
        for (int i = 0; i < 16; i++) {
            w[i] = ((unsigned int)padded[off + i * 4] << 24) |
                   ((unsigned int)padded[off + i * 4 + 1] << 16) |
                   ((unsigned int)padded[off + i * 4 + 2] << 8) |
                   ((unsigned int)padded[off + i * 4 + 3]);
        }
        for (int i = 16; i < 80; i++) {
            w[i] = (w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16]);
            w[i] = (w[i] << 1) | (w[i] >> 31);
        }
        unsigned int a = h0, b = h1, c = h2, d = h3, e = h4;
        for (int i = 0; i < 80; i++) {
            unsigned int f, k;
            if (i < 20) {
                f = (b & c) | (~b & d);
                k = 0x5A827999;
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1;
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDC;
            } else {
                f = b ^ c ^ d;
                k = 0xCA62C1D6;
            }
            unsigned int temp = ((a << 5) | (a >> 27)) + f + e + k + w[i];
            e = d;
            d = c;
            c = (b << 30) | (b >> 2);
            b = a;
            a = temp;
        }
        h0 += a;
        h1 += b;
        h2 += c;
        h3 += d;
        h4 += e;
    }
    AGENTOS_FREE(padded);

    sha1[0] = (unsigned char)(h0 >> 24);
    sha1[1] = (unsigned char)(h0 >> 16);
    sha1[2] = (unsigned char)(h0 >> 8);
    sha1[3] = (unsigned char)(h0);
    sha1[4] = (unsigned char)(h1 >> 24);
    sha1[5] = (unsigned char)(h1 >> 16);
    sha1[6] = (unsigned char)(h1 >> 8);
    sha1[7] = (unsigned char)(h1);
    sha1[8] = (unsigned char)(h2 >> 24);
    sha1[9] = (unsigned char)(h2 >> 16);
    sha1[10] = (unsigned char)(h2 >> 8);
    sha1[11] = (unsigned char)(h2);
    sha1[12] = (unsigned char)(h3 >> 24);
    sha1[13] = (unsigned char)(h3 >> 16);
    sha1[14] = (unsigned char)(h3 >> 8);
    sha1[15] = (unsigned char)(h3);
    sha1[16] = (unsigned char)(h4 >> 24);
    sha1[17] = (unsigned char)(h4 >> 16);
    sha1[18] = (unsigned char)(h4 >> 8);
    sha1[19] = (unsigned char)(h4);

    static const char *b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t off = 0;
    for (int i = 0; i < 20 && off < out_size - 4; i += 3) {
        unsigned int val = ((unsigned int)sha1[i] << 16) | ((unsigned int)sha1[i + 1] << 8) |
                           (unsigned int)sha1[i + 2];
        out_key[off++] = b64[(val >> 18) & 0x3F];
        out_key[off++] = b64[(val >> 12) & 0x3F];
        out_key[off++] = b64[(val >> 6) & 0x3F];
        out_key[off++] = b64[val & 0x3F];
    }
    out_key[27] = '=';
    out_key[28] = '\0';

    return 0;
}

static int notify_d_handle_ws_upgrade(notify_d_service_t *svc, notify_client_t *client,
                                      const char *request)
{
    const char *key_tag = "Sec-WebSocket-Key: ";
    const char *key_start = strstr(request, key_tag);
    if (!key_start) {
    AGENTOS_ERROR_HANDLE(AGENTOS_ERR_UNKNOWN, "missing Sec-WebSocket-Key header");
        return AGENTOS_ERR_UNKNOWN;
    }
    key_start += strlen(key_tag);

    const char *key_end = strstr(key_start, "\r\n");
    if (!key_end) {
    AGENTOS_ERROR_HANDLE(AGENTOS_ERR_UNKNOWN, "Sec-WebSocket-Key value not terminated by CRLF");
        return AGENTOS_ERR_UNKNOWN;
    }

    char client_key[256];
    size_t key_len = (size_t)(key_end - key_start);
    if (key_len >= sizeof(client_key)) {
    AGENTOS_ERROR_HANDLE(AGENTOS_ERR_UNKNOWN, "Sec-WebSocket-Key too long");
        return AGENTOS_ERR_UNKNOWN;
    }
    memcpy(client_key, key_start, key_len);
    client_key[key_len] = '\0';

    char accept_key[64];
    if (notify_d_compute_ws_accept_key(client_key, accept_key, sizeof(accept_key)) != 0) {
    AGENTOS_ERROR_HANDLE(AGENTOS_ERR_UNKNOWN, "failed to compute WebSocket accept key");
        return AGENTOS_ERR_UNKNOWN;
    }

    char response[1024];
    int resp_len = snprintf(response, sizeof(response),
                            "HTTP/1.1 101 Switching Protocols\r\n"
                            "Upgrade: websocket\r\n"
                            "Connection: Upgrade\r\n"
                            "Sec-WebSocket-Accept: %s\r\n"
                            "\r\n",
                            accept_key);

    if (agentos_socket_send(client->fd, response, (size_t)resp_len) <= 0) {
    AGENTOS_ERROR_HANDLE(AGENTOS_ERR_UNKNOWN, "failed to send WebSocket 101 response");
        return AGENTOS_ERR_UNKNOWN;
    }

    client->type = NOTIFY_CLIENT_WEBSOCKET;
    client->handshake_done = 1;
    return 0;
}

static int notify_d_send_ws_frame(notify_client_t *client, const char *payload, size_t payload_len)
{
    if (!client || !payload || client->fd == AGENTOS_INVALID_SOCKET) {
    AGENTOS_ERROR_HANDLE(AGENTOS_ERR_INVALID_PARAM, "null parameter or invalid socket");
        return AGENTOS_ERR_INVALID_PARAM;
    }

    unsigned char frame[10];
    size_t header_len = 2;
    frame[0] = 0x81;

    if (payload_len <= 125) {
        frame[1] = (unsigned char)payload_len;
    } else if (payload_len <= 65535) {
        frame[1] = 126;
        frame[2] = (unsigned char)(payload_len >> 8);
        frame[3] = (unsigned char)(payload_len & 0xFF);
        header_len = 4;
    } else {
        frame[1] = 127;
        for (int i = 0; i < 8; i++)
            frame[2 + i] = (unsigned char)(payload_len >> (56 - 8 * i));
        header_len = 10;
    }

    if (agentos_socket_send(client->fd, (const char *)frame, header_len) <= 0) {
    AGENTOS_ERROR_HANDLE(AGENTOS_ERR_UNKNOWN, "failed to send WS frame header");
        return AGENTOS_ERR_UNKNOWN;
    }
    if (agentos_socket_send(client->fd, payload, payload_len) <= 0) {
    AGENTOS_ERROR_HANDLE(AGENTOS_ERR_UNKNOWN, "failed to send WS frame payload");
        return AGENTOS_ERR_UNKNOWN;
    }

    return 0;
}

static int notify_d_broadcast_event(notify_d_service_t *svc, const notify_event_t *event)
{
    if (!svc || !event) {
    AGENTOS_ERROR_HANDLE(AGENTOS_ERR_INVALID_PARAM, "null parameter");
        return AGENTOS_ERR_INVALID_PARAM;
    }

    char json_msg[8192];
    int msg_len =
        snprintf(json_msg, sizeof(json_msg),
                 "{"
                 "\"event\":\"%s\","
                 "\"channel\":\"%s\","
                 "\"message\":\"%s\","
                 "\"timestamp\":%llu"
                 "}",
                 event->event_type ? event->event_type : "message",
                 event->channel ? event->channel : "default", event->message ? event->message : "",
                 (unsigned long long)event->timestamp);

    size_t broadcast_count = 0;

    for (size_t i = 0; i < svc->client_count; i++) {
        notify_client_t *client = &svc->clients[i];
        if (!client->active)
            continue;

        int subscribed = !event->channel || !client->channel ||
                         strcmp(event->channel, "broadcast") == 0 ||
                         strcmp(client->channel, event->channel) == 0;

        if (!subscribed)
            continue;

        if (client->type == NOTIFY_CLIENT_WEBSOCKET && client->handshake_done) {
            notify_d_send_ws_frame(client, json_msg, (size_t)msg_len);
            client->messages_sent++;
            broadcast_count++;
        } else if (client->type == NOTIFY_CLIENT_SOCKET) {
            agentos_socket_send(client->fd, json_msg, (size_t)msg_len);
            client->messages_sent++;
            broadcast_count++;
        } else if (client->type == NOTIFY_CLIENT_SSE) {
            char sse_msg[8448];
            int sse_len = snprintf(sse_msg, sizeof(sse_msg), "event: %s\ndata: %s\n\n",
                                   event->event_type ? event->event_type : "message", json_msg);
            agentos_socket_send(client->fd, sse_msg, (size_t)sse_len);
            client->messages_sent++;
            broadcast_count++;
        }
    }

    return (int)broadcast_count;
}

#ifndef _WIN32
static void *notify_d_event_loop(void *arg)
{
#else
static DWORD WINAPI notify_d_event_loop(LPVOID arg)
{
#endif
    notify_d_service_t *svc = (notify_d_service_t *)arg;
    if (!svc) {
#ifndef _WIN32
        return NULL;
#else
        return 1;
#endif
    }

    while (svc->event_running) {
        agentos_mutex_lock(&svc->lock);

        if (svc->pending_count > 0) {
            notify_event_t *event = svc->pending[svc->pending_head];
            svc->pending_head = (svc->pending_head + 1) % NOTIFY_D_MAX_PENDING;
            svc->pending_count--;

            agentos_mutex_unlock(&svc->lock);

            notify_d_broadcast_event(svc, event);

            AGENTOS_FREE(event->message);
            AGENTOS_FREE(event->channel);
            AGENTOS_FREE(event->event_type);
            AGENTOS_FREE(event);
        } else {
            agentos_mutex_unlock(&svc->lock);
#ifndef _WIN32
            sleep(1);
#else
            Sleep(1000);
#endif
        }
    }

#ifndef _WIN32
    return NULL;
#else
    return 0;
#endif
}

static int notify_d_enqueue(notify_d_service_t *svc, const char *msg, const char *channel,
                            const char *event_type)
{
    if (!svc || !msg)
        return AGENTOS_ERR_INVALID_PARAM;
    AGENTOS_ERROR_HANDLE(AGENTOS_ERR_INVALID_PARAM, "null parameter");
    if (svc->pending_count >= NOTIFY_D_MAX_PENDING) {
    AGENTOS_ERROR_HANDLE(AGENTOS_ERR_UNKNOWN, "pending queue full");
        return AGENTOS_ERR_UNKNOWN;
    }

    notify_event_t *event = (notify_event_t *)AGENTOS_CALLOC(1, sizeof(notify_event_t));
    if (!event) {
    AGENTOS_ERROR_HANDLE(AGENTOS_ERR_OUT_OF_MEMORY, "calloc failed for notify_event_t");
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }

    event->message = AGENTOS_STRDUP(msg);
    event->channel = channel ? AGENTOS_STRDUP(channel) : AGENTOS_STRDUP("default");
    event->event_type = event_type ? AGENTOS_STRDUP(event_type) : AGENTOS_STRDUP("message");
    event->timestamp = (uint64_t)time(NULL);

    svc->pending[svc->pending_tail] = event;
    svc->pending_tail = (svc->pending_tail + 1) % NOTIFY_D_MAX_PENDING;
    svc->pending_count++;

    return 0;
}

static notify_client_t *notify_d_find_client_slot(notify_d_service_t *svc)
{
    for (size_t i = 0; i < NOTIFY_D_MAX_CLIENTS; i++) {
        if (!svc->clients[i].active)
            return &svc->clients[i];
    }
    if (svc->client_count < NOTIFY_D_MAX_CLIENTS)
        return &svc->clients[svc->client_count];
    return NULL;
}

static int notify_d_init(notify_d_service_t *svc, int port, const char *sock)
{
    if (!svc)
    AGENTOS_ERROR_HANDLE(AGENTOS_EINVAL, "svc is NULL");
        return AGENTOS_EINVAL;

    memset(svc, 0, sizeof(*svc));
    svc->tcp_port = port > 0 ? port : NOTIFY_D_DEFAULT_PORT;
    svc->socket_path = sock ? AGENTOS_STRDUP(sock) : AGENTOS_STRDUP(NOTIFY_D_DEFAULT_SOCKET);
    svc->start_time = (uint64_t)time(NULL);

    agentos_mutex_init(&svc->lock);
    agentos_socket_init();

    SVC_LOG_INFO("notify_d: init complete (max_clients=%d)", NOTIFY_D_MAX_CLIENTS);
    return AGENTOS_SUCCESS;
}

static int notify_d_start(notify_d_service_t *svc)
{
    if (!svc)
    AGENTOS_ERROR_HANDLE(AGENTOS_EINVAL, "svc is NULL");
        return AGENTOS_EINVAL;

#ifndef _WIN32
    svc->server_fd = agentos_socket_create_unix_server(svc->socket_path);
    if (svc->server_fd == AGENTOS_INVALID_SOCKET) {
        SVC_LOG_ERROR("notify_d: failed to create socket at %s", svc->socket_path);
    AGENTOS_ERROR_HANDLE(AGENTOS_ERR_UNKNOWN, "failed to create unix socket");
        return AGENTOS_ERR_UNKNOWN;
    }
#else
    svc->server_fd = agentos_socket_create_tcp_server("127.0.0.1", (uint16_t)svc->tcp_port);
    if (svc->server_fd == AGENTOS_INVALID_SOCKET) {
        SVC_LOG_ERROR("notify_d: failed to create TCP server");
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_UNKNOWN, "failed to create TCP server");
        return AGENTOS_ERR_UNKNOWN;
    }
#endif

    svc->running = 1;
    svc->event_running = 1;
    svc->force_stop = 0;

    agentos_thread_create(&svc->event_thread, notify_d_event_loop, svc);

    SVC_LOG_INFO("notify_d: service started (event_loop=active)");
    return AGENTOS_SUCCESS;
}

static int notify_d_stop(notify_d_service_t *svc, int force)
{
    if (!svc)
    AGENTOS_ERROR_HANDLE(AGENTOS_EINVAL, "svc is NULL");
        return AGENTOS_EINVAL;

    agentos_mutex_lock(&svc->lock);
    svc->running = 0;
    svc->event_running = 0;
    if (force)
        svc->force_stop = 1;
    agentos_mutex_unlock(&svc->lock);

    if (!force) {
        agentos_thread_join(svc->event_thread, NULL);
    }

    for (size_t i = 0; i < svc->client_count; i++) {
        if (svc->clients[i].active && svc->clients[i].fd != AGENTOS_INVALID_SOCKET) {
            if (force) {
                agentos_socket_close(svc->clients[i].fd);
                svc->clients[i].fd = AGENTOS_INVALID_SOCKET;
                svc->clients[i].active = 0;
            }
        }
    }

    if (force) {
        for (size_t i = 0; i < svc->pending_count; i++) {
            size_t idx = (svc->pending_head + i) % NOTIFY_D_MAX_PENDING;
            AGENTOS_FREE(svc->pending[idx]->message);
            AGENTOS_FREE(svc->pending[idx]->channel);
            AGENTOS_FREE(svc->pending[idx]->event_type);
            AGENTOS_FREE(svc->pending[idx]);
        }
        svc->pending_count = 0;
        svc->pending_head = 0;
        svc->pending_tail = 0;
    }

    if (svc->server_fd != AGENTOS_INVALID_SOCKET) {
        agentos_socket_close(svc->server_fd);
        svc->server_fd = AGENTOS_INVALID_SOCKET;
    }

    if (force) {
#ifndef _WIN32
        unlink(svc->socket_path);
#endif
    }

    SVC_LOG_INFO("notify_d: service stopped (force=%d, pending=%zu, clients=%zu)", force,
                 svc->pending_count, svc->client_count);
    return AGENTOS_SUCCESS;
}

static int notify_d_destroy(notify_d_service_t *svc)
{
    if (!svc)
    AGENTOS_ERROR_HANDLE(AGENTOS_EINVAL, "svc is NULL");
        return AGENTOS_EINVAL;

    notify_d_stop(svc, 1);

    for (size_t i = 0; i < svc->client_count; i++) {
        AGENTOS_FREE(svc->clients[i].channel);
    }
    agentos_socket_cleanup();
    agentos_mutex_destroy(&svc->lock);
    AGENTOS_FREE(svc->socket_path);
    memset(svc, 0, sizeof(*svc));
    SVC_LOG_INFO("notify_d: service destroyed");
    return AGENTOS_SUCCESS;
}

static int notify_d_healthcheck(notify_d_service_t *svc)
{
    if (!svc)
        return 0;

    agentos_mutex_lock(&svc->lock);
    int healthy = svc->running && svc->event_running ? 1 : 0;
    size_t active_clients = 0;
    for (size_t i = 0; i < svc->client_count; i++) {
        if (svc->clients[i].active)
            active_clients++;
    }
    (void)svc->pending_count;
    agentos_mutex_unlock(&svc->lock);

    if (svc->pending_count >= NOTIFY_D_MAX_PENDING)
        healthy = 0;
    if (svc->error_count > svc->notified_count / 2 && svc->notified_count > 10)
        healthy = 0;

    return healthy;
}

static void notify_d_handle_request(notify_d_service_t *svc, agentos_socket_t client_fd)
{
    char buffer[NOTIFY_D_MAX_BUFFER];
    ssize_t n = agentos_socket_recv(client_fd, buffer, sizeof(buffer) - 1);
    if (n <= 0) {
        agentos_socket_close(client_fd);
        return;
    }
    buffer[n] = '\0';

    agentos_mutex_lock(&svc->lock);

    int is_upgrade = (strstr(buffer, "Upgrade: websocket") != NULL ||
                      strstr(buffer, "Upgrade: WebSocket") != NULL);
    int is_sse = (strstr(buffer, "Accept: text/event-stream") != NULL);

    notify_client_t *client = notify_d_find_client_slot(svc);
    if (!client) {
        agentos_mutex_unlock(&svc->lock);
        const char *busy = "{\"error\":\"max_clients_reached\"}";
        agentos_socket_send(client_fd, busy, strlen(busy));
        agentos_socket_close(client_fd);
        return;
    }

    memset(client, 0, sizeof(*client));
    client->fd = client_fd;
    client->connected_at = (uint64_t)time(NULL);
    client->last_activity = client->connected_at;
    client->active = 1;

    if (is_sse) {
        client->type = NOTIFY_CLIENT_SSE;
        const char *sse_headers = "HTTP/1.1 200 OK\r\n"
                                  "Content-Type: text/event-stream\r\n"
                                  "Cache-Control: no-cache\r\n"
                                  "Connection: keep-alive\r\n"
                                  "\r\n";
        agentos_socket_send(client_fd, sse_headers, strlen(sse_headers));
        client->handshake_done = 1;
        svc->client_count++;
    } else if (is_upgrade) {
        client->type = NOTIFY_CLIENT_WEBSOCKET;
        if (notify_d_handle_ws_upgrade(svc, client, buffer) == 0) {
            svc->client_count++;
        } else {
            client->active = 0;
            agentos_mutex_unlock(&svc->lock);
            agentos_socket_close(client_fd);
            return;
        }
    } else {
        client->type = NOTIFY_CLIENT_SOCKET;

        const char *channel = "inbound";
        const char *channel_hdr = "X-Channel: ";
        const char *ch = strstr(buffer, channel_hdr);
        if (ch) {
            const char *che = strstr(ch + strlen(channel_hdr), "\r\n");
            if (che) {
                size_t clen = (size_t)(che - (ch + strlen(channel_hdr)));
                char *cn = (char *)AGENTOS_MALLOC(clen + 1);
                if (cn) {
                    memcpy(cn, ch + strlen(channel_hdr), clen);
                    cn[clen] = '\0';
                    channel = cn;
                }
            }
        }
        client->channel = AGENTOS_STRDUP(channel);
        if (strcmp(channel, "inbound") != 0)
            AGENTOS_FREE((void *)channel);
        svc->client_count++;

        int ret = notify_d_enqueue(svc, buffer, client->channel, NULL);
        if (ret == 0) {
            svc->notified_count++;
        } else {
            svc->error_count++;
        }
    }

    uint64_t uptime = (uint64_t)time(NULL) - svc->start_time;
    size_t active_clients = 0;
    for (size_t i = 0; i < svc->client_count; i++) {
        if (svc->clients[i].active)
            active_clients++;
    }

    size_t depth = svc->pending_count;
    agentos_mutex_unlock(&svc->lock);

    if (client->type == NOTIFY_CLIENT_SOCKET) {
        char response[4096];
        snprintf(response, sizeof(response),
                 "{"
                 "\"service\":\"notify_d\","
                 "\"status\":\"%s\","
                 "\"queued\":%llu,"
                 "\"pending\":%zu,"
                 "\"active_clients\":%zu,"
                 "\"uptime_sec\":%llu,"
                 "\"healthy\":%s"
                 "}",
                 svc->error_count > svc->notified_count / 2 ? "degraded" : "ok",
                 (unsigned long long)svc->notified_count, depth, active_clients,
                 (unsigned long long)uptime, notify_d_healthcheck(svc) ? "true" : "false");

        agentos_socket_send(client_fd, response, strlen(response));
        agentos_socket_close(client_fd);

        agentos_mutex_lock(&svc->lock);
        client->active = 0;
        client->fd = AGENTOS_INVALID_SOCKET;
        agentos_mutex_unlock(&svc->lock);
    }
}

int main(int argc __attribute__((unused)), char **argv __attribute__((unused)))
{

#ifndef _WIN32
    signal(SIGINT, notify_d_signal_handler);
    signal(SIGTERM, notify_d_signal_handler);
    signal(SIGPIPE, SIG_IGN);
#endif

    if (notify_d_init(&g_service, NOTIFY_D_DEFAULT_PORT, NOTIFY_D_DEFAULT_SOCKET) !=
        AGENTOS_SUCCESS)
        return 1;
    if (notify_d_start(&g_service) != AGENTOS_SUCCESS) {
        notify_d_destroy(&g_service);
        return 1;
    }

    while (!g_shutdown && g_service.running) {
        agentos_socket_t client = agentos_socket_accept(g_service.server_fd, 1000);
        if (client != AGENTOS_INVALID_SOCKET) {
            notify_d_handle_request(&g_service, client);
        }
    }

    notify_d_stop(&g_service, g_shutdown ? 1 : 0);
    notify_d_destroy(&g_service);
    return 0;
}
