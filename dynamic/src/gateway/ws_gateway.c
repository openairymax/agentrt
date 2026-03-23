/**
 * @file ws_gateway.c
 * @brief WebSocket 网关实现
 * 
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "ws_gateway.h"
#include "../server.h"
#include "../logger.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/* SHA-1 用于 WebSocket 握手 */
#include <openssl/sha.h>

/* WebSocket 帧操作码 */
#define WS_OPCODE_CONTINUATION  0x0
#define WS_OPCODE_TEXT          0x1
#define WS_OPCODE_BINARY        0x2
#define WS_OPCODE_CLOSE         0x8
#define WS_OPCODE_PING          0x9
#define WS_OPCODE_PONG          0xA

/* WebSocket 魔法字符串 */
#define WS_MAGIC "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

/* 缓冲区大小 */
#define WS_BUFFER_SIZE      65536
#define WS_MAX_MESSAGE_SIZE 1048576

/* 连接状态 */
typedef enum {
    WS_STATE_HANDSHAKE = 0,
    WS_STATE_OPEN,
    WS_STATE_CLOSING,
    WS_STATE_CLOSED
} ws_state_t;

/* WebSocket 连接结构 */
typedef struct ws_connection {
    int                 fd;
    ws_state_t          state;
    char*               session_id;
    uint8_t*            recv_buf;
    size_t              recv_len;
    size_t              recv_cap;
    struct ws_connection* next;
} ws_connection_t;

/* WebSocket 网关实现结构 */
typedef struct ws_gateway_impl {
    char                host[64];
    uint16_t            port;
    dynamic_server_t*   server;
    
    int                 listen_fd;
    pthread_t           accept_thread;
    pthread_t           event_thread;
    atomic_bool         running;
    
    pthread_mutex_t     connections_lock;
    ws_connection_t*    connections;
    size_t              connection_count;
    
    int                 pipe_fds[2];  /* 用于唤醒事件线程 */
    
    atomic_uint_fast64_t messages_total;
    atomic_uint_fast64_t messages_failed;
    atomic_uint_fast64_t bytes_received;
    atomic_uint_fast64_t bytes_sent;
} ws_gateway_impl_t;

/* ========== Base64 编码（用于握手响应） ========== */

static const char base64_table[] = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void base64_encode(const uint8_t* data, size_t len, char* out) {
    size_t i, j;
    for (i = 0, j = 0; i < len; i += 3, j += 4) {
        uint32_t octet_a = i < len ? data[i] : 0;
        uint32_t octet_b = i + 1 < len ? data[i + 1] : 0;
        uint32_t octet_c = i + 2 < len ? data[i + 2] : 0;
        
        uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;
        
        out[j] = base64_table[(triple >> 18) & 0x3F];
        out[j + 1] = base64_table[(triple >> 12) & 0x3F];
        out[j + 2] = (i + 1 < len) ? base64_table[(triple >> 6) & 0x3F] : '=';
        out[j + 3] = (i + 2 < len) ? base64_table[triple & 0x3F] : '=';
    }
    out[j] = '\0';
}

/* ========== WebSocket 握手 ========== */

static int perform_handshake(ws_connection_t* conn) {
    char buffer[4096];
    ssize_t n = recv(conn->fd, buffer, sizeof(buffer) - 1, 0);
    if (n <= 0) return -1;
    
    buffer[n] = '\0';
    
    /* 解析 Sec-WebSocket-Key */
    const char* key_header = strcasestr(buffer, "Sec-WebSocket-Key:");
    if (!key_header) return -1;
    
    key_header += 18;
    while (*key_header == ' ') key_header++;
    
    const char* key_end = strstr(key_header, "\r\n");
    if (!key_end) return -1;
    
    char client_key[64];
    size_t key_len = key_end - key_header;
    if (key_len >= sizeof(client_key)) return -1;
    
    memcpy(client_key, key_header, key_len);
    client_key[key_len] = '\0';
    
    /* 计算 accept key */
    char accept_input[128];
    snprintf(accept_input, sizeof(accept_input), "%s%s", client_key, WS_MAGIC);
    
    unsigned char sha1_hash[SHA_DIGEST_LENGTH];
    SHA1((unsigned char*)accept_input, strlen(accept_input), sha1_hash);
    
    char accept_key[64];
    base64_encode(sha1_hash, SHA_DIGEST_LENGTH, accept_key);
    
    /* 发送握手响应 */
    char response[512];
    int len = snprintf(response, sizeof(response),
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n"
        "Server: AgentOS-Dynamic/1.1\r\n"
        "\r\n",
        accept_key);
    
    if (send(conn->fd, response, len, 0) != len) {
        return -1;
    }
    
    conn->state = WS_STATE_OPEN;
    return 0;
}

/* ========== WebSocket 帧处理 ========== */

/**
 * @brief 发送 WebSocket 帧
 */
static int ws_send_frame(ws_connection_t* conn, uint8_t opcode, 
    const uint8_t* payload, size_t len) {
    
    uint8_t header[10];
    size_t header_len = 2;
    
    header[0] = 0x80 | opcode;  /* FIN + opcode */
    
    if (len <= 125) {
        header[1] = (uint8_t)len;
    } else if (len <= 65535) {
        header[1] = 126;
        header[2] = (len >> 8) & 0xFF;
        header[3] = len & 0xFF;
        header_len = 4;
    } else {
        header[1] = 127;
        for (int i = 0; i < 8; i++) {
            header[2 + i] = (len >> (56 - i * 8)) & 0xFF;
        }
        header_len = 10;
    }
    
    /* 发送头部 */
    if (send(conn->fd, header, header_len, 0) != (ssize_t)header_len) {
        return -1;
    }
    
    /* 发送负载 */
    if (len > 0 && payload) {
        if (send(conn->fd, payload, len, 0) != (ssize_t)len) {
            return -1;
        }
    }
    
    return 0;
}

/**
 * @brief 处理 WebSocket 消息
 */
static void handle_message(ws_gateway_impl_t* gw, ws_connection_t* conn,
    const uint8_t* data, size_t len) {
    
    atomic_fetch_add(&gw->messages_total, 1);
    atomic_fetch_add(&gw->bytes_received, len);
    
    /* 简化的 JSON-RPC 处理 */
    const char* msg = (const char*)data;
    
    /* 检查是否为 JSON */
    if (len > 0 && msg[0] == '{') {
        /* 提取 method */
        const char* method_start = strstr(msg, "\"method\"");
        if (method_start) {
            method_start = strchr(method_start + 8, ':');
            if (method_start) {
                method_start = strchr(method_start + 1, '"');
                if (method_start) {
                    const char* method_end = strchr(method_start + 1, '"');
                    if (method_end) {
                        char method[64] = {0};
                        size_t method_len = method_end - method_start - 1;
                        if (method_len < sizeof(method)) {
                            memcpy(method, method_start + 1, method_len);
                            
                            /* 路由 */
                            char* result = NULL;
                            
                            if (strcmp(method, "server.status") == 0) {
                                dynamic_server_get_stats(gw->server, &result);
                            } else if (strcmp(method, "server.health") == 0) {
                                dynamic_server_get_health(gw->server, &result);
                            } else if (strcmp(method, "session.create") == 0) {
                                char* session_id = NULL;
                                if (session_manager_create_session(
                                        gw->server->session_mgr, NULL, &session_id) 
                                        == AGENTOS_SUCCESS) {
                                    asprintf(&result, 
                                        "{\"session_id\":\"%s\"}", session_id);
                                    conn->session_id = session_id;
                                }
                            } else if (strcmp(method, "echo") == 0) {
                                /* 回显测试 */
                                result = strdup(msg);
                            }
                            
                            if (result) {
                                char* response = NULL;
                                asprintf(&response, 
                                    "{\"jsonrpc\":\"2.0\",\"result\":%s,\"id\":1}",
                                    result);
                                
                                ws_send_frame(conn, WS_OPCODE_TEXT,
                                    (uint8_t*)response, strlen(response));
                                
                                atomic_fetch_add(&gw->bytes_sent, strlen(response));
                                free(response);
                                free(result);
                            } else {
                                const char* error = 
                                    "{\"jsonrpc\":\"2.0\",\"error\":"
                                    "{\"code\":-32601,\"message\":\"Method not found\"},"
                                    "\"id\":null}";
                                ws_send_frame(conn, WS_OPCODE_TEXT,
                                    (uint8_t*)error, strlen(error));
                            }
                        }
                    }
                }
            }
        }
    }
}

/**
 * @brief 处理 WebSocket 帧
 */
static int handle_frame(ws_gateway_impl_t* gw, ws_connection_t* conn) {
    uint8_t* data = conn->recv_buf;
    size_t len = conn->recv_len;
    
    if (len < 2) return 0;  /* 需要更多数据 */
    
    uint8_t opcode = data[0] & 0x0F;
    bool masked = (data[1] & 0x80) != 0;
    uint64_t payload_len = data[1] & 0x7F;
    
    size_t header_len = 2;
    
    if (payload_len == 126) {
        if (len < 4) return 0;
        payload_len = (data[2] << 8) | data[3];
        header_len = 4;
    } else if (payload_len == 127) {
        if (len < 10) return 0;
        payload_len = 0;
        for (int i = 0; i < 8; i++) {
            payload_len = (payload_len << 8) | data[2 + i];
        }
        header_len = 10;
    }
    
    uint8_t mask[4] = {0};
    if (masked) {
        if (len < header_len + 4) return 0;
        memcpy(mask, data + header_len, 4);
        header_len += 4;
    }
    
    if (len < header_len + payload_len) return 0;  /* 需要更多数据 */
    
    /* 解码负载 */
    uint8_t* payload = data + header_len;
    if (masked) {
        for (uint64_t i = 0; i < payload_len; i++) {
            payload[i] ^= mask[i % 4];
        }
    }
    
    /* 处理操作码 */
    switch (opcode) {
        case WS_OPCODE_TEXT:
        case WS_OPCODE_BINARY:
            if (payload_len <= WS_MAX_MESSAGE_SIZE) {
                handle_message(gw, conn, payload, payload_len);
            }
            break;
            
        case WS_OPCODE_PING:
            ws_send_frame(conn, WS_OPCODE_PONG, payload, payload_len);
            break;
            
        case WS_OPCODE_CLOSE:
            conn->state = WS_STATE_CLOSING;
            ws_send_frame(conn, WS_OPCODE_CLOSE, NULL, 0);
            return -1;
            
        default:
            break;
    }
    
    /* 移除已处理的数据 */
    size_t consumed = header_len + payload_len;
    if (consumed < len) {
        memmove(data, data + consumed, len - consumed);
        conn->recv_len = len - consumed;
    } else {
        conn->recv_len = 0;
    }
    
    return 0;
}

/* ========== 连接管理 ========== */

static void close_connection(ws_gateway_impl_t* gw, ws_connection_t* conn) {
    if (conn->fd >= 0) {
        close(conn->fd);
        conn->fd = -1;
    }
    
    if (conn->session_id) {
        session_manager_close_session(gw->server->session_mgr, conn->session_id);
        free(conn->session_id);
        conn->session_id = NULL;
    }
    
    free(conn->recv_buf);
    conn->state = WS_STATE_CLOSED;
}

static void add_connection(ws_gateway_impl_t* gw, ws_connection_t* conn) {
    pthread_mutex_lock(&gw->connections_lock);
    conn->next = gw->connections;
    gw->connections = conn;
    gw->connection_count++;
    pthread_mutex_unlock(&gw->connections_lock);
}

static void remove_connection(ws_gateway_impl_t* gw, ws_connection_t* conn) {
    pthread_mutex_lock(&gw->connections_lock);
    
    ws_connection_t** pp = &gw->connections;
    while (*pp && *pp != conn) {
        pp = &(*pp)->next;
    }
    
    if (*pp == conn) {
        *pp = conn->next;
        gw->connection_count--;
    }
    
    pthread_mutex_unlock(&gw->connections_lock);
}

/* ========== 事件线程 ========== */

static void* event_thread_func(void* arg) {
    ws_gateway_impl_t* gw = (ws_gateway_impl_t*)arg;
    
    while (atomic_load(&gw->running)) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        
        int max_fd = gw->pipe_fds[0];
        FD_SET(gw->pipe_fds[0], &read_fds);
        
        /* 添加所有连接 */
        pthread_mutex_lock(&gw->connections_lock);
        for (ws_connection_t* c = gw->connections; c; c = c->next) {
            if (c->fd >= 0 && c->state == WS_STATE_OPEN) {
                FD_SET(c->fd, &read_fds);
                if (c->fd > max_fd) max_fd = c->fd;
            }
        }
        pthread_mutex_unlock(&gw->connections_lock);
        
        struct timeval tv = {.tv_sec = 1, .tv_usec = 0};
        int ret = select(max_fd + 1, &read_fds, NULL, NULL, &tv);
        
        if (ret < 0) {
            if (errno == EINTR) continue;
            break;
        }
        
        if (ret == 0) continue;
        
        /* 检查管道（用于唤醒） */
        if (FD_ISSET(gw->pipe_fds[0], &read_fds)) {
            char buf[64];
            read(gw->pipe_fds[0], buf, sizeof(buf));
        }
        
        /* 处理连接数据 */
        ws_connection_t* to_close[256];
        size_t close_count = 0;
        
        pthread_mutex_lock(&gw->connections_lock);
        for (ws_connection_t* c = gw->connections; c; c = c->next) {
            if (c->fd >= 0 && FD_ISSET(c->fd, &read_fds)) {
                /* 接收数据 */
                ssize_t n = recv(c->fd, 
                    c->recv_buf + c->recv_len, 
                    c->recv_cap - c->recv_len, 0);
                
                if (n <= 0) {
                    to_close[close_count++] = c;
                    continue;
                }
                
                c->recv_len += n;
                
                /* 处理帧 */
                while (handle_frame(gw, c) == 0 && c->recv_len > 0) {
                    if (c->recv_len >= c->recv_cap) break;
                }
                
                /* 检查缓冲区溢出 */
                if (c->recv_len >= c->recv_cap) {
                    to_close[close_count++] = c;
                }
            }
        }
        pthread_mutex_unlock(&gw->connections_lock);
        
        /* 关闭需要关闭的连接 */
        for (size_t i = 0; i < close_count; i++) {
            remove_connection(gw, to_close[i]);
            close_connection(gw, to_close[i]);
            free(to_close[i]);
        }
    }
    
    return NULL;
}

/* ========== 接受线程 ========== */

static void* accept_thread_func(void* arg) {
    ws_gateway_impl_t* gw = (ws_gateway_impl_t*)arg;
    
    AGENTOS_LOG_INFO("WebSocket gateway listening on %s:%u", gw->host, gw->port);
    
    while (atomic_load(&gw->running)) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_fd = accept(gw->listen_fd, 
            (struct sockaddr*)&client_addr, &client_len);
        
        if (client_fd < 0) {
            if (errno == EINTR || !atomic_load(&gw->running)) {
                break;
            }
            continue;
        }
        
        /* 创建连接 */
        ws_connection_t* conn = (ws_connection_t*)calloc(1, sizeof(ws_connection_t));
        if (!conn) {
            close(client_fd);
            continue;
        }
        
        conn->fd = client_fd;
        conn->state = WS_STATE_HANDSHAKE;
        conn->recv_cap = WS_BUFFER_SIZE;
        conn->recv_buf = (uint8_t*)malloc(conn->recv_cap);
        
        if (!conn->recv_buf) {
            free(conn);
            close(client_fd);
            continue;
        }
        
        /* 执行握手 */
        if (perform_handshake(conn) != 0) {
            free(conn->recv_buf);
            free(conn);
            close(client_fd);
            continue;
        }
        
        /* 设置非阻塞 */
        int flags = fcntl(client_fd, F_GETFL, 0);
        fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
        
        /* 添加到连接列表 */
        add_connection(gw, conn);
        
        /* 唤醒事件线程 */
        char c = 1;
        write(gw->pipe_fds[1], &c, 1);
        
        AGENTOS_LOG_DEBUG("WebSocket connection accepted");
    }
    
    AGENTOS_LOG_INFO("WebSocket gateway stopped");
    return NULL;
}

/* ========== 网关操作实现 ========== */

static agentos_error_t ws_start(void* impl) {
    ws_gateway_impl_t* gw = (ws_gateway_impl_t*)impl;
    if (!gw) return AGENTOS_EINVAL;
    
    /* 创建管道 */
    if (pipe(gw->pipe_fds) != 0) {
        return AGENTOS_ERROR;
    }
    
    /* 创建 socket */
    gw->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (gw->listen_fd < 0) {
        close(gw->pipe_fds[0]);
        close(gw->pipe_fds[1]);
        return AGENTOS_ERROR;
    }
    
    int opt = 1;
    setsockopt(gw->listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(gw->port);
    inet_pton(AF_INET, gw->host, &addr.sin_addr);
    
    if (bind(gw->listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(gw->listen_fd);
        close(gw->pipe_fds[0]);
        close(gw->pipe_fds[1]);
        return AGENTOS_ERROR;
    }
    
    if (listen(gw->listen_fd, 128) < 0) {
        close(gw->listen_fd);
        close(gw->pipe_fds[0]);
        close(gw->pipe_fds[1]);
        return AGENTOS_ERROR;
    }
    
    atomic_store(&gw->running, true);
    
    /* 启动事件线程 */
    if (pthread_create(&gw->event_thread, NULL, event_thread_func, gw) != 0) {
        close(gw->listen_fd);
        close(gw->pipe_fds[0]);
        close(gw->pipe_fds[1]);
        return AGENTOS_ERROR;
    }
    
    /* 启动接受线程 */
    if (pthread_create(&gw->accept_thread, NULL, accept_thread_func, gw) != 0) {
        atomic_store(&gw->running, false);
        char c = 1;
        write(gw->pipe_fds[1], &c, 1);
        pthread_join(gw->event_thread, NULL);
        close(gw->listen_fd);
        close(gw->pipe_fds[0]);
        close(gw->pipe_fds[1]);
        return AGENTOS_ERROR;
    }
    
    return AGENTOS_SUCCESS;
}

static void ws_stop(void* impl) {
    ws_gateway_impl_t* gw = (ws_gateway_impl_t*)impl;
    if (!gw) return;
    
    atomic_store(&gw->running, false);
    
    /* 唤醒事件线程 */
    char c = 1;
    write(gw->pipe_fds[1], &c, 1);
    
    if (gw->listen_fd >= 0) {
        shutdown(gw->listen_fd, SHUT_RDWR);
        close(gw->listen_fd);
        gw->listen_fd = -1;
    }
    
    pthread_join(gw->accept_thread, NULL);
    pthread_join(gw->event_thread, NULL);
    
    /* 关闭所有连接 */
    pthread_mutex_lock(&gw->connections_lock);
    ws_connection_t* c = gw->connections;
    while (c) {
        ws_connection_t* next = c->next;
        close_connection(gw, c);
        free(c);
        c = next;
    }
    gw->connections = NULL;
    gw->connection_count = 0;
    pthread_mutex_unlock(&gw->connections_lock);
    
    close(gw->pipe_fds[0]);
    close(gw->pipe_fds[1]);
}

static void ws_destroy(void* impl) {
    ws_gateway_impl_t* gw = (ws_gateway_impl_t*)impl;
    if (!gw) return;
    
    ws_stop(impl);
    pthread_mutex_destroy(&gw->connections_lock);
    free(gw);
}

static const char* ws_get_name(void* impl) {
    (void)impl;
    return "websocket";
}

static agentos_error_t ws_get_stats(void* impl, char** out_json) {
    ws_gateway_impl_t* gw = (ws_gateway_impl_t*)impl;
    if (!gw || !out_json) return AGENTOS_EINVAL;
    
    char* json = NULL;
    int len = asprintf(&json,
        "{\"name\":\"websocket\",\"host\":\"%s\",\"port\":%u,"
        "\"connections\":%zu,"
        "\"messages_total\":%llu,\"messages_failed\":%llu,"
        "\"bytes_received\":%llu,\"bytes_sent\":%llu}",
        gw->host, gw->port,
        gw->connection_count,
        (unsigned long long)atomic_load(&gw->messages_total),
        (unsigned long long)atomic_load(&gw->messages_failed),
        (unsigned long long)atomic_load(&gw->bytes_received),
        (unsigned long long)atomic_load(&gw->bytes_sent));
    
    if (len < 0 || !json) return AGENTOS_ENOMEM;
    
    *out_json = json;
    return AGENTOS_SUCCESS;
}

static const gateway_ops_t ws_ops = {
    .start = ws_start,
    .stop = ws_stop,
    .destroy = ws_destroy,
    .get_name = ws_get_name,
    .get_stats = ws_get_stats
};

/* ========== 公共 API ========== */

gateway_t* ws_gateway_create(
    const char* host,
    uint16_t port,
    dynamic_server_t* server) {
    
    if (!host || !server) return NULL;
    
    ws_gateway_impl_t* impl = (ws_gateway_impl_t*)calloc(1, sizeof(ws_gateway_impl_t));
    if (!impl) return NULL;
    
    strncpy(impl->host, host, sizeof(impl->host) - 1);
    impl->port = port;
    impl->server = server;
    impl->listen_fd = -1;
    impl->pipe_fds[0] = -1;
    impl->pipe_fds[1] = -1;
    
    pthread_mutex_init(&impl->connections_lock, NULL);
    
    atomic_init(&impl->messages_total, 0);
    atomic_init(&impl->messages_failed, 0);
    atomic_init(&impl->bytes_received, 0);
    atomic_init(&impl->bytes_sent, 0);
    
    gateway_t* gateway = (gateway_t*)malloc(sizeof(gateway_t));
    if (!gateway) {
        pthread_mutex_destroy(&impl->connections_lock);
        free(impl);
        return NULL;
    }
    
    gateway->ops = &ws_ops;
    gateway->server = server;
    gateway->impl = impl;
    
    return gateway;
}
