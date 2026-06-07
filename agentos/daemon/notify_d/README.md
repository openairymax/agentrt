# Notify Daemon — 多协议通知守护进程

> **模块路径**: `agentos/daemon/notify_d/` | **版本**: v0.1.0

## 概述

`daemon/notify_d/` 是 AgentOS 的多协议通知守护进程，负责系统事件的广播与推送。它支持 WebSocket、Unix Socket 和 SSE（Server-Sent Events）三种客户端协议，提供事件队列管理、频道订阅过滤和实时广播能力，是 AgentOS 事件驱动架构的核心通知枢纽。

### 架构定位

```
notify_d/ → IPC Service Bus → 各守护进程事件源
    ↑
 多协议通知层（WebSocket / Socket / SSE）
```

### 核心职责

- **多协议客户端支持**：WebSocket、Unix Socket、SSE 三种客户端接入方式
- **WebSocket 握手**：解析 Sec-WebSocket-Key，计算 SHA-1 + Base64 生成 Accept Key，返回 101 Switching Protocols
- **事件广播**：遍历所有活跃客户端，按客户端类型分发事件（WS 帧 / 原始 Socket / SSE 格式）
- **频道订阅过滤**：支持按频道（channel）过滤事件，客户端仅接收订阅频道的事件
- **事件队列**：环形缓冲区事件队列（容量 1024），后台线程消费并广播
- **内嵌 SHA-1**：内置 SHA-1 实现，无需外部加密库依赖

## 目录结构

```
notify_d/
├── CMakeLists.txt                    # 构建配置
├── README.md                         # 本文件
└── src/                              # 实现文件
    └── main.c                        # 守护进程入口（含通知逻辑与协议处理）
```

## 核心组件说明

### 客户端类型

| 客户端类型 | 枚举值 | 说明 |
|------------|--------|------|
| Unix Socket | `NOTIFY_CLIENT_SOCKET` | 原始 Socket 连接，直接发送原始数据 |
| WebSocket | `NOTIFY_CLIENT_WEBSOCKET` | WebSocket 协议连接，发送 WS 文本帧 |
| SSE | `NOTIFY_CLIENT_SSE` | Server-Sent Events 连接，发送 SSE 格式数据 |

### WebSocket 协议实现

- **握手**：解析 HTTP Upgrade 请求中的 `Sec-WebSocket-Key`，拼接 WebSocket GUID `258EAFA5-E914-47DA-95CA-C5AB0DC85B11`，计算 SHA-1 摘要后 Base64 编码，作为 `Sec-WebSocket-Accept` 返回 101 响应
- **帧发送**：opcode=0x81（文本帧），支持三种载荷长度编码：
  - ≤ 125 字节：单字节载荷长度
  - 126–65535 字节：2 字节扩展载荷长度
  - > 65535 字节：8 字节扩展载荷长度
- **SHA-1**：内嵌实现，无外部依赖

### 事件广播流程

```
事件源（守护进程）
       ↓
  事件队列（ring buffer, 容量 1024）
       ↓
  后台线程（notify_d_event_loop）消费
       ↓
  ┌──────────┬──────────┬──────────┐
  │ WebSocket │  Socket  │   SSE    │
  │ WS 文本帧 │ 原始数据  │ SSE 格式  │
  └──────────┴──────────┴──────────┘
       ↓
  频道订阅过滤 → 推送到匹配客户端
```

### SSE 格式

```
data: {"event":"...","data":"..."}\n\n
```

## 接口说明

### 请求处理

请求处理器自动检测客户端协议类型：

| 检测条件 | 处理方式 |
|----------|----------|
| HTTP Header 包含 `Upgrade: websocket` | 执行 WebSocket 握手，升级为 WS 连接 |
| HTTP Header 包含 `Accept: text/event-stream` | 切换为 SSE 模式，推送事件流 |
| 其他 | 作为原始 Socket 客户端处理 |

### JSON-RPC 2.0 方法

| 方法 | 说明 |
|------|------|
| `notify.subscribe` | 订阅指定频道 |
| `notify.unsubscribe` | 取消订阅频道 |
| `notify.publish` | 发布事件到指定频道 |
| `notify.list` | 列出当前活跃客户端 |
| `notify.health` | 查询通知服务健康状态 |

## 通信方式

| 方向 | 协议 | 说明 |
|------|------|------|
| 入站 | JSON-RPC 2.0 | 通过 IPC Service Bus 接收请求 |
| 入站 | TCP | 默认监听端口 8084 |
| 入站 | Unix Socket | `AGENTOS_RUNTIME_DIR/notify.sock` |
| 出站 | WebSocket | WS 文本帧广播 |
| 出站 | SSE | Server-Sent Events 推送 |
| 出站 | 原始 Socket | 直接数据推送 |

## 配置选项

| 配置项 | 默认值 | 说明 |
|--------|--------|------|
| TCP 端口 | 8084 | HTTP/TCP 监听端口 |
| Unix Socket | `AGENTOS_RUNTIME_DIR/notify.sock` | IPC 通信 Socket 路径 |
| 最大待处理事件 | 1024 | 环形缓冲区事件队列容量 |
| 最大客户端数 | 128 | 同时连接的最大客户端数量 |
| WebSocket GUID | `258EAFA5-E914-47DA-95CA-C5AB0DC85B11` | WebSocket 协议握手 GUID |

## 健康检查机制

- **运行状态检测**：检查服务是否处于正常运行状态
- **队列满检测**：检查事件队列是否已满，队列满则判定为不健康
- **错误率检测**：检查广播错误率，错误率过高则判定为不健康

## 跨平台支持

| 平台 | WebSocket | SSE | Unix Socket | 说明 |
|------|-----------|-----|-------------|------|
| Linux | ✅ | ✅ | ✅ | 完整支持 |
| Windows | ✅ | ✅ | ⚠️ 有限 | Socket 路径需适配 |

## 依赖关系

```
notify_d
├── common (agentos_common, svc_common)
├── Threads::Threads
└── Windows 额外: ws2_32
```

## 构建说明

```bash
# 构建通知守护进程
cmake -B build -DBUILD_TESTS=ON
cmake --build build --target agentos-notify-d
```

## 使用示例

### 启动通知守护进程

```bash
# 默认启动（TCP 端口 8084）
./agentos-notify-d

# 指定配置文件
./agentos-notify-d --config notify_config.json
```

### WebSocket 客户端连接

```javascript
// JavaScript WebSocket 客户端示例
const ws = new WebSocket('ws://localhost:8084');
ws.onopen = () => console.log('Connected to notify_d');
ws.onmessage = (event) => console.log('Event:', event.data);
```

### SSE 客户端连接

```javascript
// JavaScript SSE 客户端示例
const source = new EventSource('http://localhost:8084');
source.onmessage = (event) => console.log('Event:', event.data);
```

### 信号处理

| 信号 | 行为 |
|------|------|
| SIGINT | 优雅关闭 |
| SIGTERM | 优雅关闭 |
| SIGUSR1 | 动态调整日志级别 |
| SIGPIPE | 忽略（防止写入已关闭连接导致进程终止） |

---

© 2026 SPHARX Ltd. All Rights Reserved.
