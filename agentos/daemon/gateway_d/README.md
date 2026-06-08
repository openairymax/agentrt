# Gateway Daemon — API 网关守护进程

> **模块路径**: `agentos/daemon/gateway_d/` | **版本**: v0.0.5

## 概述

`daemon/gateway_d/` 是 AgentOS 的 API 网关守护进程，作为整个系统的流量入口，负责将外部客户端的 HTTP/WebSocket/Stdio 请求转换为内部 JSON-RPC 2.0 调用，并路由到对应的服务守护进程。它是外部世界与 AgentOS 内部服务之间的唯一入口点，承担协议转换、请求路由、连接管理、限流熔断和 TLS 终止等核心职责。

### 架构定位

```
gateway_d/ → agentos/gateway/ → agentos/atoms/syscall/
    ↑           ↑
 守护进程    协议转换层
```

### 核心职责

- **协议转换**：HTTP/WebSocket/Stdio/MCP/A2A/OpenAI API → JSON-RPC 2.0 内部协议
- **请求路由**：根据请求路径和方法分发到对应的服务守护进程
- **连接管理**：管理客户端长连接和 WebSocket 会话
- **限流熔断**：请求速率限制和服务熔断保护
- **TLS 终止**：HTTPS 连接管理与证书维护
- **MCP 协议支持**：允许 MCP 客户端通过网关与 AgentOS 后端服务交互
- **A2A 协议支持**：Agent-to-Agent 通信协议处理
- **OpenAI API 兼容**：支持 OpenAI API 格式的请求直接接入

## 目录结构

```
gateway_d/
├── CMakeLists.txt                        # 构建配置
├── README.md                             # 本文件
├── include/                              # 公共头文件
│   ├── gateway_service.h                 # 网关服务接口定义
│   └── gateway_svc_adapter.h             # 网关服务适配器接口
├── src/                                  # 实现文件
│   ├── main.c                            # 守护进程入口
│   ├── service.c                         # 网关服务核心实现
│   ├── gateway_svc_adapter.c             # 请求解析与协议转换适配器
│   └── protocol/                         # 协议处理模块
│       ├── gateway_protocol_router.h     # 协议路由器头文件
│       ├── gateway_protocol_router.c     # 多协议路由分发
│       ├── gateway_mcp_server.h          # MCP 服务器头文件
│       ├── gateway_mcp_server.c          # MCP 协议处理
│       ├── gateway_a2a_handler.h         # A2A 处理器头文件
│       ├── gateway_a2a_handler.c         # A2A 协议处理
│       ├── gateway_openai_compat.h       # OpenAI 兼容层头文件
│       └── gateway_openai_compat.c       # OpenAI API 兼容处理
└── tests/                                # 单元测试
    ├── CMakeLists.txt
    └── test_service.c                    # 服务测试
```

## 核心组件说明

### 网关服务（gateway_service）

网关服务的核心实现，管理 HTTP/WS/Stdio 三种网关实例的生命周期：

| 网关类型 | 枚举值 | 说明 |
|----------|--------|------|
| HTTP | `GATEWAY_DAEMON_TYPE_HTTP` | HTTP/HTTPS 请求接入 |
| WebSocket | `GATEWAY_DAEMON_TYPE_WS` | WebSocket 长连接接入 |
| Stdio | `GATEWAY_DAEMON_TYPE_STDIO` | 标准输入/输出接入（CLI 模式） |

### 协议路由器（gateway_protocol_router）

多协议路由分发器，根据请求的协议类型将请求路由到对应的协议处理器：

| 协议 | 处理器 | 说明 |
|------|--------|------|
| MCP | `gateway_mcp_server` | Model Context Protocol，允许 MCP 客户端与 AgentOS 交互 |
| A2A | `gateway_a2a_handler` | Agent-to-Agent Protocol，Agent 间通信 |
| OpenAI API | `gateway_openai_compat` | OpenAI API 兼容层，支持 Chat/Completions 等端点 |

### 服务适配器（gateway_svc_adapter）

请求解析与协议转换层，将外部请求标准化为内部 JSON-RPC 2.0 格式：

```
外部请求 (HTTP/WS/MCP/A2A/OpenAI)
       ↓
  gateway_svc_adapter  ← 请求解析与协议转换
       ↓
  gateway_service      ← 核心服务逻辑（路由、限流、鉴权）
       ↓
  IPC Service Bus      ← 通过 JSON-RPC 2.0 转发到后端服务
```

## 接口说明

### 网关服务生命周期（gateway_service.h）

```c
agentos_error_t gateway_service_create(gateway_service_t *service,
                                       const gateway_service_config_t *config);
void gateway_service_destroy(gateway_service_t service);
agentos_error_t gateway_service_init(gateway_service_t service);
agentos_error_t gateway_service_start(gateway_service_t service);
agentos_error_t gateway_service_stop(gateway_service_t service, bool force);
```

### 网关服务状态查询

```c
agentos_svc_state_t gateway_service_get_state(gateway_service_t service);
bool gateway_service_is_running(gateway_service_t service);
agentos_error_t gateway_service_get_stats(gateway_service_t service,
                                          agentos_svc_stats_t *stats);
agentos_error_t gateway_service_healthcheck(gateway_service_t service);
```

### 网关配置管理

```c
agentos_error_t gateway_service_load_config(gateway_service_config_t *config,
                                            const char *config_path);
void gateway_service_get_default_config(gateway_service_config_t *config);
```

### 配置结构体

```c
typedef struct {
    gateway_daemon_type_t type;   // 网关类型
    const char *host;             // 监听地址
    uint16_t port;                // 监听端口
    bool enabled;                 // 是否启用
    size_t max_request_size;      // 最大请求大小
    uint32_t timeout_ms;          // 超时时间
} gateway_daemon_config_t;

typedef struct {
    const char *name;
    const char *version;
    gateway_daemon_config_t http;   // HTTP 网关配置
    gateway_daemon_config_t ws;     // WebSocket 网关配置
    gateway_daemon_config_t stdio;  // Stdio 网关配置
    bool enable_metrics;
    bool enable_tracing;
    uint32_t shutdown_timeout_ms;
} gateway_service_config_t;
```

## 通信方式

| 方向 | 协议 | 说明 |
|------|------|------|
| 入站 | HTTP/HTTPS | REST API 请求 |
| 入站 | WebSocket | 长连接双向通信 |
| 入站 | Stdio | 命令行交互模式 |
| 入站 | MCP | Model Context Protocol |
| 入站 | A2A | Agent-to-Agent Protocol |
| 入站 | OpenAI API | OpenAI 兼容 API |
| 出站 | JSON-RPC 2.0 | 通过 IPC Service Bus 转发到后端服务 |

## 依赖关系

```
gateway_d
├── common (svc_common, svc_logger, svc_config, svc_auth, ipc_service_bus,
│           method_dispatcher, jsonrpc_helpers, circuit_breaker)
├── agentos/gateway/       # 协议转换层
└── agentos/atoms/syscall/ # 系统调用接口
```

## 构建说明

```bash
# 构建网关守护进程（包含在 daemon 顶层构建中）
cmake -B build -DBUILD_TESTS=ON
cmake --build build --target agentos-gateway-d

# 运行网关测试
ctest --test-dir build -R "test_service" -V
```

## 使用示例

### 启动网关守护进程

```bash
# 启动网关守护进程
./gateway_d --config config.json

# 指定监听端口
./gateway_d --port 8080 --tls-port 8443

# 调试模式
./gateway_d --verbose --log-level debug
```

### 配置示例

```json
{
    "gateway": {
        "http_port": 8080,
        "https_port": 8443,
        "tls_cert": "/etc/agentos/certs/server.crt",
        "tls_key": "/etc/agentos/certs/server.key",
        "rate_limit": {
            "requests_per_second": 1000,
            "burst": 2000
        },
        "services": {
            "llm": "unix:///tmp/llm_d.sock",
            "tool": "unix:///tmp/tool_d.sock",
            "sched": "unix:///tmp/sched_d.sock"
        }
    }
}
```

---

© 2026 SPHARX Ltd. All Rights Reserved.
