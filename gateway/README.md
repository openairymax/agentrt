# gateway – AgentOS 统一通信网关

<div align="center">

**版本**: v1.1.0  
**最后更新**: 2026-03-29  
**许可证**: Apache License 2.0  
**构建状态**: [![CI/CD](https://github.com/spharx/agentos/actions/workflows/ci.yml/badge.svg)](https://github.com/spharx/agentos/actions)  
**Docker**: [![Docker](https://img.shields.io/badge/docker-ghcr.io/spharx/agentos--gateway-blue)](https://ghcr.io/spharx/agentos-gateway)

**[快速开始](#-快速开始)** | **[API 参考](#-api-参考)** | **[架构设计](#-架构设计)** | **[部署指南](#-部署指南)**

</div>

---

## 🎯 概述

**gateway** 是 AgentOS 微内核架构中的**统一通信网关层**，作为连接内核与外部世界的唯一桥梁。它提供标准化的多协议接入层，将外部请求安全、高效地路由至内核系统调用接口。

### 核心价值

- ✅ **多协议统一接入**：HTTP/1.1、WebSocket、Stdio 三协议支持
- ✅ **会话生命周期管理**：自动清理、LRU 策略、状态追踪
- ✅ **企业级健康检查**：定期轮询、异常检测、集群支持
- ✅ **完整可观测性**：Prometheus 指标、分布式追踪、实时监控
- ✅ **零信任安全**：API Key + JWT 双重认证、令牌桶限流

### 快速了解（60 秒）

```bash
# 1. 使用默认配置启动
./bin/agentos_gateway

# 2. 测试 HTTP 网关
curl -X POST http://localhost:18789/rpc \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","method":"task.submit","params":{"description":"Hello"},"id":1}'

# 3. 查看 Prometheus 指标
curl http://localhost:18789/metrics

# 4. 查看健康状态
curl http://localhost:18789/health
```

---

## 📁 目录结构

```
gateway/
├── CMakeLists.txt              # 构建配置（CMake 3.16+）
├── README.md                   # 本文档
├── include/
│   └── gateway.h               # 对外公共 API 头文件
├── src/
│   ├── server.c                # 核心控制器（生命周期管理）
│   ├── server.h                # 内部类型定义
│   ├── session.c               # 会话管理器实现
│   ├── session.h               # 会话数据结构
│   ├── health.c                # 健康检查器实现
│   ├── health.h                # 健康检查接口
│   ├── telemetry.c             # 可观测性实现
│   ├── telemetry.h             # 遥测数据结构
│   ├── auth.c                  # 认证与授权（API Key + JWT）
│   ├── auth.h
│   ├── ratelimit.c             # 限流器实现（令牌桶算法）
│   ├── ratelimit.h
│   ├── config.c                # 配置管理
│   ├── config.h
│   ├── main.c                  # 程序入口
│   └── gateway/                # 网关协议实现
│       ├── gateway.h           # 网关抽象基类
│       ├── http_gateway.c      # HTTP 网关（libmicrohttpd）
│       ├── ws_gateway.c        # WebSocket 网关（libwebsockets）
│       └── stdio_gateway.c     # stdio 网关（本地 IPC）
├── docs/
│   └── CI_CD.md                # CI/CD 配置文档
├── docker/
│   ├── Dockerfile              # 多阶段构建文件
│   ├── docker-compose.yml      # Docker Compose 配置
│   └── monitoring/
│       ├── prometheus.yml      # Prometheus 配置
│       └── alerts.yml          # 告警规则
├── deploy/
│   └── k8s/                    # Kubernetes 部署配置
└── tests/
    └── test_gateway.c          # 单元测试
```

---

## 🚀 快速开始

### 环境要求

| 依赖项 | 最低版本 | 说明 | 安装命令（Ubuntu） |
|--------|----------|------|-------------------|
| CMake | 3.16+ | 构建系统 | `apt install cmake` |
| GCC/Clang | gcc 11+ / clang 13+ | C11 编译器 | `apt install build-essential` |
| libmicrohttpd | 0.9.70+ | HTTP 服务器库 | `apt install libmicrohttpd-dev` |
| libwebsockets | 4.3+ | WebSocket 库 | `apt install libwebsockets-dev` |
| cJSON | 1.7.15+ | JSON 解析库 | `apt install libcjson-dev` |
| OpenSSL | 1.1.1+ | TLS/SSL 支持 | `apt install libssl-dev` |
| pthread | - | POSIX 线程库 | 已包含在 libc 中 |
| agentos_common | v1.0.0+ | AgentOS 基础库 | 从 commons 模块编译 |

### 编译步骤

```bash
# 1. 克隆项目
git clone https://github.com/spharx/agentos.git
cd agentos/gateway

# 2. 创建构建目录
mkdir -p build && cd build

# 3. 配置 CMake（Release 模式）
cmake .. -DCMAKE_BUILD_TYPE=Release

# 4. 编译
make -j$(nproc)

# 5. 验证构建产物
ls -lh bin/
# 输出：
# agentos_gateway      # 可执行文件
# libagentos_gateway.a # 静态库（可选）
```

### 运行服务

```bash
# 方式 1：使用默认配置启动
./bin/agentos_gateway

# 方式 2：后台运行
./bin/agentos_gateway &

# 方式 3：使用 Docker 运行
docker run -d -p 18789:18789 -p 18790:18790 \
  ghcr.io/spharx/agentos-gateway:latest
```

### 停止服务

```bash
# 方式 1：Ctrl+C（交互式终端）

# 方式 2：发送 SIGTERM（优雅关闭）
kill -TERM <pid>

# 方式 3：使用 gateway_request_stop() API
```

**优雅关闭流程**：
1. 停止接受新连接
2. 等待现有请求处理完成（最长 30 秒）
3. 关闭所有网关实例
4. 释放会话资源
5. 导出最终指标
6. 退出进程

---

## 🔌 API 参考

### 核心 API

#### gateway_run（单例模式，推荐）

最简单的启动方式，使用单例模式运行网关服务器。

```c
#include <gateway.h>

int main(void) {
    // 使用默认配置启动（单例模式）
    agentos_error_t err = gateway_run(NULL);
    
    // 此函数会阻塞直到收到停止信号
    if (err != AGENTOS_SUCCESS) {
        // 错误处理
    }
    
    return 0;
}
```

#### gateway_server_create（多实例模式）

创建并管理多个网关服务器实例。

```c
#include <gateway.h>

int main(void) {
    // 自定义配置
    gateway_config_t config = {
        .http_host = "0.0.0.0",        // 监听所有网卡
        .http_port = 8080,             // 自定义 HTTP 端口
        .ws_host = "0.0.0.0",          // 监听所有网卡
        .ws_port = 8081,               // 自定义 WebSocket 端口
        .enable_stdio = 0,             // 禁用 stdio 网关
        .max_sessions = 5000,          // 提高并发会话数
        .session_timeout_sec = 7200,   // 延长会话超时（2 小时）
        .health_interval_sec = 15,     // 缩短健康检查间隔
        .metrics_path = "/metrics",    // Prometheus 指标端点
        .max_request_size = 2097152,   // 2MB 最大请求体
        .request_timeout_ms = 60000    // 60 秒请求超时
    };
    
    // 创建服务器实例
    gateway_server_t* server = NULL;
    agentos_error_t err = gateway_server_create(&config, &server);
    if (err != AGENTOS_SUCCESS) {
        // 错误处理
        return 1;
    }
    
    // 启动服务器（阻塞调用）
    err = gateway_server_start(server);
    
    // 等待服务器退出
    gateway_server_wait(server, 0);  // 0 表示无限等待
    
    // 销毁服务器
    gateway_server_destroy(server);
    
    return 0;
}
```

### 配置结构体详解

```c
typedef struct gateway_config {
    /* HTTP 网关配置 */
    const char* http_host;            // HTTP 监听地址，默认 "127.0.0.1"
    uint16_t    http_port;            // HTTP 端口，默认 18789
    
    /* WebSocket 网关配置 */
    const char* ws_host;              // WebSocket 监听地址，默认 "127.0.0.1"
    uint16_t    ws_port;              // WebSocket 端口，默认 18790
    
    /* stdio 网关配置 */
    int         enable_stdio;         // 是否启用 stdio 网关，默认 1（启用）
    
    /* 会话管理配置 */
    uint32_t    max_sessions;         // 最大并发会话数，默认 1000
    uint32_t    session_timeout_sec;  // 会话闲置超时（秒），默认 3600（1 小时）
    
    /* 健康检查配置 */
    uint32_t    health_interval_sec;  // 健康检查间隔（秒），默认 30
    
    /* 可观测性配置 */
    const char* metrics_path;         // 指标导出路径，默认 "/metrics"
    
    /* 请求限制配置 */
    uint32_t    max_request_size;     // 最大请求体大小（字节），默认 1MB
    uint32_t    request_timeout_ms;   // 请求超时（毫秒），默认 30000（30 秒）
} gateway_config_t;
```

### 健康检查 API

```c
// 获取健康状态（JSON 格式）
char* health_json = NULL;
agentos_error_t err = gateway_server_get_health(server, &health_json);
if (err == AGENTOS_SUCCESS) {
    printf("Health: %s\n", health_json);
    free(health_json);
}

// 示例输出：
// {"status":"healthy","gateways":{"http":"up","websocket":"up","stdio":"up"},"sessions":{"active":42,"max":1000}}
```

### 指标导出 API

```c
// 获取 Prometheus 格式指标
char* metrics = NULL;
agentos_error_t err = gateway_server_get_metrics(server, &metrics);
if (err == AGENTOS_SUCCESS) {
    printf("Metrics:\n%s\n", metrics);
    free(metrics);
}

// 示例输出：
// # HELP gateway_http_requests_total Total HTTP requests
// # TYPE gateway_http_requests_total counter
// gateway_http_requests_total{method="POST",endpoint="/rpc",status="200"} 1234
// gateway_sessions_active 42
// gateway_request_duration_seconds{quantile="0.95"} 0.125
```

---

## 📐 架构设计

### 模块分层

```
┌─────────────────────────────────────────────────────────────────┐
│                     外部客户端 / 工具 / 进程                      │
└───────────────────────────┬─────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│              HTTP / WebSocket / stdio 协议解析层                  │
│         (libmicrohttpd / libwebsockets / stdio)                 │
└───────────────────────────┬─────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│                    认证层 (auth.c)                               │
│              API Key 验证 + JWT 令牌验证                          │
└───────────────────────────┬─────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│                    限流层 (ratelimit.c)                          │
│                令牌桶算法，默认 100 QPS/客户端                     │
└───────────────────────────┬─────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│                  会话管理层 (session.c)                          │
│        创建/查询/关闭/自动清理（LRU 策略 + 超时清理）              │
└───────────────────────────┬─────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│                核心控制器 (server.c)                             │
│            路由分发、生命周期管理、状态监控                       │
└───────────────────────────┬─────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│              AgentOS 内核系统调用接口 (syscall)                   │
│    agentos_sys_session_* / task_* / memory_* / telemetry_*      │
└─────────────────────────────────────────────────────────────────┘
```

### 线程模型

```
主线程（server.c）
├── 信号处理线程（优雅关闭）
│   └── 处理 SIGINT/SIGTERM，触发 gateway_server_stop()
├── HTTP 网关线程池（libmicrohttpd 管理）
│   └── 多线程处理 HTTP 请求
├── WebSocket 事件循环（libwebsockets 管理）
│   └── 单线程事件驱动
├── stdio 轮询线程（阻塞式读取）
│   └── 阻塞读取 stdin
├── 健康检查线程（定时轮询）
│   └── 每 30 秒检查各组件状态
└── 会话清理线程（定时扫描）
    └── 扫描过期会话并清理
```

**线程同步机制**：
- **互斥锁**（`pthread_mutex_t`）：保护共享数据结构
- **条件变量**（`pthread_cond_t`）：优雅关闭等待
- **原子操作**（`atomic_int`）：无锁状态标志
- **读写锁**（`pthread_rwlock_t`）：会话表高并发读取

### 设计原则

gateway 严格遵循 AgentOS 微内核设计哲学：

| 原则 | 说明 | 实现方式 |
|------|------|----------|
| **单一职责** | 仅负责通信协议转换和会话管理 | 不包含业务逻辑，通过 syscall 与内核交互 |
| **syscall 隔离** | 仅通过系统调用接口与内核交互 | 禁止直接访问内核内部实现 |
| **零信任安全** | 所有请求必须经过认证和限权验证 | API Key + JWT 双重认证，令牌桶限流 |
| **优雅降级** | 资源不足时自动降级服务 | 部分网关故障不影响其他网关 |

---

## 📊 与内核交互

gateway 通过**系统调用接口**（syscall）与 AgentOS 内核交互，所有调用均定义在 `atoms/syscall` 模块中。

### 系统调用列表

| 系统调用 | 说明 | 使用场景 |
|----------|------|----------|
| `agentos_sys_session_create` | 创建新会话 | 客户端发起新请求 |
| `agentos_sys_session_query` | 查询会话状态 | 获取会话信息 |
| `agentos_sys_session_close` | 关闭会话 | 客户端断开连接 |
| `agentos_sys_task_submit` | 提交任务到内核 | 执行 AI 任务 |
| `agentos_sys_task_query` | 查询任务状态 | 获取任务进度 |
| `agentos_sys_memory_search` | 搜索记忆数据 | 知识检索 |
| `agentos_sys_memory_store` | 存储记忆数据 | 持久化存储 |
| `agentos_sys_component_status` | 查询组件状态 | 健康检查 |

### JSON-RPC 2.0 协议示例

**请求**：
```json
POST /rpc HTTP/1.1
Content-Type: application/json
X-API-Key: your-api-key

{
  "jsonrpc": "2.0",
  "method": "task.submit",
  "params": {
    "description": "分析这份销售数据报告"
  },
  "id": 1
}
```

**响应**：
```json
HTTP/1.1 200 OK
Content-Type: application/json

{
  "jsonrpc": "2.0",
  "result": {
    "task_id": "task_12345",
    "status": "pending"
  },
  "id": 1
}
```

---

## 🔍 可观测性

### Prometheus 指标

gateway 自动导出以下指标（端点：`/metrics`）：

```prometheus
# 网关指标
gateway_http_requests_total{method="POST", endpoint="/rpc", status="200"}
gateway_ws_connections_active
gateway_stdio_bytes_read_total
gateway_stdio_bytes_written_total

# 会话指标
gateway_sessions_active
gateway_sessions_created_total
gateway_sessions_closed_total
gateway_sessions_expired_total

# 性能指标
gateway_request_duration_seconds{quantile="0.5"}    # P50 延迟
gateway_request_duration_seconds{quantile="0.95"}   # P95 延迟
gateway_request_duration_seconds{quantile="0.99"}   # P99 延迟

# 错误指标
gateway_errors_total{type="auth_failed"}
gateway_errors_total{type="rate_limited"}
gateway_errors_total{type="internal_error"}
```

### 分布式链路追踪

启用链路追踪后（`enable_tracing=1`），gateway 会为每个请求生成追踪上下文：

- **Trace ID**：全局唯一请求标识
- **Span ID**：当前操作标识
- **Parent Span ID**：父操作标识（如有）

**导出格式**：OpenTelemetry 兼容，可对接 Jaeger、Zipkin 等后端。

---

## 🛡️ 安全机制

### 认证方式

#### 1. API Key 认证（服务端到服务端）

```bash
curl -H "X-API-Key: your-secret-key" http://localhost:18789/rpc
```

#### 2. JWT 令牌认证（客户端应用）

```bash
curl -H "Authorization: Bearer <jwt-token>" http://localhost:18789/rpc
```

### 限流保护

采用**令牌桶算法**限制每客户端 QPS：

- **默认限制**：100 QPS/客户端
- **突发容量**：200 令牌
- **超限响应**：HTTP 429 Too Many Requests

### TLS/SSL 支持

编译时启用 OpenSSL 后，可配置 HTTPS/WSS：

```bash
cmake .. -DENABLE_TLS=ON
```

---

## 🚀 部署指南

### Docker 部署

```bash
# 方式 1：使用 docker-compose
cd gateway/docker
docker-compose up -d

# 方式 2：直接运行
docker run -d \
  -p 18789:18789 \
  -p 18790:18790 \
  -v /path/to/config:/etc/agentos \
  ghcr.io/spharx/agentos-gateway:latest
```

### Kubernetes 部署

```bash
# 部署到 Kubernetes
kubectl apply -f gateway/deploy/k8s/

# 查看状态
kubectl get pods -n agentos-gateway

# 查看日志
kubectl logs -f deployment/agentos-gateway -n agentos-gateway
```

### 生产环境建议

1. **使用反向代理**：
   - 在 gateway 前部署 Nginx 或 HAProxy
   - 提供 SSL 终止、负载均衡、DDoS 防护

2. **调整内核参数**：
   ```bash
   # 增加文件描述符限制
   ulimit -n 65535
   
   # 调整 TCP 参数
   sysctl -w net.core.somaxconn=65535
   sysctl -w net.ipv4.tcp_max_syn_backlog=65535
   ```

3. **监控告警**：
   - 配置 Prometheus 告警规则（错误率 > 1%、P99 延迟 > 1s）
   - 设置日志聚合（ELK Stack 或 Loki）

---

## 🧪 测试

### 单元测试

```bash
# 构建时启用测试
cmake .. -DBUILD_TESTS=ON

# 运行测试
make test
# 或
ctest --output-on-failure
```

### 集成测试

```bash
# 启动测试服务器
./bin/agentos_gateway --test-mode &

# 运行集成测试脚本
python3 tests/integration/test_gateway.py
```

---

## 🛠️ 扩展性指南

### 新增网关类型

1. **实现网关操作表**：

```c
// my_custom_gateway.c
#include "gateway/gateway.h"

static int my_gateway_init(gateway_t* gw) {
    // 初始化逻辑
    return 0;
}

static void my_gateway_cleanup(gateway_t* gw) {
    // 清理逻辑
}

static int my_gateway_start(gateway_t* gw) {
    // 启动监听
    return 0;
}

static void my_gateway_stop(gateway_t* gw) {
    // 停止监听
}

// 定义操作表
const gateway_ops_t my_gateway_ops = {
    .init = my_gateway_init,
    .cleanup = my_gateway_cleanup,
    .start = my_gateway_start,
    .stop = my_gateway_stop
};

// 创建网关实例
gateway_t* my_gateway_create(gateway_server_t* server) {
    gateway_t* gw = malloc(sizeof(gateway_t));
    gw->ops = &my_gateway_ops;
    gw->server = server;
    return gw;
}
```

2. **注册到服务器**：

在 `server.c` 的 `server_start_gateways()` 函数中添加：

```c
server->gateways[GATEWAY_TYPE_CUSTOM] = my_gateway_create(server);
```

---

## 📝 最佳实践

### 配置调优

| 场景 | 推荐配置 |
|------|----------|
| **开发环境** | `max_sessions=100`, `session_timeout_sec=300` |
| **生产环境** | `max_sessions=10000`, `session_timeout_sec=7200` |
| **高并发场景** | `max_sessions=50000`, `max_request_size=5MB` |
| **低延迟场景** | `request_timeout_ms=10000`, `health_interval_sec=10` |

### 调试技巧

1. **启用调试日志**：
   ```c
   // 修改 server.c 或配置文件
   config->log_level = LOG_LEVEL_DEBUG;
   ```

2. **单步调试**：
   ```bash
   gdb --args ./bin/agentos_gateway
   ```

3. **性能分析**：
   ```bash
   # 使用 perf 分析
   perf record -g ./bin/agentos_gateway
   perf report
   
   # 生成火焰图
   perf script | stackcollapse-perf.pl | flamegraph.pl > flamegraph.svg
   ```

---

## 🚧 已知限制

| 限制 | 当前状态 | 计划版本 |
|------|----------|----------|
| **外部配置文件** | 不支持，需硬编码修改 | v1.2.0 |
| **动态重载配置** | 不支持，需重启服务 | v1.3.0 |
| **集群模式** | 需配合外部负载均衡器 | v2.0.0 |
| **gRPC 网关** | 不支持 | 规划中 |

---

## 📚 相关文档

### 项目内部文档

- [AgentOS 架构概述](../README.md)
- [架构设计原则](../manuals/architecture/ARCHITECTURAL_PRINCIPLES.md)
- [atoms 模块文档](../atoms/README.md)
- [commons 模块文档](../commons/README.md)
- [CI/CD 配置](docs/CI_CD.md)

### 外部资源

- [libmicrohttpd 官方文档](https://www.gnu.org/software/libmicrohttpd/)
- [libwebsockets 官方文档](https://libwebsockets.org/documentation.html)
- [Prometheus 指标最佳实践](https://prometheus.io/docs/practices/)
- [OpenTelemetry 规范](https://opentelemetry.io/docs/)

---

## 📄 许可证

Copyright © 2026 SPHARX Ltd.  
Licensed under the Apache License, Version 2.0.

详见 [LICENSE](../LICENSE) 文件。

---

<div align="center">

**"From data intelligence emerges."** - SPHARX

---

[🏠 返回项目首页](../README.md) | [📧 联系我们](https://spharx.cn)

</div>
