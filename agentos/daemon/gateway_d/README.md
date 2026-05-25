# Gateway Daemon — API 网关守护进程

> **Version**: AgentOS v0.0.5 | **BAN-12**: 依赖由根 CMakeLists.txt 集中检测 | **BAN-33**: 遵循源外构建规则

`daemon/gateway_d/` 是 AgentOS 的 API 网关守护进程，负责将外部客户端的 HTTP/WebSocket/MCP 请求转换为内部 JSON-RPC 2.0 调用，并路由到对应的服务守护进程。

## 核心职责

- **协议转换**：HTTP/WebSocket/MCP → JSON-RPC 2.0 内部协议
- **请求路由**：根据请求路径和方法分发到对应的服务守护进程
- **连接管理**：管理客户端长连接和 WebSocket 会话
- **限流熔断**：请求速率限制和服务熔断保护
- **TLS 终止**：HTTPS 连接管理与证书维护

## 适配器架构

```
外部请求 (HTTP/WS/MCP)
       ↓
  gateway_svc_adapter  ← 请求解析与协议转换
       ↓
  gateway_service      ← 核心服务逻辑（路由、限流、鉴权）
       ↓
  IPC Service Bus     ← 通过 JSON-RPC 2.0 转发到后端服务
```

> **MCP 协议支持**: gateway_d 从 v0.0.5 起支持 Model Context Protocol (MCP)，允许 MCP 客户端通过网关与 AgentOS 后端服务交互。MCP 请求在适配器层被转换为标准 JSON-RPC 2.0 调用。

## 使用方式

```bash
# 启动网关守护进程
./gateway_d --config config.json

# 指定监听端口
./gateway_d --port 8080 --tls-port 8443

# 调试模式
./gateway_d --verbose --log-level debug
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
        "services": {
            "llm": "unix:///tmp/llm_d.sock",
            "tool": "unix:///tmp/tool_d.sock",
            "sched": "unix:///tmp/sched_d.sock"
        }
    }
}
```

---

*AgentOS Daemon — Gateway Daemon*
