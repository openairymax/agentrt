# AgentOS 通信协议规范

**版本**: Doc V1.7  
**状态**: 正式发布  
**归属**: AgentOS 技术规范核心组成部分  
**作者**: LirenWang  
**最后更新**: 2026-03-31  
**许可证**: GPL-3.0  
**理论基础**: 工程两论（控制论与系统工程）、双系统认知理论、微内核哲学、设计美学  
**关联规范**: [架构设计原则](../architecture/ARCHITECTURAL_PRINCIPLES.md)、[系统调用 API 规范](./syscall_api_contract.md)、[统一术语表](../TERMINOLOGY.md)

---

## 编制说明

### 本文档定位

通信协议规范是 AgentOS 规范体系的核心组成部分，属于**战术层规范**。本规范定义了 AgentOS 分层架构中各组件之间的通信机制，确保系统内部以及系统与外部世界之间能够清晰、可靠、安全地交互。

### 与设计哲学的关系

本规范根植于 AgentOS 五维正交设计体系，是架构设计原则在通信层的具体体现：

- **系统观（S维度）**: 协议栈按层次组织（外部网关、服务间通信、系统调用），体现层次分解方法，每层有明确的职责边界
- **内核观（K维度）**: 通信接口保持长期稳定，支持向后兼容和渐进式演进，体现微内核的接口稳定原则
- **认知观（C维度）**: 协议设计支持双系统认知模型的效率与精度权衡，通过不同的传输策略适配不同认知需求
- **工程观（E维度）**: 每一层都内置安全机制（认证、授权、审计），体现安全内生原则；协议设计考虑 TraceID 贯穿、结构化日志、指标收集等可观测性需求
- **设计美学（A维度）**: 协议消息格式遵循简约、对称、自解释的美学原则，确保人类可读性和机器可处理性的平衡

### 适用范围

本规范适用于以下场景：

1. **系统集成商**: 实现客户端与 AgentOS 的通信对接
2. **服务开发者**: 开发 daemon/守护进程，实现服务间通信
3. **内核开发者**: 实现和维护 syscall 层接口
4. **安全审计人员**: 审查通信安全性和权限控制

### 术语定义

本规范使用的术语定义见 [统一术语表](../TERMINOLOGY.md),关键术语包括：

| 术语 | 简要定义 | 来源 |
|------|---------|------|
| Habitat 网关 | 统一的通信网关，处理外部请求和服务间通信 | 本规范 |
| JSON-RPC 2.0 | 基于 JSON 的远程过程调用协议 | 本规范 |
| 系统调用 (Syscall) | 用户态进入内核的唯一入口 | [syscall_api_contract.md](./syscall_api_contract.md) |
| TraceID | 分布式追踪的唯一标识 | [日志打印规范](../coding_standard/Log_guide.md) |

---

## 第 1 章 引言

### 1.1 背景与意义

AgentOS 采用分层架构，各层之间以及层与外部世界之间通过明确的协议通信。清晰的协议设计是系统稳定性、安全性和可维护性的基础。本规范定义了以下通信场景：

1. **系统调用**: 用户态服务与内核之间的交互 (通过 syscall 层)
2. **服务间通信**: daemon/ 各守护进程之间的通信 (通过 Habitat 网关或直接 RPC)
3. **外部网关**: 客户端 (如 CLI、SDK、Web 应用) 与 Habitat 网关的交互

协议设计遵循 AgentOS 的一贯思想：**层次分明、接口稳定、安全内生、可观测**。

### 1.2 目标与范围

本规范旨在实现以下目标：

1. **层次清晰**: 明确定义各层协议的职责和边界
2. **格式统一**: 使用 JSON-RPC 2.0 作为基础协议格式
3. **安全可靠**: 内置认证、授权、审计机制
4. **可观测完备**: 支持 TraceID 贯穿、结构化日志、指标收集
5. **向后兼容**: 支持版本演进和平滑升级

### 1.3 协议设计原则

#### 原则 1-1【层次分离】：各层协议应职责单一，避免跨层耦合

**解释**: 每一层只关注自己的职责，不越界处理其他层的事务。

**实施指南:**
- 外部网关层：负责协议转换、认证、限流
- 服务间通信层：负责消息路由、负载均衡
- 系统调用层：负责参数校验、权限检查、内核分发

#### 原则 1-2【接口稳定】：公开接口应保持长期稳定，变更需经过严格评审

**解释**: 接口变更会影响所有调用方，必须谨慎处理。

**实施指南:**
- 新增功能优先扩展，避免修改现有接口
- 必须修改时，提供至少 6 个月的过渡期
- 废弃接口标记为 `@deprecated`,并在文档中明确说明

#### 原则 1-3【安全默认】：默认情况下应启用最严格的安全策略

**解释**: 安全不应是可选配置，而应是默认行为。

**实施指南:**
- 所有外部请求必须认证
- 敏感操作必须授权
- 所有操作必须审计

---

## 第 2 章 协议分层架构

### 2.1 分层模型

AgentOS 采用四层协议栈，自顶向下分别为：

```
┌─────────────────────────────────────────────────────────────┐
│                     外部客户端 (CLI/toolkit/Web)                  │
│                     HTTP/WebSocket/stdio                      │
├─────────────────────────────────────────────────────────────┤
│                    Habitat 网关 (协议转换、认证、路由)         │
│                    HTTP/1.1, HTTP/2, WebSocket, stdio        │
├─────────────────────────────────────────────────────────────┤
│                         服务层 (JSON-RPC 2.0)                 │
│                    daemon/ 各守护进程之间的通信                │
├─────────────────────────────────────────────────────────────┤
│                         系统调用层 (C ABI)                    │
│                    atoms/syscall/include/syscalls.h          │
├─────────────────────────────────────────────────────────────┤
│                          内核 (atoms/)                       │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 各层职责

#### 2.2.1 外部网关层

**职责:**
- 接收外部客户端请求 (HTTP/WebSocket/stdio)
- 协议转换 (将不同协议统一为 JSON-RPC 2.0)
- 身份认证 (验证 API Key、Token)
- 流量控制 (限流、防抖)
- 请求路由 (转发到相应的服务)

**传输协议:**
- HTTP/1.1: 兼容传统客户端
- HTTP/2: 支持多路复用，提高性能
- WebSocket: 支持双向流式通信
- stdio: 本地进程通信 (CLI 工具)

#### 2.2.2 服务间通信层

**职责:**
- 服务注册与发现
- 消息路由和转发
- 负载均衡
- 故障恢复

**通信模式:**
- 同步 RPC: 请求 - 响应模式
- 异步消息：发布 - 订阅模式
- 流式通信：双向数据流

#### 2.2.3 系统调用层

**职责:**
- 提供用户态进入内核的唯一入口
- 参数校验和权限检查
- 内核功能分发
- 错误处理和返回

**调用方式:**
- C ABI 兼容的函数调用
- 通过 `agentos_syscall_invoke` 统一入口

---

## 第 3 章 外部网关协议

### 3.1 HTTP 网关

#### 3.1.1 端点定义

**主端点:**
```
POST /rpc
Content-Type: application/json
```

**辅助端点:**
```
GET  /health      # 健康检查
GET  /metrics     # Prometheus 指标导出
GET  /api/v1/*    # RESTful API(可选)
```

#### 3.1.2 请求格式

**请求头:**
```http
POST /rpc HTTP/1.1
Host: localhost:18789
Content-Type: application/json
Authorization: Bearer <API_KEY>
X-Trace-ID: abc123def456  # 可选，用于链路追踪
```

**请求体 (JSON-RPC 2.0):**
```json
{
  "jsonrpc": "2.0",
  "method": "llm.complete",
  "params": {
    "model": "gpt-4",
    "messages": [
      {"role": "user", "content": "Hello"}
    ],
    "temperature": 0.7
  },
  "id": 1
}
```

#### 3.1.3 响应格式

**成功响应:**
```json
{
  "jsonrpc": "2.0",
  "result": {
    "content": "Hi there!",
    "tokens_used": 15
  },
  "id": 1
}
```

**错误响应:**
```json
{
  "jsonrpc": "2.0",
  "error": {
    "code": -32601,
    "message": "Method not found",
    "data": {
      "error_code": "AGENTOS_ENOENT"
    }
  },
  "id": 1
}
```

**HTTP 状态码:**
```
200 OK: 成功
400 Bad Request: 请求格式错误
401 Unauthorized: 认证失败
403 Forbidden: 权限不足
404 Not Found: 方法不存在
429 Too Many Requests: 请求限流
500 Internal Server Error: 服务器内部错误
```

### 3.2 WebSocket 网关

#### 3.2.1 连接建立

**端点:**
```
ws://localhost:18789/ws
```

**握手请求:**
```http
GET /ws HTTP/1.1
Host: localhost:18789
Upgrade: websocket
Connection: Upgrade
Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==
Sec-WebSocket-Version: 13
Sec-WebSocket-Protocol: json-rpc-2.0
```

**认证参数 (可选):**
```
ws://localhost:18789/ws?token=eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...
```

#### 3.2.2 消息格式

每一条消息为一个完整的 JSON-RPC 请求或响应对象 (JSON 文本)。

**客户端 → 服务端:**
```json
{
  "jsonrpc": "2.0",
  "method": "llm.complete_stream",
  "params": {
    "prompt": "Tell me a story"
  },
  "id": 2
}
```

**服务端 → 客户端 (流式响应):**
```json
{"jsonrpc":"2.0","result":{"chunk":"Once"},"id":2}
{"jsonrpc":"2.0","result":{"chunk":" upon"},"id":2}
{"jsonrpc":"2.0","result":{"chunk":" a time"},"id":2}
{"jsonrpc":"2.0","result":{"done":true},"id":2}
```

#### 3.2.3 连接生命周期

- **建立**: 握手成功，进入就绪状态
- **活跃**: 正常收发消息
- **空闲超时**: 超过 `idle_timeout`(默认 300 秒) 无活动，自动断开
- **主动关闭**: 任一方发送 `close` 帧
- **异常关闭**: 网络错误、协议错误等

### 3.3 stdio 网关

#### 3.3.1 输入输出格式

**输入 (stdin):**
- 每行一个完整的 JSON-RPC 请求对象
- 以换行符 `\n` 结尾

**示例:**
```bash
echo '{"jsonrpc":"2.0","method":"task.submit","params":{"input":"Hello"},"id":1}' | agentos-gateway
```

**输出 (stdout):**
- 每行一个完整的 JSON-RPC 响应对象
- 以换行符 `\n` 结尾

#### 3.3.2 适用场景

- **CLI 工具**: 本地命令行工具与 AgentOS 交互
- **脚本集成**: Shell/Python 脚本调用 AgentOS 功能
- **进程间通信**: 父子进程之间的通信

### 3.4 认证机制

#### 3.4.1 HTTP 认证

**Bearer Token 方式:**
```http
Authorization: Bearer <API_KEY>
```

**Basic Auth 方式 (不推荐):**
```http
Authorization: Basic base64(username:password)
```

#### 3.4.2 WebSocket 认证

**URL 参数方式:**
```
ws://localhost:18789/ws?token=<JWT_TOKEN>
```

**握手后认证:**
```json
{
  "jsonrpc": "2.0",
  "method": "auth.login",
  "params": {
    "api_key": "<API_KEY>"
  },
  "id": 0
}
```

#### 3.4.3 认证策略配置

认证策略由 `manager/security.yaml` 配置：

```yaml
authentication:
  enabled: true
  methods:
    - bearer_token
    - jwt
  token_expiry: 3600  # 秒
  refresh_enabled: true
```

---

## 第 4 章 服务间通信协议

### 4.1 JSON-RPC 2.0 基础

#### 4.1.1 请求格式

```json
{
  "jsonrpc": "2.0",
  "method": "string",
  "params": {},
  "id": "string|number|null"
}
```

**字段说明:**
- `jsonrpc`: 固定为 `"2.0"`
- `method`: 方法名，采用 `service.method` 格式
- `params`: 方法参数，可以是对象或数组
- `id`: 请求 ID，用于匹配响应，通知消息可为 `null`

#### 4.1.2 响应格式

**成功响应:**
```json
{
  "jsonrpc": "2.0",
  "result": {},
  "id": "string|number"
}
```

**错误响应:**
```json
{
  "jsonrpc": "2.0",
  "error": {
    "code": -32000,
    "message": "string",
    "data": {}
  },
  "id": "string|number"
}
```

### 4.2 标准错误码

| 错误码 | 含义 | 说明 |
|--------|------|------|
| -32700 | Parse error | JSON 解析失败 |
| -32600 | Invalid Request | 请求格式无效 |
| -32601 | Method not found | 方法不存在 |
| -32602 | Invalid params | 参数无效 |
| -32603 | Internal error | 服务器内部错误 |
| -32000..-32099 | Server error | 服务器自定义错误 (对应 `agentos_error_t`) |

### 4.3 服务方法命名规范

方法名采用 `service.method` 格式，例如：

```
llm.complete              # LLM 服务：完成文本生成
llm.complete_stream       # LLM 服务：流式生成
tool.execute              # 工具服务：执行工具
market.search_agents      # 市场服务：搜索 Agent
market.install_skill      # 市场服务：安装技能
sched.dispatch            # 调度服务：任务分发
memory.write              # 记忆服务：写入记忆
memory.search             # 记忆服务：搜索记忆
```

### 4.4 直接 RPC 通信

除通过 Habitat 网关间接通信外，服务之间也可通过直接 RPC 通信：

**gRPC 方式:**
```protobuf
service LLMService {
  rpc Complete(CompleteRequest) returns (CompleteResponse);
  rpc CompleteStream(CompleteRequest) returns (stream CompleteChunk);
}
```

**JSON-RPC over HTTP:**
```http
POST http://llm_d:8080/rpc
Content-Type: application/json

{"jsonrpc":"2.0","method":"llm.complete","params":{...},"id":1}
```

---

## 第 5 章 系统调用协议

### 5.1 概述

系统调用是用户态进入内核的唯一入口，定义在 [`atoms/syscall/include/syscalls.h`](../../atoms/syscall/include/syscalls.h) 中。所有系统调用遵循统一的错误处理约定。

### 5.2 函数签名规范

#### 5.2.1 返回值类型

系统调用函数返回 `agentos_error_t` 类型 (整数错误码):

```c
typedef int32_t agentos_error_t;

// 常见错误码
#define AGENTOS_SUCCESS      0    // 成功
#define AGENTOS_EINVAL      -1    // 参数无效
#define AGENTOS_ENOMEM      -2    // 内存不足
#define AGENTOS_EBUSY       -3    // 资源忙
#define AGENTOS_ENOENT      -4    // 资源不存在
#define AGENTOS_EPERM       -5    // 权限不足
#define AGENTOS_ETIMEDOUT   -6    // 操作超时
```

完整错误码列表见 [第 7 章 错误码](#第 7 章 错误码)。

#### 5.2.2 参数传递规则

- **输入参数**: 通过值或 `const` 指针传递
- **输出参数**: 通过指针传递，调用者负责分配和释放内存
- **输入输出参数**: 通过指针传递，既传入初始值，也接收返回值

**示例:**
```c
// input: 输入参数 (const 指针)
// input_len: 输入长度
// timeout_ms: 超时时间
// out_result: 输出参数 (指针的指针，调用者需 free)
agentos_error_t agentos_sys_task_submit(
    const char* input,
    size_t input_len,
    uint32_t timeout_ms,
    char** out_result
);
```

### 5.3 内存管理

#### 5.3.1 内核分配的内存

内核分配的内存 (如 `out_result`) 由调用者通过 `free()` 释放：

```c
char* result = NULL;
agentos_error_t err = agentos_sys_task_submit("...", 10, 30000, &result);
if (err == AGENTOS_SUCCESS) {
    printf("Result: %s\n", result);
    free(result);  // 必须释放
}
```

#### 5.3.2 调用者分配的内存

内核不持有调用者传入的指针，调用者需保证其生命周期：

```c
char input[] = "task description";
agentos_error_t err = agentos_sys_task_submit(input, strlen(input), 30000, &result);
// input 的生命周期由调用者管理，内核不会保存引用
```

### 5.4 并发安全

所有系统调用内部使用互斥锁保护共享数据，可安全并发调用：

```c
// 多线程环境下安全使用
#pragma omp parallel for
for (int i = 0; i < 10; i++) {
    char* result;
    agentos_sys_task_submit(task[i], strlen(task[i]), 30000, &result);
    // 处理结果...
    free(result);
}
```

---

## 第 6 章 安全与可观测性

### 6.1 安全机制

#### 6.1.1 传输加密

**生产环境要求:**
- HTTP 网关必须使用 HTTPS (TLS 1.3)
- WebSocket 网关必须使用 WSS (TLS 1.3)
- 证书应来自可信 CA，定期更新

**配置示例:**
```yaml
security:
  tls:
    enabled: true
    cert_file: /etc/agentos/ssl/server.crt
    key_file: /etc/agentos/ssl/server.key
    min_version: TLS1.3
```

#### 6.1.2 认证与授权

**认证流程:**
1. 客户端提交凭证 (API Key/Token)
2. 网关验证凭证有效性
3. 验证通过，生成会话标识
4. 后续请求携带会话标识

**授权流程:**
1. 请求到达，提取方法和资源
2. 查询权限策略
3. 检查会话是否有相应权限
4. 授权通过，转发请求；否则拒绝

#### 6.1.3 输入净化

所有输入经过 sanitizer 过滤，防止注入攻击：

- **SQL 注入**: 过滤 SQL 关键字和特殊字符
- **命令注入**: 过滤 Shell 元字符
- **路径遍历**: 规范化路径，禁止 `..`
- **XSS 攻击**: 转义 HTML 标签

### 6.2 可观测性设计

#### 6.2.1 TraceID 贯穿

每个请求在入口处生成唯一 TraceID，通过以下方式传递：

**HTTP 头:**
```http
X-Trace-ID: abc123def456
```

**JSON-RPC 参数:**
```json
{
  "jsonrpc": "2.0",
  "method": "llm.complete",
  "params": {
    "_trace_id": "abc123def456"
  },
  "id": 1
}
```

**日志记录:**
```json
{
  "timestamp": 1701234567.890,
  "level": "info",
  "logger": "agentos.llm_d",
  "trace_id": "abc123def456",
  "message": "LLM request processed"
}
```

#### 6.2.2 结构化日志

所有组件使用统一的结构化日志格式，包含：

- `timestamp`: Unix 时间戳
- `level`: 日志级别 (DEBUG/INFO/WARN/ERROR/FATAL)
- `logger`: 记录器名称
- `trace_id`: 追踪 ID
- `message`: 日志消息
- `file`: 源文件名
- `line`: 源文件行号

详见 [日志格式规范](./logging_format.md)。

#### 6.2.3 指标收集

记录以下指标供监控系统采集：

**请求指标:**
- `request_total`: 请求总数
- `request_duration_seconds`: 请求耗时直方图
- `request_errors_total`: 错误请求数

**业务指标:**
- `llm_tokens_total`: Token 消耗总数
- `task_completed_total`: 完成任务数
- `agent_active_count`: 活跃 Agent 数

**导出格式:** Prometheus 兼容格式

```prometheus
# HELP agentos_request_total Total number of requests
# TYPE agentos_request_total counter
agentos_request_total{method="llm.complete",status="success"} 1234
```

---

## 第 7 章 错误码

### 7.1 系统调用错误码

错误码定义在 [`atoms/corekern/include/error.h`](../../atoms/corekern/include/error.h) 中：

| 错误码 | 值 | 说明 |
|--------|-----|------|
| `AGENTOS_SUCCESS` | 0 | 成功 |
| `AGENTOS_EINVAL` | -1 | 无效参数 |
| `AGENTOS_ENOMEM` | -2 | 内存不足 |
| `AGENTOS_EBUSY` | -3 | 资源忙 |
| `AGENTOS_ENOENT` | -4 | 资源不存在 |
| `AGENTOS_EPERM` | -5 | 权限不足 |
| `AGENTOS_ETIMEDOUT` | -6 | 操作超时 |
| `AGENTOS_EEXIST` | -7 | 资源已存在 |
| `AGENTOS_ECANCELED` | -8 | 操作取消 |
| `AGENTOS_ENOTSUP` | -9 | 不支持 |
| `AGENTOS_EIO` | -10 | I/O 错误 |
| `AGENTOS_EINTR` | -11 | 被中断 |
| `AGENTOS_EOVERFLOW` | -12 | 溢出 |
| `AGENTOS_EBADF` | -13 | 无效状态或句柄 |
| `AGENTOS_ENOTINIT` | -14 | 未初始化 |
| `AGENTOS_ERESOURCE` | -15 | 资源耗尽 |

### 7.2 JSON-RPC 错误码映射

系统调用错误码会映射到 JSON-RPC 错误码：

```python
# 伪代码示例
def map_error(agentos_error):
    if agentos_error == AGENTOS_EINVAL:
        return -32602  # Invalid params
    elif agentos_error == AGENTOS_ENOENT:
        return -32601  # Method not found
    elif agentos_error == AGENTOS_EPERM:
        return -32003  # Permission denied
    else:
        return -32000  # Server error
```

---

## 第 8 章 示例

### 8.1 系统调用示例

```c
#include "syscalls.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
    // 假设内核已初始化

    // 1. 创建会话
    char* session_id = NULL;
    if (agentos_sys_session_create("{\"user\":\"alice\"}", &session_id) != AGENTOS_SUCCESS) {
        fprintf(stderr, "Failed to create session\n");
        return 1;
    }
    printf("Session created: %s\n", session_id);
    free(session_id);

    // 2. 提交任务
    char* result = NULL;
    agentos_error_t err = agentos_sys_task_submit(
        "Develop a simple e-commerce product listing page",
        50, 30000, &result);
    if (err == AGENTOS_SUCCESS) {
        printf("Task result: %s\n", result);
        free(result);
    } else {
        fprintf(stderr, "Task failed: %d\n", err);
    }

    // 3. 关闭会话
    agentos_sys_session_close(session_id);

    return 0;
}
```

### 8.2 HTTP 请求示例

```bash
# 使用 curl 调用 HTTP 网关
curl -X POST http://localhost:18789/rpc \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer YOUR_API_KEY" \
  -d '{
    "jsonrpc": "2.0",
    "method": "llm.complete",
    "params": {
      "model": "gpt-4",
      "messages": [{"role": "user", "content": "Hello"}],
      "temperature": 0.7
    },
    "id": 1
  }'
```

**响应:**
```json
{
  "jsonrpc": "2.0",
  "result": {
    "content": "Hi there!",
    "tokens_used": 15
  },
  "id": 1
}
```

### 8.3 WebSocket 流式示例

**客户端发送:**
```json
{"jsonrpc":"2.0","method":"llm.complete_stream","params":{"prompt":"Tell me a story"},"id":2}
```

**服务端推送:**
```json
{"jsonrpc":"2.0","result":{"chunk":"Once"},"id":2}
{"jsonrpc":"2.0","result":{"chunk":" upon"},"id":2}
{"jsonrpc":"2.0","result":{"chunk":" a time"},"id":2}
{"jsonrpc":"2.0","result":{"done":true},"id":2}
```

---

## 附录 A: 快速索引

### A.1 按主题分类

**协议分层**: 第 2 章  
**HTTP 网关**: 3.1 节  
**WebSocket 网关**: 3.2 节  
**stdio 网关**: 3.3 节  
**认证机制**: 3.4 节  
**服务间通信**: 第 4 章  
**系统调用**: 第 5 章  
**安全机制**: 6.1 节  
**可观测性**: 6.2 节  
**错误码**: 第 7 章  

### A.2 常用方法列表

**LLM 服务:**
- `llm.complete`: 文本生成
- `llm.complete_stream`: 流式生成

**工具服务:**
- `tool.execute`: 执行工具

**市场服务:**
- `market.search_agents`: 搜索 Agent
- `market.install_skill`: 安装技能

**调度服务:**
- `sched.dispatch`: 任务分发

**记忆服务:**
- `memory.write`: 写入记忆
- `memory.search`: 搜索记忆

---

## 附录 B: 与其他规范的引用关系

| 引用规范 | 关系说明 |
|---------|---------|
| [架构设计原则](../架构设计原则.md) | 本规范是架构原则在通信层面的具体实现，特别是层次分解和接口稳定原则 |
| [系统调用 API 规范](./syscall_api_contract.md) | 本规范定义了系统调用的传输协议，syscall_api_contract.md 定义了具体的 API 接口 |
| [日志格式规范](./logging_format.md) | 本规范要求所有组件使用统一的日志格式，支持 TraceID 贯穿 |
| [日志打印规范](../coding_standard/Log_guide.md) | 通信过程中的日志记录应遵循日志打印规范 |
| [统一术语表](../TERMINOLOGY.md) | 本规范使用的术语定义和解释 |

---

## 参考文献

[1] AgentOS 设计哲学。../../philosophy/Design_Principles.md  
[2] 架构设计原则。../架构设计原则.md  
[3] 统一术语表。../TERMINOLOGY.md  
[4] JSON-RPC 2.0 Specification. https://www.jsonrpc.org/specification  
[5] RFC 6455: The WebSocket Protocol. https://tools.ietf.org/html/rfc6455  
[6] RFC 7231: Hypertext Transfer Protocol (HTTP/1.1). https://tools.ietf.org/html/rfc7231  

---

## 版本历史

| 版本 | 日期 | 作者 | 变更说明 |
|------|------|------|---------|
| v2.0.0 | 2026-03-22 | AgentOS 架构委员会 | 基于设计哲学和系统工程方法进行全面重构，优化结构和表达 |
| v1.0.0 | - | 初始版本 | 初始发布 |

---

**最后更新**: 2026-03-22  
**维护者**: AgentOS 架构委员会

