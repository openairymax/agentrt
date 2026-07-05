# Network — 网络工具模块

**模块路径**: `agentrt/commons/utils/network/`
**版本**: v0.1.0

## 概述

Network 模块提供跨平台的网络通信抽象层，支持 TCP/UDP Socket 连接、HTTP/HTTPS 客户端、连接池管理和 DNS 解析。该模块是 AgentRT 中 Agent 与外部服务通信的基础设施，所有网络操作均经过统一的超时控制、重试机制和事件回调，确保通信的可靠性和可观测性。

## 设计目标

- **跨平台一致性**：统一 API 屏蔽 Windows（Winsock2）与 Linux/macOS（POSIX Socket）的网络编程差异
- **安全通信**：内置 SSL/TLS 支持，可配置证书验证模式
- **高可用性**：连接池复用、自动重试、超时控制和健康检查
- **可观测性**：连接统计信息（字节数、包数、延迟）、事件回调机制
- **HTTP 客户端**：内置 HTTP/HTTPS 请求支持，自动构建请求头并解析响应

## 目录结构

```
network/
├── include/
│   └── network_common.h       # 网络通信模块公共接口定义
├── src/
│   └── network_common.c       # 网络通信模块实现
└── README.md                  # 本文档
```

## 核心数据结构

### network_config_t — 网络配置

| 字段 | 类型 | 说明 |
|------|------|------|
| `host` | `const char *` | 主机名或 IP 地址 |
| `port` | `int` | 端口号 |
| `timeout_ms` | `int` | 连接超时（毫秒），默认 30000 |
| `read_timeout_ms` | `int` | 读取超时（毫秒） |
| `write_timeout_ms` | `int` | 写入超时（毫秒） |
| `max_retries` | `int` | 最大重试次数，默认 3 |
| `retry_interval_ms` | `int` | 重试间隔（毫秒） |
| `sock_type` | `network_sock_type_t` | Socket 类型（STREAM/DGRAM/RAW） |
| `af` | `network_af_t` | 地址族（INET/INET6） |
| `keepalive` | `bool` | 是否启用 TCP 保活 |
| `nonblocking` | `bool` | 是否非阻塞模式 |
| `ssl_enable` | `bool` | 是否启用 SSL/TLS |
| `ssl_verify` | `network_ssl_verify_t` | SSL 证书验证模式 |
| `ssl_cert_path` | `const char *` | SSL 证书路径 |
| `ssl_key_path` | `const char *` | SSL 私钥路径 |
| `ssl_ca_path` | `const char *` | CA 证书路径 |

### network_stats_t — 网络统计信息

| 字段 | 类型 | 说明 |
|------|------|------|
| `bytes_sent` | `uint64_t` | 已发送字节数 |
| `bytes_received` | `uint64_t` | 已接收字节数 |
| `packets_sent` | `uint64_t` | 已发送包数 |
| `packets_received` | `uint64_t` | 已接收包数 |
| `connect_count` | `uint64_t` | 连接次数 |
| `error_count` | `uint64_t` | 错误次数 |
| `retry_count` | `uint64_t` | 重试次数 |
| `avg_latency_us` | `uint64_t` | 平均延迟（微秒） |

### network_http_request_t — HTTP 请求

| 字段 | 类型 | 说明 |
|------|------|------|
| `method` | `const char *` | HTTP 方法（GET/POST/PUT/DELETE） |
| `path` | `const char *` | 请求路径 |
| `content_type` | `const char *` | Content-Type |
| `body` | `const void *` | 请求体 |
| `body_len` | `size_t` | 请求体长度 |
| `headers` | `const char **` | 自定义请求头数组 |
| `header_count` | `size_t` | 请求头数量 |
| `timeout_ms` | `int` | 超时时间 |
| `follow_redirects` | `bool` | 是否跟随重定向 |
| `max_redirects` | `int` | 最大重定向次数 |

### network_http_response_t — HTTP 响应

| 字段 | 类型 | 说明 |
|------|------|------|
| `status_code` | `int` | HTTP 状态码 |
| `status_text` | `char *` | 状态文本 |
| `headers` | `char **` | 响应头数组 |
| `header_count` | `size_t` | 响应头数量 |
| `body` | `void *` | 响应体 |
| `body_len` | `size_t` | 响应体长度 |
| `error` | `agentrt_error_t` | 错误码 |
| `error_message` | `char *` | 错误消息 |
| `latency_us` | `uint64_t` | 响应延迟（微秒） |

### network_dns_result_t — DNS 解析结果

| 字段 | 类型 | 说明 |
|------|------|------|
| `addresses` | `char **` | IP 地址数组 |
| `count` | `size_t` | 地址数量 |
| `ports` | `int *` | 端口数组 |

## 接口说明

### 基础连接 API

| 函数 | 说明 |
|------|------|
| `network_create_default_config()` | 创建默认网络配置 |
| `network_connection_create(config)` | 创建网络连接句柄 |
| `network_connection_destroy(connection)` | 销毁网络连接句柄 |
| `network_connect(connection)` | 建立网络连接 |
| `network_disconnect(connection)` | 断开网络连接 |
| `network_send(connection, data, length, sent)` | 发送数据 |
| `network_receive(connection, buffer, length, received)` | 接收数据 |
| `network_send_all(connection, data, length)` | 循环发送直到全部完成 |
| `network_receive_exact(connection, buffer, length)` | 接收指定长度的数据 |
| `network_get_status(connection)` | 获取连接状态 |
| `network_set_timeout(connection, timeout_ms)` | 设置连接超时 |
| `network_set_rw_timeout(connection, read_ms, write_ms)` | 设置读写超时 |
| `network_get_stats(connection, stats)` | 获取统计信息 |
| `network_reset_stats(connection)` | 重置统计信息 |
| `network_set_event_callback(connection, callback, user_data)` | 设置事件回调 |
| `network_get_error_message(connection)` | 获取错误消息 |

### HTTP 客户端 API

| 函数 | 说明 |
|------|------|
| `network_http_request(connection, request, response)` | 执行 HTTP 请求 |
| `network_http_get(connection, path, response)` | 执行 HTTP GET 请求 |
| `network_http_post(connection, path, content_type, body, body_len, response)` | 执行 HTTP POST 请求 |
| `network_http_response_free(response)` | 释放 HTTP 响应资源 |

### 连接池 API

| 函数 | 说明 |
|------|------|
| `network_pool_create(config, pool_size)` | 创建连接池（最大 32 个连接） |
| `network_pool_destroy(pool)` | 销毁连接池 |
| `network_pool_acquire(pool, timeout_ms)` | 从连接池获取连接 |
| `network_pool_release(pool, connection)` | 释放连接回连接池 |
| `network_pool_available(pool)` | 获取可用连接数 |
| `network_pool_size(pool)` | 获取连接池当前大小 |
| `network_pool_health_check(pool)` | 健康检查并移除不健康连接 |

### DNS 解析 API

| 函数 | 说明 |
|------|------|
| `network_dns_resolve(hostname, af, result)` | 执行 DNS 解析 |
| `network_dns_result_free(result)` | 释放 DNS 解析结果 |

### 工具函数

| 函数 | 说明 |
|------|------|
| `network_is_reachable(host, timeout_ms)` | 检查主机是否可达 |
| `network_get_local_ip(af, buffer, buffer_len)` | 获取本机 IP 地址 |
| `network_addr_to_string(af, addr, buffer, buffer_len)` | IP 地址转字符串 |
| `network_init()` | 初始化网络子系统（Windows Winsock） |
| `network_cleanup()` | 清理网络子系统 |

## 使用示例

```c
#include "network_common.h"

// 初始化网络子系统
network_init();

// 创建默认配置
network_config_t config = network_create_default_config();
config.host = "api.example.com";
config.port = 443;
config.ssl_enable = true;
config.ssl_verify = NETWORK_SSL_VERIFY_PEER;
config.timeout_ms = 10000;

// 创建连接并连接
network_connection_t *conn = network_connection_create(&config);
if (!conn) {
    fprintf(stderr, "Failed to create connection\n");
    network_cleanup();
    return;
}

agentrt_error_t err = network_connect(conn);
if (err == AGENTRT_SUCCESS) {
    // 发送 HTTP GET 请求
    network_http_response_t response;
    err = network_http_get(conn, "/api/v1/status", &response);
    if (err == AGENTRT_SUCCESS && response.status_code == 200) {
        printf("Response: %.*s\n", (int)response.body_len, (char *)response.body);
    }
    network_http_response_free(&response);

    network_disconnect(conn);
}

// 获取统计信息
network_stats_t stats;
network_get_stats(conn, &stats);
printf("Sent: %llu bytes, Received: %llu bytes\n",
       (unsigned long long)stats.bytes_sent,
       (unsigned long long)stats.bytes_received);

network_connection_destroy(conn);
network_cleanup();
```

```c
// 使用连接池
network_pool_t *pool = network_pool_create(&config, 8);
network_connection_t *conn2 = network_pool_acquire(pool, 5000);
if (conn2) {
    network_http_response_t resp;
    network_http_get(conn2, "/api/data", &resp);
    network_http_response_free(&resp);
    network_pool_release(pool, conn2);
}

// 健康检查
size_t healthy = network_pool_health_check(pool);
printf("Healthy connections: %zu / %zu\n", healthy, network_pool_size(pool));

network_pool_destroy(pool);
```

## 连接状态

| 状态 | 说明 |
|------|------|
| `NETWORK_STATUS_DISCONNECTED` | 已断开连接 |
| `NETWORK_STATUS_CONNECTING` | 正在连接中 |
| `NETWORK_STATUS_CONNECTED` | 已建立连接 |
| `NETWORK_STATUS_DISCONNECTING` | 正在断开连接 |
| `NETWORK_STATUS_ERROR` | 错误状态 |

## 事件回调类型

| 事件 | 说明 |
|------|------|
| `NETWORK_EVENT_CONNECTED` | 连接成功时触发 |
| `NETWORK_EVENT_DISCONNECTED` | 连接断开时触发 |
| `NETWORK_EVENT_DATA_RECEIVED` | 数据接收时触发 |
| `NETWORK_EVENT_DATA_SENT` | 数据发送时触发 |
| `NETWORK_EVENT_ERROR` | 错误发生时触发 |
| `NETWORK_EVENT_TIMEOUT` | 超时时触发 |

## 平台差异

| 特性 | Linux | Windows | macOS |
|------|-------|---------|-------|
| Socket API | POSIX (fd) | Winsock2 (SOCKET) | POSIX (fd) |
| 初始化 | 无需 | `WSAStartup()` | 无需 |
| 清理 | 无需 | `WSACleanup()` | 无需 |
| 非阻塞 | `fcntl(O_NONBLOCK)` | `ioctlsocket(FIONBIO)` | `fcntl(O_NONBLOCK)` |

## 依赖关系

| 依赖 | 说明 |
|------|------|
| `memory_compat.h` | 统一内存管理宏（`AGENTRT_MALLOC`、`AGENTRT_FREE` 等） |
| `error.h` | 统一错误码定义 |
| `types.h` | 基础类型定义 |
| `atomic_compat.h` | 跨平台原子操作 |

---

© 2026 SPHARX Ltd. All Rights Reserved.