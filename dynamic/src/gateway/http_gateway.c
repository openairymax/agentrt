/**
 * @file http_gateway.c
 * @brief HTTP 网关实现（JSON-RPC 2.0）
 * 
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "http_gateway.h"
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

/* 缓冲区大小 */
#define HTTP_READ_BUFFER_SIZE   8192
#define HTTP_MAX_HEADER_SIZE    16384
#define HTTP_MAX_BODY_SIZE      1048576

/* HTTP 响应状态 */
typedef enum {
    HTTP_STATUS_OK = 200,
    HTTP_STATUS_BAD_REQUEST = 400,
    HTTP_STATUS_NOT_FOUND = 404,
    HTTP_STATUS_METHOD_NOT_ALLOWED = 405,
    HTTP_STATUS_PAYLOAD_TOO_LARGE = 413,
    HTTP_STATUS_INTERNAL_ERROR = 500,
    HTTP_STATUS_SERVICE_UNAVAILABLE = 503
} http_status_t;

/* HTTP 请求结构 */
typedef struct {
    char    method[16];
    char    path[256];
    char*   body;
    size_t  body_len;
    char    content_type[64];
} http_request_t;

/* HTTP 网关实现结构 */
typedef struct http_gateway_impl {
    char                host[64];
    uint16_t            port;
    dynamic_server_t*   server;
    
    int                 listen_fd;
    pthread_t           accept_thread;
    atomic_bool         running;
    
    atomic_uint_fast64_t requests_total;
    atomic_uint_fast64_t requests_failed;
    atomic_uint_fast64_t bytes_received;
    atomic_uint_fast64_t bytes_sent;
} http_gateway_impl_t;

/* ========== HTTP 辅助函数 ========== */

/**
 * @brief 解析 HTTP 请求行和头部
 */
static int parse_http_request(const char* data, size_t len, http_request_t* req) {
    memset(req, 0, sizeof(http_request_t));
    
    /* 解析请求行 */
    const char* line_end = strstr(data, "\r\n");
    if (!line_end) return -1;
    
    /* 解析方法 */
    const char* space = strchr(data, ' ');
    if (!space || (size_t)(space - data) >= sizeof(req->method)) return -1;
    
    memcpy(req->method, data, space - data);
    req->method[space - data] = '\0';
    
    /* 解析路径 */
    const char* path_start = space + 1;
    space = strchr(path_start, ' ');
    if (!space || (size_t)(space - path_start) >= sizeof(req->path)) return -1;
    
    memcpy(req->path, path_start, space - path_start);
    req->path[space - path_start] = '\0';
    
    /* 查找 Content-Length 和 Content-Type */
    const char* header_start = line_end + 2;
    const char* body_start = strstr(header_start, "\r\n\r\n");
    if (!body_start) {
        req->body = NULL;
        req->body_len = 0;
        return 0;
    }
    
    body_start += 4;
    
    /* 解析 Content-Length */
    const char* cl_header = strcasestr(header_start, "Content-Length:");
    if (cl_header) {
        req->body_len = strtoul(cl_header + 15, NULL, 10);
    }
    
    /* 解析 Content-Type */
    const char* ct_header = strcasestr(header_start, "Content-Type:");
    if (ct_header) {
        ct_header += 13;
        while (*ct_header == ' ') ct_header++;
        const char* ct_end = strstr(ct_header, "\r\n");
        if (ct_end && (size_t)(ct_end - ct_header) < sizeof(req->content_type)) {
            memcpy(req->content_type, ct_header, ct_end - ct_header);
            req->content_type[ct_end - ct_header] = '\0';
        }
    }
    
    /* 复制 body */
    if (req->body_len > 0 && req->body_len < HTTP_MAX_BODY_SIZE) {
        req->body = (char*)malloc(req->body_len + 1);
        if (req->body) {
            memcpy(req->body, body_start, req->body_len);
            req->body[req->body_len] = '\0';
        }
    }
    
    return 0;
}

/**
 * @brief 构建 HTTP 响应
 */
static char* build_http_response(http_status_t status, const char* content_type,
    const char* body, size_t body_len, size_t* out_len) {
    
    const char* status_text = 
        (status == HTTP_STATUS_OK) ? "OK" :
        (status == HTTP_STATUS_BAD_REQUEST) ? "Bad Request" :
        (status == HTTP_STATUS_NOT_FOUND) ? "Not Found" :
        (status == HTTP_STATUS_METHOD_NOT_ALLOWED) ? "Method Not Allowed" :
        (status == HTTP_STATUS_PAYLOAD_TOO_LARGE) ? "Payload Too Large" :
        (status == HTTP_STATUS_INTERNAL_ERROR) ? "Internal Server Error" :
        "Service Unavailable";
    
    char* response = NULL;
    int header_len = 0;
    
    if (body && body_len > 0) {
        header_len = asprintf(&response,
            "HTTP/1.1 %d %s\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %zu\r\n"
            "Connection: close\r\n"
            "Server: AgentOS-Dynamic/1.1\r\n"
            "\r\n",
            status, status_text, content_type, body_len);
    } else {
        header_len = asprintf(&response,
            "HTTP/1.1 %d %s\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 0\r\n"
            "Connection: close\r\n"
            "Server: AgentOS-Dynamic/1.1\r\n"
            "\r\n",
            status, status_text);
    }
    
    if (header_len < 0 || !response) {
        return NULL;
    }
    
    /* 追加 body */
    if (body && body_len > 0) {
        char* new_response = (char*)realloc(response, header_len + body_len);
        if (!new_response) {
            free(response);
            return NULL;
        }
        response = new_response;
        memcpy(response + header_len, body, body_len);
        *out_len = header_len + body_len;
    } else {
        *out_len = header_len;
    }
    
    return response;
}

/* ========== JSON-RPC 处理 ========== */

/**
 * @brief 处理 JSON-RPC 请求
 */
static char* handle_jsonrpc(http_gateway_impl_t* gw, const char* body, size_t* out_len) {
    /* 简化的 JSON-RPC 解析（生产环境应使用 cJSON） */
    
    /* 检查是否为有效的 JSON */
    if (!body || body[0] != '{') {
        const char* error = "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32700,\"message\":\"Parse error\"},\"id\":null}";
        return build_http_response(HTTP_STATUS_BAD_REQUEST, "application/json",
            error, strlen(error), out_len);
    }
    
    /* 提取 method（简化实现） */
    const char* method_start = strstr(body, "\"method\"");
    if (!method_start) {
        const char* error = "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32600,\"message\":\"Invalid Request\"},\"id\":null}";
        return build_http_response(HTTP_STATUS_BAD_REQUEST, "application/json",
            error, strlen(error), out_len);
    }
    
    method_start = strchr(method_start + 8, ':');
    if (!method_start) {
        const char* error = "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32600,\"message\":\"Invalid Request\"},\"id\":null}";
        return build_http_response(HTTP_STATUS_BAD_REQUEST, "application/json",
            error, strlen(error), out_len);
    }
    
    method_start = strchr(method_start + 1, '"');
    if (!method_start) {
        const char* error = "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32600,\"message\":\"Invalid Request\"},\"id\":null}";
        return build_http_response(HTTP_STATUS_BAD_REQUEST, "application/json",
            error, strlen(error), out_len);
    }
    
    const char* method_end = strchr(method_start + 1, '"');
    if (!method_end) {
        const char* error = "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32600,\"message\":\"Invalid Request\"},\"id\":null}";
        return build_http_response(HTTP_STATUS_BAD_REQUEST, "application/json",
            error, strlen(error), out_len);
    }
    
    char method[64] = {0};
    size_t method_len = method_end - method_start - 1;
    if (method_len >= sizeof(method)) method_len = sizeof(method) - 1;
    memcpy(method, method_start + 1, method_len);
    
    /* 路由到对应处理器 */
    char* result = NULL;
    
    if (strcmp(method, "server.status") == 0) {
        agentos_error_t err = dynamic_server_get_stats(gw->server, &result);
        if (err != AGENTOS_SUCCESS) {
            const char* error = "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32603,\"message\":\"Internal error\"},\"id\":null}";
            return build_http_response(HTTP_STATUS_INTERNAL_ERROR, "application/json",
                error, strlen(error), out_len);
        }
    } else if (strcmp(method, "server.health") == 0) {
        agentos_error_t err = dynamic_server_get_health(gw->server, &result);
        if (err != AGENTOS_SUCCESS) {
            const char* error = "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32603,\"message\":\"Internal error\"},\"id\":null}";
            return build_http_response(HTTP_STATUS_INTERNAL_ERROR, "application/json",
                error, strlen(error), out_len);
        }
    } else if (strcmp(method, "server.metrics") == 0) {
        agentos_error_t err = dynamic_server_get_metrics(gw->server, &result);
        if (err != AGENTOS_SUCCESS) {
            const char* error = "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32603,\"message\":\"Internal error\"},\"id\":null}";
            return build_http_response(HTTP_STATUS_INTERNAL_ERROR, "application/json",
                error, strlen(error), out_len);
        }
    } else if (strcmp(method, "session.create") == 0) {
        char* session_id = NULL;
        agentos_error_t err = session_manager_create_session(
            gw->server->session_mgr, NULL, &session_id);
        if (err == AGENTOS_SUCCESS) {
            asprintf(&result, "{\"jsonrpc\":\"2.0\",\"result\":{\"session_id\":\"%s\"},\"id\":1}", session_id);
            free(session_id);
        } else {
            const char* error = "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32603,\"message\":\"Failed to create session\"},\"id\":null}";
            return build_http_response(HTTP_STATUS_INTERNAL_ERROR, "application/json",
                error, strlen(error), out_len);
        }
    } else {
        /* 方法不存在 */
        char* error_json = NULL;
        asprintf(&error_json, 
            "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32601,\"message\":\"Method not found: %s\"},\"id\":null}",
            method);
        char* resp = build_http_response(HTTP_STATUS_OK, "application/json",
            error_json, strlen(error_json), out_len);
        free(error_json);
        return resp;
    }
    
    /* 构建成功响应 */
    char* response = NULL;
    if (result) {
        char* wrapped = NULL;
        asprintf(&wrapped, "{\"jsonrpc\":\"2.0\",\"result\":%s,\"id\":1}", result);
        response = build_http_response(HTTP_STATUS_OK, "application/json",
            wrapped, strlen(wrapped), out_len);
        free(wrapped);
        free(result);
    } else {
        const char* success = "{\"jsonrpc\":\"2.0\",\"result\":null,\"id\":1}";
        response = build_http_response(HTTP_STATUS_OK, "application/json",
            success, strlen(success), out_len);
    }
    
    return response;
}

/* ========== 连接处理 ========== */

/**
 * @brief 处理单个连接
 */
static void handle_connection(http_gateway_impl_t* gw, int client_fd) {
    char buffer[HTTP_READ_BUFFER_SIZE];
    ssize_t total_read = 0;
    ssize_t n;
    
    /* 读取请求 */
    while ((n = read(client_fd, buffer + total_read, 
            sizeof(buffer) - total_read - 1)) > 0) {
        total_read += n;
        buffer[total_read] = '\0';
        
        /* 检查是否收到完整头部 */
        if (strstr(buffer, "\r\n\r\n")) {
            break;
        }
        
        if ((size_t)total_read >= HTTP_MAX_HEADER_SIZE) {
            /* 头部过大 */
            const char* error = "HTTP/1.1 413 Payload Too Large\r\n\r\n";
            write(client_fd, error, strlen(error));
            close(client_fd);
            return;
        }
    }
    
    if (total_read <= 0) {
        close(client_fd);
        return;
    }
    
    atomic_fetch_add(&gw->bytes_received, total_read);
    atomic_fetch_add(&gw->requests_total, 1);
    
    /* 解析请求 */
    http_request_t req;
    if (parse_http_request(buffer, total_read, &req) != 0) {
        const char* error = "HTTP/1.1 400 Bad Request\r\n\r\n";
        write(client_fd, error, strlen(error));
        close(client_fd);
        atomic_fetch_add(&gw->requests_failed, 1);
        return;
    }
    
    /* 路由请求 */
    char* response = NULL;
    size_t response_len = 0;
    
    if (strcmp(req.method, "GET") == 0) {
        /* GET 请求处理 */
        if (strcmp(req.path, "/health") == 0 || strcmp(req.path, "/healthz") == 0) {
            char* health_json = NULL;
            dynamic_server_get_health(gw->server, &health_json);
            response = build_http_response(HTTP_STATUS_OK, "application/json",
                health_json, health_json ? strlen(health_json) : 0, &response_len);
            free(health_json);
        } else if (strcmp(req.path, "/metrics") == 0) {
            char* metrics = NULL;
            dynamic_server_get_metrics(gw->server, &metrics);
            response = build_http_response(HTTP_STATUS_OK, "text/plain; version=0.0.4",
                metrics, metrics ? strlen(metrics) : 0, &response_len);
            free(metrics);
        } else {
            const char* not_found = "HTTP/1.1 404 Not Found\r\n\r\n";
            response = strdup(not_found);
            response_len = strlen(not_found);
        }
    } else if (strcmp(req.method, "POST") == 0) {
        /* POST 请求处理（JSON-RPC） */
        if (strcmp(req.path, "/rpc") == 0 || strcmp(req.path, "/") == 0) {
            if (req.body && strstr(req.content_type, "application/json")) {
                response = handle_jsonrpc(gw, req.body, &response_len);
            } else {
                const char* error = "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32700,\"message\":\"Parse error\"},\"id\":null}";
                response = build_http_response(HTTP_STATUS_BAD_REQUEST, "application/json",
                    error, strlen(error), &response_len);
            }
        } else {
            const char* not_found = "HTTP/1.1 404 Not Found\r\n\r\n";
            response = strdup(not_found);
            response_len = strlen(not_found);
        }
    } else {
        const char* not_allowed = "HTTP/1.1 405 Method Not Allowed\r\nAllow: GET, POST\r\n\r\n";
        response = strdup(not_allowed);
        response_len = strlen(not_allowed);
    }
    
    /* 发送响应 */
    if (response) {
        write(client_fd, response, response_len);
        atomic_fetch_add(&gw->bytes_sent, response_len);
        free(response);
    } else {
        atomic_fetch_add(&gw->requests_failed, 1);
    }
    
    /* 清理 */
    free(req.body);
    close(client_fd);
}

/**
 * @brief 接受连接的线程函数
 */
static void* accept_thread_func(void* arg) {
    http_gateway_impl_t* gw = (http_gateway_impl_t*)arg;
    
    AGENTOS_LOG_INFO("HTTP gateway listening on %s:%u", gw->host, gw->port);
    
    while (atomic_load(&gw->running)) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_fd = accept(gw->listen_fd, 
            (struct sockaddr*)&client_addr, &client_len);
        
        if (client_fd < 0) {
            if (errno == EINTR || !atomic_load(&gw->running)) {
                break;
            }
            AGENTOS_LOG_ERROR("accept() failed: %s", strerror(errno));
            continue;
        }
        
        /* 设置非阻塞 */
        int flags = fcntl(client_fd, F_GETFL, 0);
        fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
        
        /* 设置超时 */
        struct timeval tv = {.tv_sec = 30, .tv_usec = 0};
        setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        
        handle_connection(gw, client_fd);
    }
    
    AGENTOS_LOG_INFO("HTTP gateway stopped");
    return NULL;
}

/* ========== 网关操作实现 ========== */

static agentos_error_t http_start(void* impl) {
    http_gateway_impl_t* gw = (http_gateway_impl_t*)impl;
    if (!gw) return AGENTOS_EINVAL;
    
    /* 创建 socket */
    gw->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (gw->listen_fd < 0) {
        AGENTOS_LOG_ERROR("socket() failed: %s", strerror(errno));
        return AGENTOS_ERROR;
    }
    
    /* 设置 SO_REUSEADDR */
    int opt = 1;
    setsockopt(gw->listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    /* 绑定地址 */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(gw->port);
    inet_pton(AF_INET, gw->host, &addr.sin_addr);
    
    if (bind(gw->listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        AGENTOS_LOG_ERROR("bind() failed: %s", strerror(errno));
        close(gw->listen_fd);
        gw->listen_fd = -1;
        return AGENTOS_ERROR;
    }
    
    /* 监听 */
    if (listen(gw->listen_fd, 128) < 0) {
        AGENTOS_LOG_ERROR("listen() failed: %s", strerror(errno));
        close(gw->listen_fd);
        gw->listen_fd = -1;
        return AGENTOS_ERROR;
    }
    
    /* 启动接受线程 */
    atomic_store(&gw->running, true);
    
    if (pthread_create(&gw->accept_thread, NULL, accept_thread_func, gw) != 0) {
        AGENTOS_LOG_ERROR("pthread_create() failed");
        close(gw->listen_fd);
        gw->listen_fd = -1;
        return AGENTOS_ERROR;
    }
    
    return AGENTOS_SUCCESS;
}

static void http_stop(void* impl) {
    http_gateway_impl_t* gw = (http_gateway_impl_t*)impl;
    if (!gw) return;
    
    atomic_store(&gw->running, false);
    
    if (gw->listen_fd >= 0) {
        shutdown(gw->listen_fd, SHUT_RDWR);
        close(gw->listen_fd);
        gw->listen_fd = -1;
    }
    
    pthread_join(gw->accept_thread, NULL);
}

static void http_destroy(void* impl) {
    http_gateway_impl_t* gw = (http_gateway_impl_t*)impl;
    if (!gw) return;
    
    http_stop(impl);
    free(gw);
}

static const char* http_get_name(void* impl) {
    (void)impl;
    return "http";
}

static agentos_error_t http_get_stats(void* impl, char** out_json) {
    http_gateway_impl_t* gw = (http_gateway_impl_t*)impl;
    if (!gw || !out_json) return AGENTOS_EINVAL;
    
    char* json = NULL;
    int len = asprintf(&json,
        "{\"name\":\"http\",\"host\":\"%s\",\"port\":%u,"
        "\"requests_total\":%llu,\"requests_failed\":%llu,"
        "\"bytes_received\":%llu,\"bytes_sent\":%llu}",
        gw->host, gw->port,
        (unsigned long long)atomic_load(&gw->requests_total),
        (unsigned long long)atomic_load(&gw->requests_failed),
        (unsigned long long)atomic_load(&gw->bytes_received),
        (unsigned long long)atomic_load(&gw->bytes_sent));
    
    if (len < 0 || !json) return AGENTOS_ENOMEM;
    
    *out_json = json;
    return AGENTOS_SUCCESS;
}

static const gateway_ops_t http_ops = {
    .start = http_start,
    .stop = http_stop,
    .destroy = http_destroy,
    .get_name = http_get_name,
    .get_stats = http_get_stats
};

/* ========== 公共 API ========== */

gateway_t* http_gateway_create(
    const char* host,
    uint16_t port,
    dynamic_server_t* server) {
    
    if (!host || !server) return NULL;
    
    /* 分配实现 */
    http_gateway_impl_t* impl = (http_gateway_impl_t*)calloc(1, sizeof(http_gateway_impl_t));
    if (!impl) return NULL;
    
    strncpy(impl->host, host, sizeof(impl->host) - 1);
    impl->port = port;
    impl->server = server;
    impl->listen_fd = -1;
    
    atomic_init(&impl->requests_total, 0);
    atomic_init(&impl->requests_failed, 0);
    atomic_init(&impl->bytes_received, 0);
    atomic_init(&impl->bytes_sent, 0);
    
    /* 分配网关包装 */
    gateway_t* gateway = (gateway_t*)malloc(sizeof(gateway_t));
    if (!gateway) {
        free(impl);
        return NULL;
    }
    
    gateway->ops = &http_ops;
    gateway->server = server;
    gateway->impl = impl;
    
    return gateway;
}
