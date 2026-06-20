# IPC — 进程间通信模块

**模块路径**: `agentos/commons/utils/ipc/`
**版本**: v0.1.0

## 概述

IPC 模块提供跨平台的进程间通信抽象层，支持管道、命名管道、Unix Domain Socket、共享内存、消息队列和 RPC 调用框架。该模块是 AgentRT 中组件间通信的基础设施，统一消息格式和协议，支持同步和异步通信模式。

> **注意**：本模块仅在非 Windows 平台上可用。

## 设计目标

- **跨平台抽象**：统一 Windows Named Pipe / Unix Socket / POSIX MQ 等底层机制，提供一致的 API
- **统一消息格式**：标准化的消息头和负载结构，支持 CRC32 校验、请求-响应关联和序列化
- **同步与异步**：支持阻塞、非阻塞和回调模式的消息收发
- **内置可靠性**：超时控制、重试机制、事件通知和统计信息
- **RPC 框架**：基于传输通道的远程过程调用，支持方法注册和同步调用

## 目录结构

```
ipc/
├── include/
│   └── ipc_common.h              # IPC 模块公共接口定义
├── src/
│   └── ipc_common.c              # IPC 机制实现（管道、消息队列、共享内存等）
└── README.md                     # 本文档
```

## 核心数据结构

### ipc_type_t — IPC 通道类型

| 枚举值 | 说明 |
|------|------|
| `IPC_TYPE_PIPE` | 匿名管道 |
| `IPC_TYPE_NAMED_PIPE` | 命名管道 |
| `IPC_TYPE_SOCKET` | Unix Socket / Windows Named Pipe |
| `IPC_TYPE_SHM` | 共享内存 |
| `IPC_TYPE_MQ` | 消息队列 |
| `IPC_TYPE_RPC` | RPC 调用 |

### ipc_config_t — 通道配置

| 字段 | 类型 | 说明 |
|------|------|------|
| `type` | `ipc_type_t` | 通道类型 |
| `name` | `const char *` | 通道名称 |
| `mode` | `ipc_mode_t` | 读写模式（`IPC_MODE_READ` / `IPC_MODE_WRITE` / `IPC_MODE_READ_WRITE`） |
| `buffer_size` | `size_t` | 缓冲区大小（默认 65536） |
| `max_message_size` | `size_t` | 最大消息大小（默认 1MB） |
| `timeout_ms` | `uint32_t` | 默认超时（默认 5000ms） |
| `max_connections` | `uint32_t` | 最大连接数（服务端，默认 128） |
| `nonblocking` | `bool` | 是否非阻塞 |
| `persistent` | `bool` | 是否持久化 |
| `permissions` | `const char *` | 权限设置（Unix 权限字符串） |

### ipc_message_header_t — 消息头

| 字段 | 类型 | 说明 |
|------|------|------|
| `magic` | `uint32_t` | 魔数（`IPC_MAGIC` = `0x49504300`） |
| `version` | `uint32_t` | 协议版本 |
| `type` | `uint32_t` | 消息类型（数据/请求/响应/通知/错误/控制） |
| `flags` | `uint32_t` | 消息标志（非阻塞/优先级/广播/独占/持久化） |
| `msg_id` | `uint64_t` | 消息 ID |
| `correlation_id` | `uint64_t` | 关联 ID（请求-响应模式） |
| `source` | `char[64]` | 发送者标识 |
| `target` | `char[64]` | 目标标识 |
| `payload_len` | `uint64_t` | 负载长度 |
| `checksum` | `uint32_t` | CRC32 校验和 |
| `timestamp` | `agentos_timestamp_t` | 时间戳 |

### ipc_message_t — 消息结构

| 字段 | 类型 | 说明 |
|------|------|------|
| `header` | `ipc_message_header_t` | 消息头 |
| `payload` | `void *` | 负载数据 |
| `payload_size` | `size_t` | 负载大小 |

### ipc_stats_t — 统计信息

| 字段 | 类型 | 说明 |
|------|------|------|
| `messages_sent` | `uint64_t` | 已发送消息数 |
| `messages_received` | `uint64_t` | 已接收消息数 |
| `bytes_sent` | `uint64_t` | 已发送字节数 |
| `bytes_received` | `uint64_t` | 已接收字节数 |
| `errors` | `uint64_t` | 错误次数 |
| `timeouts` | `uint64_t` | 超时次数 |
| `avg_latency_us` | `uint64_t` | 平均延迟（微秒） |
| `max_latency_us` | `uint64_t` | 最大延迟（微秒） |

## 接口说明

### 初始化与清理

| 函数 | 说明 |
|------|------|
| `ipc_init()` | 初始化 IPC 子系统 |
| `ipc_cleanup()` | 清理 IPC 子系统 |

### 通道管理

| 函数 | 说明 |
|------|------|
| `ipc_create_default_config(type)` | 创建默认 IPC 配置 |
| `ipc_channel_create(config)` | 创建 IPC 通道 |
| `ipc_channel_destroy(channel)` | 销毁 IPC 通道 |
| `ipc_channel_open(channel)` | 打开 IPC 通道 |
| `ipc_channel_close(channel)` | 关闭 IPC 通道 |
| `ipc_channel_get_state(channel)` | 获取通道状态 |
| `ipc_channel_get_name(channel)` | 获取通道名称 |
| `ipc_channel_get_type(channel)` | 获取通道类型 |
| `ipc_channel_set_timeout(channel, timeout_ms)` | 设置通道超时 |
| `ipc_channel_set_event_callback(channel, callback, user_data)` | 设置事件回调 |
| `ipc_channel_get_stats(channel, stats)` | 获取统计信息 |
| `ipc_channel_reset_stats(channel)` | 重置统计信息 |

### 消息发送

| 函数 | 说明 |
|------|------|
| `ipc_send(channel, message)` | 发送消息 |
| `ipc_send_data(channel, data, len, sent)` | 发送数据（简化接口） |
| `ipc_send_request(channel, request, response, timeout_ms)` | 发送请求并等待响应 |
| `ipc_broadcast(channel, message)` | 发送广播消息 |
| `ipc_notify(channel, notification, len)` | 发送通知消息 |

### 消息接收

| 函数 | 说明 |
|------|------|
| `ipc_receive(channel, message, timeout_ms)` | 接收消息 |
| `ipc_receive_data(channel, buffer, len, received)` | 接收数据（简化接口） |
| `ipc_try_receive(channel, message)` | 尝试接收消息（非阻塞） |
| `ipc_set_message_callback(channel, callback, user_data)` | 设置消息回调 |

### 服务端 API

| 函数 | 说明 |
|------|------|
| `ipc_server_create(config)` | 创建 IPC 服务端 |
| `ipc_server_destroy(server)` | 销毁 IPC 服务端 |
| `ipc_server_start(server)` | 启动 IPC 服务端 |
| `ipc_server_stop(server)` | 停止 IPC 服务端 |
| `ipc_server_accept(server, timeout_ms)` | 接受客户端连接 |
| `ipc_server_connection_count(server)` | 获取服务端连接数 |
| `ipc_server_broadcast(server, message)` | 广播消息给所有客户端 |

### 客户端 API

| 函数 | 说明 |
|------|------|
| `ipc_client_create(config)` | 创建 IPC 客户端 |
| `ipc_client_destroy(client)` | 销毁 IPC 客户端 |
| `ipc_client_connect(client, timeout_ms)` | 连接到服务端 |
| `ipc_client_disconnect(client)` | 断开连接 |
| `ipc_client_get_channel(client)` | 获取客户端通道 |

### 共享内存 API

| 函数 | 说明 |
|------|------|
| `ipc_shm_create(config)` | 创建共享内存 |
| `ipc_shm_destroy(shm)` | 销毁共享内存 |
| `ipc_shm_map(shm)` | 映射共享内存到进程地址空间 |
| `ipc_shm_unmap(shm)` | 取消映射共享内存 |
| `ipc_shm_get_size(shm)` | 获取共享内存大小 |
| `ipc_shm_sync(shm)` | 同步共享内存 |

### 消息队列 API

| 函数 | 说明 |
|------|------|
| `ipc_mq_create(config)` | 创建消息队列 |
| `ipc_mq_destroy(mq)` | 销毁消息队列 |
| `ipc_mq_send(mq, data, len, priority)` | 发送消息到队列 |
| `ipc_mq_receive(mq, buffer, len, received, priority, timeout_ms)` | 从队列接收消息 |
| `ipc_mq_count(mq)` | 获取队列当前消息数 |
| `ipc_mq_clear(mq)` | 清空消息队列 |

### RPC 框架

| 函数 | 说明 |
|------|------|
| `ipc_rpc_server_create(config)` | 创建 RPC 服务端 |
| `ipc_rpc_server_destroy(server)` | 销毁 RPC 服务端 |
| `ipc_rpc_server_start(server)` | 启动 RPC 服务端 |
| `ipc_rpc_server_stop(server)` | 停止 RPC 服务端 |
| `ipc_rpc_server_process(server, timeout_ms)` | 处理单个 RPC 请求 |
| `ipc_rpc_server_register_method(server, method)` | 注册 RPC 方法 |
| `ipc_rpc_server_find_method(server, method_name)` | 查找已注册的 RPC 方法 |
| `ipc_rpc_client_create(config)` | 创建 RPC 客户端 |
| `ipc_rpc_client_destroy(client)` | 销毁 RPC 客户端 |
| `ipc_rpc_call_sync(client, method_name, request, ...)` | 同步 RPC 调用 |

### 消息辅助函数

| 函数 | 说明 |
|------|------|
| `ipc_message_create(type, payload, payload_len)` | 创建消息 |
| `ipc_message_free(message)` | 释放消息 |
| `ipc_message_clone(message)` | 复制消息 |
| `ipc_message_checksum(message)` | 计算消息校验和 |
| `ipc_message_verify(message)` | 验证消息校验和 |
| `ipc_message_serialize(message, buffer, buffer_len, written)` | 序列化消息为字节流 |
| `ipc_message_deserialize(buffer, len, message)` | 从字节流反序列化消息 |

### 工具函数

| 函数 | 说明 |
|------|------|
| `ipc_get_error_message(channel)` | 获取错误消息 |
| `ipc_is_valid(channel)` | 检查通道是否可用 |
| `ipc_flush(channel)` | 刷新通道缓冲区 |

## 使用示例

```c
#include "ipc_common.h"

// 初始化 IPC 子系统
ipc_init();

// === 服务端示例 ===
ipc_config_t server_config = ipc_create_default_config(IPC_TYPE_SOCKET);
server_config.name = "/tmp/agentos.sock";
server_config.mode = IPC_MODE_READ_WRITE;

ipc_server_t *server = ipc_server_create(&server_config);
ipc_server_start(server);

ipc_channel_t *client_chan = ipc_server_accept(server, 5000);
if (client_chan != NULL) {
    ipc_message_t msg;
    agentos_error_t err = ipc_receive(client_chan, &msg, 5000);
    if (err == AGENTOS_OK) {
        printf("Received: %.*s\n", (int)msg.payload_size, (char *)msg.payload);
        ipc_send(client_chan, &msg);  // echo back
    }
    ipc_channel_destroy(client_chan);
}
ipc_server_stop(server);
ipc_server_destroy(server);

// === 客户端示例 ===
ipc_config_t client_config = ipc_create_default_config(IPC_TYPE_SOCKET);
client_config.name = "/tmp/agentos.sock";
client_config.mode = IPC_MODE_READ_WRITE;

ipc_client_t *client = ipc_client_create(&client_config);
ipc_client_connect(client, 5000);

ipc_channel_t *chan = ipc_client_get_channel(client);
const char *data = "Hello from client";
ipc_send_data(chan, data, strlen(data), NULL);

ipc_client_disconnect(client);
ipc_client_destroy(client);

// 清理 IPC 子系统
ipc_cleanup();
```

## 平台可用性

| 特性 | Linux | macOS | Windows |
|------|-------|-------|---------|
| 匿名管道 (Pipe) | ✓ | ✓ | ✓ |
| 命名管道 (Named Pipe) | ✓ | ✓ | ✓ |
| Unix Socket | ✓ | ✓ | ✗ (使用 Named Pipe) |
| 共享内存 (SHM) | ✓ | ✓ | ✓ |
| 消息队列 (MQ) | ✓ | ✓ | ✗ |
| RPC 框架 | ✓ | ✓ | ✓ |

## 依赖关系

| 依赖 | 说明 |
|------|------|
| `error.h` | 错误码定义（`agentos_error_t`） |
| `types.h` | 基础类型定义（`agentos_timestamp_t`、`agentos_ipc_type_t`） |

---

© 2026 SPHARX Ltd. All Rights Reserved.