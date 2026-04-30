/**
 * @file mcp_transport.c
 * @brief MCP Transport Layer Implementation
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * Implements STDIO and HTTP+SSE transport for MCP protocol.
 */

#include "mcp_transport.h"
#include "agentos.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <time.h>

struct mcp_transport {
    mcp_transport_type_t type;
    mcp_transport_state_t state;
    mcp_transport_message_fn on_message;
    mcp_transport_error_fn on_error;
    mcp_transport_state_fn on_state_change;
    void* user_data;
    uint32_t read_timeout_ms;
    uint32_t write_timeout_ms;
    size_t max_message_size;
    int input_fd;
    int output_fd;
    char* base_url;
    char* api_key;
    char* sse_endpoint;
    char* post_endpoint;
    uint32_t reconnect_interval_ms;
    uint32_t max_reconnect_attempts;
    int http_socket;
    char* recv_buffer;
    size_t recv_buffer_size;
    size_t recv_buffer_capacity;
    volatile int running;
};

static void set_state(mcp_transport_t* t, mcp_transport_state_t new_state) {
    if (!t) return;
    mcp_transport_state_t old = t->state;
    t->state = new_state;
    if (t->on_state_change && old != new_state) {
        t->on_state_change(new_state, t->user_data);
    }
}

static void notify_error(mcp_transport_t* t, int code, const char* msg) {
    if (t && t->on_error) {
        t->on_error(code, msg, t->user_data);
    }
}

static void notify_message(mcp_transport_t* t, const char* msg, size_t len) {
    if (t && t->on_message) {
        t->on_message(msg, len, t->user_data);
    }
}

static int read_line(int fd, char* buf, size_t buf_size, uint32_t timeout_ms) {
    size_t pos = 0;
    struct timeval tv;
    fd_set fds;

    while (pos < buf_size - 1) {
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;

        int ret = select(fd + 1, &fds, NULL, NULL, &tv);
        if (ret <= 0) {
            if (ret == 0) return -2;
            return -1;
        }

        char c;
        ssize_t n = read(fd, &c, 1);
        if (n <= 0) return -1;

        if (c == '\n') {
            buf[pos] = '\0';
            return (int)pos;
        }
        if (c != '\r') {
            buf[pos++] = c;
        }
    }
    buf[pos] = '\0';
    return (int)pos;
}

static int write_all(int fd, const char* buf, size_t len, uint32_t timeout_ms) {
    struct timeval tv;
    fd_set fds;
    size_t written = 0;

    while (written < len) {
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;

        int ret = select(fd + 1, NULL, &fds, NULL, &tv);
        if (ret <= 0) {
            if (ret == 0) return -2;
            return -1;
        }

        ssize_t n = write(fd, buf + written, len - written);
        if (n <= 0) return -1;
        written += (size_t)n;
    }
    return 0;
}

mcp_transport_config_t mcp_transport_config_stdio_default(void) {
    mcp_transport_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.type = MCP_TRANSPORT_STDIO;
    cfg.read_timeout_ms = 30000;
    cfg.write_timeout_ms = 30000;
    cfg.max_message_size = 10 * 1024 * 1024;
    cfg.config.stdio.input_fd = STDIN_FILENO;
    cfg.config.stdio.output_fd = STDOUT_FILENO;
    return cfg;
}

mcp_transport_config_t mcp_transport_config_http_default(const char* base_url) {
    mcp_transport_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.type = MCP_TRANSPORT_HTTP_SSE;
    cfg.read_timeout_ms = 30000;
    cfg.write_timeout_ms = 30000;
    cfg.max_message_size = 10 * 1024 * 1024;
    cfg.config.http.base_url = base_url;
    cfg.config.http.sse_endpoint = "/sse";
    cfg.config.http.post_endpoint = "/messages";
    cfg.config.http.reconnect_interval_ms = 5000;
    cfg.config.http.max_reconnect_attempts = 10;
    return cfg;
}

mcp_transport_t* mcp_transport_create(const mcp_transport_config_t* config) {
    if (!config) return NULL;

    mcp_transport_t* t = (mcp_transport_t*)calloc(1, sizeof(mcp_transport_t));
    if (!t) return NULL;

    t->type = config->type;
    t->state = MCP_TRANSPORT_DISCONNECTED;
    t->on_message = config->on_message;
    t->on_error = config->on_error;
    t->on_state_change = config->on_state_change;
    t->user_data = config->user_data;
    t->read_timeout_ms = config->read_timeout_ms > 0 ? config->read_timeout_ms : 30000;
    t->write_timeout_ms = config->write_timeout_ms > 0 ? config->write_timeout_ms : 30000;
    t->max_message_size = config->max_message_size > 0 ? config->max_message_size : (10 * 1024 * 1024);
    t->running = 0;

    t->recv_buffer_capacity = 65536;
    t->recv_buffer = (char*)malloc(t->recv_buffer_capacity);
    if (!t->recv_buffer) {
        free(t);
        return NULL;
    }
    t->recv_buffer_size = 0;

    if (t->type == MCP_TRANSPORT_STDIO) {
        t->input_fd = config->config.stdio.input_fd > 0 ?
            config->config.stdio.input_fd : STDIN_FILENO;
        t->output_fd = config->config.stdio.output_fd > 0 ?
            config->config.stdio.output_fd : STDOUT_FILENO;
    } else if (t->type == MCP_TRANSPORT_HTTP_SSE) {
        if (config->config.http.base_url) {
            t->base_url = strdup(config->config.http.base_url);
        }
        if (config->config.http.api_key) {
            t->api_key = strdup(config->config.http.api_key);
        }
        t->sse_endpoint = config->config.http.sse_endpoint ?
            strdup(config->config.http.sse_endpoint) : strdup("/sse");
        t->post_endpoint = config->config.http.post_endpoint ?
            strdup(config->config.http.post_endpoint) : strdup("/messages");
        t->reconnect_interval_ms = config->config.http.reconnect_interval_ms > 0 ?
            config->config.http.reconnect_interval_ms : 5000;
        t->max_reconnect_attempts = config->config.http.max_reconnect_attempts > 0 ?
            config->config.http.max_reconnect_attempts : 10;
        t->http_socket = -1;
    }

    return t;
}

void mcp_transport_destroy(mcp_transport_t* transport) {
    if (!transport) return;

    if (transport->running) {
        mcp_transport_stop(transport);
    }

    if (transport->http_socket >= 0) {
        close(transport->http_socket);
    }

    free(transport->base_url);
    free(transport->api_key);
    free(transport->sse_endpoint);
    free(transport->post_endpoint);
    free(transport->recv_buffer);
    free(transport);
}

int mcp_transport_start(mcp_transport_t* transport) {
    if (!transport) return -1;
    if (transport->state == MCP_TRANSPORT_CONNECTED) return 0;

    set_state(transport, MCP_TRANSPORT_CONNECTING);

    if (transport->type == MCP_TRANSPORT_STDIO) {
        int flags = fcntl(transport->input_fd, F_GETFL, 0);
        if (flags >= 0) {
            fcntl(transport->input_fd, F_SETFL, flags & ~O_NONBLOCK);
        }
        transport->running = 1;
        set_state(transport, MCP_TRANSPORT_CONNECTED);
        return 0;
    }

    if (transport->type == MCP_TRANSPORT_HTTP_SSE || transport->type == MCP_TRANSPORT_STREAMABLE_HTTP) {
        if (!transport->base_url) {
            notify_error(transport, -1, "HTTP transport requires base_url");
            set_state(transport, MCP_TRANSPORT_ERROR);
            return -1;
        }

        char host[256] = {0};
        int port = 80;
        char* url_copy = strdup(transport->base_url);
        if (!url_copy) {
            set_state(transport, MCP_TRANSPORT_ERROR);
            return -1;
        }

        char* host_start = url_copy;
        if (strncmp(url_copy, "https://", 8) == 0) {
            host_start = url_copy + 8;
            port = 443;
        } else if (strncmp(url_copy, "http://", 7) == 0) {
            host_start = url_copy + 7;
        }

        char* port_sep = strchr(host_start, ':');
        char* path_sep = strchr(host_start, '/');

        if (port_sep && (!path_sep || port_sep < path_sep)) {
            *port_sep = '\0';
            port = atoi(port_sep + 1);
        }
        if (path_sep) {
            *path_sep = '\0';
        }
        snprintf(host, sizeof(host), "%s", host_start);
        free(url_copy);

        struct addrinfo hints, *result;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        char port_str[16];
        snprintf(port_str, sizeof(port_str), "%d", port);

        int gai_err = getaddrinfo(host, port_str, &hints, &result);
        if (gai_err != 0) {
            char err_msg[256];
            snprintf(err_msg, sizeof(err_msg), "DNS resolution failed: %s", gai_strerror(gai_err));
            notify_error(transport, -2, err_msg);
            set_state(transport, MCP_TRANSPORT_ERROR);
            return -1;
        }

        int sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
        if (sock < 0) {
            notify_error(transport, -3, "Socket creation failed");
            freeaddrinfo(result);
            set_state(transport, MCP_TRANSPORT_ERROR);
            return -1;
        }

        struct timeval tv;
        tv.tv_sec = transport->read_timeout_ms / 1000;
        tv.tv_usec = (transport->read_timeout_ms % 1000) * 1000;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

        if (connect(sock, result->ai_addr, result->ai_addrlen) < 0) {
            char err_msg[256];
            snprintf(err_msg, sizeof(err_msg), "Connection failed: %s", strerror(errno));
            notify_error(transport, -4, err_msg);
            close(sock);
            freeaddrinfo(result);
            set_state(transport, MCP_TRANSPORT_ERROR);
            return -1;
        }

        freeaddrinfo(result);
        transport->http_socket = sock;
        transport->running = 1;
        set_state(transport, MCP_TRANSPORT_CONNECTED);
        return 0;
    }

    set_state(transport, MCP_TRANSPORT_ERROR);
    return -1;
}

int mcp_transport_stop(mcp_transport_t* transport) {
    if (!transport) return -1;

    transport->running = 0;

    if (transport->http_socket >= 0) {
        close(transport->http_socket);
        transport->http_socket = -1;
    }

    set_state(transport, MCP_TRANSPORT_DISCONNECTED);
    return 0;
}

int mcp_transport_send(mcp_transport_t* transport,
                       const char* message,
                       size_t length) {
    if (!transport || !message || length == 0) return -1;
    if (transport->state != MCP_TRANSPORT_CONNECTED) return -2;

    if (transport->type == MCP_TRANSPORT_STDIO) {
        char header[32];
        int header_len = snprintf(header, sizeof(header),
                                  "Content-Length: %zu\r\n\r\n", length);

        if (write_all(transport->output_fd, header, (size_t)header_len,
                      transport->write_timeout_ms) < 0) {
            notify_error(transport, -10, "Failed to write message header");
            set_state(transport, MCP_TRANSPORT_ERROR);
            return -3;
        }

        if (write_all(transport->output_fd, message, length,
                      transport->write_timeout_ms) < 0) {
            notify_error(transport, -11, "Failed to write message body");
            set_state(transport, MCP_TRANSPORT_ERROR);
            return -3;
        }

        return 0;
    }

    if (transport->type == MCP_TRANSPORT_HTTP_SSE ||
        transport->type == MCP_TRANSPORT_STREAMABLE_HTTP) {
        if (transport->http_socket < 0) return -3;

        char request[4096];
        const char* path = transport->post_endpoint ? transport->post_endpoint : "/messages";
        int req_len = snprintf(request, sizeof(request),
            "POST %s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: %zu\r\n"
            "%s%s%s"
            "\r\n",
            path,
            transport->base_url ? transport->base_url : "localhost",
            length,
            transport->api_key ? "Authorization: Bearer " : "",
            transport->api_key ? transport->api_key : "",
            transport->api_key ? "\r\n" : "");

        if (write_all(transport->http_socket, request, (size_t)req_len,
                      transport->write_timeout_ms) < 0) {
            notify_error(transport, -12, "Failed to send HTTP request header");
            return -3;
        }

        if (write_all(transport->http_socket, message, length,
                      transport->write_timeout_ms) < 0) {
            notify_error(transport, -13, "Failed to send HTTP request body");
            return -3;
        }

        return 0;
    }

    return -1;
}

int mcp_transport_receive(mcp_transport_t* transport,
                          char** out_message,
                          size_t* out_length,
                          uint32_t timeout_ms) {
    if (!transport || !out_message || !out_length) return -1;
    if (transport->state != MCP_TRANSPORT_CONNECTED) return -2;

    *out_message = NULL;
    *out_length = 0;

    if (transport->type == MCP_TRANSPORT_STDIO) {
        char header_buf[256];
        int hdr_result = read_line(transport->input_fd, header_buf,
                                    sizeof(header_buf), timeout_ms);
        if (hdr_result < 0) {
            if (hdr_result == -2) return -2;
            notify_error(transport, -20, "Failed to read message header");
            return -1;
        }

        size_t content_length = 0;
        if (strncmp(header_buf, "Content-Length: ", 15) == 0) {
            content_length = (size_t)atol(header_buf + 15);
        } else {
            return -1;
        }

        char empty_line[4];
        read_line(transport->input_fd, empty_line, sizeof(empty_line), timeout_ms);

        if (content_length == 0 || content_length > transport->max_message_size) {
            notify_error(transport, -21, "Invalid content length");
            return -1;
        }

        char* body = (char*)malloc(content_length + 1);
        if (!body) return -1;

        size_t total_read = 0;
        while (total_read < content_length) {
            struct timeval tv;
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(transport->input_fd, &fds);
            tv.tv_sec = timeout_ms / 1000;
            tv.tv_usec = (timeout_ms % 1000) * 1000;

            int ret = select(transport->input_fd + 1, &fds, NULL, NULL, &tv);
            if (ret <= 0) {
                free(body);
                return ret == 0 ? -2 : -1;
            }

            ssize_t n = read(transport->input_fd, body + total_read,
                             content_length - total_read);
            if (n <= 0) {
                free(body);
                return -1;
            }
            total_read += (size_t)n;
        }

        body[content_length] = '\0';
        *out_message = body;
        *out_length = content_length;
        return 0;
    }

    if (transport->type == MCP_TRANSPORT_HTTP_SSE ||
        transport->type == MCP_TRANSPORT_STREAMABLE_HTTP) {
        if (transport->http_socket < 0) return -3;

        char line_buf[8192];
        int line_result = read_line(transport->http_socket, line_buf,
                                     sizeof(line_buf), timeout_ms);
        if (line_result < 0) return -1;

        if (strncmp(line_buf, "data: ", 6) == 0) {
            const char* data = line_buf + 6;
            size_t data_len = strlen(data);
            char* msg = (char*)malloc(data_len + 1);
            if (!msg) return -1;
            memcpy(msg, data, data_len);
            msg[data_len] = '\0';
            *out_message = msg;
            *out_length = data_len;
            return 0;
        }

        if (strncmp(line_buf, "event: ", 7) == 0) {
            return mcp_transport_receive(transport, out_message, out_length, timeout_ms);
        }

        if (strlen(line_buf) == 0) {
            return mcp_transport_receive(transport, out_message, out_length, timeout_ms);
        }

        return -1;
    }

    return -1;
}

mcp_transport_state_t mcp_transport_get_state(const mcp_transport_t* transport) {
    if (!transport) return MCP_TRANSPORT_DISCONNECTED;
    return transport->state;
}

mcp_transport_type_t mcp_transport_get_type(const mcp_transport_t* transport) {
    if (!transport) return MCP_TRANSPORT_STDIO;
    return transport->type;
}

const char* mcp_transport_state_string(mcp_transport_state_t state) {
    switch (state) {
        case MCP_TRANSPORT_DISCONNECTED: return "disconnected";
        case MCP_TRANSPORT_CONNECTING:   return "connecting";
        case MCP_TRANSPORT_CONNECTED:    return "connected";
        case MCP_TRANSPORT_ERROR:        return "error";
        default:                         return "unknown";
    }
}

const char* mcp_transport_type_string(mcp_transport_type_t type) {
    switch (type) {
        case MCP_TRANSPORT_STDIO:           return "stdio";
        case MCP_TRANSPORT_HTTP_SSE:        return "http+sse";
        case MCP_TRANSPORT_STREAMABLE_HTTP: return "streamable-http";
        default:                            return "unknown";
    }
}
