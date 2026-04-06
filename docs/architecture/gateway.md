Copyright (c) 2026 SPHARX. All Rights Reserved.
"From data intelligence emerges."

# AgentOS Gateway 网关系统

**版本**: 1.0.0
**最后更新**: 2026-04-06
**状态**: 生产就绪
**核心文档**: [Gateway 源码](../../agentos/gateway/)
**支持协议**: HTTP/1.1, WebSocket, stdio (标准输入输出)

---

## 📋 概述

Gateway 是 AgentOS 的**统一入口网关层**，负责协议转换、请求路由、负载均衡、限流熔断等核心功能。基于实际源码实现，Gateway 支持三种传输协议：

| 协议 | 实现文件 | 适用场景 |
|------|----------|----------|
| **HTTP/REST** | [http_gateway.c](../../agentos/gateway/src/gateway/http_gateway.c) | Web API、外部集成 |
| **WebSocket** | [ws_gateway.c](../../agentos/gateway/src/gateway/ws_gateway.c) | 实时通信、流式响应 |
| **stdio** | [stdio_gateway.c](../../agentos/gateway/src/gateway/stdio_gateway.c) | CLI 工具、管道处理 |

---

## 🏗️ 架构设计

```
┌─────────────────────────────────────────────────────────────┐
│                     Client / External System                │
└───────────────────────────┬─────────────────────────────────┘
                            │
          ┌─────────────────┼─────────────────┐
          ▼                 ▼                 ▼
   ┌──────────────┐  ┌──────────────┐  ┌──────────────┐
   │  HTTP Gateway │  │  WS Gateway   │  │ Stdio Gateway│
   │  :18789/http │  │ :18789/ws    │  │ stdin/stdout │
   └──────┬───────┘  └──────┬───────┘  └──────┬───────┘
          └─────────────────┼─────────────────┘
                            ▼
            ┌───────────────────────────────┐
            │      Core Router Engine        │
            │  ┌─────────────────────────┐  │
            │  │  JSON-RPC Handler        │  │
            │  │  (jsonrpc.c)            │  │
            │  └─────────────────────────┘  │
            │  ┌─────────────────────────┐  │
            │  │  Syscall Router         │  │
            │  │  (syscall_router.c)     │  │
            │  └─────────────────────────┘  │
            │  ┌─────────────────────────┐  │
            │  │  Rate Limiter           │  │
            │  │  (gateway_rate_limiter.c)│ │
            │  └─────────────────────────┘  │
            └──────────────┬────────────────┘
                           ▼
        ┌──────────────────────────────────────────┐
        │           AgentOS Kernel (IPC)          │
        │     atoms/corekern → atoms/syscall       │
        └──────────────────────────────────────────┘
```

---

## 🔌 核心组件详解

### 1. HTTP Gateway ([http_gateway.c](../../agentos/gateway/src/gateway/http_gateway.c))

**功能**: RESTful API 网关，处理 HTTP/HTTPS 请求

#### 路由规则（来自 http_gateway_routes.c）

```c
// 核心路由表（简化示例）
static const route_entry_t routes[] = {
    // 健康检查
    { "GET",  "/api/v1/health",        &handle_health_check },
    { "GET",  "/api/v1/metrics",       &handle_metrics },

    // 任务管理（Syscall 路由）
    { "POST", "/api/v1/tasks",         &handle_task_submit },
    { "GET",  "/api/v1/tasks/{id}",    &handle_task_query },
    { "DELETE", "/api/v1/tasks/{id}", &handle_task_cancel },

    // 记忆管理
    { "POST", "/api/v1/memory/store",  &handle_memory_write },
    { "POST", "/api/v1/memory/search", &handle_memory_search },
    { "GET",  "/api/v1/memory/{id}",   &handle_memory_get },

    // 会话管理
    { "POST", "/api/v1/sessions",      &handle_session_create },
    { "GET",  "/api/v1/sessions/{id}", &handle_session_get },
    { "DELETE","/api/v1/sessions/{id}",&handle_session_close },

    // Agent 管理
    { "POST", "/api/v1/agents",        &handle_agent_spawn },
    { "DELETE","/api/v1/agents/{id}",  &handle_agent_terminate },
    { "POST", "/api/v1/agents/{id}/invoke", &handle_agent_invoke },

    // 终止符
    { NULL, NULL, NULL }
};
```

#### HTTP 响应格式

```json
{
    "success": true,
    "data": {
        // 具体业务数据
    },
    "message": "Operation completed",
    "request_id": "req-1678901234-000001"
}
```

### 2. WebSocket Gateway ([ws_gateway.c](../../agentos/gateway/src/gateway/ws_gateway.c))

**功能**: 双向实时通信，适用于流式 LLM 输出、实时监控

#### 连接生命周期

```javascript
// 客户端连接示例
const ws = new WebSocket('ws://localhost:18789/ws');

ws.onopen = () => {
    // 发送认证信息
    ws.send(JSON.stringify({
        type: 'auth',
        token: 'your-jwt-token'
    }));

    // 提交流式任务
    ws.send(JSON.stringify({
        type: 'task',
        action: 'chat_stream',
        payload: {
            messages: [{ role: 'user', content: 'Hello!' }],
            stream: true
        }
    }));
};

ws.onmessage = (event) => {
    const data = JSON.parse(event.data);

    switch (data.type) {
        case 'chunk':
            // 流式 Token 片段
            process.stdout.write(data.content);
            break;
        case 'complete':
            // 任务完成
            console.log('\n[Complete]', data.result);
            break;
        case 'error':
            console.error('[Error]', data.error);
            break;
    }
};
```

#### 服务端事件类型

```c
typedef enum {
    WS_EVENT_AUTH_RESULT,      // 认证结果
    WS_EVENT_TASK_SUBMITTED,   // 任务已提交
    WS_EVENT_CHUNK,            // 数据块（流式）
    WS_EVENT_COMPLETE,         // 完成
    WS_EVENT_ERROR,            // 错误
    WS_EVENT_HEARTBEAT,        // 心跳
    WS_EVENT_STATUS_UPDATE     // 状态更新
} ws_event_type_t;
```

### 3. Stdio Gateway ([stdio_gateway.c](../../agentos/gateway/src/gateway/stdio_gateway.c))

**功能**: 标准输入输出网关，用于 CLI 工具和管道集成

#### 使用场景

```bash
# 场景 1: CLI 直接调用
echo '{"action":"chat","message":"Hello"}' | agentos-gateway --protocol stdio

# 场景 2: 管道处理
cat requests.json | agentos-gateway --protocol stdio --batch

# 场景 3: 与其他工具链组合
generate_prompt | agentos-gateway --protocol stdio | llm-service | format-output
```

#### 输入/输出格式

```json
// stdin 输入（JSON Lines 格式）
{"id": "req-001", "method": "task.submit", "params": {"input": "Hello"}}

// stdout 输出（JSON Lines 格式）
{"id": "req-001", "result": {"task_id": "task-abc123"}, "error": null}
{"id": "req-002", "result": {"output": "Hello! How can I help?"}, "error": null}
```

---

## ⚡ 高级功能

### 速率限制器 ([gateway_rate_limiter.c](../../agentos/gateway/src/utils/gateway_rate_limiter.c))

```c
// 令牌桶算法实现
typedef struct {
    double rate;              // 每秒生成令牌数
    double burst;             // 桶容量
    double tokens;            // 当前令牌数
    time_t last_update;       // 上次更新时间
} rate_limiter_t;

// 使用示例
rate_limiter_t* limiter = rate_limiter_create(1000.0, 100.0);  // 1000 QPS, 突发 100

if (rate_limiter_allow(limiter)) {
    // 处理请求
} else {
    // 返回 429 Too Many Requests
}
```

### JSON-RPC 处理器 ([jsonrpc.c](../../agentos/gateway/src/utils/jsonrpc.c))

```c
// 解析 JSON-RPC 2.0 请求
jsonrpc_request_t* req = jsonrpc_parse_request(raw_json);

// 验证请求格式
if (!jsonrpc_is_valid_request(req)) {
    response = jsonrpc_error_response(req->id, JSONRPC_INVALID_REQUEST);
    return response;
}

// 路由到对应的 Syscall 处理器
handler = syscall_router_find_handler(req->method);
if (!handler) {
    response = jsonrpc_error_response(req->id, JSONRPC_METHOD_NOT_FOUND);
    return response;
}

// 执行并返回结果
result = handler(req->params);
response = jsonrpc_success_response(req->id, result);
```

### Syscall 路由器 ([syscall_router.c](../../agentos/gateway/src/utils/syscall_router.c))

```c
// 注册 Syscall 处理函数
syscall_router_register("task.submit", handle_task_submit);
syscall_router_register("task.query", handle_task_query);
syscall_router_register("memory.write", handle_memory_write);
syscall_router_register("memory.search", handle_memory_search);
syscall_router_register("session.create", handle_session_create);

// 动态查找处理器
syscall_handler_t handler = syscall_router_lookup(method_name);
if (handler) {
    result = handler(params);
}
```

---

## 🔧 配置与部署

### 配置文件结构

```yaml
# gateway.yaml
gateway:
  # 监听地址
  bind_address: "0.0.0.0"
  port: 18789

  # 协议配置
  protocols:
    http:
      enabled: true
      read_timeout_ms: 30000
      write_timeout_ms: 30000
      max_request_size_mb: 10
      cors:
        allowed_origins: ["*"]
        allowed_methods: ["GET", "POST", "PUT", "DELETE"]
        allowed_headers: ["Content-Type", "Authorization"]

    websocket:
      enabled: true
      ping_interval_sec: 30
      pong_timeout_sec: 10
      max_message_size_mb: 5

    stdio:
      enabled: true
      batch_mode: false

  # 限流配置
  rate_limiting:
    global_qps: 10000
    per_ip_qps: 100
    burst_size: 50

  # 日志配置
  logging:
    level: info
    access_log: "/var/log/agentos/gateway_access.log"
    error_log: "/var/log/agentos/gateway_error.log"

  # 上游连接（Kernel IPC）
  upstream:
    endpoint: "http://localhost:18080"  # 内核 IPC 地址
    timeout_ms: 5000
    max_retries: 3
    retry_delay_ms: 100
```

### Nginx 反向代理配置（生产环境推荐）

```nginx
upstream agentos_gateway {
    server 127.0.0.1:18789;
    keepalive 64;
}

server {
    listen 443 ssl http2;
    server_name api.agentos.spharx.cn;

    ssl_certificate /etc/ssl/certs/agentos.crt;
    ssl_certificate_key /etc/ssl/private/agentos.key;

    location /api/ {
        proxy_pass http://agentos_gateway;
        proxy_http_version 1.1;

        # WebSocket 支持
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "upgrade";

        # 超时设置
        proxy_read_timeout 3600s;
        proxy_send_timeout 3600s;

        # 限流
        limit_req zone=api burst=20 nodelay;
    }

    location /ws {
        proxy_pass http://agentos_gateway;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "upgrade";
        proxy_read_timeout 3600s;
    }
}

# 限流区域定义
limit_req_zone $binary_remote_addr zone=api:10m rate=10r/s;
```

---

## 📊 性能指标

| 指标 | HTTP | WebSocket | Stdio |
|------|------|-----------|-------|
| **吞吐量** | ~5000 QPS | ~1000 conn/s | ~2000 ops/s |
| **延迟 (P99)** | < 50ms | < 10ms | < 5ms |
| **并发连接** | 10000 | 5000 | 单线程 |
| **适用场景** | REST API | 流式实时 | CLI/管道 |

### Prometheus 指标导出

```yaml
# gateway metrics
- name: agentos_gateway_requests_total
  type: Counter
  labels: [protocol, method, status_code]
  help: "Total number of requests"

- name: agentos_gateway_request_duration_seconds
  type: Histogram
  labels: [protocol, method]
  help: "Request latency distribution"

- name: agentos_gateway_active_connections
  type: Gauge
  labels: [protocol]
  help: "Current active connections"

- name: agentos_gateway_rate_limited_total
  type: Counter
  help: "Total number of rate-limited requests"
```

---

## 🧪 测试指南

### 单元测试

```bash
cd AgentOS/agentos/gateway
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make gateway_tests

# 运行所有测试
./test_gateway
./test_jsonrpc
./test_syscall_router
./test_gateway_rpc_handler
```

### 集成测试

```bash
# 启动测试环境
docker compose -f docker/docker-compose.yml up -d kernel gateway

# HTTP 测试
curl -s http://localhost:18789/api/v1/health | jq .

# WebSocket 测试
wscat -c ws://localhost:18789/ws

# Stdio 测试
echo '{"method":"health.check","params":{}}' | ./bin/agentos-gateway --protocol stdio
```

### 压力测试

```bash
# HTTP 压测
ab -n 10000 -c 100 http://localhost:18789/api/v1/health

# WebSocket 并发
wstest -c 100 -m 10 ws://localhost:18789/ws
```

---

## 📚 相关文档

- **[守护进程 API](daemon-api.md)** — 后端服务接口
- **[Python SDK](python-sdk.md)** — Python 客户端使用 Gateway
- **[Go SDK](go-sdk.md)** — Go 客户端使用 Gateway
- **[HeapStore 系统](architecture/heapstore.md)** — 数据存储层
- **[Docker 部署](../docker/README.md)** — 容器化部署配置

---

## 🔗 源码索引

| 文件路径 | 功能描述 |
|----------|----------|
| [http_gateway.c](../../agentos/gateway/src/gateway/http_gateway.c) | HTTP 协议实现 |
| [ws_gateway.c](../../agentos/gateway/src/gateway/ws_gateway.c) | WebSocket 实现 |
| [stdio_gateway.c](../../agentos/gateway/src/gateway/stdio_gateway.c) | stdio 协议实现 |
| [http_gateway_routes.c](../../agentos/gateway/src/gateway/http_gateway_routes.c) | HTTP 路由表 |
| [gateway_api.c](../../agentos/gateway/src/gateway/gateway_api.c) | 统一 API 封装 |
| [jsonrpc.c](../../agentos/gateway/src/utils/jsonrpc.c) | JSON-RPC 2.0 解析 |
| [syscall_router.c](../../agentos/gateway/src/utils/syscall_router.c) | Syscall 路由分发 |
| [gateway_rpc_handler.c](../../agentos/gateway/src/utils/gateway_rpc_handler.c) | RPC 请求处理 |
| [gateway_rate_limiter.c](../../agentos/gateway/src/utils/gateway_rate_limiter.c) | 速率限制 |

---

**© 2026 SPHARX Ltd. All Rights Reserved.**

*"From data intelligence emerges."*
