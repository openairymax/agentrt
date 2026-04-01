# Gateway – AgentOS 网关层

<div align="center">

[![Version](https://img.shields.io/badge/version-2.0.0-blue.svg)](https://gitee.com/spharx/agentos)
[![License](https://img.shields.io/badge/license-Apache--2.0-green.svg)](https://gitee.com/spharx/agentos/blob/main/LICENSE)
[![C11](https://img.shields.io/badge/C-11-blue.svg)](https://en.cppreference.com/w/c/11)

**协议转换层：连接内核与外部世界的桥梁**

</div>

---

## 📐 架构定位

```
┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
┃                          AgentOS 总体架构                                     ┃
┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛
┌──────────────────────────────────────────────────────────────────────────────┐
│                           服务层 (daemon)                                     │
│              llm_d | market_d | monit_d | sched_d | tool_d | gateway_d       │
└──────────────────────────────────────────────────────────────────────────────┘
                                    ↓↑
┌──────────────────────────────────────────────────────────────────────────────┐
│                     ★ 网关层 (gateway) ★ 本模块 ★                            │
│                   HTTP · WebSocket · Stdio                                    │
│                   协议转换：外部请求 → JSON-RPC 2.0                            │
└──────────────────────────────────────────────────────────────────────────────┘
                                    ↓↑
┌──────────────────────────────────────────────────────────────────────────────┐
│                          系统调用层 (atoms/syscall)                           │
│              task · memory · session · telemetry · agent                      │
└──────────────────────────────────────────────────────────────────────────────┘
```

**核心定位**：gateway 层只做一件事 —— **协议转换**，作为连接内核与外部世界的桥梁。

---

## 🎯 职责边界

### ✅ 职责范围内

| 功能 | 说明 | 实现文件 |
|------|------|----------|
| **HTTP 协议转换** | HTTP 请求 → JSON-RPC 2.0 | `http_gateway.c` |
| **WebSocket 协议转换** | WebSocket 消息 → JSON-RPC 2.0 | `ws_gateway.c` |
| **Stdio 协议转换** | 标准输入输出 → JSON-RPC 2.0 | `stdio_gateway.c` |
| **JSON-RPC 工具** | 请求验证、响应生成 | `jsonrpc.c` |
| **连接生命周期** | 连接建立、维护、关闭 | 各 gateway 实现 |

### ❌ 职责范围外

| 功能 | 正确归属 | 说明 |
|------|----------|------|
| 会话管理 | `atoms/syscall/session.c` | 通过 `agentos_sys_session_*` 调用 |
| 可观测性 | `atoms/syscall/telemetry.c` | 通过 `agentos_sys_telemetry_*` 调用 |
| 速率限制 | `atoms/syscall/rate_limiter.c` | 通过 syscall 接口调用 |
| 认证授权 | `cupolas/permission/` | 安全穹顶负责 |
| 配置管理 | `commons/utils/config/` | 统一配置库 |
| 平台适配 | `commons/platform/` | 跨平台抽象 |
| 网络工具 | `commons/utils/network/` | 网络连接池等 |
| 健康检查 | `daemon/monit_d/` | 监控守护进程 |
| 服务管理 | `daemon/gateway_d/` | 守护进程层 |

---

## 📁 目录结构

```
gateway/
├── include/
│   └── gateway.h              # 统一公共接口
├── src/
│   ├── gateway/
│   │   ├── gateway.h          # 内部抽象接口
│   │   ├── http_gateway.c     # HTTP 协议转换实现
│   │   ├── http_gateway.h     # HTTP 接口
│   │   ├── ws_gateway.c       # WebSocket 协议转换实现
│   │   ├── ws_gateway.h       # WebSocket 接口
│   │   ├── stdio_gateway.c    # Stdio 协议转换实现
│   │   └── stdio_gateway.h    # Stdio 接口
│   └── utils/
│       ├── jsonrpc.c          # JSON-RPC 2.0 工具实现
│       └── jsonrpc.h          # JSON-RPC 2.0 工具接口
├── tests/
│   └── test_gateway.c         # 单元测试
├── CMakeLists.txt             # 构建配置
└── README.md                  # 本文档
```

---

## 🔌 API 接口

### 创建网关

```c
#include <gateway.h>

/* HTTP 网关 - 监听 8080 端口 */
gateway_t* http_gw = gateway_http_create("0.0.0.0", 8080);

/* WebSocket 网关 - 监听 8081 端口 */
gateway_t* ws_gw = gateway_ws_create("0.0.0.0", 8081);

/* Stdio 网关 - 命令行交互 */
gateway_t* stdio_gw = gateway_stdio_create();
```

### 生命周期管理

```c
/* 启动网关 */
gateway_error_t err = gateway_start(gw);
if (err != GATEWAY_SUCCESS) {
    fprintf(stderr, "Failed to start gateway: %d\n", err);
    return;
}

/* 运行中... */

/* 停止网关 */
gateway_stop(gw);

/* 销毁网关 */
gateway_destroy(gw);
```

### 获取统计信息

```c
char* stats_json = NULL;
gateway_get_stats(gw, &stats_json);
printf("Gateway stats: %s\n", stats_json);
free(stats_json);
```

---

## 📡 支持的系统调用

网关层通过系统调用接口与内核通信，支持以下 syscall：

### 任务管理

| 系统调用 | 说明 |
|----------|------|
| `agentos_sys_task_submit` | 提交任务 |
| `agentos_sys_task_query` | 查询任务状态 |
| `agentos_sys_task_wait` | 等待任务完成 |
| `agentos_sys_task_cancel` | 取消任务 |

### 记忆管理

| 系统调用 | 说明 |
|----------|------|
| `agentos_sys_memory_write` | 写入记忆 |
| `agentos_sys_memory_search` | 搜索记忆 |
| `agentos_sys_memory_get` | 获取记忆 |
| `agentos_sys_memory_delete` | 删除记忆 |

### 会话管理

| 系统调用 | 说明 |
|----------|------|
| `agentos_sys_session_create` | 创建会话 |
| `agentos_sys_session_get` | 获取会话信息 |
| `agentos_sys_session_close` | 关闭会话 |
| `agentos_sys_session_list` | 列出会话 |

### 可观测性

| 系统调用 | 说明 |
|----------|------|
| `agentos_sys_telemetry_metrics` | 获取指标 |
| `agentos_sys_telemetry_traces` | 获取追踪 |

---

## 🔧 HTTP 端点

### JSON-RPC 端点

```
POST /              # JSON-RPC 2.0 请求
GET  /health        # 健康检查
GET  /metrics       # Prometheus 指标导出
OPTIONS /           # CORS 预检
```

### JSON-RPC 请求示例

```json
{
    "jsonrpc": "2.0",
    "method": "agentos_sys_task_submit",
    "params": {
        "input": "分析这份报告并生成摘要",
        "timeout_ms": 30000
    },
    "id": 1
}
```

### JSON-RPC 响应示例

```json
{
    "jsonrpc": "2.0",
    "result": {
        "result": "任务已提交，ID: task_abc123"
    },
    "id": 1
}
```

---

## 🌐 WebSocket 消息协议

### 消息类型

| 类型 | 说明 |
|------|------|
| `ping` | 心跳请求 |
| `pong` | 心跳响应 |
| `rpc_request` | RPC 请求 |
| `rpc_response` | RPC 响应 |
| `notification` | 服务端通知 |
| `error` | 错误消息 |

### 消息格式

```json
{
    "type": "rpc_request",
    "session_id": "sess_abc123",
    "timestamp": 1711926000.123,
    "payload": {
        "jsonrpc": "2.0",
        "method": "agentos_sys_memory_search",
        "params": {
            "query": "项目进度报告",
            "limit": 10
        },
        "id": 1
    }
}
```

---

## 💻 Stdio 命令行接口

### 可用命令

| 命令 | 说明 |
|------|------|
| `help` | 显示帮助信息 |
| `rpc <json>` | 执行 JSON-RPC 调用 |
| `stats` | 显示网关统计 |
| `exit` | 退出网关 |

### 使用示例

```
AgentOS Stdio Gateway started. Type 'help' for available commands.
> rpc {"jsonrpc":"2.0","method":"agentos_sys_session_create","id":1}
{"jsonrpc":"2.0","result":{"session_id":"sess_xyz789"},"id":1}
> stats
{"commands_total":1,"commands_failed":0,"bytes_received":56,"bytes_sent":45}
> exit
Gateway shutting down...
```

---

## 🏗️ 依赖关系

```
gateway/
├── 依赖 atoms/syscall/         # 系统调用接口
│   ├── session.c               # 会话管理
│   ├── telemetry.c             # 可观测性
│   └── rate_limiter.c          # 速率限制
├── 依赖 commons/               # 基础库
│   ├── agentos/                # 公共类型定义
│   └── utils/                  # 工具函数
├── 依赖外部库
│   ├── libmicrohttpd           # HTTP 服务器
│   ├── libwebsockets           # WebSocket 服务器
│   └── cJSON                   # JSON 解析
└── 被 daemon/gateway_d/        # 守护进程调用
```

---

## ⚙️ 编译

### 依赖安装

```bash
# Ubuntu/Debian
sudo apt install libmicrohttpd-dev libwebsockets-dev libcjson-dev

# macOS
brew install libmicrohttpd libwebsockets cjson

# Windows (vcpkg)
vcpkg install libmicrohttpd libwebsockets cjson
```

### 构建

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel $(nproc)
```

### 测试

```bash
cmake --build . --target test_gateway
./test_gateway
```

---

## 📊 性能指标

| 指标 | 数值 | 说明 |
|------|------|------|
| HTTP 请求延迟 | < 5ms | 不含 syscall 处理时间 |
| WebSocket 消息延迟 | < 3ms | 单向消息处理 |
| 并发连接数 | 1000+ | HTTP 连接池 |
| 请求解析 | < 1ms | JSON-RPC 验证 |
| 内存占用 | < 10MB | 空闲状态 |

---

## 📜 重构历史

### v2.0.0 (2026-04-01)

**重大重构**：删除所有重复功能，恢复正确的架构定位。

**删除的文件**（16 个，重复/错位实现）：

| 文件 | 原因 | 正确归属 |
|------|------|----------|
| `session.c/h` | 重复实现 | `atoms/syscall/session.c` |
| `session_stats.c` | 依赖已删除模块 | - |
| `telemetry.c/h` | 重复实现 | `atoms/syscall/telemetry.c` |
| `ratelimit.c/h` | 重复实现 | `atoms/syscall/rate_limiter.c` |
| `auth.c/h` | 错位功能 | `cupolas/permission/` |
| `config.c/h` | 错位功能 | `commons/utils/config/` |
| `config_runtime.c` | 依赖已删除模块 | - |
| `platform.c/h` | 错位功能 | `commons/platform/` |
| `connection_pool.c/h` | 错位功能 | `commons/utils/network/` |
| `health.c/h` | 错位功能 | `daemon/monit_d/` |
| `server.c/h` | 错位功能 | `daemon/gateway_d/` |
| `main.c` | 错位功能 | `daemon/gateway_d/` |
| `agentos.c/h` | 错位功能 | `commons/agentos/` |
| `logger.h` | 错位功能 | `commons/utils/logging/` |
| `utils/hash.c/h` | 错位功能 | `commons/utils/` |
| `utils/secure_mem.c/h` | 错位功能 | `commons/utils/` |

**保留的文件**（8 个，核心功能）：

| 文件 | 功能 |
|------|------|
| `include/gateway.h` | 统一公共接口 |
| `src/gateway/gateway.h` | 内部抽象接口 |
| `src/gateway/http_gateway.c/h` | HTTP 协议转换 |
| `src/gateway/ws_gateway.c/h` | WebSocket 协议转换 |
| `src/gateway/stdio_gateway.c/h` | Stdio 协议转换 |
| `src/utils/jsonrpc.c/h` | JSON-RPC 工具 |

**新增**：

- `daemon/gateway_d/` - 用户态守护进程服务

---

## 🏛️ 架构原则遵循

本模块严格遵循 `ARCHITECTURAL_PRINCIPLES.md` 中定义的五维正交系统原则：

### 系统观 (S-1~S-4)

- **S-2 层次分解**: 只负责协议转换，不越级访问内核
- **S-3 总体设计部**: 只做协议转换，不做业务执行

### 内核观 (K-1~K-4)

- **K-1 微内核化**: 保持单一职责，代码精简
- **K-2 接口约束**: 所有公共 API 都有 Doxygen 注释

### 工程观 (E-1~E-8)

- **E-1 安全内生**: 通过 syscall 接口隔离内核
- **E-2 可观测性**: 支持指标导出和链路追踪
- **E-4 跨平台一致**: 支持 Linux/macOS/Windows

### 设计美学 (A-1~A-4)

- **A-1 简约至上**: 只做协议转换，不做其他
- **A-2 细节极致**: 完整的 Doxygen 注释和错误处理

---

## 📄 许可证

Copyright (C) 2026 SPHARX. All Rights Reserved.

SPDX-License-Identifier: Apache-2.0

---

<div align="center">

*From data intelligence emerges.*

**[返回主目录](../README.md)**

</div>
