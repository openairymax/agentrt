# gateway – AgentOS 统一通信网关

**版本**: v1.1.0  
**最后更新**: 2026-03-28  
**许可证**: Apache License 2.0  
**归属**: AgentOS 核心运行时管理模块

---

## 🎯 概述

gateway 是 AgentOS 微内核架构中的**统一通信网关模块**，作为连接内核与外部世界的唯一桥梁。它提供标准化的多协议接入层，将外部请求安全、高效地路由至内核系统调用接口。

### 核心职责

- **多协议网关接入**：
  - HTTP/1.1 网关：基于 libmicrohttpd，支持 RESTful API 和 JSON-RPC 2.0
  - WebSocket 网关：基于 libwebsockets，支持全双工实时通信
  - stdio 网关：基于标准输入输出，支持本地进程间通信（IPC）
  
- **会话生命周期管理**：
  - 创建、查询、关闭会话
  - 自动清理过期会话（基于 LRU 策略）
  - 会话状态追踪与统计
  
- **健康检查**：
  - 定期轮询各网关组件状态
  - 异常检测与日志记录
  - 为集群管理提供健康状态数据
  
- **可观测性**：
  - Prometheus 格式指标导出（QPS、延迟、错误率等）
  - 分布式链路追踪（OpenTelemetry 兼容）
  - 实时性能监控

### 设计原则

gateway 严格遵循 AgentOS 微内核设计哲学：
- **单一职责**：仅负责通信协议转换和会话管理，不包含业务逻辑
- ** syscall 隔离**：仅通过系统调用接口（`agentos_sys_*`）与内核交互，不直接访问内核内部实现
- **零信任安全**：所有请求必须经过认证和限权验证
- **优雅降级**：在资源不足或部分组件故障时自动降级服务

---

## 📁 目录结构

```
gateway/
├── CMakeLists.txt          # 构建配置（CMake 3.20+）
├── README.md               # 本文档
├── include/
│   └── gateway.h           # 对外公共 API 头文件
└── src/
    ├── server.c            # 核心控制器（生命周期管理）
    ├── server.h            # 内部类型定义与常量
    ├── session.c           # 会话管理器实现
    ├── session.h           # 会话数据结构
    ├── health.c            # 健康检查器实现
    ├── health.h            # 健康检查接口
    ├── telemetry.c         # 可观测性实现
    ├── telemetry.h         # 遥测数据结构
    ├── auth.c              # 认证与授权（API Key + JWT）
    ├── auth.h
    ├── ratelimit.c         # 限流器实现（令牌桶算法）
    ├── ratelimit.h
    └── gateway/            # 网关协议实现
        ├── gateway.h       # 网关抽象基类及操作表
        ├── http_gateway.c  # HTTP 网关（libmicrohttpd）
        ├── ws_gateway.c    # WebSocket 网关（libwebsockets）
        └── stdio_gateway.c # stdio 网关（本地 IPC）
```

---

## 🔧 编译

### 依赖要求

| 依赖项 | 最低版本 | 说明 |
|--------|----------|------|
| CMake | 3.20+ | 构建系统 |
| C 编译器 | gcc 11+ / clang 13+ | 支持 C11 标准 |
| pthread | - | POSIX 线程库 |
| libmicrohttpd | 0.9.70+ | HTTP 服务器库 |
| libwebsockets | 4.3+ | WebSocket 库 |
| cJSON | 1.7.15+ | JSON 解析库 |
| OpenSSL | 1.1.1+ | TLS/SSL 支持（可选） |
| agentos_common | v1.0.0+ | AgentOS 基础库（bases 模块） |

### 构建步骤

```bash
# 1. 创建构建目录
mkdir -p gateway/build && cd gateway/build

# 2. 配置 CMake（Release 模式）
cmake .. -DCMAKE_BUILD_TYPE=Release

# 3. 编译
make -j$(nproc)

# 4. 验证构建产物
ls -lh bin/
# 输出：
# agentos_gateway      # 可执行文件
# libgateway.a         # 静态库（可选）
```

### 构建选项

```bash
# 启用调试模式
cmake .. -DCMAKE_BUILD_TYPE=Debug

# 禁用 WebSocket 网关
cmake .. -DENABLE_WS_GATEWAY=OFF

# 禁用 stdio 网关
cmake .. -DENABLE_STDIO_GATEWAY=OFF

# 自定义安装路径
cmake .. -DCMAKE_INSTALL_PREFIX=/opt/agentos
```

---

## 🚀 运行

### 基本启动

```bash
# 使用默认配置启动
./bin/agentos_gateway
```

**默认配置参数**：

| 参数 | 默认值 | 说明 |
|------|--------|------|
| HTTP 监听地址 | 127.0.0.1 | HTTP 网关绑定地址 |
| HTTP 端口 | 18789 | HTTP 网关监听端口 |
| WebSocket 监听地址 | 127.0.0.1 | WebSocket 网关绑定地址 |
| WebSocket 端口 | 18790 | WebSocket 网关监听端口 |
| stdio 网关 | 启用 | 是否启用标准输入输出网关 |
| 最大会话数 | 1000 | 并发会话上限 |
| 会话超时 | 3600 秒 | 会话闲置超时时间 |
| 健康检查间隔 | 30 秒 | 健康检查轮询周期 |
| 指标导出路径 | /metrics | Prometheus 指标端点 |

### 自定义配置

当前版本通过修改 `src/server.c` 中的 `set_default_config()` 函数配置参数：

```c
static void set_default_config(gateway_config_t* config) {
    memset(config, 0, sizeof(gateway_config_t));
    
    config->http_host = "0.0.0.0";        // 监听所有网卡
    config->http_port = 8080;             // 自定义 HTTP 端口
    config->ws_host = "0.0.0.0";          // 监听所有网卡
    config->ws_port = 8081;               // 自定义 WebSocket 端口
    config->enable_stdio = 0;             // 禁用 stdio 网关
    config->max_sessions = 5000;          // 提高并发会话数
    config->session_timeout_sec = 7200;   // 延长会话超时
    config->health_check_interval_sec = 15; // 缩短健康检查间隔
}
```

**计划支持**：v1.2.0 版本将支持通过环境变量或 YAML 配置文件覆盖默认参数。

```bash
# 未来版本示例
export GATEWAY_HTTP_PORT=9000
export GATEWAY_MAX_SESSIONS=10000
./bin/agentos_gateway --config /etc/agentos/gateway.yaml
```

### 停止服务

向进程发送信号即可优雅关闭：

```bash
# 方式 1：Ctrl+C（交互式终端）
# 方式 2：发送 SIGTERM
kill -TERM <pid>

# 方式 3：发送 SIGINT
kill -INT <pid>
```

**优雅关闭流程**：
1. 停止接受新连接
2. 等待现有请求处理完成（最长 30 秒）
3. 关闭所有网关实例
4. 释放会话资源
5. 导出最终指标
6. 退出进程

---

## 📐 架构设计

### 模块分层

```
┌─────────────────────────────────────────────┐
│          外部客户端 / 工具 / 进程            │
└─────────────────┬───────────────────────────┘
                  │
┌─────────────────▼───────────────────────────┐
│         HTTP / WebSocket / stdio            │
│              协议解析层                      │
└─────────────────┬───────────────────────────┘
                  │
┌─────────────────▼───────────────────────────┐
│           认证 (auth.c)                     │
│        API Key / JWT 验证                    │
└─────────────────┬───────────────────────────┘
                  │
┌─────────────────▼───────────────────────────┐
│           限流 (ratelimit.c)                │
│         令牌桶算法限流                       │
└─────────────────┬───────────────────────────┘
                  │
┌─────────────────▼───────────────────────────┐
│         会话管理 (session.c)                │
│      创建/查询/关闭/自动清理                 │
└─────────────────┬───────────────────────────┘
                  │
┌─────────────────▼───────────────────────────┐
│         核心控制器 (server.c)               │
│      路由分发、生命周期管理                  │
└─────────────────┬───────────────────────────┘
                  │
┌─────────────────▼───────────────────────────┐
│      AgentOS 内核系统调用接口                │
│    agentos_sys_* (atoms 模块提供)            │
└─────────────────────────────────────────────┘
```

### 线程模型

```
主线程（server.c）
├── 信号处理线程（优雅关闭）
├── HTTP 网关线程池（libmicrohttpd 管理）
├── WebSocket 事件循环（libwebsockets 管理）
├── stdio 轮询线程（阻塞式读取）
├── 健康检查线程（定时轮询）
└── 会话清理线程（定时扫描过期会话）
```

**线程同步机制**：
- 互斥锁（`pthread_mutex_t`）：保护共享数据结构
- 条件变量（`pthread_cond_t`）：优雅关闭等待
- 原子操作（`atomic_int`）：无锁状态标志
- 读写锁（`pthread_rwlock_t`）：会话表高并发读取

---

## 🔌 API 参考

### 核心 API

#### gateway_server_create

创建网关服务器实例。

```c
AGENTOS_API agentos_error_t gateway_server_create(
    const gateway_config_t* config,
    gateway_server_t** out_server
);
```

**参数**：
- `config`：网关配置结构体指针
- `out_server`：输出参数，接收创建的服务器实例指针

**返回值**：
- `AGENTOS_SUCCESS`：成功
- `AGENTOS_ERROR_INVALID_ARG`：参数无效
- `AGENTOS_ERROR_OUT_OF_MEMORY`：内存不足

**示例**：
```c
gateway_config_t config = {0};
config.http_port = 8080;
config.ws_port = 8081;

gateway_server_t* server = NULL;
agentos_error_t err = gateway_server_create(&config, &server);
if (err != AGENTOS_SUCCESS) {
    // 错误处理
}
```

#### gateway_server_start

启动网关服务器，开始接受连接。

```c
AGENTOS_API agentos_error_t gateway_server_start(gateway_server_t* server);
```

**注意**：此函数为阻塞调用，会一直运行直到调用 `gateway_server_stop()`。

#### gateway_server_stop

停止网关服务器，拒绝新连接并等待现有连接关闭。

```c
AGENTOS_API void gateway_server_stop(gateway_server_t* server);
```

#### gateway_server_destroy

销毁网关服务器实例，释放所有资源。

```c
AGENTOS_API void gateway_server_destroy(gateway_server_t* server);
```

### 配置结构体

```c
typedef struct gateway_config {
    // HTTP 网关配置
    const char* http_host;            // 监听地址，默认 "127.0.0.1"
    uint16_t    http_port;            // 监听端口，默认 18789
    
    // WebSocket 网关配置
    const char* ws_host;              // 监听地址，默认 "127.0.0.1"
    uint16_t    ws_port;              // 监听端口，默认 18790
    
    // stdio 网关配置
    int         enable_stdio;         // 是否启用，默认 1（启用）
    
    // 会话管理配置
    uint32_t    max_sessions;         // 最大并发会话数，默认 1000
    uint32_t    session_timeout_sec;  // 会话超时（秒），默认 3600
    
    // 健康检查配置
    uint32_t    health_check_interval_sec;  // 检查间隔，默认 30
    
    // 可观测性配置
    const char* metrics_path;         // 指标导出路径，默认 "/metrics"
    int         enable_tracing;       // 是否启用链路追踪，默认 1
    
    // 安全配置
    const char* api_key;              // API 密钥（可选）
    uint32_t    rate_limit_qps;       // 每客户端 QPS 限制，默认 100
} gateway_config_t;
```

---

## 📊 与内核交互

gateway 通过 **系统调用接口**（syscall）与 AgentOS 内核交互，所有调用均定义在 `atoms` 模块中。

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

在 `server.c` 的 `gateway_server_create()` 中添加：

```c
server->gateways[GATEWAY_TYPE_CUSTOM] = my_gateway_create(server);
```

### 新增健康检查组件

```c
// 定义检查函数
static int check_database_health(health_checker_t* checker) {
    // 检查数据库连接
    if (db_is_connected()) {
        return HEALTH_STATUS_OK;
    } else {
        return HEALTH_STATUS_UNHEALTHY;
    }
}

// 注册检查器
health_checker_register(health, check_database_health, "database");
```

### 自定义指标导出

```c
// 在适当位置调用
agentos_metrics_increment("custom_events_total");
agentos_metrics_set("custom_gauge_value", 42.0);
agentos_metrics_histogram_observe("custom_request_duration_ms", duration_ms);
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
gateway_request_duration_seconds{quantile="0.5"}
gateway_request_duration_seconds{quantile="0.95"}
gateway_request_duration_seconds{quantile="0.99"}

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

1. **API Key 认证**（推荐用于服务端到服务端调用）：
   ```bash
   curl -H "X-API-Key: your-secret-key" http://localhost:18789/rpc
   ```

2. **JWT 令牌认证**（推荐用于客户端应用）：
   ```bash
   curl -H "Authorization: Bearer <jwt-token>" http://localhost:18789/rpc
   ```

### 限流保护

采用**令牌桶算法**限制每客户端 QPS：

- 默认限制：100 QPS/客户端
- 突发容量：200 令牌
- 超限响应：HTTP 429 Too Many Requests

### TLS/SSL 支持

编译时启用 OpenSSL 后，可配置 HTTPS/WSS：

```bash
cmake .. -DENABLE_TLS=ON
```

**未来版本**：支持通过配置文件指定证书路径。

---

## 🧪 测试

### 单元测试

```bash
# 构建测试可执行文件
make gateway_tests

# 运行测试
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

## 📝 最佳实践

### 生产环境部署

1. **使用反向代理**：
   - 在生产环境中，建议在 gateway 前部署 Nginx 或 HAProxy
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

### 调试技巧

1. **启用调试日志**：
   ```c
   // 修改 server.c
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
   ```

---

## 🚧 已知限制

- **配置文件支持**：v1.1.0 暂不支持外部配置文件，需硬编码修改（v1.2.0 计划支持）
- **动态重载**：配置修改后需重启服务（未来版本支持热重载）
- **集群模式**：当前版本为单实例，集群支持需配合外部负载均衡器

---

## 📚 相关文档

- [AgentOS 架构概述](../README.md)
- [atoms 模块文档](../atoms/README.md)
- [cupolas 模块文档](../cupolas/README.md)
- [daemon 模块文档](../daemon/README.md)
- [bases 模块文档](../bases/README.md)
- [lodges 模块文档](../lodges/README.md)
- [tools 模块文档](../tools/README.md)

---

## 📄 许可证

Copyright © 2026 SPHARX Ltd.  
Licensed under the Apache License, Version 2.0.

---

**From data intelligence emerges.** by spharx
