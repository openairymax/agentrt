Copyright (c) 2026 SPHARX. All Rights Reserved.
"From data intelligence emerges."

# AgentOS 守护进程 API 参考

**版本**: 1.0.0
**最后更新**: 2026-04-06
**状态**: 生产就绪
**协议**: JSON-RPC 2.0 over HTTP/1.1 & WebSocket
**基础路径**: `http://localhost:8001-8006/api/v1`

---

## 📋 概述

AgentOS 守护进程层提供用户态业务服务，包括：

| 服务 | 端口 | 功能描述 |
|------|------|----------|
| **llm_d** | 8001 | LLM 模型推理服务 |
| **market_d** | 8002 | Agent 技能市场 |
| **monit_d** | 8003 | 监控与告警服务 |
| **tool_d** | 8004 | 工具执行服务 |
| **sched_d** | 8005 | 任务调度服务 |
| **gateway_d** | 8006 | API 网关（统一入口） |

所有守护进程通过 **JSON-RPC 2.0** 协议对外提供服务。

---

## 🔌 通用协议规范

### 请求格式

```json
{
    "jsonrpc": "2.0",
    "method": "service.method",
    "params": {
        "key": "value"
    },
    "id": "uuid-or-integer"
}
```

### 响应格式

#### 成功响应

```json
{
    "jsonrpc": "2.0",
    "result": {
        "data": {}
    },
    "id": "uuid-or-integer"
}
```

#### 错误响应

```json
{
    "jsonrpc": "2.0",
    "error": {
        "code": -32600,
        "message": "Invalid Request",
        "data": null
    },
    "id": null
}
```

### 标准错误码

| 错误码 | 名称 | 描述 |
|--------|------|------|
| -32700 | Parse error | 解析错误（无效 JSON） |
| -32600 | Invalid Request | 无效请求 |
| -32601 | Method not found | 方法不存在 |
| -32602 | Invalid params | 无效参数 |
| -32603 | Internal error | 内部错误 |
| -32000 ~ -32099 | Server error | 服务端自定义错误 |

### 认证方式

所有请求需携带认证令牌：

```http
POST /api/v1/llm/chat HTTP/1.1
Host: localhost:8001
Content-Type: application/json
Authorization: Bearer <JWT_TOKEN>
X-Request-ID: <UUID>
```

---

## 🤖 llm_d - LLM 推理服务 (Port 8001)

### 聊天补全

**Endpoint**: `POST /api/v1/llm/chat`

#### 请求参数

```typescript
interface ChatRequest {
    messages: Array<{
        role: 'system' | 'user' | 'assistant';
        content: string;
    }>;
    model?: string;              // 模型名称（默认：gpt-4-turbo）
    temperature?: number;         // 温度参数 [0, 2]（默认：0.7）
    max_tokens?: number;          // 最大 Token 数（默认：4096）
    top_p?: number;               // 核采样 [0, 1]（默认：1.0）
    stream?: boolean;             // 是否流式输出（默认：false）
    stop?: string[];              // 停止词列表
    presence_penalty?: number;    // 存在惩罚 [-2, 2]
    frequency_penalty?: number;   // 频率惩罚 [-2, 2]
}
```

#### 响应示例

```json
{
    "jsonrpc": "2.0",
    "result": {
        "id": "chatcmpl-abc123",
        "object": "chat.completion",
        "created": 1677858242,
        "model": "gpt-4-turbo",
        "choices": [
            {
                "index": 0,
                "message": {
                    "role": "assistant",
                    "content": "Hello! How can I help you today?"
                },
                "finish_reason": "stop"
            }
        ],
        "usage": {
            "prompt_tokens": 20,
            "completion_tokens": 10,
            "total_tokens": 30
        }
    },
    "id": "req-001"
}
```

#### 流式响应示例

当 `stream: true` 时，返回 Server-Sent Events (SSE) 格式：

```
data: {"id":"chatcmpl-abc","object":"chat.completion.chunk","choices":[{"delta":{"content":"Hello"},"index":0}]}

data: {"id":"chatcmpl-abc","object":"chat.completion.chunk","choices":[{"delta":{"content":"!"},"index":0,"finish_reason":"stop"}]}

data: [DONE]
```

---

### Embedding 向量生成

**Endpoint**: `POST /api/v1/llm/embeddings`

#### 请求参数

```typescript
interface EmbeddingRequest {
    input: string | string[];      // 输入文本或文本数组
    model?: string;                 // 模型名称（默认：text-embedding-ada-002）
    dimensions?: number;            // 输出维度（可选降维）
}
```

#### 响应示例

```json
{
    "jsonrpc": "2.0",
    "result": {
        "object": "list",
        "data": [
            {
                "object": "embedding",
                "embedding": [0.0023, -0.0094, ...],
                "index": 0
            }
        ],
        "model": "text-embedding-ada-002",
        "usage": {
            "prompt_tokens": 5,
            "total_tokens": 5
        }
    },
    "id": "req-002"
}
```

---

### Token 计数

**Endpoint**: `POST /api/v1/llm/tokens/count`

#### 请求参数

```typescript
interface TokenCountRequest {
    text: string;
    model?: string;
}
```

#### 响应示例

```json
{
    "jsonrpc": "2.0",
    "result": {
        "token_count": 25,
        "character_count": 120,
        "estimated_cost_usd": 0.000375
    },
    "id": "req-003"
}
```

---

## 🛒 market_d - 技能市场服务 (Port 8002)

### 列出可用技能

**Endpoint**: `GET /api/v1/market/skills`

#### 查询参数

| 参数 | 类型 | 必填 | 描述 |
|------|------|------|------|
| category | string | 否 | 技能分类（如：data_analysis, web_scraping） |
| limit | integer | 否 | 返回数量限制（默认：20，最大：100） |
| offset | integer | 否 | 分页偏移量（默认：0） |
| sort_by | string | 否 | 排序字段（popular, newest, rating） |
| search | string | 否 | 关键词搜索 |

#### 响应示例

```json
{
    "jsonrpc": "2.0",
    "result": {
        "skills": [
            {
                "id": "skill-web-scrape-v1",
                "name": "Web Scraper",
                "description": "高性能网页数据抓取技能",
                "version": "1.2.0",
                "category": "web_scraping",
                "author": "@spharx-team",
                "rating": 4.8,
                "download_count": 15234,
                "tags": ["scraping", "parsing", "data-extraction"],
                "permissions_required": ["network.access", "file.write"],
                "input_schema": { "$ref": "#/definitions/WebScrapeInput" },
                "output_schema": { "$ref": "#/definitions/WebScrapeOutput" }
            }
        ],
        "total": 156,
        "limit": 20,
        "offset": 0
    },
    "id": "req-004"
}
```

---

### 安装技能

**Endpoint**: `POST /api/v1/market/skills/install`

#### 请求参数

```typescript
interface InstallSkillRequest {
    skill_id: string;
    version?: string;           // 版本号（默认：最新版）
    config?: Record<string, any>; // 安装配置
}
```

#### 响应示例

```json
{
    "jsonrpc": "2.0",
    "result": {
        "skill_id": "skill-web-scrape-v1",
        "installed_version": "1.2.0",
        "status": "installed",
        "install_path": "/app/skills/web-scraper/",
        "dependencies": [
            {"name": "beautifulsoup4", "version": "4.12.0"},
            {"name": "requests", "version": "2.31.0"}
        ]
    },
    "id": "req-005"
}
```

---

## 📊 monit_d - 监控服务 (Port 8003)

### 获取系统指标

**Endpoint**: `GET /api/v1/monitoring/metrics`

#### 查询参数

| 参数 | 类型 | 必填 | 描述 |
|------|------|------|------|
| scope | string | 否 | 指标范围（system, kernel, agents, memory） |
| granularity | string | 否 | 时间粒度（1m, 5m, 15m, 1h, 1d） |
| start_time | datetime | 否 | 开始时间（ISO 8601） |
| end_time | datetime | 否 | 结束时间（ISO 8601） |

#### 响应示例

```json
{
    "jsonrpc": "2.0",
    "result": {
        "timestamp": "2026-04-06T10:00:00Z",
        "metrics": {
            "system": {
                "cpu_usage_percent": 45.2,
                "memory_usage_bytes": 8589934592,
                "memory_usage_percent": 65.3,
                "disk_usage_percent": 42.1,
                "load_average_1m": 2.34,
                "load_average_5m": 2.12,
                "load_average_15m": 1.98
            },
            "kernel": {
                "ipc_requests_per_sec": 1250.5,
                "avg_ipc_latency_ms": 0.85,
                "active_tasks_count": 42,
                "queued_tasks_count": 7,
                "syscall_success_rate": 99.97
            },
            "memory": {
                "l1_records_total": 1523456,
                "l2_index_size": 987654,
                "l3_relations_total": 45678,
                "l4_patterns_total": 1234,
                "l2_query_p99_latency_ms": 8.5
            },
            "agents": {
                "active_agents_count": 15,
                "total_agents_count": 128,
                "avg_session_duration_min": 23.5,
                "tasks_completed_today": 892
            }
        }
    },
    "id": "req-006"
}
```

---

### 创建告警规则

**Endpoint**: `POST /api/v1/monitoring/alerts/rules`

#### 请求参数

```typescript
interface CreateAlertRuleRequest {
    name: string;
    description?: string;
    metric_path: string;          // Prometheus 指标路径
    condition: 'gt' | 'lt' | 'eq' | 'ne' | 'gte' | 'lte';
    threshold: number;
    duration_sec?: number;       // 持续时间（默认：60s）
    severity: 'info' | 'warning' | 'critical';
    channels: Array<'email' | 'slack' | 'webhook' | 'pagerduty'>;
    recipients: string[];        // 通知接收者
    enabled?: boolean;           // 是否启用（默认：true）
    cooldown_min?: number;       # 冷却时间（默认：5分钟）
}
```

#### 响应示例

```json
{
    "jsonrpc": "2.0",
    "result": {
        "rule_id": "alert-rule-001",
        "name": "High CPU Usage Alert",
        "status": "created",
        "created_at": "2026-04-06T10:00:00Z"
    },
    "id": "req-007"
}
```

---

## 🔧 tool_d - 工具执行服务 (Port 8004)

### 执行工具

**Endpoint**: `POST /api/v1/tools/execute`

#### 请求参数

```typescript
interface ToolExecuteRequest {
    tool_name: string;           // 工具名称或 ID
    action: string;              // 操作名称
    params: Record<string, any>; // 操作参数
    timeout_ms?: number;         // 超时时间（默认：30000ms）
    sandbox_config?: {           // 沙箱配置
        enable_network?: boolean;
        max_memory_mb?: number;
        allowed_paths?: string[];
    };
}
```

#### 响应示例

```json
{
    "jsonrpc": "2.0",
    "result": {
        "execution_id": "exec-abc123",
        "tool_name": "python-executor",
        "action": "run_script",
        "status": "completed",
        "started_at": "2026-04-06T10:00:00Z",
        "completed_at": "2026-04-06T10:00:02Z",
        "duration_ms": 2015,
        "output": {
            "stdout": "Result: 42\n",
            "stderr": "",
            "exit_code": 0
        },
        "resources_used": {
            "cpu_time_ms": 1950,
            "memory_peak_mb": 128,
            "network_bytes_sent": 0,
            "network_bytes_received": 1024
        }
    },
    "id": "req-008"
}
```

---

### 列出已注册工具

**Endpoint**: `GET /api/v1/tools`

#### 响应示例

```json
{
    "jsonrpc": "2.0",
    "result": {
        "tools": [
            {
                "name": "python-executor",
                "description": "Python 代码执行环境",
                "version": "3.11.0",
                "actions": ["run_script", "check_syntax", "install_package"],
                "permissions": ["sandbox.execute"],
                "schema": { "$ref": "#/definitions/PythonExecutorSchema" }
            },
            {
                "name": "shell-command",
                "description": "Shell 命令执行器",
                "version": "1.0.0",
                "actions": ["execute", "validate"],
                "permissions": ["sandbox.execute", "filesystem.read"],
                "schema": { "$ref": "#/definitions/ShellCommandSchema" }
            }
        ]
    },
    "id": "req-009"
}
```

---

## 📅 sched_d - 任务调度服务 (Port 8005)

### 创建任务

**Endpoint**: `POST /api/v1/scheduler/tasks`

#### 请求参数

```typescript
interface CreateTaskRequest {
    name: string;
    description?: string;
    agent_id?: string;            // 执行任务的 Agent ID
    priority?: number;            // 优先级 [0-100]（默认：50）
    schedule_type: 'immediate' | 'delayed' | 'recurring' | 'cron';
    payload: Record<string, any>; // 任务负载数据
    delay_ms?: number;            // 延迟时间（schedule_type=delayed 时必填）
    cron_expression?: string;     // Cron 表达式（schedule_type=cron 时必填）
    recurring_interval_ms?: number; // 重复间隔（schedule_type=recurring 时必填）
    max_retries?: number;         // 最大重试次数（默认：3）
    retry_delay_ms?: number;      // 重试延迟（默认：1000ms）
    timeout_ms?: number;          // 任务超时（默认：300000ms）
    tags?: string[];              // 标签
    metadata?: Record<string, any>;
}
```

#### 响应示例

```json
{
    "jsonrpc": "2.0",
    "result": {
        "task_id": "task-abc123",
        "status": "queued",
        "priority": 50,
        "created_at": "2026-04-06T10:00:00Z",
        "estimated_start_at": "2026-04-06T10:00:01Z"
    },
    "id": "req-010"
}
```

---

### 查询任务状态

**Endpoint**: `GET /api/v1/scheduler/tasks/{task_id}`

#### 响应示例

```json
{
    "jsonrpc": "2.0",
    "result": {
        "task_id": "task-abc123",
        "name": "Data Analysis Task",
        "status": "running",
        "progress": 0.65,
        "created_at": "2026-04-06T10:00:00Z",
        "started_at": "2026-04-06T10:00:01Z",
        "updated_at": "2026-04-06T10:05:30Z",
        "execution_history": [
            {
                "status": "queued",
                "timestamp": "2026-04-06T10:00:00Z"
            },
            {
                "status": "running",
                "timestamp": "2026-04-06T10:00:01Z"
            }
        ],
        "result": null,
        "error": null
    },
    "id": "req-011"
}
```

---

## 🌐 gateway_d - API 网关 (Port 8006)

### 统一入口

Gateway 是所有守护进程的统一入口点，提供路由、负载均衡、限流等功能。

#### 请求路由规则

| 路径前缀 | 目标服务 | 说明 |
|----------|----------|------|
| `/api/v1/llm/*` | llm_d:8001 | LLM 推理 |
| `/api/v1/market/*` | market_d:8002 | 技能市场 |
| `/api/v1/monitoring/*` | monit_d:8003 | 监控 |
| `/api/v1/tools/*` | tool_d:8004 | 工具执行 |
| `/api/v1/scheduler/*` | sched_d:8005 | 任务调度 |
| `/health` | All | 健康检查聚合 |

#### Gateway 特性

- **路由转发**: 基于 URL 路径的服务发现与负载均衡
- **认证鉴权**: JWT Token 验证和权限检查
- **限流熔断**: 令牌桶算法 + 断路器模式
- **请求日志**: 全链路追踪和审计日志
- **协议转换**: WebSocket ↔ HTTP 双向支持

---

## 📈 性能基准

| 服务 | P50 延迟 | P99 延迟 | 吞吐量 (QPS) | 最大并发 |
|------|----------|----------|--------------|----------|
| llm_d (chat) | 500ms | 2000ms | 100 | 50 |
| llm_d (embeddings) | 50ms | 200ms | 1000 | 100 |
| market_d | 10ms | 50ms | 5000 | 200 |
| monit_d | 5ms | 20ms | 10000 | 500 |
| tool_d | 取决于工具 | - | 100 | 50 |
| sched_d | 2ms | 10ms | 20000 | 1000 |
| gateway_d | 1ms | 5ms | 50000 | 2000 |

---

## 🔐 安全最佳实践

### 1. 使用 HTTPS

生产环境必须启用 TLS 1.3：

```yaml
# gateway 配置
tls:
  enabled: true
  cert_file: /etc/ssl/certs/agentos.crt
  key_file: /etc/ssl/private/agentos.key
  min_version: TLSv1.3
  cipher_suites:
    - TLS_AES_256_GCM_SHA384
    - TLS_CHACHA20_POLY1305_SHA256
```

### 2. API Key 管理

```bash
# 生成 API Key
openssl rand -hex 32 > api_key.txt

# 设置环境变量
export AGENTOS_API_KEY=$(cat api_key.txt)
```

### 3. IP 白名单

```yaml
# 仅允许内网访问
ip_whitelist:
  - "10.0.0.0/8"
  - "172.16.0.0/12"
  - "192.168.0.0/16"
```

---

## 🧪 测试示例

### cURL 示例

```bash
# LLM 聊天补全
curl -X POST http://localhost:8001/api/v1/llm/chat \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $TOKEN" \
  -d '{
    "messages": [{"role": "user", "content": "Hello"}],
    "temperature": 0.7
  }'

# 列出技能
curl http://localhost:8002/api/v1/market/skills?category=data_analysis&limit=10 \
  -H "Authorization: Bearer $TOKEN"

# 获取系统指标
curl "http://localhost:8003/api/v1/monitoring/metrics?scope=kernel&granularity=5m" \
  -H "Authorization: Bearer $TOKEN"

# 执行工具
curl -X POST http://localhost:8004/api/v1/tools/execute \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $TOKEN" \
  -d '{
    "tool_name": "python-executor",
    "action": "run_script",
    "params": {"code": "print(42)"},
    "timeout_ms": 5000
  }'

# 创建定时任务
curl -X POST http://localhost:8005/api/v1/scheduler/tasks \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $TOKEN" \
  -d '{
    "name": "Daily Report",
    "schedule_type": "cron",
    "cron_expression": "0 8 * * *",
    "payload": {"type": "report"}
  }'
```

### Python SDK 示例

```python
from agentos import DaemonClient

client = DaemonClient(
    base_url="http://localhost:8006",
    api_key="your-api-key"
)

# LLM 聊天
response = client.llm.chat(
    messages=[{"role": "user", "content": "Hello!"}],
    stream=False
)
print(response.choices[0].message.content)
```

---

## 📚 相关文档

- **[内核 API](kernel-api.md)** — Syscall 接口参考
- **[Python SDK](python-sdk.md)** — Python 语言绑定
- **[Go SDK](go-sdk.md)** — Go 语言绑定
- **[错误码手册](error-codes.md)** — 完整错误码定义
- **[契约规范 - 协议](../../agentos/manuals/specifications/agentos_contract/protocol_contract.md)** — JSON-RPC 2.0 规范

---

**© 2026 SPHARX Ltd. All Rights Reserved.**

*"From data intelligence emerges."*
