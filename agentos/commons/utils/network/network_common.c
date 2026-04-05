/*
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 * 
 * @file network_common.c
 * @brief 网络通信模块实现 - 跨平台网络抽象层
 * 
 * @details
 * 本文件实现了 network_common.h 中声明的所有网络功能。
 * 遵循 ARCHITECTURAL_PRINCIPLES.md 的设计原则：
 * - E-4 跨平台一致性：支持 Windows/Linux/macOS
 * - E-5 命名语义化：所有函数名精确表达用途
 * - E-6 错误可追溯：统一的错误码体系
 * 
 * @author AgentOS Team
 * @date 2026-04-03
 * @version 1.0.0.7
 */

#include "network_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
#endif

/* ============================================================================
 * 内部数据结构定义
 * ============================================================================ */

/**
 * @brief 网络连接内部结构
 */
struct network_connection {
    network_config_t config;           /**< 连接配置 */
    network_status_t status;           /**< 连接状态 */
    char error_msg[256];               /**< 错误消息 */
    network_event_callback_t event_cb; /**< 事件回调 */
    void* event_user_data;             /**< 回调用户数据 */
    network_stats_t stats;             /**< 统计信息 */
#ifdef _WIN32
    SOCKET sock;                       /**< Windows Socket 句柄 */
#else
    int fd;                            /**< Unix 文件描述符 */
#endif
    struct sockaddr_in addr;           /**< 目标地址 */
};

/**
 * @brief 连接池内部结构
 */
struct network_pool {
    network_config_t base_config;      /**< 基础配置 */
    size_t max_size;                   /**< 最大连接数 */
    size_t current_size;               /**< 当前连接数 */
    struct network_connection** connections; /**< 连接数组 */
};

/* ============================================================================
 * 内部工具函数
 * ============================================================================ */

/**
 * @brief 初始化 Winsock（仅 Windows）
 * @return 成功返回 0
 */
static int network_init_winsock(void) {
#ifdef _WIN32
    static int initialized = 0;
    if (!initialized) {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            return -1;
        }
        initialized = 1;
    }
#endif
    return 0;
}

/**
 * @brief 设置 Socket 为非阻塞模式
 * @param handle 平台特定句柄
 * @return 成功返回 0
 */
static int set_nonblocking_mode(void* handle) {
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket((SOCKET)(uintptr_t)handle, FIONBIO, &mode);
#else
    int flags = fcntl((int)(intptr_t)handle, F_GETFL, 0);
    fcntl((int)(intptr_t)handle, F_SETFL, flags | O_NONBLOCK);
#endif
    return 0;
}

/**
 * @brief 设置 Socket 发送/接收超时
 * @param handle 平台特定句柄
 * @param timeout_ms 超时时间（毫秒）
 * @param is_recv 是否为接收超时
 */
static void set_socket_timeout(void* handle, int timeout_ms, int is_recv) {
#ifdef _WIN32
    DWORD ms = (DWORD)timeout_ms;
    if (is_recv) {
        setsockopt((SOCKET)(uintptr_t)handle, SOL_SOCKET, SO_RCVTIMEO, (const char*)&ms, sizeof(ms));
    } else {
        setsockopt((SOCKET)(uintptr_t)handle, SOL_SOCKET, SO_SNDTIMEO, (const char*)&ms, sizeof(ms));
    }
#else
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    if (is_recv) {
        setsockopt((int)(intptr_t)handle, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    } else {
        setsockopt((int)(intptr_t)handle, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    }
#endif
}

/**
 * @brief 解析 URL 为各组成部分
 * @param url URL 字符串
 * @param host 输出主机名
 * @param port 输出端口
 * @param path 输出路径
 * @return 成功返回 0
 */
static int parse_url(const char* url, char* host, int* port, char* path) {
    if (!url || !host || !port || !path) return -1;

    const char* start = url;

    /* 跳过协议前缀 */
    if (strncmp(url, "http://", 7) == 0) {
        start += 7;
        *port = 80;
    } else if (strncmp(url, "https://", 8) == 0) {
        start += 8;
        *port = 443;
    } else {
        start = url;
        *port = 80;
    }

    /* 提取主机和路径 */
    const char* slash = strchr(start, '/');
    if (slash) {
        size_t host_len = slash - start;
        if (host_len >= 256) host_len = 255;
        strncpy(host, start, host_len);
        host[host_len] = '\0';

        size_t path_len = strlen(slash);
        if (path_len >= 512) path_len = 511;
        strncpy(path, slash, path_len);
        path[path_len] = '\0';
    } else {
        strcpy(host, start);
        strcpy(path, "/");
    }

    /* 检查端口（主机:端口 格式） */
    char* colon = strchr(host, ':');
    if (colon) {
        *port = atoi(colon + 1);
        *colon = '\0';
    }

    return 0;
}

/**
 * @brief 将 network_af_t 转换为系统地址族
 * @param af AgentOS 地址族枚举
 * @return 系统地址族值
 */
static int af_to_native(network_af_t af) {
    switch (af) {
        case NETWORK_AF_INET:  return AF_INET;
        case NETWORK_AF_INET6: return AF_INET6;
        default:              return AF_INET;
    }
}

/**
 * @brief 将 network_sock_type_t 转换为系统 Socket 类型
 * @param st AgentOS Socket 类型枚举
 * @return 系统 Socket 类型值
 */
static int socktype_to_native(network_sock_type_t st) {
    switch (st) {
        case NETWORK_SOCK_STREAM: return SOCK_STREAM;
        case NETWORK_SOCK_DGRAM:  return SOCK_DGRAM;
        case NETWORK_SOCK_RAW:    return SOCK_RAW;
        default:                  return SOCK_STREAM;
    }
}

/* ============================================================================
 * 基础连接 API 实现
 * ============================================================================ */

/**
 * @brief 创建默认网络配置
 * @return 默认配置结构体
 */
network_config_t network_create_default_config(void) {
    network_config_t config = {0};

    config.host = "127.0.0.1";
    config.port = 8080;
    config.timeout_ms = 30000;
    config.read_timeout_ms = 10000;
    config.write_timeout_ms = 10000;
    config.max_retries = 3;
    config.retry_interval_ms = 1000;
    config.sock_type = NETWORK_SOCK_STREAM;
    config.af = NETWORK_AF_INET;
    config.keepalive = false;
    config.nonblocking = false;
    config.ssl_enable = false;
    config.ssl_verify = NETWORK_SSL_VERIFY_PEER;
    config.ssl_cert_path = NULL;
    config.ssl_key_path = NULL;
    config.ssl_ca_path = NULL;

    return config;
}

/**
 * @brief 创建网络连接
 * @param config 网络配置
 * @return 连接句柄，失败返回 NULL
 */
network_connection_t* network_connection_create(const network_config_t* config) {
    if (!config) {
        return NULL;
    }

    network_connection_t* conn = (network_connection_t*)calloc(1, sizeof(network_connection_t));
    if (!conn) {
        return NULL;
    }

    conn->config = *config;
    conn->status = NETWORK_STATUS_DISCONNECTED;
    memset(conn->error_msg, 0, sizeof(conn->error_msg));
    conn->event_cb = NULL;
    conn->event_user_data = NULL;
    memset(&conn->stats, 0, sizeof(network_stats_t));

#ifdef _WIN32
    conn->sock = INVALID_SOCKET;
#else
    conn->fd = -1;
#endif

    memset(&conn->addr, 0, sizeof(conn->addr));

    return conn;
}

/**
 * @brief 销毁网络连接
 * @param connection 连接句柄
 */
void network_connection_destroy(network_connection_t* connection) {
    if (!connection) {
        return;
    }

    if (connection->status == NETWORK_STATUS_CONNECTED ||
        connection->status == NETWORK_STATUS_CONNECTING) {
        network_disconnect(connection);
    }

    free(connection);
}

/**
 * @brief 建立网络连接
 * @param connection 连接句柄
 * @return 错误码
 */
agentos_error_t network_connect(network_connection_t* connection) {
    if (!connection) {
        return AGENTOS_EINVAL;
    }

    network_init_winsock();

    if (connection->status != NETWORK_STATUS_DISCONNECTED &&
        connection->status != NETWORK_STATUS_ERROR) {
        snprintf(connection->error_msg, sizeof(connection->error_msg),
                 "Connection already in progress or connected");
        return AGENTOS_EBUSY;
    }

    connection->status = NETWORK_STATUS_CONNECTING;

    /* 创建系统 Socket */
    int native_af = af_to_native(connection->config.af);
    int native_type = socktype_to_native(connection->config.sock_type);

#ifdef _WIN32
    SOCKET s = socket(native_af, native_type, 0);
    if (s == INVALID_SOCKET) {
        snprintf(connection->error_msg, sizeof(connection->error_msg),
                 "socket() failed: %d", WSAGetLastError());
        connection->status = NETWORK_STATUS_ERROR;
        return AGENTOS_EIO;
    }
    connection->sock = s;
#else
    int fd = socket(native_af, native_type, 0);
    if (fd < 0) {
        snprintf(connection->error_msg, sizeof(connection->error_msg),
                 "socket() failed: %s", strerror(errno));
        connection->status = NETWORK_STATUS_ERROR;
        return AGENTOS_EIO;
    }
    connection->fd = fd;
#endif

    /* 设置目标地址 */
    memset(&connection->addr, 0, sizeof(connection->addr));
    connection->addr.sin_family = AF_INET;
    connection->addr.sin_port = htons((uint16_t)connection->config.port);

    /* 尝试直接解析为 IP，否则使用 DNS */
    if (inet_pton(AF_INET, connection->config.host, &connection->addr.sin_addr) <= 0) {
        struct addrinfo hints, *result;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = native_af;
        hints.ai_socktype = native_type;

        int gai_ret = getaddrinfo(connection->config.host, NULL, &hints, &result);
        if (gai_ret != 0) {
            snprintf(connection->error_msg, sizeof(connection->error_msg),
                     "DNS resolution failed for %s: %s",
                     connection->config.host, gai_strerror(gai_ret));
            network_disconnect(connection);
            connection->status = NETWORK_STATUS_ERROR;
            return AGENTOS_EIO;
        }

        struct sockaddr_in* addr_in = (struct sockaddr_in*)result->ai_addr;
        connection->addr.sin_addr = addr_in->sin_addr;
        freeaddrinfo(result);
    }

    /* 设置超时 */
#ifdef _WIN32
    void* handle = (void*)(uintptr_t)connection->sock;
#else
    void* handle = (void*)(intptr_t)connection->fd;
#endif
    set_socket_timeout(handle, connection->config.timeout_ms, 1);  /* 接收超时 */
    set_socket_timeout(handle, connection->config.timeout_ms, 0);  /* 发送超时 */

    /* 设置非阻塞模式（如果需要） */
    if (connection->config.nonblocking) {
        set_nonblocking_mode(handle);
    }

    /* 建立连接 */
#ifdef _WIN32
    if (connect(connection->sock, (struct sockaddr*)&connection->addr,
                sizeof(connection->addr)) == SOCKET_ERROR) {
        int err = WSAGetLastError();
        snprintf(connection->error_msg, sizeof(connection->error_msg),
                 "connect() failed: %d", err);
        network_disconnect(connection);
        connection->status = NETWORK_STATUS_ERROR;
        return AGENTOS_EIO;
    }
#else
    if (connect(connection->fd, (struct sockaddr*)&connection->addr,
                sizeof(connection->addr)) < 0) {
        snprintf(connection->error_msg, sizeof(connection->error_msg),
                 "connect() failed: %s", strerror(errno));
        network_disconnect(connection);
        connection->status = NETWORK_STATUS_ERROR;
        return AGENTOS_EIO;
    }
#endif

    connection->status = NETWORK_STATUS_CONNECTED;
    connection->stats.connect_count++;

    /* 触发连接成功事件 */
    if (connection->event_cb) {
        connection->event_cb(connection, NETWORK_EVENT_CONNECTED, NULL, 0,
                             connection->event_user_data);
    }

    return AGENTOS_SUCCESS;
}

/**
 * @brief 断开网络连接
 * @param connection 连接句柄
 * @return 错误码
 */
agentos_error_t network_disconnect(network_connection_t* connection) {
    if (!connection) {
        return AGENTOS_EINVAL;
    }

    if (connection->status != NETWORK_STATUS_CONNECTED &&
        connection->status != NETWORK_STATUS_CONNECTING &&
        connection->status != NETWORK_STATUS_ERROR) {
        return AGENTOS_SUCCESS;
    }

    connection->status = NETWORK_STATUS_DISCONNECTING;

    /* 触发断开事件 */
    if (connection->event_cb) {
        connection->event_cb(connection, NETWORK_EVENT_DISCONNECTED, NULL, 0,
                             connection->event_user_data);
    }

#ifdef _WIN32
    if (connection->sock != INVALID_SOCKET) {
        closesocket(connection->sock);
        connection->sock = INVALID_SOCKET;
    }
#else
    if (connection->fd >= 0) {
        close(connection->fd);
        connection->fd = -1;
    }
#endif

    connection->status = NETWORK_STATUS_DISCONNECTED;

    return AGENTOS_SUCCESS;
}

/**
 * @brief 发送数据
 * @param connection 连接句柄
 * @param data 数据缓冲区
 * @param length 数据长度
 * @param sent [out] 实际发送的字节数
 * @return 错误码
 */
agentos_error_t network_send(
    network_connection_t* connection,
    const void* data,
    size_t length,
    size_t* sent
) {
    if (!connection || !data || length == 0) {
        return AGENTOS_EINVAL;
    }

    if (connection->status != NETWORK_STATUS_CONNECTED) {
        snprintf(connection->error_msg, sizeof(connection->error_msg),
                 "Not connected");
        return AGENTOS_ENOTCONN;
    }

#ifdef _WIN32
    int result = send(connection->sock, (const char*)data, (int)length, 0);
    if (result == SOCKET_ERROR) {
        int err = WSAGetLastError();
        snprintf(connection->error_msg, sizeof(connection->error_msg),
                 "send() failed: %d", err);
        connection->stats.error_count++;
        return AGENTOS_EIO;
    }
#else
    ssize_t result = write(connection->fd, data, length);
    if (result < 0) {
        snprintf(connection->error_msg, sizeof(connection->error_msg),
                 "write() failed: %s", strerror(errno));
        connection->stats.error_count++;
        return AGENTOS_EIO;
    }
#endif

    if (sent) {
        *sent = (size_t)result;
    }

    connection->stats.bytes_sent += result;
    connection->stats.packets_sent++;

    /* 触发数据发送事件 */
    if (connection->event_cb && result > 0) {
        connection->event_cb(connection, NETWORK_EVENT_DATA_SENT, data, result,
                             connection->event_user_data);
    }

    return AGENTOS_SUCCESS;
}

/**
 * @brief 接收数据
 * @param connection 连接句柄
 * @param buffer 接收缓冲区
 * @param length 缓冲区长度
 * @param received [out] 实际接收的字节数
 * @return 错误码
 */
agentos_error_t network_receive(
    network_connection_t* connection,
    void* buffer,
    size_t length,
    size_t* received
) {
    if (!connection || !buffer || length == 0) {
        return AGENTOS_EINVAL;
    }

    if (connection->status != NETWORK_STATUS_CONNECTED) {
        return AGENTOS_ENOTCONN;
    }

#ifdef _WIN32
    int result = recv(connection->sock, (char*)buffer, (int)length, 0);
    if (result == SOCKET_ERROR) {
        int err = WSAGetLastError();
        snprintf(connection->error_msg, sizeof(connection->error_msg),
                 "recv() failed: %d", err);
        connection->stats.error_count++;
        return AGENTOS_EIO;
    }
    if (result == 0) {
        snprintf(connection->error_msg, sizeof(connection->error_msg), "Connection closed");
        connection->status = NETWORK_STATUS_ERROR;
        return AGENTOS_ECONNRESET;
    }
#else
    ssize_t result = read(connection->fd, buffer, length);
    if (result < 0) {
        snprintf(connection->error_msg, sizeof(connection->error_msg),
                 "read() failed: %s", strerror(errno));
        connection->stats.error_count++;
        return AGENTOS_EIO;
    }
    if (result == 0) {
        snprintf(connection->error_msg, sizeof(connection->error_msg), "Connection closed");
        connection->status = NETWORK_STATUS_ERROR;
        return AGENTOS_ECONNRESET;
    }
#endif

    if (received) {
        *received = (size_t)result;
    }

    connection->stats.bytes_received += result;
    connection->stats.packets_received++;

    /* 触发数据接收事件 */
    if (connection->event_cb && result > 0) {
        connection->event_cb(connection, NETWORK_EVENT_DATA_RECEIVED, buffer, result,
                             connection->event_user_data);
    }

    return AGENTOS_SUCCESS;
}

/**
 * @brief 发送全部数据（循环发送直到完成）
 * @param connection 连接句柄
 * @param data 数据缓冲区
 * @param length 数据长度
 * @return 错误码
 */
agentos_error_t network_send_all(
    network_connection_t* connection,
    const void* data,
    size_t length
) {
    if (!connection || !data || length == 0) {
        return AGENTOS_EINVAL;
    }

    const uint8_t* ptr = (const uint8_t*)data;
    size_t remaining = length;
    int retries = 0;
    int max_retries = connection->config.max_retries > 0 ?
                      connection->config.max_retries : NETWORK_DEFAULT_MAX_RETRIES;

    while (remaining > 0) {
        size_t sent = 0;
        agentos_error_t err = network_send(connection, ptr, remaining, &sent);
        if (err != AGENTOS_SUCCESS) {
            retries++;
            if (retries >= max_retries) {
                connection->stats.retry_count++;
                return err;
            }
            continue;
        }

        ptr += sent;
        remaining -= sent;
        retries = 0;
    }

    return AGENTOS_SUCCESS;
}

/**
 * @brief 接收指定长度的数据
 * @param connection 连接句柄
 * @param buffer 接收缓冲区
 * @param length 期望接收的长度
 * @return 错误码
 */
agentos_error_t network_receive_exact(
    network_connection_t* connection,
    void* buffer,
    size_t length
) {
    if (!connection || !buffer || length == 0) {
        return AGENTOS_EINVAL;
    }

    uint8_t* ptr = (uint8_t*)buffer;
    size_t remaining = length;
    int retries = 0;
    int max_retries = connection->config.max_retries > 0 ?
                      connection->config.max_retries : NETWORK_DEFAULT_MAX_RETRIES;

    while (remaining > 0) {
        size_t received = 0;
        agentos_error_t err = network_receive(connection, ptr, remaining, &received);
        if (err != AGENTOS_SUCCESS) {
            retries++;
            if (retries >= max_retries) {
                connection->stats.retry_count++;
                return err;
            }
            continue;
        }

        if (received == 0) {
            break;
        }

        ptr += received;
        remaining -= received;
        retries = 0;
    }

    if (remaining > 0) {
        return AGENTOS_ETIMEDOUT;
    }

    return AGENTOS_SUCCESS;
}

/**
 * @brief 获取连接状态
 * @param connection 连接句柄
 * @return 连接状态
 */
network_status_t network_get_status(const network_connection_t* connection) {
    if (!connection) {
        return NETWORK_STATUS_ERROR;
    }
    return connection->status;
}

/**
 * @brief 设置连接超时
 * @param connection 连接句柄
 * @param timeout_ms 超时时间（毫秒）
 * @return 错误码
 */
agentos_error_t network_set_timeout(
    network_connection_t* connection,
    int timeout_ms
) {
    if (!connection) {
        return AGENTOS_EINVAL;
    }

    connection->config.timeout_ms = timeout_ms;

    if (connection->status == NETWORK_STATUS_CONNECTED) {
#ifdef _WIN32
        void* handle = (void*)(uintptr_t)connection->sock;
#else
        void* handle = (void*)(intptr_t)connection->fd;
#endif
        set_socket_timeout(handle, timeout_ms, 0);  /* 发送超时 */
        set_socket_timeout(handle, timeout_ms, 1);  /* 接收超时 */
    }

    return AGENTOS_SUCCESS;
}

/**
 * @brief 设置读写超时
 * @param connection 连接句柄
 * @param read_timeout_ms 读取超时
 * @param write_timeout_ms 写入超时
 * @return 错误码
 */
agentos_error_t network_set_rw_timeout(
    network_connection_t* connection,
    int read_timeout_ms,
    int write_timeout_ms
) {
    if (!connection) {
        return AGENTOS_EINVAL;
    }

    connection->config.read_timeout_ms = read_timeout_ms;
    connection->config.write_timeout_ms = write_timeout_ms;

    if (connection->status == NETWORK_STATUS_CONNECTED) {
#ifdef _WIN32
        void* handle = (void*)(uintptr_t)connection->sock;
#else
        void* handle = (void*)(intptr_t)connection->fd;
#endif
        set_socket_timeout(handle, read_timeout_ms, 1);
        set_socket_timeout(handle, write_timeout_ms, 0);
    }

    return AGENTOS_SUCCESS;
}

/**
 * @brief 获取统计信息
 * @param connection 连接句柄
 * @param stats [out] 统计信息
 * @return 错误码
 */
agentos_error_t network_get_stats(
    const network_connection_t* connection,
    network_stats_t* stats
) {
    if (!connection || !stats) {
        return AGENTOS_EINVAL;
    }

    *stats = connection->stats;
    return AGENTOS_SUCCESS;
}

/**
 * @brief 重置统计信息
 * @param connection 连接句柄
 * @return 错误码
 */
agentos_error_t network_reset_stats(network_connection_t* connection) {
    if (!connection) {
        return AGENTOS_EINVAL;
    }

    memset(&connection->stats, 0, sizeof(network_stats_t));
    return AGENTOS_SUCCESS;
}

/**
 * @brief 设置事件回调
 * @param connection 连接句柄
 * @param callback 回调函数
 * @param user_data 用户数据
 * @return 错误码
 */
agentos_error_t network_set_event_callback(
    network_connection_t* connection,
    network_event_callback_t callback,
    void* user_data
) {
    if (!connection) {
        return AGENTOS_EINVAL;
    }

    connection->event_cb = callback;
    connection->event_user_data = user_data;

    return AGENTOS_SUCCESS;
}

/**
 * @brief 获取错误消息
 * @param connection 连接句柄
 * @return 错误消息字符串
 */
const char* network_get_error_message(const network_connection_t* connection) {
    if (!connection) {
        return "Invalid connection handle";
    }
    return connection->error_msg[0] ? connection->error_msg : "No error";
}

/* ============================================================================
 * HTTP 客户端 API 实现
 * ============================================================================ */

/**
 * @brief 执行 HTTP 请求
 * @param connection 已连接的句柄
 * @param request HTTP 请求配置
 * @param response [out] HTTP 响应
 * @return 错误码
 */
agentos_error_t network_http_request(
    network_connection_t* connection,
    const network_http_request_t* request,
    network_http_response_t* response
) {
    if (!connection || !request || !response) {
        return AGENTOS_EINVAL;
    }

    if (connection->status != NETWORK_STATUS_CONNECTED) {
        return AGENTOS_ENOTCONN;
    }

    memset(response, 0, sizeof(network_http_response_t));

    /* 构建 HTTP 请求行 */
    char request_buf[NETWORK_DEFAULT_BUFFER_SIZE * 2];
    int offset = 0;

    /* 请求行 */
    offset += snprintf(request_buf + offset, sizeof(request_buf) - offset,
                       "%s %s HTTP/1.1\r\n",
                       request->method ? request->method : "GET",
                       request->path ? request->path : "/");

    /* Host 头 */
    if (connection->config.host) {
        offset += snprintf(request_buf + offset, sizeof(request_buf) - offset,
                           "Host: %s\r\n", connection->config.host);
    }

    /* Content-Type */
    if (request->content_type) {
        offset += snprintf(request_buf + offset, sizeof(request_buf) - offset,
                           "Content-Type: %s\r\n", request->content_type);
    }

    /* Content-Length */
    if (request->body && request->body_len > 0) {
        offset += snprintf(request_buf + offset, sizeof(request_buf) - offset,
                           "Content-Length: %zu\r\n", request->body_len);
    }

    /* 自定义请求头 */
    if (request->headers && request->header_count > 0) {
        for (size_t i = 0; i < request->header_count; i++) {
            if (request->headers[i]) {
                offset += snprintf(request_buf + offset, sizeof(request_buf) - offset,
                                  "%s\r\n", request->headers[i]);
            }
        }
    }

    /* 结束头部 */
    offset += snprintf(request_buf + offset, sizeof(request_buf) - offset, "\r\n");

    /* 发送请求头 */
    agentos_error_t err = network_send_all(connection, request_buf, (size_t)offset);
    if (err != AGENTOS_SUCCESS) {
        response->error = err;
        response->error_message = strdup("Failed to send request headers");
        return err;
    }

    /* 发送请求体 */
    if (request->body && request->body_len > 0) {
        err = network_send_all(connection, request->body, request->body_len);
        if (err != AGENTOS_SUCCESS) {
            response->error = err;
            response->error_message = strdup("Failed to send request body");
            return err;
        }
    }

    /* 接收响应 */
    char recv_buffer[65536];
    size_t total_received = 0;
    size_t received = 0;
    int retry_count = 0;

    do {
        received = 0;
        err = network_receive(connection, recv_buffer + total_received,
                              sizeof(recv_buffer) - total_received - 1, &received);
        if (err == AGENTOS_SUCCESS && received > 0) {
            total_received += received;
            retry_count = 0;
        } else if (err != AGENTOS_SUCCESS) {
            retry_count++;
            if (retry_count > 10 || err == AGENTOS_ECONNRESET) {
                break;
            }
        }
    } while (received > 0 && total_received < sizeof(recv_buffer) - 1);

    recv_buffer[total_received] = '\0';

    /* 解析响应状态码 */
    if (total_received >= 12) {
        sscanf(recv_buffer, "HTTP/1.%*d %d", &response->status_code);
    } else {
        response->status_code = 200;
    }

    /* 分离响应头和响应体 */
    char* body_start = strstr(recv_buffer, "\r\n\r\n");
    if (body_start) {
        size_t header_len = body_start - recv_buffer + 4;

        /* 提取响应头 */
        response->headers = (char**)calloc(1, sizeof(char*));
        if (response->headers) {
            response->headers[0] = (char*)malloc(header_len + 1);
            if (response->headers[0]) {
                memcpy(response->headers[0], recv_buffer, header_len);
                response->headers[0][header_len] = '\0';
            }
            response->header_count = 1;
        }

        /* 提取响应体 */
        body_start += 4;
        size_t body_len = total_received - (body_start - recv_buffer);
        response->body = malloc(body_len + 1);
        if (response->body) {
            memcpy(response->body, body_start, body_len);
            response->body[body_len] = '\0';
            response->body_len = body_len;
        }
    } else {
        /* 没有找到头部/体分隔符，整个作为响应体 */
        response->body = malloc(total_received + 1);
        if (response->body) {
            memcpy(response->body, recv_buffer, total_received);
            response->body[total_received] = '\0';
            response->body_len = total_received;
        }
    }

    response->error = AGENTOS_SUCCESS;
    return AGENTOS_SUCCESS;
}

/**
 * @brief 执行 HTTP GET 请求
 * @param connection 已连接的句柄
 * @param path 请求路径
 * @param response [out] HTTP 响应
 * @return 错误码
 */
agentos_error_t network_http_get(
    network_connection_t* connection,
    const char* path,
    network_http_response_t* response
) {
    network_http_request_t request = {0};
    request.method = "GET";
    request.path = path;

    return network_http_request(connection, &request, response);
}

/**
 * @brief 执行 HTTP POST 请求
 * @param connection 已连接的句柄
 * @param path 请求路径
 * @param content_type Content-Type
 * @param body 请求体
 * @param body_len 请求体长度
 * @param response [out] HTTP 响应
 * @return 错误码
 */
agentos_error_t network_http_post(
    network_connection_t* connection,
    const char* path,
    const char* content_type,
    const void* body,
    size_t body_len,
    network_http_response_t* response
) {
    network_http_request_t request = {0};
    request.method = "POST";
    request.path = path;
    request.content_type = content_type ? content_type : "application/json";
    request.body = body;
    request.body_len = body_len;

    return network_http_request(connection, &request, response);
}

/**
 * @brief 释放 HTTP 响应资源
 * @param response HTTP 响应结构体
 */
void network_http_response_free(network_http_response_t* response) {
    if (!response) {
        return;
    }

    if (response->body) {
        free(response->body);
        response->body = NULL;
    }

    if (response->headers) {
        for (size_t i = 0; i < response->header_count; i++) {
            if (response->headers[i]) {
                free(response->headers[i]);
            }
        }
        free(response->headers);
        response->headers = NULL;
    }

    if (response->error_message) {
        free(response->error_message);
        response->error_message = NULL;
    }

    if (response->status_text) {
        free(response->status_text);
        response->status_text = NULL;
    }

    memset(response, 0, sizeof(network_http_response_t));
}

/* ============================================================================
 * 连接池 API 实现
 * ============================================================================ */

/**
 * @brief 创建连接池
 * @param config 基础网络配置
 * @param pool_size 池大小
 * @return 连接池句柄
 */
network_pool_t* network_pool_create(
    const network_config_t* config,
    size_t pool_size
) {
    if (!config || pool_size == 0 || pool_size > NETWORK_MAX_POOL_SIZE) {
        return NULL;
    }

    network_pool_t* pool = (network_pool_t*)calloc(1, sizeof(network_pool_t));
    if (!pool) {
        return NULL;
    }

    pool->base_config = *config;
    pool->max_size = pool_size;
    pool->current_size = 0;
    pool->connections = (network_connection_t**)calloc(pool_size, sizeof(network_connection_t*));

    if (!pool->connections) {
        free(pool);
        return NULL;
    }

    return pool;
}

/**
 * @brief 销毁连接池
 * @param pool 连接池句柄
 */
void network_pool_destroy(network_pool_t* pool) {
    if (!pool) {
        return;
    }

    for (size_t i = 0; i < pool->current_size; i++) {
        if (pool->connections[i]) {
            network_connection_destroy(pool->connections[i]);
        }
    }

    free(pool->connections);
    free(pool);
}

/**
 * @brief 从连接池获取连接
 * @param pool 连接池句柄
 * @param timeout_ms 超时时间
 * @return 连接句柄
 */
network_connection_t* network_pool_acquire(
    network_pool_t* pool,
    int timeout_ms
) {
    if (!pool) {
        return NULL;
    }

    (void)timeout_ms;

    /* 优先复用已有连接 */
    for (size_t i = 0; i < pool->current_size; i++) {
        if (pool->connections[i] &&
            network_get_status(pool->connections[i]) == NETWORK_STATUS_CONNECTED) {
            return pool->connections[i];
        }
    }

    /* 如果未达到最大连接数，创建新连接 */
    if (pool->current_size < pool->max_size) {
        network_connection_t* conn = network_connection_create(&pool->base_config);
        if (!conn) {
            return NULL;
        }

        agentos_error_t err = network_connect(conn);
        if (err != AGENTOS_SUCCESS) {
            network_connection_destroy(conn);
            return NULL;
        }

        pool->connections[pool->current_size] = conn;
        pool->current_size++;

        return conn;
    }

    return NULL;
}

/**
 * @brief 释放连接回连接池
 * @param pool 连接池句柄
 * @param connection 连接句柄
 */
void network_pool_release(
    network_pool_t* pool,
    network_connection_t* connection
) {
    if (!pool || !connection) {
        return;
    }
    /* 连接保持在池中供后续复用 */
}

/**
 * @brief 获取连接池可用连接数
 * @param pool 连接池句柄
 * @return 可用连接数
 */
size_t network_pool_available(const network_pool_t* pool) {
    if (!pool) {
        return 0;
    }

    size_t available = 0;
    for (size_t i = 0; i < pool->current_size; i++) {
        if (pool->connections[i] &&
            network_get_status(pool->connections[i]) == NETWORK_STATUS_CONNECTED) {
            available++;
        }
    }

    /* 加上可以新建的连接槽位 */
    if (pool->current_size < pool->max_size) {
        available += (pool->max_size - pool->current_size);
    }

    return available;
}

/**
 * @brief 获取连接池总大小
 * @param pool 连接池句柄
 * @return 总大小
 */
size_t network_pool_size(const network_pool_t* pool) {
    if (!pool) {
        return 0;
    }
    return pool->current_size;
}

/**
 * @brief 健康检查连接池
 * @param pool 连接池句柄
 * @return 健康连接数
 */
size_t network_pool_health_check(network_pool_t* pool) {
    if (!pool) {
        return 0;
    }

    size_t healthy = 0;

    for (size_t i = 0; i < pool->current_size; ) {
        if (pool->connections[i]) {
            network_status_t status = network_get_status(pool->connections[i]);

            if (status == NETWORK_STATUS_CONNECTED) {
                healthy++;
                i++;
            } else if (status == NETWORK_STATUS_ERROR ||
                       status == NETWORK_STATUS_DISCONNECTED) {
                /* 移除不健康的连接 */
                network_connection_destroy(pool->connections[i]);

                /* 将最后一个连接移到当前位置以保持紧凑 */
                if (i < pool->current_size - 1) {
                    pool->connections[i] = pool->connections[pool->current_size - 1];
                    pool->connections[pool->current_size - 1] = NULL;
                } else {
                    pool->connections[i] = NULL;
                }
                pool->current_size--;
            } else {
                i++;
            }
        } else {
            i++;
        }
    }

    return healthy;
}

/* ============================================================================
 * DNS 解析 API 实现
 * ============================================================================ */

/**
 * @brief 执行 DNS 解析
 * @param hostname 主机名
 * @param af 地址族
 * @param result [out] 解析结果
 * @return 错误码
 */
agentos_error_t network_dns_resolve(
    const char* hostname,
    network_af_t af,
    network_dns_result_t* result
) {
    if (!hostname || !result) {
        return AGENTOS_EINVAL;
    }

    network_init_winsock();

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = af_to_native(af);
    hints.ai_socktype = SOCK_STREAM;

    int gai_ret = getaddrinfo(hostname, NULL, &hints, &res);
    if (gai_ret != 0) {
        return AGENTOS_EIO;
    }

    /* 统计结果数量 */
    int count = 0;
    struct addrinfo* p = res;
    while (p) {
        count++;
        p = p->ai_next;
    }

    if (count == 0) {
        freeaddrinfo(res);
        return AGENTOS_ENOENT;
    }

    /* 分配结果内存 */
    result->addresses = (char**)calloc((size_t)count, sizeof(char*));
    result->ports = (int*)calloc((size_t)count, sizeof(int));
    result->count = (size_t)count;

    if (!result->addresses || !result->ports) {
        free(result->addresses);
        free(result->ports);
        freeaddrinfo(res);
        return AGENTOS_ENOMEM;
    }

    /* 提取 IP 地址 */
    p = res;
    for (int i = 0; i < count && p; i++) {
        char ip_str[INET6_ADDRSTRLEN];

        if (p->ai_family == AF_INET) {
            struct sockaddr_in* addr_in = (struct sockaddr_in*)p->ai_addr;
            inet_ntop(AF_INET, &addr_in->sin_addr, ip_str, sizeof(ip_str));
            result->ports[i] = ntohs(addr_in->sin_port);
        } else if (p->ai_family == AF_INET6) {
            struct sockaddr_in6* addr_in6 = (struct sockaddr_in6*)p->ai_addr;
            inet_ntop(AF_INET6, &addr_in6->sin6_addr, ip_str, sizeof(ip_str));
            result->ports[i] = ntohs(addr_in6->sin6_port);
        } else {
            strcpy(ip_str, "unknown");
            result->ports[i] = 0;
        }

        result->addresses[i] = strdup(ip_str);
        p = p->ai_next;
    }

    freeaddrinfo(res);
    return AGENTOS_SUCCESS;
}

/**
 * @brief 释放 DNS 解析结果
 * @param result 解析结果
 */
void network_dns_result_free(network_dns_result_t* result) {
    if (!result) {
        return;
    }

    if (result->addresses) {
        for (size_t i = 0; i < result->count; i++) {
            if (result->addresses[i]) {
                free(result->addresses[i]);
            }
        }
        free(result->addresses);
        result->addresses = NULL;
    }

    if (result->ports) {
        free(result->ports);
        result->ports = NULL;
    }

    result->count = 0;
}

/* ============================================================================
 * 工具函数实现
 * ============================================================================ */

/**
 * @brief 检查主机是否可达
 * @param host 主机名或 IP
 * @param timeout_ms 超时时间
 * @return true 可达，false 不可达
 */
bool network_is_reachable(const char* host, int timeout_ms) {
    if (!host) {
        return false;
    }

    network_init_winsock();

    /* 尝试创建 TCP 连接来检测可达性 */
    network_config_t config = network_create_default_config();
    config.host = host;
    config.timeout_ms = timeout_ms > 0 ? timeout_ms : 5000;

    network_connection_t* conn = network_connection_create(&config);
    if (!conn) {
        return false;
    }

    agentos_error_t err = network_connect(conn);
    bool reachable = (err == AGENTOS_SUCCESS);

    if (reachable) {
        network_disconnect(conn);
    }

    network_connection_destroy(conn);

    return reachable;
}

/**
 * @brief 获取本机 IP 地址
 * @param af 地址族
 * @param buffer 输出缓冲区
 * @param buffer_len 缓冲区长度
 * @return 错误码
 */
agentos_error_t network_get_local_ip(
    network_af_t af,
    char* buffer,
    size_t buffer_len
) {
    if (!buffer || buffer_len == 0) {
        return AGENTOS_EINVAL;
    }

    network_init_winsock();

#ifdef _WIN32
    char hostname[256];
    gethostname(hostname, sizeof(hostname));

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = af_to_native(af);

    if (getaddrinfo(hostname, NULL, &hints, &res) != 0) {
        strncpy(buffer, "127.0.0.1", buffer_len - 1);
        buffer[buffer_len - 1] = '\0';
        return AGENTOS_SUCCESS;
    }

    if (res->ai_family == AF_INET) {
        struct sockaddr_in* addr_in = (struct sockaddr_in*)res->ai_addr;
        inet_ntop(AF_INET, &addr_in->sin_addr, buffer, (socklen_t)buffer_len);
    } else if (res->ai_family == AF_INET6) {
        struct sockaddr_in6* addr_in6 = (struct sockaddr_in6*)res->ai_addr;
        inet_ntop(AF_INET6, &addr_in6->sin6_addr, buffer, (socklen_t)buffer_len);
    } else {
        strncpy(buffer, "127.0.0.1", buffer_len - 1);
        buffer[buffer_len - 1] = '\0';
    }

    freeaddrinfo(res);
#else
    /* 使用 UDP socket 连接到外部地址来获取本地 IP */
    const char* test_host = "8.8.8.8";
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        strncpy(buffer, "127.0.0.1", buffer_len - 1);
        buffer[buffer_len - 1] = '\0';
        return AGENTOS_SUCCESS;
    }

    struct sockaddr_in server;
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(80);
    inet_pton(AF_INET, test_host, &server.sin_addr);

    connect(sockfd, (struct sockaddr*)&server, sizeof(server));

    struct sockaddr_in local_addr;
    socklen_t addr_len = sizeof(local_addr);
    getsockname(sockfd, (struct sockaddr*)&local_addr, &addr_len);
    inet_ntop(AF_INET, &local_addr.sin_addr, buffer, (socklen_t)buffer_len);

    close(sockfd);
#endif

    return AGENTOS_SUCCESS;
}

/**
 * @brief 将 IP 地址转换为字符串
 * @param af 地址族
 * @param addr 地址结构
 * @param buffer 输出缓冲区
 * @param buffer_len 缓冲区长度
 * @return 错误码
 */
agentos_error_t network_addr_to_string(
    network_af_t af,
    const void* addr,
    char* buffer,
    size_t buffer_len
) {
    if (!addr || !buffer || buffer_len == 0) {
        return AGENTOS_EINVAL;
    }

    if (af == NETWORK_AF_INET) {
        const struct sockaddr_in* addr_in = (const struct sockaddr_in*)addr;
        inet_ntop(AF_INET, &addr_in->sin_addr, buffer, (socklen_t)buffer_len);
    } else if (af == NETWORK_AF_INET6) {
        const struct sockaddr_in6* addr_in6 = (const struct sockaddr_in6*)addr;
        inet_ntop(AF_INET6, &addr_in6->sin6_addr, buffer, (socklen_t)buffer_len);
    } else {
        strncpy(buffer, "unknown", buffer_len - 1);
        buffer[buffer_len - 1] = '\0';
    }

    return AGENTOS_SUCCESS;
}

/**
 * @brief 初始化网络子系统
 * @return 错误码
 */
agentos_error_t network_init(void) {
    if (network_init_winsock() != 0) {
        return AGENTOS_EIO;
    }
    return AGENTOS_SUCCESS;
}

/**
 * @brief 清理网络子系统
 */
void network_cleanup(void) {
#ifdef _WIN32
    WSACleanup();
#endif
}
