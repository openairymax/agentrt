# Gateway — 网关层

`agentos/gateway/` 是 AgentOS 的网关层，负责外部协议与内部 JSON-RPC 2.0 协议之间的转换，是连接外部客户端与内核服务的桥梁。

## 设计目标

- **协议转换**：将 HTTP/WebSocket/Stdio 请求转换为内部 JSON-RPC 2.0 调用
- **连接管理**：管理客户端长连接、WebSocket 会话和并发请求
- **安全防护**：TLS 终止、请求鉴权、速率限制、CORS 策略
- **高并发**：基于 libmicrohttpd 和 libwebsockets 的事件驱动架构

## 架构

```
外部客户端
  │
  ├─ HTTP REST ───→ gateway_svc_adapter ──→ JSON-RPC 2.0 ──→ 后端服务
  ├─ WebSocket ──→ gateway_svc_adapter ──→ JSON-RPC 2.0 ──→ 后端服务
  └─ Stdio ──────→ gateway_svc_adapter ──→ JSON-RPC 2.0 ──→ 后端服务
                         │
                    gateway_service
                    (路由/限流/鉴权)
                         │
                    IPC Service Bus
```

## 核心组件

| 组件 | 文件 | 职责 |
|------|------|------|
| 服务适配器 | `gateway_svc_adapter.c/h` | 外部协议解析与内部协议转换 |
| 核心服务 | `gateway_service.c/h` | 请求路由、限流、鉴权、监控 |
| 主程序 | `gateway_main.c` | 服务入口、组件初始化和关闭 |

## API 接口

| 函数 | 说明 |
|------|------|
| `gateway_service_init` | 初始化网关服务 |
| `gateway_service_start` | 启动网关监听 |
| `gateway_service_stop` | 停止网关服务 |
| `svc_adapter_register` | 注册协议适配器 |
| `svc_adapter_convert` | 协议转换 |
| `router_dispatch` | 请求路由分发 |
| `rate_limiter_check` | 速率限制检查 |
| `auth_middleware` | 鉴权中间件 |
| `monitor_collect` | 监控指标采集 |

## 使用方式

```bash
# 启动网关
./gateway --config config.json

# HTTP 端口
./gateway --port 8080

# HTTPS 端口
./gateway --tls-port 8443 --cert server.crt --key server.key

# 调试模式
./gateway --verbose
```

## 配置示例

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
        "cors": {
            "allowed_origins": ["*"],
            "allowed_methods": ["GET", "POST", "OPTIONS"]
        }
    }
}
```

## 依赖组件

| 组件 | 版本 | 用途 |
|------|------|------|
| libmicrohttpd | ≥ 0.9.70 | HTTP 服务器 |
| libwebsockets | ≥ 4.3.0 | WebSocket 支持 |
| cJSON | ≥ 1.7.15 | JSON 解析 |
| OpenSSL | ≥ 1.1.1 | TLS/SSL |

---

*AgentOS Gateway*
