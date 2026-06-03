# Gateway — 协议网关层

**模块路径**: `agentos/gateway/`
**版本**: v0.1.0

## 概述

Gateway 是 AgentOS 的协议网关层，负责将外部客户端的 HTTP、WebSocket、Stdio 请求统一转换为内部 JSON-RPC 2.0 协议调用，是连接外部世界与 AgentOS 内核服务的桥梁。网关层遵循 **K-1 内核极简** 原则——只做协议翻译，零业务逻辑，所有业务逻辑通过 `agentos/atoms/syscall` 接口调用。

架构定位：

```
agentos/daemon/gateway_d/ --> agentos/gateway/ --> agentos/atoms/syscall/
                      ^
                 协议转换层
```

## 设计目标

- **协议转换**：将 HTTP / WebSocket / Stdio 请求转换为内部 JSON-RPC 2.0 调用
- **多协议支持**：支持 JSON-RPC、MCP、A2A、OpenAI API 等多种协议的自适应检测与转换
- **连接管理**：管理客户端长连接、WebSocket 会话和并发请求
- **安全防护**：基于令牌桶的速率限制、请求鉴权、CORS 策略
- **高并发**：基于 libmicrohttpd 和 libwebsockets 的事件驱动架构
- **可扩展**：支持动态端点注册、自定义请求处理器注入

## 目录结构

```
gateway/
├── include/                          # 公共头文件
│   ├── gateway.h                     # 网关统一公共接口（生命周期、控制、查询）
│   └── gateway_protocol_bridge.h     # 网关与协议系统的桥接层接口
├── src/
│   ├── gateway/                      # 核心网关实现
│   │   ├── gateway.h                 # 内部网关头文件（ops 表、结构体定义）
│   │   ├── gateway_internal.h        # 内部类型与函数声明
│   │   ├── gateway_api.c             # 公共 API 实现（create/start/stop/destroy）
│   │   ├── http_gateway.h            # HTTP 网关头文件
│   │   ├── http_gateway.c            # HTTP 网关实现（基于 libmicrohttpd）
│   │   ├── http_gateway_routes.h     # HTTP 路由表头文件
│   │   ├── http_gateway_routes.c     # HTTP 静态路由表定义
│   │   ├── ws_gateway.h              # WebSocket 网关头文件
│   │   ├── ws_gateway.c              # WebSocket 网关实现（基于 libwebsockets）
│   │   ├── stdio_gateway.h           # Stdio 网关头文件
│   │   ├── stdio_gateway.c           # Stdio 网关实现（REPL 交互模式）
│   │   └── gateway_protocol_bridge.c # 协议桥接层实现
│   └── utils/                        # 工具模块
│       ├── jsonrpc.h                 # JSON-RPC 2.0 协议工具函数
│       ├── jsonrpc.c                 # JSON-RPC 2.0 请求验证、响应生成、批量处理
│       ├── syscall_router.h          # 系统调用路由器接口
│       ├── syscall_router.c          # JSON-RPC 方法到 syscall 的路由分发
│       ├── gateway_rpc_handler.h     # 统一 RPC 请求处理模块
│       ├── gateway_rpc_handler.c     # HTTP/WS/Stdio 共享的 RPC 处理逻辑
│       ├── gateway_protocol_handler.h # 多协议网关请求处理器
│       ├── gateway_protocol_handler.c # MCP/A2A/OpenAI 协议自适应处理
│       ├── gateway_rate_limiter.h    # 速率限制器接口
│       ├── gateway_rate_limiter.c    # 基于令牌桶的速率限制实现
│       ├── gateway_utils.h           # 通用工具宏与内联函数
│       ├── gateway_compat.h          # 兼容性定义
│       └── mcp_server.h / .c         # MCP 协议服务端实现
├── tests/                            # 测试与基准
│   ├── CMakeLists.txt                # 测试构建配置
│   ├── test_gateway.c                # 网关主测试（7 个用例）
│   ├── test_jsonrpc.c                # JSON-RPC 协议测试（17 个用例）
│   ├── test_syscall_router.c         # 系统调用路由测试（8 个用例）
│   ├── test_gateway_rpc_handler.c    # RPC 处理模块测试（14 个用例）
│   └── gateway_benchmark.c           # 性能基准测试工具
├── docker/                           # Docker 部署配置
│   ├── Dockerfile                    # 容器构建文件
│   ├── docker-compose.yml            # 开发环境编排
│   ├── docker-compose.dev.yml        # 开发调试编排
│   ├── docker-compose.prod.yml       # 生产环境编排
│   ├── .env.example                  # 环境变量示例
│   └── monitoring/                   # 监控配置
│       ├── prometheus.yml            # Prometheus 采集配置
│       ├── alerts.yml                # 告警规则
│       └── grafana_agentos_dashboard.json  # Grafana 仪表盘
├── deploy/                           # K8s 部署配置
│   ├── k8s/                          # Kubernetes 资源清单
│   │   ├── namespace.yaml            # 命名空间定义
│   │   ├── configmap.yaml            # 多环境 ConfigMap
│   │   ├── deployment.yaml           # Deployment + HPA
│   │   └── service.yaml              # Service + Ingress + NetworkPolicy
│   └── README.md                     # 部署指南
├── config/                           # 静态分析配置
│   └── cppcheck.cfg                  # cppcheck 规则
├── CMakeLists.txt                    # CMake 构建配置
└── README.md                         # 本文件
```

## 核心组件

### 网关实例（Gateway Instance）

采用 **ops 虚表** 设计模式，三种网关类型共享统一接口：

| 组件 | 文件 | 职责 |
|------|------|------|
| 公共接口 | `include/gateway.h` | 统一生命周期 API：create / start / stop / destroy |
| HTTP 网关 | `src/gateway/http_gateway.c` | 基于 libmicrohttpd 的 HTTP 服务器，支持动态端点注册 |
| WebSocket 网关 | `src/gateway/ws_gateway.c` | 基于 libwebsockets 的双向 RPC 通信 |
| Stdio 网关 | `src/gateway/stdio_gateway.c` | 标准输入输出的 REPL 交互模式，阻塞式单线程 |
| 协议桥接 | `src/gateway/gateway_protocol_bridge.c` | Gateway ↔ Protocols 模块桥接，支持协议自动检测 |

### 工具模块

| 组件 | 文件 | 职责 |
|------|------|------|
| JSON-RPC 2.0 | `src/utils/jsonrpc.c` | 请求验证、响应生成、批量请求、通知支持 |
| 系统调用路由 | `src/utils/syscall_router.c` | JSON-RPC 方法名到 syscall 函数的路由分发 |
| RPC 处理器 | `src/utils/gateway_rpc_handler.c` | 三种网关共享的统一 RPC 处理逻辑 |
| 多协议处理器 | `src/utils/gateway_protocol_handler.c` | MCP/A2A/OpenAI 协议自适应检测与转换 |
| 速率限制器 | `src/utils/gateway_rate_limiter.c` | 基于令牌桶算法，支持按 IP/API Key 限流 |
| MCP 服务器 | `src/utils/mcp_server.c` | MCP 协议服务端，支持工具/资源/提示词注册 |

## 架构

```
外部客户端
  │
  ├─ HTTP REST ───→ http_gateway ──→ JSON-RPC 2.0 ──→ syscall_router ──→ 后端服务
  ├─ WebSocket ──→ ws_gateway ────→ JSON-RPC 2.0 ──→ syscall_router ──→ 后端服务
  ├─ Stdio ──────→ stdio_gateway ─→ JSON-RPC 2.0 ──→ syscall_router ──→ 后端服务
  └─ MCP ────────→ mcp_server ────→ JSON-RPC 2.0 ──→ syscall_router ──→ 后端服务
                         │
              gateway_protocol_handler
              (协议检测/转换/统一处理)
                         │
              gateway_rpc_handler
              (统一 RPC 处理逻辑)
                         │
                  IPC Service Bus
```

## 接口说明

### 生命周期 API

| 函数 | 说明 |
|------|------|
| `gateway_http_create(host, port)` | 创建 HTTP 网关实例 |
| `gateway_ws_create(host, port)` | 创建 WebSocket 网关实例 |
| `gateway_stdio_create()` | 创建 Stdio 网关实例 |
| `gateway_destroy(gw)` | 销毁网关实例并释放资源 |
| `gateway_start(gw)` | 启动网关（HTTP/WS 非阻塞，Stdio 阻塞） |
| `gateway_stop(gw)` | 优雅停止网关 |

### 控制与查询 API

| 函数 | 说明 |
|------|------|
| `gateway_set_handler(gw, handler, user_data)` | 设置自定义请求处理回调 |
| `gateway_register_endpoint(gw, method, path, handler, user_data)` | 注册动态 HTTP 端点 |
| `gateway_get_type(gw)` | 获取网关类型枚举 |
| `gateway_is_running(gw)` | 检查网关是否运行中 |
| `gateway_get_stats(gw, out_json)` | 获取 JSON 格式统计信息 |
| `gateway_get_name(gw)` | 获取网关名称 |

### 错误码

| 枚举值 | 说明 |
|--------|------|
| `GATEWAY_SUCCESS` | 成功 (0) |
| `GATEWAY_ERROR_INVALID` | 无效参数 (-1) |
| `GATEWAY_ERROR_MEMORY` | 内存不足 (-2) |
| `GATEWAY_ERROR_IO` | I/O 错误 (-3) |
| `GATEWAY_ERROR_TIMEOUT` | 超时 (-4) |
| `GATEWAY_ERROR_CLOSED` | 连接已关闭 (-5) |
| `GATEWAY_ERROR_PROTOCOL` | 协议错误 (-6) |

### JSON-RPC 2.0 工具函数

| 函数 | 说明 |
|------|------|
| `jsonrpc_validate_request(json)` | 验证请求格式（必需字段 + 版本检查） |
| `jsonrpc_get_method(json)` | 提取方法名 |
| `jsonrpc_get_params(json)` | 提取参数对象 |
| `jsonrpc_create_success_response(id, result)` | 创建成功响应 |
| `jsonrpc_create_error_response(id, code, msg, data)` | 创建错误响应 |
| `jsonrpc_validate_batch_request(batch, count)` | 验证批量请求（最大 64 项） |
| `jsonrpc_process_batch(batch, handler, user_data)` | 处理批量请求 |
| `jsonrpc_create_notification(method, params)` | 创建通知（无 id 字段） |

### 速率限制器 API

| 函数 | 说明 |
|------|------|
| `gateway_rate_limiter_create(config)` | 创建速率限制器 |
| `gateway_rate_limiter_destroy(limiter)` | 销毁速率限制器 |
| `gateway_rate_limiter_allow(limiter, client_key)` | 检查请求是否允许 |
| `gateway_rate_limiter_get_stats(...)` | 获取限制状态统计 |
| `gateway_rate_limiter_reset_client(limiter, key)` | 重置指定客户端计数 |

### MCP 服务器 API

| 函数 | 说明 |
|------|------|
| `mcp_server_create(config)` | 创建 MCP 服务器实例 |
| `mcp_server_register_tool(...)` | 注册工具处理器 |
| `mcp_server_register_resource(...)` | 注册资源处理器 |
| `mcp_server_register_prompt(...)` | 注册提示词处理器 |
| `mcp_server_handle_request(...)` | 处理 MCP 请求 |

## 条件编译

| 依赖 | 条件宏 | 缺失时行为 |
|------|--------|-----------|
| cJSON | `AGENTOS_HAS_CJSON` | 整个 Gateway 模块跳过编译 |
| libmicrohttpd | `AGENTOS_HAS_MICROHTTPD` | HTTP 网关不可用 |
| libwebsockets | `AGENTOS_HAS_LIBWEBSOCKETS` | WebSocket 网关不可用 |
| libcurl | `AGENTOS_HAS_CURL` | 基准测试使用模拟模式 |

## 依赖关系

| 组件 | 版本 | 用途 |
|------|------|------|
| libmicrohttpd | ≥ 0.9.70 | HTTP 服务器 |
| libwebsockets | ≥ 4.3.0 | WebSocket 支持 |
| cJSON | ≥ 1.7.15 | JSON 解析（必需，缺失则跳过整个模块） |
| OpenSSL | ≥ 1.1.1 | TLS/SSL |
| agentos_protocols | 内部 | 协议路由与转换 |
| agentos_common | 内部 | 公共工具库 |
| agentos_memory | 内部 | 内存管理 |

## 构建说明

```bash
# 构建 Gateway 模块（需要 cJSON 开发库）
mkdir build && cd build
cmake .. -DBUILD_TESTS=ON -DBUILD_BENCHMARK=ON
make gateway

# 运行测试
ctest --test-dir build -R "gateway|jsonrpc|syscall|rpc_handler"

# 运行性能基准
./build/gateway_benchmark

# 静态分析
make cppcheck

# 代码格式化
make format
```

## 使用示例

### 创建并启动 HTTP 网关

```c
#include "gateway.h"

int main(void) {
    gateway_t *gw = gateway_http_create("0.0.0.0", 8080);
    if (!gw) return 1;

    gateway_start(gw);

    char *stats = NULL;
    gateway_get_stats(gw, &stats);
    printf("Stats: %s\n", stats);
    free(stats);

    gateway_stop(gw);
    gateway_destroy(gw);
    return 0;
}
```

### 注册自定义请求处理器

```c
int my_handler(const char *request_json, char **response_json, void *user_data) {
    *response_json = strdup("{\"jsonrpc\":\"2.0\",\"result\":\"ok\",\"id\":1}");
    return 0;
}

gateway_set_handler(gw, my_handler, NULL);
```

### 注册动态 HTTP 端点

```c
int metrics_handler(const gateway_endpoint_request_t *req,
                    gateway_endpoint_response_t *resp) {
    resp->status_code = 200;
    resp->content_type = "text/plain";
    resp->body = strdup("requests_total 12345");
    resp->body_len = strlen(resp->body);
    return 0;
}

gateway_register_endpoint(gw, "GET", "/metrics", metrics_handler, NULL);
```

## 配置示例

```json
{
    "gateway": {
        "http_port": 8080,
        "ws_port": 8081,
        "metrics_port": 9090,
        "tls_cert": "/etc/agentos/certs/server.crt",
        "tls_key": "/etc/agentos/certs/server.key",
        "rate_limit": {
            "enabled": true,
            "requests_per_second": 1000,
            "requests_per_minute": 50000,
            "burst_size": 2000
        },
        "cors": {
            "allowed_origins": ["*"],
            "allowed_methods": ["GET", "POST", "OPTIONS"]
        }
    }
}
```

---

© 2026 SPHARX Ltd. All Rights Reserved.
